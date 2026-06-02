![WIP](https://img.shields.io/badge/status-wip-orange)

🚧 Work In Progress (WIP) 🚧  
このプロジェクトはまだ開発段階です。

# MOFS

MOFS (My Original File System) は C99 で書かれた学習用ファイルシステムです。
オンディスクフォーマット、コア実装、POSIX 風 API、そして Linux 上でイメージを
フォーマット／FUSE 経由でマウントするツールを、なるべく小さく自前で実装しています。

## Features

- スーパーブロック、inode/data ビットマップ、inode テーブル、データ領域からなるオンディスクフォーマット
- 64 バイト inode と、オンディスク list node の連鎖によるデータブロック管理
- 呼び出し元ユーザ情報に基づく Unix 風の owner/group/other パーミッションチェック
- 通常ファイル／ディレクトリ（`.` / `..`）と、`atime` / `mtime` / `ctime`
- POSIX 風 API: `open`, `close`, `read`, `write`, `pread`, `pwrite`, `truncate`, `ftruncate`,
  `unlink`, `stat`, `mkdir`, `rmdir`, `opendir`, `readdir`, `closedir`
- ツール: イメージをフォーマットする `mkfs.mofs` と、FUSE マウントツール（`mofs`）
- OS / core / POSIX 各レイヤをカバーする cmocka ベースのテスト

## Architecture

MOFS は静的ライブラリをレイヤとして積み重ねています。依存関係は下方向に流れます。

```
tools (mkfs.mofs, mofs/FUSE)
        │
        ▼
   posix_api          src/posix/
        │
        ▼
mofs_core / mofs_format   src/core/
        │
        ▼
   os_service          src/os/<platform>/service/
```

| Layer          | Library        | Location                          |
|----------------|----------------|-----------------------------------|
| OS abstraction | `os_service`   | `src/os/linux/service/`           |
| Formatter      | `mofs_format`  | `src/core/modules/mofs_format.c`  |
| Filesystem core| `mofs_core`    | `src/core/modules/`               |
| POSIX wrapper  | `posix_api`    | `src/posix/`                      |
| Tools          | `mkfs.mofs`, `mofs` | `src/os/linux/tools/`        |

公開ヘッダは `include/`（例: `mofs_posix.h`, `mofs_format.h`, `mofs_lifecycle.h`）にあります。
core 内部ヘッダは `src/core/include/`、プラットフォーム抽象化ヘッダは `src/os/<platform>/include/` にあります。

## Directory structure

```
.
├── CMakeLists.txt
├── include/            # 公開 API ヘッダ（posix/ を含む）
├── src/
│   ├── core/           # format / inode / block / dir / path / perm / file / lifecycle
│   ├── posix/          # POSIX 風 API ラッパ
│   └── os/linux/       # OS service、ヘッダ、ツール（mkfs, fuse）
├── test/               # cmocka テスト（os / core / posix）
└── docs/               # オンディスク構造などのメモ
```

## Build

MOFS は CMake（3.10+、C99）でビルドします。デフォルトは `Debug`（`-O0 -g3`）です。

```sh
cmake -S . -B build
cmake --build build
```

生成物（典型パス）:

- `build/src/os/linux/tools/mkfs/mkfs.mofs`
- `build/src/os/linux/tools/fuse/mofs`

### Dependencies

- [cmocka](https://cmocka.org/) — ユニットテスト（`libcmocka-dev`）
- [libfuse 3](https://github.com/libfuse/libfuse) — FUSE マウントツール（`fuse3`、`pkg-config` で検出）

## Usage

### イメージをフォーマットする

```sh
# バッキングとなるイメージファイルを作成（例: 64 MiB）
dd if=/dev/zero of=mofs.img bs=1M count=64

# フォーマットする（ブロックサイズは 4096 がデフォルト。512..65536 の 2 の冪）
build/src/os/linux/tools/mkfs/mkfs.mofs mofs.img

# オプション:
#   -s, --size  <NUM>  ファイルシステムサイズ（ブロック数、デフォルト: 自動）
#   -b, --block <NUM>  論理ブロックサイズ（バイト）
```

### FUSE でマウントする

```sh
mkdir -p /tmp/mofs
build/src/os/linux/tools/fuse/mofs mofs.img /tmp/mofs
```

## Testing

テストは cmocka を使い、CTest から実行します。

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

# 利用可能なテスト一覧:
ctest --test-dir build -N
```

テストは `test/os/linux/`, `test/core/`, `test/posix/` に分かれており、共通のヘルパは `test/fixtures/` にあります。

## On-disk layout

フォーマット済みボリュームは、絶対ブロック番号で概ね次のように配置されます。

```
┌────────────┬──────────────┬─────────────┬─────────────┬───────────────────────────┐
│ Superblock │ Inode bitmap │ Data bitmap │ Inode table │ Data region               │
│  (block 0) │              │             │ (64B/inode) │ file data + list nodes    │
└────────────┴──────────────┴─────────────┴─────────────┴───────────────────────────┘
```

各ファイルの inode は list node の連鎖を指し、各 list node はデータブロックの絶対ブロック番号配列を保持します。

### Limits

- マジックナンバー: `0x53464F4D`（"MOFS"）
- デフォルトブロックサイズ: 4096 バイト（有効範囲 512–65536、2 の冪）
- 1 ファイルあたり最大データブロック数: 1024（ブロックサイズ 4KiB の場合 4MiB）
- ファイル名長: 28 バイト
- ルートディレクトリ inode: #2

