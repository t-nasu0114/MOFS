---
name: jj_operations
description: 構成管理に関する指示を受けた時に参照する。構成管理ツール jj コマンド (jujutsu) の操作方法についての説明。
---

# jj operations

このスキルは構成管理ツール jj の操作方法について説明するものです。
構成管理に関する指示を受けた時に参照してください。

## 履歴を見る（jj log）

```sh
jj log
```

## 現在の状態を確認

```sh
jj status
```

## 編集中のコミットにコミットメッセージをつける

```sh
jj desc -m "Description about current changes"
```

## 新しいコミットに切り替える

```sh
jj new
```

## 任意のファイルを追跡対象から外す

```sh
jj file untrack [file_path]
```

## ローカルの変更をリモートに push する

この操作を行う場合は git と連携して実行します。
また、bookmark を設定するコミットには必ずコミットメッセージをつけてから実行してください。
以下は最新のコミットに bookmark を設定して push する場合の操作です。

途中でエラーが発生した場合は操作を中断してください。

```sh
jj bookmark set working_branch -r @
git checkout working_branch
git push
```
