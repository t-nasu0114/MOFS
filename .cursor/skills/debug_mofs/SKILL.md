---
name: debug_mofs
description: mofs のデバッグ方法を説明したスキル。mofs の問題調査で必要に応じて使用する。
---

# debug_mofs

このスキルには mofs をデバッグする時の手順について説明します。
問題調査を依頼されたときに必要に応じて使用してください。

## mkfs.mofs

フォーマッタ (build/src/os/linux/tools/mkfs/mkfs.mofs) のデバッグを gdb を使用して行う方法は以下のとおりです。
このコマンドの実行後のデバッグ操作は、一般的な gdb の操作方法と変わりません。

```sh
# カレントディレクトリはプロジェクトルート
cd /home/t-nasu/work/mofs_git/MOFS
# デバッグ開始
gdb ./build/src/os/linux/tools/mkfs/mkfs.mofs /home/t-nasu/work/test/test.img
```

## FUSE

FUSE (build/src/os/linux/tools/fuse/mofs) のデバッグを gdb を使用して行う方法は以下のとおりです。
このコマンドの実行後のデバッグ操作は、一般的な gdb の操作方法と変わりません。

```sh
# カレントディレクトリはプロジェクトルート
cd /home/t-nasu/work/mofs_git/MOFS
# 一度 test.img をフォーマットしておく
./build/src/os/linux/tools/mkfs/mkfs.mofs /home/t-nasu/work/test/test.img
# デバッグ開始
gdb ./build/src/os/linux/tools/fuse/mofs /home/t-nasu/work/test/test.img /home/t-nasu/work/mnt
```
