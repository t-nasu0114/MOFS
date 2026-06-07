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
- 公開する型は `mofs_` プレフィックス付き typedef（例: `mofs_off_t`, `mofs_uid_t`）。
- 型の契約入口は `src/port/include/mofs_port_types.h`（`mofs_os_types.h` を include）。
- 型の実体は各 `src/os/<platform>/include/mofs_os_types.h` に置く（Linux では `<stdint.h>` ベース）。
- 公開エントリは `include/mofs_types.h`。
- デバイス I/O の抽象 API は `dev_` で始める（`src/port/include/mofs_devio.h` の `dev_open`, `dev_read` など）。オープン・シーク用のフラグは `MOFS_IO_OPEN_FLAG_*`, `MOFS_SEEK_*` を使う。

### 2.2 ヘッダの置き場所とインクルードガード

- 外部に公開する API・型は `include/mofs_*.h` に置く。
- OS 移植契約 (HAL) は `src/port/include/mofs_port_*.h` および `src/port/include/mofs_devio.h` に置く。
- core モジュール間の内部共有は `src/core/include/` に置く（`mofs_inode.h` 等）。
- そのヘッダのインクルードガードは `__MOFS_<名前>__` 形式とする（例: `__MOFS_CORE__`, `__MOFS_ERRNO__`, `__MOFS_PORT_MEM__`）。

### 2.3 グローバルコンテキスト

- マウント単位の状態は `mofs_ctx_t` 型の `ctx` で持ち、`include/mofs_core.h` で `extern` 宣言し、core の `.c` で定義する。

### 2.4 エラー値（MOFS errno）

- 成功は `0`。失敗時は `mofs_errno.h` の `MOFS_E*`（POSIX の errno 番号に揃えた正の整数）を返す。
- core 層の API は戻り値で `MOFS_E*` を返す。OS の `errno` は core から更新しない。
- OS の `errno` を MOFS 側の値に揃えるときは `get_errno()` を使う（宣言は `src/port/include/mofs_port_errno.h`、実装は `os_errno.c`）。
- OS の errno 値と明示的に変換するときは `os_to_mofs_errno` / `mofs_to_os_errno` を使う（同上、`mofs_port_errno.h`）。`MOFS_E*` 定数は `include/mofs_errno.h`。
- 引数不正など FS 独自の検証失敗には `MOFS_EINVAL` を使う例がある。
- POSIX 層（`src/posix/`）は OS の `errno` を更新せず、スレッドローカルな `mofs_errno`（`MOFS_E*` 値）を更新する。公開 API は `include/posix/mofs_posix_errno.h` の `mofs_errno` マクロ。
- `mofs_errno_location()` の実装は OS 抽象層（例: Linux では `os_posix_errno.c` の `pthread_key_t`）。非 Linux 向けは同シグネチャで差し替える。

### 2.5 ブロック I/O とレイアウト定数

- 論理ブロックサイズはオンディスクのスーパーブロックおよび `ctx.sp_blk.blk_size` が真実であり、既定のフォーマット引数のみ `MOFS_BLK_SIZE_DEFAULT`（4096、`MOFS_BLK_SIZE` と同値）である。連続ブロックの読み書きは `read_continuous_blocks` / `write_continuous_blocks` 経由。

### 2.6 メモリ

- core からのヒープ利用は `src/port/include/mofs_port_mem.h` の `mofs_malloc` / `mofs_free` / `mofs_memcpy` を使う（OS 層で libc に委譲）。

### 2.7 ログ

- `src/port/include/mofs_port_log.h` の `mofs_log_dbg`, `mofs_log_inf`, `mofs_log_wrn`, `mofs_log_err` で出力する。プレフィックスは `[DBG]` など固定形式。Linux 実装は `os_log.c`。

### 2.8 コメント（Doxygen 風）

- 基本的にすべての関数で `@brief`, 本文に「Function behavior:」の箇条書き、`@param`, `@return` を付けるスタイルが使われている。
- 関数の中にも、処理の流れを説明するコメントを入れて可読性を上げるように務める。

### 2.9 FUSE コールバックと errno の符号

- libfuse のコールバックでは、POSIX 層 API 失敗後に `mofs_errno`（`MOFS_E*`）を読み、`mofs_to_os_errno()` で OS errno に変換して負値で返す（例: `return -(mofs_to_os_errno(mofs_errno));`）。
- core API を直接呼ぶ箇所では、戻り値の `MOFS_E*` を `mofs_to_os_errno()` 経由で返す。

## 3. 個別ルール

### 3.1 `src/core`（本体ロジック）

- ファイルシステムの入口に近い操作で `include/mofs_core.h` に載せる API は、初期化・属性取得など `_core` サフィックスの名前（`mofs_init_core`, `mofs_getattr_core`, `mofs_readdir_core`）になっている。
- ブロック単位のヘルパやパス解決の内部処理は `src/core/modules/mofs_block.h` 等の core 内部ヘッダ / 同一 `.c` 内に置く。
- `mofs_format` は `mofs_format.c` で定義されているが、現状トップレベル `include/` には宣言がなく、`mkfs` 側で `extern` 宣言している（移行予定のコメントあり）。新規コードではこのずれを踏まえるか、ヘッダに載せるなど方針を揃える。
- ここに実装される関数は POSIX に近い実装としておく。

### 3.1.1 `src/posix`（POSIX API 層）

- `mofs_*`（`_core` なし）API は core を呼び出し、失敗時に `mofs_errno` を更新する。`<errno.h>` に依存しない。
- 成功時は `mofs_errno` をクリアしない（POSIX `errno` と同様）。`mofs_readdir` の EOF 時も `mofs_errno` を変更しない。

### 3.2 `src/os/linux`（OS 抽象の Linux 実装）

- `os_devio.c` はファイル先頭で `#define _GNU_SOURCE` してからシステムヘッダを読む。
- `get_errno` / `os_to_mofs_errno` / `mofs_to_os_errno` の実体は `os_errno.c` に置き、`<errno.h>` の定数と `mofs_errno.h` を対応付ける。
- `mofs_log_*` の実体は `os_log.c` に置く。
- `mofs_os_types.h` の実体は `src/os/linux/include/mofs_os_types.h` に置く（他 platform も同ファイル名で差し替え）。
- `mofs_errno_location()` の実体は `os_posix_errno.c` に置き、`pthread_key_t` でスレッドごとの `mofs_errno` スロットを確保する。

### 3.3 `src/proc/linux/fuse`（FUSE 統合）

- ソース先頭に `/* ファイル名.c` と 1 行説明のブロックコメントを付けるファイルがある（`fuse.c`, `fuse_ops.c`）。
- コールバックと `mofs_fuse_ctx_t` は `fuse_ops.h` に宣言。インクルードガードは `MOFS_FUSE_OPS_H`（他ヘッダの `__MOFS_*__` 形式とは別パターン）。
- `struct fuse_operations op` をグローバルで定義し、`fuse_main` に渡す。
- 実行ファイル名を Usage 表示に使う `SELF_NAME` は、CMake の `target_compile_definitions` でビルド時に定義されている。

### 3.4 `src/os/linux/tools/mkfs`（mkfs ツール）

- `-b` で論理ブロックサイズを渡し `mofs_format` に伝える（`-1`/省略時はデフォルト 4096 バイト）。非対応サイズは `mofs_validate_logical_blk_size` で拒否される。
