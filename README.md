![WIP](https://img.shields.io/badge/status-wip-orange)

🚧 Work In Progress (WIP) 🚧  
このプロジェクトはまだ開発段階です。

# MOFS

MOFS (My Original File System) は C99 で書かれた学習用ファイルシステムです。
オンディスクフォーマット、コア実装、POSIX 風 API、そして Linux 上でイメージを
フォーマット／FUSE 経由でマウントするツールを、なるべく小さく自前で実装しています。
Zephyr では VFS 経由で同じコアを載せられます。

## Features

- スーパーブロック、inode/data ビットマップ、inode テーブル、データ領域からなるオンディスクフォーマット
- 64 バイト inode と、オンディスク list node の連鎖によるデータブロック管理
- 呼び出し元ユーザ情報に基づく Unix 風の owner/group/other パーミッションチェック
- 通常ファイル／ディレクトリ（`.` / `..`）と、`atime` / `mtime` / `ctime`
- POSIX 風 API: `open`, `close`, `read`, `write`, `pread`, `pwrite`, `truncate`, `ftruncate`,
  `unlink`, `stat`, `mkdir`, `rmdir`, `opendir`, `readdir`, `closedir`, `rename`, `fsync`, `lseek`
- ツール: イメージをフォーマットする `mkfs.mofs` と、FUSE マウントツール（`mofs`）
- Zephyr VFS アダプタと評価用ホスト（`src/os/zephyr/tools/vfs/`、既定で FS shell 有効）
- OS / core / POSIX 各レイヤをカバーする cmocka ベースのテスト（Linux）
- Zephyr 共通 FS ztest ハーネス（`test/os/zephyr/`）

## Architecture

MOFS は静的ライブラリをレイヤとして積み重ねています。依存関係は下方向に流れます。

```
tools (mkfs.mofs, mofs/FUSE, Zephyr VFS host / ztest)
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

| Layer          | Library / app        | Location                          |
|----------------|----------------------|-----------------------------------|
| OS abstraction | `os_service`         | `src/os/linux/service/`, `src/os/zephyr/service/` |
| Formatter      | `mofs_format`        | `src/core/modules/mofs_format.c`  |
| Filesystem core| `mofs_core`          | `src/core/modules/`               |
| POSIX wrapper  | `posix_api`          | `src/posix/`                      |
| Tools (Linux)  | `mkfs.mofs`, `mofs`  | `src/os/linux/tools/`             |
| VFS (Zephyr)   | `mofs_vfs.c`         | `src/os/zephyr/tools/vfs/`        |
| Host / tests   | `mofs_vfs_main`, ztest | `src/os/zephyr/tools/vfs/`, `test/os/zephyr/` |

公開ヘッダは `include/`（例: `mofs_posix.h`, `mofs_format.h`, `mofs_lifecycle.h`）にあります。
core 内部ヘッダは `src/core/include/`、プラットフォーム抽象化ヘッダは `src/os/<platform>/include/` にあります。
Zephyr west モジュール定義は [`zephyr/module.yml`](zephyr/module.yml) です。

## Directory structure

```
.
├── CMakeLists.txt
├── include/            # 公開 API ヘッダ（posix/ を含む）
├── src/
│   ├── core/           # format / inode / block / dir / path / perm / file / lifecycle
│   ├── posix/          # POSIX 風 API ラッパ
│   ├── port/           # 移植契約 (HAL)
│   └── os/
│       ├── linux/      # OS service、ヘッダ、ツール（mkfs, fuse）
│       └── zephyr/     # OS service、ヘッダ、VFS（mofs_vfs.c / mofs_vfs_main）
├── zephyr/             # west モジュール（CMake / Kconfig）
├── test/
│   ├── os/linux/       # cmocka（Linux）
│   ├── os/zephyr/      # Zephyr 共通 FS ztest ハーネス
│   ├── core/
│   ├── posix/
│   └── fixtures/
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

### Build options

`cmake -S . -B build` の際に `-D<NAME>=<VALUE>` で指定できます。定義はルート [`CMakeLists.txt`](CMakeLists.txt) を参照してください。

| オプション | 既定値 | 説明 |
|-----------|--------|------|
| `CMAKE_BUILD_TYPE` | `Debug` | ビルド種別（`Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel`）。`Debug` は `-O0 -g3` |
| `MOFS_ENABLE_BUFFER_CACHE` | `ON` | ブロックバッファキャッシュの有効化（`ON` / `OFF`） |
| `MOFS_BCACHE_IMPL` | `unified` | キャッシュ実装の選択（`MOFS_ENABLE_BUFFER_CACHE=ON` 時のみ）。`unified`: 単一 LRU プール / `split`: メタデータ・データの 2 プール（実験用） |

例:

```sh
# 既定（Debug、キャッシュ ON、unified）
cmake -S . -B build

# リリースビルド
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# キャッシュなし（ベンチマーク比較用）
cmake -S . -B build -DMOFS_ENABLE_BUFFER_CACHE=OFF

# 分割プール実装（実験用）
cmake -S . -B build -DMOFS_ENABLE_BUFFER_CACHE=ON -DMOFS_BCACHE_IMPL=split
```

configure 済みの値は `build/CMakeCache.txt` で確認できます。

キャッシュの動作・プールサイズ等の compile-time マクロ（`MOFS_BUFFER_CACHE_NUM` など）は [`src/core/include/mofs_config.h`](src/core/include/mofs_config.h) に既定値があり、詳細は [`docs/buffer_cache.md`](docs/buffer_cache.md) を参照してください。

### Dependencies

- [cmocka](https://cmocka.org/) — ユニットテスト（`libcmocka-dev`）
- [libfuse 3](https://github.com/libfuse/libfuse) — FUSE マウントツール（`fuse3`、`pkg-config` で検出）

## Build on Zephyr

Zephyr 向けは west モジュールとしてビルドします。ホストの CMake ツリーには含めません。

前提:

- Zephyr west ワークスペース（例: `~/work/zephyrproject`）と venv
- ビルド時に `-DEXTRA_ZEPHYR_MODULES=$MOFS` でこのリポジトリのルートを渡す（[`zephyr/module.yml`](zephyr/module.yml)）
- ボード例: `qemu_cortex_r5`
- `$MOFS` はリポジトリの絶対パス

評価用ホストは [`src/os/zephyr/tools/vfs/`](src/os/zephyr/tools/vfs/)（`mofs_vfs_main.c`）です。起動時に `mofs_format` → `fs_mount` まで行い、マウントしたままシェルで待ちます。デフォルトで `CONFIG_SHELL` / `CONFIG_FILE_SYSTEM_SHELL` が有効です。組み込みのファイル／ディレクトリ演習と `fs_unmount` は `ENABLE_FILE_IO_TESTS` / `ENABLE_UNMOUNT`（どちらも既定 `0`）でオフです。

```sh
export MOFS=/path/to/MOFS
cd ~/work/zephyrproject && source ./.venv/bin/activate
west build -p always -b qemu_cortex_r5 $MOFS/src/os/zephyr/tools/vfs -- \
  -DEXTRA_ZEPHYR_MODULES=$MOFS
west build -t run
```

QEMU 起動後、UART コンソールのシェルから例えば次のように触れます（パスはマウントポイント付き）。

```text
fs ls /MOFS
fs write /MOFS/hello.txt 68 65 6c 6c 6f
fs read /MOFS/hello.txt
fs mkdir /MOFS/dir
```

Zephyr 標準の `fs mount` は littlefs / FAT / rpmsgfs 専用で、MOFS タイプは選べません。マウントはホストの `main` 側が行います。

共通 FS テスト用アプリは [`test/os/zephyr/`](test/os/zephyr/) です（ホスト cmocka の `test/CMakeLists.txt` には含めません）。

```sh
west build -p always -b qemu_cortex_r5 $MOFS/test/os/zephyr -- \
  -DEXTRA_ZEPHYR_MODULES=$MOFS
timeout 90 west build -t run   # ztest は成功後も QEMU が残るため
```

注記:

- `CONFIG_MOFS` / `CONFIG_MOFS_FS` / `CONFIG_FILE_SYSTEM` は各アプリの `prj.conf` 側で有効にします。
- 評価用ホストの shell は [`src/os/zephyr/tools/vfs/prj.conf`](src/os/zephyr/tools/vfs/prj.conf) で既定 ON です。ztest アプリ側は shell を使いません。
- フォーマットは mount 外の明示的 `mofs_format`（ホスト／テストフィクスチャ）です。
- `FS_O_APPEND` は `MOFS_OFLAG_APPEND` に写し、`fs_write` はファイル末尾へ書く。Zephyr の VFS フラグに `FS_O_TRUNC` / `FS_O_SYNC` は無い。
- ライブラリに入るのは `mofs_vfs.c` です。`mofs_vfs_main.c` は別アプリなので、ztest ビルドでは呼ばれません。

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

### Linux（cmocka）

テストは cmocka を使い、CTest から実行します。

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

# 利用可能なテスト一覧:
ctest --test-dir build -N
```

テストは `test/os/linux/`, `test/core/`, `test/posix/` に分かれており、共通のヘルパは `test/fixtures/` にあります。

### Zephyr（共通 FS ztest）

[`test/os/zephyr/`](test/os/zephyr/) は Zephyr の共通 FS テストを薄いハーネスから呼び出します。手順は [Build on Zephyr](#build-on-zephyr) を参照してください。

リンクする共通ソース:

- `test_fs_basic.c`（open / read / write / seek / stat / truncate / unlink / sync）
- `test_fs_dirops.c`（mkdir / readdir / rename など）
- `test_fs_open_flags.c`
- 依存の `test_fs_util.c`

対象外: `test_fs_mkfs.c`、`test_fs_gc.c`、`test_fs_mount_flags.c`（未実装の mkfs / gc、および自動 format 前提と合わないため）。

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
