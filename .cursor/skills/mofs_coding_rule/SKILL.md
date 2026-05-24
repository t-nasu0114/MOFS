---
name: mofs_coding_rule
description: MOFS のコーディングルール。MOFS の実装を行うときに使います。
---

# MOFS coding rule

## 1. 概要

この Skill は MOFS のコーディングルールについて記載しています。
MOFS の実装を行うときに使います。

## 2. 共通ルール

### 2.1 名前と公開 API

- 公開する識別子には `mofs_` プレフィックスを付ける（例: `mofs_init_core`, `mofs_read_inode`）。
- 公開する型は `mofs_` で始まる `struct` と、 typedef 名は `*_t` で終える（例: `mofs_inode_t`, `mofs_ctx_t`）。
- マクロ・定数は `MOFS_` で始める（例: `MOFS_BLK_SIZE`, `MOFS_MAGIC_NUM`, `MOFS_FTYPE_REG`）。
- デバイス I/O の抽象 API は `dev_` で始める（`mofs_devio.h` の `dev_open`, `dev_read` など）。オープン・シーク用のフラグは `MOFS_IO_OPEN_FLAG_*`, `MOFS_SEEK_*` を使う。

### 2.2 ヘッダの置き場所とインクルードガード

- 外部に公開する API・型は `include/mofs_*.h` に置く。
- そのヘッダのインクルードガードは `__MOFS_<名前>__` 形式とする（例: `__MOFS_CORE__`, `__MOFS_ERRNO__`）。
- core 内だけで共有する宣言は `src/core/mofs_util.h` のように、トップレベル `include/` ではなく実装側に置く（現状の分離方針）。

### 2.3 グローバルコンテキスト

- マウント単位の状態は `mofs_ctx_t` 型の `ctx` で持ち、`include/mofs_core.h` で `extern` 宣言し、core の `.c` で定義する。

### 2.4 エラー値（MOFS errno）

- 成功は `0`。失敗時は `mofs_errno.h` の `MOFS_E*`（POSIX の errno 番号に揃えた正の整数）を返す。
- OS の `errno` を MOFS 側の値に揃えるときは `get_errno()` を使う（実装はプラットフォーム側、例: `errno_linux.c`）。
- OS の errno 値と明示的に変換するときは `os_to_mofs_errno` / `mofs_to_os_errno` を使う。マッピングに無い値は `MOFS_EIO` / `EIO` に落とす実装になっている。
- 引数不正など FS 独自の検証失敗には `MOFS_EINVAL` を使う例がある。

### 2.5 ブロック I/O とレイアウト定数

- 論理ブロックサイズはオンディスクのスーパーブロックおよび `ctx.sp_blk.blk_size` が真実であり、既定のフォーマット引数のみ `MOFS_BLK_SIZE_DEFAULT`（4096、`MOFS_BLK_SIZE` と同値）である。連続ブロックの読み書きは `read_continuous_blocks` / `write_continuous_blocks` 経由。

### 2.6 メモリ

- core からのヒープ利用は `mofs_mem.h` の `mofs_malloc` / `mofs_free` / `mofs_memcpy` を使う（OS 層で libc に委譲）。

### 2.7 ログ

- `mofs_log.h` の `MOFS_DBG`, `MOFS_INF`, `MOFS_WRN`, `MOFS_ERR` で出力する。プレフィックスは `[DBG]` など固定形式。

### 2.8 コメント（Doxygen 風）

- core や FUSE 層の関数には `@brief`, 本文に「Function behavior:」の箇条書き、`@param`, `@return` を付けるスタイルが使われている。

### 2.9 FUSE コールバックと errno の符号

- libfuse のコールバックでは、MOFS 側の正の `MOFS_E*` を負の OS errno に変換して返す（例: `return -(mofs_to_os_errno(ret));`）。

## 3. 個別ルール

### 3.1 `src/core`（本体ロジック）

- ファイルシステムの入口に近い操作で `include/mofs_core.h` に載せる API は、初期化・属性取得など `_core` サフィックスの名前（`mofs_init_core`, `mofs_getattr_core`, `mofs_readdir_core`）になっている。
- ブロック単位のヘルパやパス解決の内部処理は `mofs_util.h` / `mofs_util.c` に集約し、必要な `.c` は `"mofs_util.h"` でインクルードする。
- `mofs_format` は `mofs_format.c` で定義されているが、現状トップレベル `include/` には宣言がなく、`mkfs` 側で `extern` 宣言している（移行予定のコメントあり）。新規コードではこのずれを踏まえるか、ヘッダに載せるなど方針を揃える。
- ここに実装される関数は POSIX に近い実装としておく。

### 3.2 `src/os/linux`（OS 抽象の Linux 実装）

- `devio_linux.c` はファイル先頭で `#define _GNU_SOURCE` してからシステムヘッダを読む。
- `get_errno` / `os_to_mofs_errno` / `mofs_to_os_errno` の実体は `errno_linux.c` に置き、`<errno.h>` の定数と `mofs_errno.h` を対応付ける。

### 3.3 `src/proc/linux/fuse`（FUSE 統合）

- ソース先頭に `/* ファイル名.c` と 1 行説明のブロックコメントを付けるファイルがある（`fuse.c`, `fuse_ops.c`）。
- コールバックと `mofs_fuse_ctx_t` は `fuse_ops.h` に宣言。インクルードガードは `MOFS_FUSE_OPS_H`（他ヘッダの `__MOFS_*__` 形式とは別パターン）。
- `struct fuse_operations op` をグローバルで定義し、`fuse_main` に渡す。
- 実行ファイル名を Usage 表示に使う `SELF_NAME` は、CMake の `target_compile_definitions` でビルド時に定義されている。

### 3.4 `src/os/linux/tools/mkfs`（mkfs ツール）

- `-b` で論理ブロックサイズを渡し `mofs_format` に伝える（`-1`/省略時はデフォルト 4096 バイト）。非対応サイズは `mofs_validate_logical_blk_size` で拒否される。
