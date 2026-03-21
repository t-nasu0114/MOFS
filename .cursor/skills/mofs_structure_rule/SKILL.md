---
name: mofs_structure_rule
description: MOFS のファイル・ディレクトリ構造のルール。MOFS の実装や設計情報を作成にするときに使います。
---

# MOFS structure rule

## 1. 概要

この Skill は MOFS のファイル・ディレクトリ構造のルールです。MOFS の実装や設計情報を作成にするときに使います。

## 2. ディレクトリ構成

### 2.1 基本構成

以下のディレクトリ構成を基本とする。

```
project_root/
├─ include/              # 公開API
├─ src/
│  ├─ core/              # 本体ロジック
│  │  ├─ include/        # core内部ヘッダ
│  │  └─ modules/        # 機能別実装
│  ├─ api/               # POSIX API 用 wrapper
│  ├─ os/
│  │  ├─ linux/
│  │  └─ zephyr/
│  └─ util/              # 共通ユーティリティ
├─ tests/                # テスト
├─ examples/             # 使用例
└─ docs/                 # 設計資料
```

### 2.2 ディレクトリ構成の原則

MOFS のディレクトリ構成は以下の原則を守る。

#### 2.2.1. `core` と `integration` を分離する

`core` は製品の本体アルゴリズム、`integration` は OS・ミドルウェア・デバイス・UI との接続層である。
本体ロジックは外部依存を薄くし、接続コードだけを外側へ寄せる。

#### 2.2.2. 公開ヘッダと内部ヘッダを分離する

`include/` は外部や上位層が使う API、`core/include/` は内部実装用、という分け方で再利用しやすくする。
「外に見せてよい実装」と「実装詳細」を混ぜない。

#### 2.2.3. OS/Platform ごとの差分は `os/<platform>` に閉じ込める

`linux`, `win32`, `stub` のように並べて影響範囲を明確にする。
プラットフォーム差分は散らさず 1 箇所に集約する。

#### 2.2.4. POSIX API 層を 1 段噛ませる

`posix/` のように、外部向け API の実装層を `core` の上に置く。
本体ロジックに直接 UI/API/CLI を混ぜないことで、複数の入口を同じ core に載せられるようにする。

#### 2.2.5. ツール・テスト・サンプル・本体を混ぜない

`tools/`, `tests/`, `projects/` を分ける。
本番コードの横に評価コードや一時ツールを混在させない方が保守しやすいため。

#### 2.2.6. 設定は「製品ごとの差分」として分離する

`projects/linux/mofsconf.*` のように、設定やビルド差分を project 単位で持つと派生製品に展開しやすいです。
本体コードに `#ifdef` を増やすより、設定ファイルに寄せる方が拡張しやすいです。

#### 2.2.7. 変更頻度と責務でディレクトリを切る

`inode.c`, `dir.c`, `volume.c`, `buffer.c` のように責務単位でファイルを分ける。
「ファイルサイズ」ではなく「設計責務」で切るのが原則。

#### 2.2.8. 依存方向を一定にする

上位層 → POSIX API層 → core → OS abstraction の向きに依存を流し、逆流させないことです。
どこからでも何でも呼べる構造にしない。

## 3. include ファイル

### 3.1 基本構成

include ファイルの基本構成は以下の通り。

```
project_root/
├─ include/                  # 外部公開ヘッダ。ファイルシステムを使用するユーザーが使用できるもの。
│  ├─ product_api.h
│  ├─ product_types.h
│  └─ product_errors.h
│
├─ src/
│  ├─ core/
│  │  ├─ include/            # core内部ヘッダ。内部の他モジュールが core の機能を使用するときに参照する。
│  │  │  ├─ product_core.h
│  │  │  ├─ product_nodes.h
│  │  │  └─ product_internal.h
│  │  └─ ...
│  │
│  ├─ os/
│  │  ├─ linux/include/      # Linux依存の情報をインクルードできるヘッダ
│  │  └─ zephyr/include/     # zephyr依存の情報をインクルードできるヘッダ
│  │
│  └─ tools/config/include/  # ツール専用ヘッダ
│     └─ version.h
```

### 3.2 各階層の意味

- `include/`
  - 外部利用者が見てよい API
  - ライブラリ利用者、アプリ、上位製品向け
  - 安定性が必要

- `core/include/`
  - 実装内部の共有ヘッダ
  - core の `.c/.cpp` 同士で利用
  - 外部非公開前提

- `os/<platform>/include/`
  - platform 差分の吸収
  - platform ごとに排他的
  - 他 platform からは見えないのが理想

- `projects/<variant>/`
  - 製品差分・設定差分
  - ビルド対象ごとに差し替える
  - feature toggle や型差分の入口

- `tools/.../include/`
  - 製品本体とは無関係なツールのローカルスコープ
  - 本体に混ぜない

### 3.3 include ファイル構成の原則

include ファイル構成の原則は以下の通り。

### 3.3.1. `include` は 1 種類ではなく、公開範囲ごとに分ける

典型的には次の 3 層。
- 公開ヘッダ: 外部利用者向け
- 内部ヘッダ: 同一コンポーネント実装向け
- 環境依存ヘッダ: OS/Platform 向け

### 3.3.2. ヘッダのスコープは「置き場所」だけでなく「include path」で決まる

ディレクトリを分けても、全部を全ターゲットに `-I` すると意味が薄れる。
そのため、誰が参照可能かはビルド定義で縛る。

### 3.3.3. 公開 API は最小にする

外に見せるヘッダは、利用者が本当に必要なものだけに絞る。
型・マクロ・内部構造体をむやみに公開しないことが重要。

### 3.3.4. 内部ヘッダは実装のための契約に限定する

`core/include` のような内部ヘッダは、同一モジュール内の複数ファイルが共有するためのもの。
他モジュールから直接触らせない方が、後で中身を変えやすい。

### 3.3.5. OS/Platform 依存ヘッダは排他的に選ばせる

`os/linux/include`, `os/win32/include`, `os/rtos/include` のように分け、ビルド時に 1 系統だけ載せる構成を基本とする。

### 3.3.6. 製品設定ヘッダは「設定の入口」として扱う

`hogeconf.h` のような設定ヘッダは、実装の都合ではなく製品構成を決める責務を持たせる。
他製品でも、設定ヘッダに runtime/OS/feature の方針を集約すると整理しやすい。

### 3.3.7. 上位層ほど狭い情報に依存させる

アプリや外部利用者は公開ヘッダだけを見るべき。
`core` 内部構造や platform 型に上位層が直接依存し始めると、境界が崩れるため。

### 3.3.8. ヘッダ名の衝突を避ける

汎用製品では `config.h`, `types.h`, `utils.h` のような名前は衝突しやすい。
そのため `product_config.h`, `hogeostypes.h` のように名前空間を持たせる方が安全。

### 3.3.9. POSIX 標準ヘッダとC標準ヘッダのインクルード

`core` の共通部、公開ヘッダ、POSIX API 層はPOSIX 標準ヘッダとC標準ヘッダをインクルードしてはならない。
具体的には以下の通り。

```
project_root/
├─ include/              # NG
├─ src/
│  ├─ core/              # NG
│  │  ├─ include/
│  │  └─ modules/
│  ├─ posix/             # NG
│  └─ os/                # OK
│      ├─ linux/
│      └─ zephyr/
├─ tests/                # OK
├─ examples/             # OK
└─ docs/                 # 設計資料
```

### 3.4 ビルド設定の原則

include path は「必要最小限」にする。

- 本体 core ターゲット
  - `include/`
  - `core/include/`
  - `os/<selected>/include/`
  - `projects/<selected>/`

- 外部利用者
  - `include/` のみ

- ツール
  - `include/`
  - 必要なら `core/include/`
  - ただし本体内部に依存しすぎない

`include` のスコープはフォルダ名ではなく、「どのターゲットにそのディレクトリを渡すか」で制御するのが原則です。
