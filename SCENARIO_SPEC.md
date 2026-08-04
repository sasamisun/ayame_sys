# シナリオデータ仕様書

`ayame_sys`（M5PaperS3 アドベンチャーゲーム基盤）が SD カードから読み込む
シナリオデータの仕様を定める。

**この文書の位置づけ**

| 文書 | 内容 |
|---|---|
| `PROGRAM_SPEC.md` | プログラムの構成 |
| `REFACTORING_LOG.md` | 改修の作業記録 |
| **`SCENARIO_SPEC.md`（本書）** | **シナリオデータの書き方** |

**本書に書かれている機能はすべて実装済みで動作する。**
SD カードに置いた `scenario.json` が `ScenarioLoader` に読まれ、
`ScenarioPlayer` が1コマンドずつ実行する。

まず試すなら、`microsd_sample/` の中身を SD カードのルートへコピーする。
20 本のサンプルが入っており、各機能の書き方と動作を確かめられる
（`microsd_sample/README.md` を参照）。

---

## 目次

1. [設計方針](#1-設計方針)
2. [フォルダ構成](#2-フォルダ構成)
3. [scenario.json](#3-scenariojson)
4. [コマンドリファレンス](#4-コマンドリファレンス)
5. [条件式](#5-条件式)
6. [セーブデータ](#6-セーブデータ)
7. [システム設定](#7-システム設定)
8. [実装状況](#8-実装状況)
9. [設計を縛る制約](#9-設計を縛る制約)
10. [完全なサンプル](#10-完全なサンプル)
11. [拡張提案](#11-拡張提案)
12. [用語集](#12-用語集)

---

## 1. 設計方針

### 1.1 なぜこの形式か

| 方針 | 理由 |
|---|---|
| **1 シナリオ 1 JSON** | 構成が単純。**約 35 万文字までメモリに載る**ので分割は当面不要（[9.7](#97-メモリ上限)） |
| **シーン = コマンドの並び** | フラグ・分岐・立ち絵・音を扱うため。「1 シーン = 1 画面」の単純形では表現しきれない |
| **列挙値は C++ の enum 名をそのまま使う** | 変換表を作らない。仕様書とコードがずれる余地を消す |
| **未実装機能も v1 に予約** | ルビや文字送りが後から実装されても、既存シナリオを書き直さずに済む |
| **ローダは未対応キーを無視する** | 前方互換。新しいキーを含む JSON を古いファームで開いても落ちない |

### 1.2 バージョニング

`format_version` はスキーマの版を表す整数。初版は `1`。

- ローダは `format_version` が自分の対応上限より大きい場合、**警告を出したうえで読み込みを試みる**
- 未知のキー・未知の `type` は**無視してログに残す**（エラーにしない）
- 破壊的変更が必要になった場合のみ `2` へ上げる

---

## 2. フォルダ構成

```
/sdcard/
├── scenarios/                    # シナリオ置き場
│   ├── ayame_001/                # シナリオID = フォルダ名
│   │   ├── scenario.json         # 固定名
│   │   ├── thumbnail.png         # 固定名。一覧用サムネイル
│   │   ├── images/
│   │   │   ├── bg/               # 背景
│   │   │   └── chara/            # 立ち絵
│   │   └── saves/                # セーブデータ
│   │       ├── slot01.json
│   │       ├── slot02.json
│   │       ├── slot03.json
│   │       └── auto.json         # オートセーブ
│   └── ayame_002/
│       └── ...
└── system/
    └── settings.json             # 本体設定
```

### 2.1 各要素の規約

| パス | 規約 |
|---|---|
| `scenarios/` | 固定名。システムメニューはここを `listDir()` で列挙する |
| `scenarios/<id>/` | **シナリオ ID = フォルダ名**。ASCII の英数字と `_` のみ、32 文字以内 |
| `scenario.json` | **固定名**。ローダは検索せずこの名前を直接開く |
| `thumbnail.png` | **固定名**。推奨 180×240px。無い場合は既定アイコンを表示 |
| `images/bg/` | 背景画像。全画面想定なら 540×960px |
| `images/chara/` | 立ち絵。透過 PNG 推奨 |
| `saves/` | セーブ。`slotNN.json`（NN は 01〜99）と `auto.json` |
| `system/settings.json` | 本体設定。シナリオに属さない |

### 2.2 なぜこの階層なのか

原案（SD ルート直下にシナリオフォルダを並べ、セーブと images をフラットに置く）から
4 点変更した。理由を残しておく。

| 変更 | 理由 |
|---|---|
| `scenarios/` コンテナを追加 | SD ルート直下だと写真や他のファイルと混ざり、一覧の列挙が破綻する。USB MSC で PC から書き込む運用では特に混ざりやすい |
| セーブを `saves/` サブフォルダへ | 複数スロット構成のため。直置きだとシナリオフォルダ内でファイルが増殖し、`scenario.json` が埋もれる |
| `images/` を `bg/` と `chara/` に分割 | 立ち絵を扱うため。役割が違う画像を同一階層に置くと管理が破綻する |
| `system/settings.json` を追加 | 「最後に遊んだシナリオ」などシナリオに属さない状態の置き場が必要 |

### 2.3 命名の制約

**フォルダ名・ファイル名は ASCII のみ**とする。日本語名は使わない。

- SD は FAT でマウントされており、日本語ファイル名の扱いが環境依存
- パスは `/sdcard` 起点で **255 文字上限**（`SDcard.cpp` の `buildFullPath()` が `char[256]`）
- 画面に出す日本語タイトルは `scenario.json` の `meta.title` に持つ

---

## 3. scenario.json

### 3.1 全体構造

```json
{
  "format_version": 1,
  "meta":      { ... },
  "assets":    { ... },
  "textboxes": { ... },
  "variables": { ... },
  "start":     "scene_id",
  "scenes":    { ... }
}
```

| キー | 型 | 必須 | 内容 |
|---|---|---|---|
| `format_version` | int | ✓ | スキーマ版。初版は `1` |
| `meta` | object | ✓ | タイトル等のメタ情報 |
| `assets` | object | ✓ | 画像の論理名 → ファイルパス |
| `textboxes` | object | | 名前付きテキストボックス（[3.6](#36-textboxes)）。省略時は既定の2つだけ |
| `variables` | object | | フラグ・変数の初期値。省略時は空 |
| `start` | string | ✓ | 開始シーンの ID |
| `scenes` | object | ✓ | シーン ID → シーン定義 |

> **`meta` はファイルの先頭付近に置くこと。**
> システムメニューの一覧は、各 `scenario.json` の**先頭 4KB だけを読んで**
> `meta.title` を取り出している（全文を解析すると、シナリオが増えるほど
> メニューの表示が待たされるため）。
> `meta` が 4KB より後ろにあるとタイトルを読み取れず、
> **フォルダ名が表示される**（動作は壊れない）。
> 上の並び順で書けば問題ない。

### 3.2 meta

```json
"meta": {
  "id": "ayame_001",
  "title": "あやめの物語",
  "author": "作者名",
  "version": "1.0.0",
  "description": "一行の説明文",
  "text_direction": "VERTICAL",
  "update_url": null
}
```

| キー | 型 | 必須 | 内容 |
|---|---|---|---|
| `id` | string | ✓ | シナリオ ID。**フォルダ名と一致させる**（不一致ならローダが警告） |
| `title` | string | ✓ | 画面に表示するタイトル。日本語可 |
| `author` | string | | 作者名 |
| `version` | string | ✓ | シナリオの版。セーブとの整合判定に使う |
| `description` | string | | 一覧に出す説明文 |
| `text_direction` | string | | 既定の文字方向。`"VERTICAL"` / `"HORIZONTAL"`。省略時 `"VERTICAL"` |
| `update_url` | string\|null | | 将来の Wi-Fi 配信用に予約。現在は未使用 |

`version` は**セーブデータとの照合に使う**。シナリオを更新するとシーン ID や
コマンド位置がずれ、古いセーブから再開すると破綻する。不一致時はローダが警告を出す。

### 3.3 assets

画像の**論理名**と**ファイルパス**の対応表。コマンド側は論理名で参照する。

```json
"assets": {
  "backgrounds": {
    "room":   "images/bg/room.png",
    "street": "images/bg/street.png"
  },
  "characters": {
    "ayame": {
      "normal": "images/chara/ayame_normal.png",
      "smile":  "images/chara/ayame_smile.png",
      "sad":    "images/chara/ayame_sad.png"
    }
  }
}
```

パスは**シナリオフォルダからの相対パス**。ローダが
`scenarios/<id>/` を前置してフルパスを組み立てる。

論理名を挟む理由は、画像を差し替えたときに `scenario.json` の
`assets` 1 箇所だけ直せば済むようにするため。

対応形式は **PNG / JPEG / BMP / QOI**（M5GFX が対応）。透過が要る立ち絵は PNG。

### 3.4 variables

フラグ・変数の**初期値**。ここに宣言されていない変数は使えない
（タイプミスを検出できるようにするため）。

```json
"variables": {
  "met_ayame": false,
  "affection": 0,
  "route": "none"
}
```

型は **bool / 数値 / 文字列** の 3 種。初期値から型が決まる。

### 3.5 scenes

シーン ID をキーとしたオブジェクト。

```json
"scenes": {
  "opening": {
    "commands": [ ... ],
    "next": "chapter1"
  }
}
```

| キー | 型 | 必須 | 内容 |
|---|---|---|---|
| `commands` | array | ✓ | 実行するコマンドの並び |
| `next` | string | | 全コマンド終了後の遷移先シーン ID |

`commands` の末尾に `choice` / `jump` / `end` がある場合、`next` は不要。
`next` も終端コマンドも無い場合、ローダは警告を出してシナリオを終了する。

シーン ID は ASCII の英数字と `_` のみ。

---

### 3.6 textboxes

本文を出す枠を名前付きで定義する。**同時にいくつでも置ける。**

```json
"textboxes": {
  "name": {
    "x": 20, "y": 640, "w": 300, "h": 46,
    "direction": "HORIZONTAL",
    "background": "frame_name"
  },
  "main": {
    "x": 20, "y": 694, "w": 300, "h": 260,
    "direction": "HORIZONTAL",
    "line_spacing": 8,
    "background": "frame_main"
  },
  "title": {
    "x": 20, "y": 20, "w": 500, "h": 60,
    "direction": "HORIZONTAL",
    "font_size": 2.0, "align": "CENTER"
  }
}
```

| キー | 型 | 既定 | 内容 |
|---|---|---|---|
| `x` / `y` | int | `0` | 左上の位置 |
| `w` / `h` | int | 必須 | 大きさ。0 以下だと作られない |
| `direction` | string | `"VERTICAL"` | `"VERTICAL"` / `"HORIZONTAL"` |
| `font_size` | float | `1.0` | 本文の倍率 |
| `line_spacing` | int | `6` | 行間（縦書きでは列の間隔） |
| `char_spacing` | int | `0` | 字間 |
| `align` | string | `"LEFT"` | `"LEFT"` / `"CENTER"` / `"RIGHT"` |
| `background` | string | なし | `assets.backgrounds` の論理名。枠に敷く画像 |
| `background_color` | string | `"BLACK"` | 画像が無いときの塗り色。`"BLACK"` / `"WHITE"` |

使うときは [`text`](#text) の `box` で指名する。

```json
{ "type": "text", "box": "name", "body": "あやめ", "wait": false }
{ "type": "text", "box": "main", "body": "こんにちは。" }
```

- **`box` を省くと従来どおり**、`direction` で既定の縦書き/横書きボックスに出る
- 未定義の名前を指定した場合は既定のボックスに出し、警告をログに出す（再生は止まらない）
- 書き換えたボックスだけが更新される。**他のボックスの内容は残る**
- ページ送りも文字送りもボックスごとに独立して働く

> **`font_size` はビットマップフォントの拡大縮小。**
> 収録しているのは 16pt の1種類だけなので、
> **2.0 倍は輪郭が粗くなり、1.0 未満は読めなくなる**。
> きれいに大きくしたい場合は、別サイズの VLW を作って増やす必要がある
> （フォントは 1.12MB/個で、アプリ領域に 7 個ほど追加できる）。

> **背景画像はボックスの寸法に合わせて用意すること。**
> 拡大縮小はしない。1bpp なので拡大すると輪郭がギザつくため。

---

## 4. コマンドリファレンス

全コマンドは `type` を持つ。ローダは未知の `type` を無視してログに残す。

### 4.1 一覧

| `type` | 内容 | 状態 |
|---|---|---|
| [`text`](#text) | 本文表示（ページ送り・ルビ対応） | ✅ |
| [`bg`](#bg) | 背景切替（画面遷移つき） | ✅ |
| [`choice`](#choice) | 選択肢と分岐 | ✅ |
| [`set`](#set) | 変数代入・演算 | ✅ |
| [`if`](#if) | 条件分岐（入れ子可） | ✅ |
| [`jump`](#jump) | シーン遷移 | ✅ |
| [`wait`](#wait) | 待機 | ✅ |
| [`beep`](#beep) | ブザー音（単音・音列） | ✅ |
| [`refresh`](#refresh) | 手動フルリフレッシュ | ✅ |
| [`clear`](#clear) | 画面クリア | ✅ |
| [`end`](#end) | シナリオ終了 | ✅ |
| [`chara`](#chara) | 立ち絵の表示・非表示 | ✅ |
| [`save`](#save--load) / [`load`](#save--load) | セーブ・ロード | ✅ |
| [`checkpoint`](#checkpoint) | 保存する状態を控える | ✅ |
| [`image`](#image) | 前面の一枚絵（イベントCG） | ✅ |
| [`random`](#random) | 変数に乱数を入れる | ✅ |
| [`call`](#call--return) / [`return`](#call--return) | シーンの再利用 | ✅ |
| [`suspend`](#suspend) | しおりを残して電源を切る | ✅ |

凡例: **✅ 実装済み（動く） / ❌ 未実装（記法のみ予約）**

未実装のコマンドは**書いても無視され、警告がログに出る**だけで、
シナリオの再生は止まらない。実装された時点でそのまま動き出す。

### 4.2 コマンド以外の未実装項目

| 項目 | 状態 |
|---|---|
| **本文への変数の埋め込み** | ✅ `{変数名}`（[4.3](#43-本文への変数の埋め込み)） |
| `text` の `speed`（文字送り） | ✅ 実装済み。ただし電子ペーパーでは遅い（上記） |
| ルビ | ✅ 実装済み |
| 本文のページ送り | ✅ 実装済み。1つの `text` が複数ページに分かれる |

**JSON で書ける命令はすべて実装済み。**

---

### `text`

本文を表示する。

```json
{ "type": "text", "speaker": "あやめ", "body": "こんにちは。", "wait": true }
```

| キー | 型 | 既定 | 内容 |
|---|---|---|---|
| `body` | string | 必須 | 本文。`\n` で改行 |
| `box` | string | なし | 出力先のボックス名（[3.6](#36-textboxes)）。省略時は既定 |
| `speaker` | string | なし | 話者名。省略時は地の文 |
| `wait` | bool | `true` | 表示後にタップを待つ |
| `direction` | string | `meta` の値 | `"VERTICAL"` / `"HORIZONTAL"` |
| `align` | string | `"LEFT"` | `"LEFT"` / `"CENTER"` / `"RIGHT"` |
| `speed` | int | `0` | 文字送り速度（ms/字）。`0` で一括表示 |

#### 文字送り（`speed`）は遅い

**電子ペーパーは1文字ごとに全面走査が必要**なので、1文字表示するたびに
1回ぶんのリフレッシュがかかる。走査は更新範囲を狭めても速くならない。

| モード | 1リフレッシュ | 40文字を送る時間 |
|---|---|---|
| `epd_quality` | 351 ms | 14.0 秒 |
| `epd_fastest` | 117 ms | **4.7 秒** |

送っている間は自動的に `epd_fastest` へ落とし、**出し切ったら本来のモードで
描き直して定着させる**。それでも40文字で約5秒かかり、40回ぶんの残像が蓄積する。

**短い台詞に絞って使うこと。** 長い本文では `speed` を指定しない方がよい。

**途中でタップすると全文が出る。** これが無いと遅さが致命的になるため。

`body` が本文ボックスに収まらない場合、**1つの `text` コマンドが複数ページに分かれる**。
続きはタップで送る（`TypoWrite::drawTextPaged()` が描き切れなかった位置を返す）。

`direction` / `align` の値は `TypoWrite` の `TextDirection` / `TextAlignment`
のメンバー名と一致する（`main/TypoWrite.hpp:17-27`）。

**ルビ**

本文中に埋め込む。**半角と全角のどちらでも書ける**（混在も可）。

```
|漢字<かんじ>       半角（入力しやすい。推奨）
｜漢字《かんじ》     全角（青空文庫式）
```

縦棒がルビを振る範囲の開始、括弧がルビの囲み。

| 役割 | 受け付ける文字 |
|---|---|
| 開始 | `\|`(U+007C) / `｜`(U+FF5C) |
| 開き | `<`(U+003C) / `《`(U+300A) / `＜`(U+FF1C) |
| 閉じ | `>`(U+003E) / `》`(U+300B) / `＞`(U+FF1E) |

シナリオは SD 上のテキストを手で書くため、`｜` や `《》` を出しにくい環境でも
入力できるよう半角を用意してある。

ルビは**縦書きなら本文の右、横書きなら本文の上**に描かれる。

- 有効にすると**行の高さ（縦書きなら列の幅）がルビ帯のぶん増える**。
  ルビの有無にかかわらず全行に帯を確保するため行間が一定に保たれ、そのぶん
  1画面に入る文字数は減る。
- ルビが本文より長い場合ははみ出して隣に重なることがある
  （本文側の送りを広げる処理は未実装）。漢字1文字にかな2文字までなら収まる。
- 本文中にたまたま `|` が現れ、後ろにルビの括弧が無い場合は普通の文字として扱われる。

---

### `bg`

背景を切り替える。

```json
{ "type": "bg", "image": "room", "transition": "WIPE_HORIZONTAL" }
```

| キー | 型 | 既定 | 内容 |
|---|---|---|---|
| `image` | string | 必須 | `assets.backgrounds` の論理名 |
| `transition` | string | `"NONE"` | 遷移演出 |
| `x` / `y` | int | `0` | 描画位置 |
| `scale` | float | `1.0` | 拡大率 |

`transition` に指定できる値（`SimpleTransitionType` のメンバー名、
`main/SimpleTransition.hpp:17-28`）:

| 値 | 内容 |
|---|---|
| `NONE` | 遷移なし（瞬間表示） |
| `FADE_IN` | フェードイン |
| `SLIDE_LEFT` / `SLIDE_RIGHT` / `SLIDE_UP` / `SLIDE_DOWN` | スライドイン |
| `WIPE_HORIZONTAL` / `WIPE_VERTICAL` | ワイプ |
| `REVEAL_CENTER` | 中央から展開 |
| `REVEAL_CORNER` | 角から展開 |

---

### `chara`

立ち絵を表示・非表示にする。

```json
{ "type": "chara", "id": "ayame", "expression": "smile", "x": 60, "y": 400 }
{ "type": "chara", "id": "ayame", "visible": false }
```

| キー | 型 | 既定 | 内容 |
|---|---|---|---|
| `id` | string | 必須 | `assets.characters` のキー |
| `expression` | string | `"normal"` | 表情差分のキー |
| `x` / `y` | int | 前回値 | 表示位置 |
| `scale` | float | `1.0` | 拡大率 |
| `visible` | bool | `true` | `false` で非表示 |

同じ `id` を再指定すると表情・位置が更新される。

---

### `choice`

選択肢を出し、選ばれた先へ分岐する。**このコマンドでシーンは終わる**。

```json
{
  "type": "choice",
  "prompt": "どうする？",
  "options": [
    { "label": "話しかける", "next": "talk" },
    { "label": "立ち去る",   "next": "leave" },
    { "label": "名前を呼ぶ", "next": "call",
      "cond": { "var": "met_ayame", "op": "==", "value": true } }
  ]
}
```

| キー | 型 | 必須 | 内容 |
|---|---|---|---|
| `prompt` | string | | 選択肢の上に出す問いかけ |
| `options` | array | ✓ | 選択肢の配列（1〜6 個） |

`options[]` の要素:

| キー | 型 | 必須 | 内容 |
|---|---|---|---|
| `label` | string | ✓ | ボタンに出す文言 |
| `next` | string | ✓ | 遷移先シーン ID |
| `cond` | object | | 条件。偽なら**この選択肢を出さない** |
| `hide_if_false` | bool | | `false` にすると条件が偽でも灰色表示で出す。既定 `true` |

条件付き選択肢が全て偽で `options` が空になった場合、
ローダは警告を出してシーンの `next` へ進む。

---

### `set`

変数に代入・演算する。

```json
{ "type": "set", "var": "affection", "op": "+=", "value": 1 }
{ "type": "set", "var": "met_ayame", "value": true }
```

| キー | 型 | 既定 | 内容 |
|---|---|---|---|
| `var` | string | 必須 | `variables` で宣言済みの変数名 |
| `op` | string | `"="` | `"="` / `"+="` / `"-="` |
| `value` | any | 必須 | 代入値 |

`+=` / `-=` は数値変数のみ。未宣言の変数を指定した場合はエラーログを出して無視する。

---

### `if`

条件で実行するコマンド列を切り替える。入れ子にできる。

```json
{
  "type": "if",
  "cond": { "var": "affection", "op": ">=", "value": 3 },
  "then": [
    { "type": "text", "body": "好感度が高い。" }
  ],
  "else": [
    { "type": "text", "body": "まだそれほどでもない。" }
  ]
}
```

| キー | 型 | 必須 | 内容 |
|---|---|---|---|
| `cond` | object | ✓ | 条件式（[5 章](#5-条件式)） |
| `then` | array | ✓ | 真のとき実行するコマンド列 |
| `else` | array | | 偽のとき実行するコマンド列 |

---

### `jump`

別のシーンへ移る。**このコマンドでシーンは終わる**。

```json
{ "type": "jump", "next": "chapter2" }
```

| キー | 型 | 必須 | 内容 |
|---|---|---|---|
| `next` | string | ✓ | 遷移先シーン ID |

---

### `wait`

一定時間待つ。

```json
{ "type": "wait", "ms": 800 }
```

| キー | 型 | 既定 | 内容 |
|---|---|---|---|
| `ms` | int | 必須 | 待機時間（ミリ秒） |
| `skippable` | bool | `true` | タップで飛ばせるか |

---

### `beep`

ブザーを鳴らす。**❌ 未実装**（音声出力の実装が無い。[8 章](#8-実装状況)参照）。

単音:

```json
{ "type": "beep", "freq": 880, "duration": 120 }
```

音列:

```json
{
  "type": "beep",
  "melody": [
    { "freq": 523, "duration": 150 },
    { "freq": 659, "duration": 150 },
    { "freq": 784, "duration": 300 },
    { "freq": 0,   "duration": 100 }
  ]
}
```

| キー | 型 | 既定 | 内容 |
|---|---|---|---|
| `freq` | int | | 周波数（Hz）。`0` で休符 |
| `duration` | int | | 長さ（ミリ秒） |
| `melody` | array | | 音列。`freq`/`duration` の配列 |
| `wait` | bool | `false` | 鳴り終わるまで待つか |

`freq` と `melody` は排他。両方あれば `melody` を優先する。

---

### `refresh`

**電子ペーパー向けの手動フルリフレッシュ。**

```json
{ "type": "refresh", "mode": "epd_quality" }
```

| キー | 型 | 既定 | 内容 |
|---|---|---|---|
| `mode` | string | `"epd_quality"` | 走査品質 |
| `clear_ghost` | bool | `false` | `true` で白黒反転を伴う残像消去 |

電子ペーパーは描画していない領域が徐々に薄くなる。場面転換の区切りに
このコマンドを置いて、シナリオ作者が明示的にリフレッシュを掛ける。

| `mode` | LUT ステップ | 用途 |
|---|---|---|
| `epd_quality` | 21 | 一枚絵。最も濃い |
| `epd_text` | 18 | 文章主体 |
| `epd_fast` | 11 | 速さ優先 |
| `epd_fastest` | 7 | 最速。全画素駆動はされない |

`clear_ghost: true` は白→黒→白の反転を行うため**画面がフラッシュする**が、
蓄積した残像を消せる。コストは約 3 倍。

---

### `clear`

画面を単色で塗る。

```json
{ "type": "clear", "color": "BLACK" }
```

| キー | 型 | 既定 | 内容 |
|---|---|---|---|
| `color` | string | `"BLACK"` | `"BLACK"` / `"WHITE"` |

---

### `save` / `load`

セーブはシステムの機能ではなく、**シナリオ作者がコマンドで組む**。
スロットを選ぶ画面もシステムでは用意しないので、`choice` と組み合わせて作る。

```json
{ "type": "save", "slot": 1 }
{ "type": "load", "slot": 1 }
```

| キー | 型 | 既定 | 内容 |
|---|---|---|---|
| `slot` | int | `1` | 保存先の番号（1〜99）。`0` はオートセーブ枠 |

保存されるのは再開位置・変数・**画面の状態**（背景と立ち絵）。
`load` は読み込んで状態を差し替え、その位置から再開する。
画面の状態も保存するのは、**再開したときに背景も立ち絵も消えた画面にならない**ようにするため。

置き場は `scenarios/<id>/saves/slotNN.json`（[6 章](#6-セーブデータ)）。

> **[`checkpoint`](#checkpoint) が控えてあると、`save` はそちらを書く。**
> 「どの状態を保存するか」は控えた側が決める。

> **`if` の中で `save` を使わないこと。**
> 実行位置はフレームのスタックで持っており、保存できるのは底の位置だけ。
> 底は `if` コマンド自身を指しているため、そこから再開すると条件が再評価され、
> **分岐の中をもう一度頭から実行**する。
> その中に `set` の `+=` があると二重に効く。
> `if` の中で保存すると警告がログに出る。

`load` は次の場合に安全側へ倒す。**いずれも再生は止まらない。**

| 状況 | 動作 |
|---|---|
| セーブが無い | 何もせず次のコマンドへ |
| 別シナリオのセーブ | 拒否して次のコマンドへ |
| `scenario_version` が違う | 警告を出したうえで読み込む |
| 保存されたシーンが消えている | `start` から開始 |
| `command_index` が範囲外 | そのシーンの先頭から |
| 宣言に無い変数が入っている | その変数だけ捨てる |

---

### `checkpoint`

**保存する状態を控える。** ファイルには書かない。
以後の [`save`](#save--load) と [`suspend`](#suspend) は、
実行中の状態ではなく**控えた方**を書き出す。

```json
{ "type": "checkpoint" }
```

| キー | 型 | 既定 | 内容 |
|---|---|---|---|
| `clear` | bool | `false` | `true` なら控えを捨てる。以後は実行中の状態が保存される |

控えるのは再開に要るもの全部。実行位置・変数・背景・立ち絵・前面絵。

#### なぜ要るか

**保存したい状態と、`save` / `suspend` を置ける場所は一致しない。**

中断は普通こう進む。

```
選択肢「ここで中断する」 → 中断メッセージ → suspend → 電源断
```

`suspend` の位置をそのまま保存すると、
次に「続きから」で出てくるのは**中断メッセージの画面**になる。
読者が戻りたいのはそこではない。

戻したい場所に `checkpoint` を置けば、`suspend` をどこに書いても関係なくなる。

```json
"p2": { "commands": [
  { "type": "set",  "var": "page", "value": 2 },
  { "type": "checkpoint" },
  { "type": "text", "body": "第 2 章。" },
  { "type": "choice", "prompt": "どうしますか？", "options": [
    { "label": "ここで中断する", "next": "suspend_p2" },
    { "label": "読み進める",     "next": "p3" }
  ]}
]},

"suspend_p2": { "commands": [
  { "type": "text",    "body": "中断します。" },
  { "type": "suspend", "message": "また明日。", "image": "bookmark" }
]}
```

再開すると「第 2 章。」から始まり、選択肢がもう一度出る。

#### 効き続ける範囲

**次の `checkpoint` まで。シーンをまたいでも消えない。**

章の頭ごとに置いておけば、どこで中断されても直前の章の頭から再開する。
逆に、置きっぱなしのまま先へ進むと**そこまで巻き戻る**ので、
章が進むたびに置き直すこと。

正確な位置で保存したくなったら控えを捨てる。

```json
{ "type": "checkpoint", "clear": true }
```

#### 控えが無いとき

実行中の状態がそのまま保存される。`checkpoint` を書かないシナリオは
これまでどおり動く。ただし `suspend` だけは
**その次のコマンドから**再開する（`suspend` を踏み直さないため）。

> **`if` の中に置かないこと。** 理由は [`save`](#save--load) と同じ。
> 置くと警告がログに出る。

---

### `suspend`

**しおりを残して電源を切る。** 読書の中断に使う。

```json
{ "type": "suspend", "message": "また明日。", "image": "bookmark" }
```

| キー | 型 | 既定 | 内容 |
|---|---|---|---|
| `message` | string | なし | 消える前に既定ボックスへ出す文言。`{変数}` が使える |
| `image` | string | なし | 画面全体に出す絵（`assets.backgrounds` の論理名） |
| `slot` | int | `0` | 中断位置を保存するスロット |

**電子ペーパーは電源を切っても像が残る。**
ここで描いた画面が、次に電源を入れるまで表示され続ける。
何を読んでいたか分かるようにしておくとよい。

処理は次の順で行われる。

1. 状態を `slot` へ保存する。
   **[`checkpoint`](#checkpoint) が控えてあればそちらを書く。**
   無ければ実行中の状態を、`suspend` の次のコマンドを指す形で書く
2. 「続きから」の情報を `system/settings.json` に記録
3. **白黒反転で粒子を振り切ってから**最終画面を描く
   （これをやらないと電源を切った後に像が薄くなる）
4. 走査の完了を待ち、さらに 3 秒置いてから電源を切る
   （待ちが短いと画面上部に横線が残る）

### どこから再開するかを決める

**`suspend` を置いた場所と、読者が戻りたい場所は普通ちがう。**

```json
"suspend_p2": { "commands": [
  { "type": "text",    "body": "中断します。" },
  { "type": "suspend", "message": "また明日。" }
]}
```

このシーンには `suspend` の先が無いので、控えが無ければ
**再開した瞬間にシナリオが終わってメニューへ戻る。**

戻したい場所に [`checkpoint`](#checkpoint) を置くこと。
`suspend` 側には何も書かなくてよい。

### 再開のしかた

次に電源を入れると、**メニューの最上段に「続きから」**が出る。
押すと保存された位置・変数・画面（背景と立ち絵）から再開する。

- **栞は一度使うと消える。** 同じ栞から何度も始められると、
  いつの状態から始まるのか分からなくなるため
- 保存先のシナリオが SD から消えていれば「続きから」は出ない
- 自動では再開しない。**別のシナリオを選ぶ自由を残す**ため

> **USB MSC 中は栞を残せない**（ファイルに書けないため）。
> 画面は出て電源も切れるが、次回「続きから」は出ない。

---

### `end`

シナリオを終了し、システムメニューへ戻る。

```json
{ "type": "end", "ending": "good_end" }
```

| キー | 型 | 必須 | 内容 |
|---|---|---|---|
| `ending` | string | | エンディング識別子。回収率の集計に使う |
| `message` | string | | 終了時に出すメッセージ |

---

### 4.3 本文への変数の埋め込み

`body` / `speaker` / `message` と、選択肢の `label` / `prompt` の中で
`{変数名}` が現在の値に置き換わる。

```json
{ "type": "set",  "var": "name", "value": "あやめ" },
{ "type": "text", "speaker": "{name}", "body": "わたしは {name} です。好感度は {affection}。" }
```

| 書き方 | 結果 |
|---|---|
| `{name}` | 変数 `name` の値 |
| `{{` / `}}` | `{` / `}` そのもの（記号を出したいとき） |
| 未定義の変数 | **置き換えず記法のまま残る**。警告がログに出る |

未定義を空文字にしないのは、**書き間違いに気づけなくなる**ため。

---

### `image`

前面に一枚絵（イベントCG）を出す。**立ち絵より手前**に1枚だけ置ける。

```json
{ "type": "image", "image": "cg_01", "x": 70, "y": 200 }
{ "type": "image", "clear": true }
```

| キー | 型 | 既定 | 内容 |
|---|---|---|---|
| `image` | string | 必須 | `assets.backgrounds` の論理名。CGも同じ入れ物に置く |
| `x` / `y` | int | `0` | 表示位置 |
| `scale` | float | `1.0` | 拡大率。**1bpp では粗くなる**ので原寸推奨 |
| `clear` | bool | `false` | `true` で消す |

立ち絵と違い差分を持たない。`clear` で消えるほか、`clear` コマンドでも消える。

---

### `random`

変数に乱数を入れる。分岐は `if` と組み合わせる。

```json
{ "type": "random", "var": "dice", "min": 1, "max": 6 },
{ "type": "if", "cond": { "var": "dice", "op": ">=", "value": 4 },
  "then": [ { "type": "text", "body": "大当たり" } ] }
```

| キー | 型 | 既定 | 内容 |
|---|---|---|---|
| `var` | string | 必須 | 書き込む先。**数値型で宣言済みであること** |
| `min` / `max` | int | `0` / `100` | 範囲（両端を含む） |

ハードウェア乱数を使うので種を蒔く必要はない。**毎回異なる**。

---

### `call` / `return`

シーンを部分プログラムとして呼ぶ。共通の演出や状態表示を使い回せる。

```json
{ "type": "call", "scene": "status" },
{ "type": "text", "body": "呼び出しから戻りました" }
```

```json
"status": { "commands": [
  { "type": "text", "body": "（好感度 {affection}）" },
  { "type": "return" }
]}
```

| コマンド | キー | 内容 |
|---|---|---|
| `call` | `scene` | 呼ぶシーンのID |
| `return` | — | 呼び出し元の**次のコマンド**へ戻る |

- **入れ子にできる**（`call` の中でさらに `call`）
- 深さの上限は 16。超えると警告を出して無視する
  （手書きの JSON が互いを呼び合っても止まるように）
- `call` していないのに `return` した場合は警告を出してシーンを終える
- **`call` の途中でセーブしないこと。** 戻り先は保存されないため、
  ロード後は呼び出し元へ戻れない

---

## 5. 条件式

`if` と `choice.options[].cond` で使う。

### 5.1 単一条件

```json
{ "var": "affection", "op": ">=", "value": 3 }
```

| キー | 型 | 必須 | 内容 |
|---|---|---|---|
| `var` | string | ✓ | 変数名 |
| `op` | string | ✓ | 比較演算子 |
| `value` | any | ✓ | 比較対象 |

演算子: `==` / `!=` / `<` / `<=` / `>` / `>=`

`<` `<=` `>` `>=` は数値変数のみ。bool と文字列は `==` `!=` のみ。

### 5.2 複合条件

```json
{ "all": [ { "var": "a", "op": "==", "value": true },
           { "var": "b", "op": ">=", "value": 2 } ] }
```

| キー | 内容 |
|---|---|
| `all` | 全て真なら真（AND） |
| `any` | いずれか真なら真（OR） |
| `not` | 単一条件を否定 |

入れ子にできる。

```json
{ "all": [
    { "var": "met_ayame", "op": "==", "value": true },
    { "any": [ { "var": "route", "op": "==", "value": "a" },
               { "var": "route", "op": "==", "value": "b" } ] }
] }
```

---

## 6. セーブデータ

> **❌ 未実装。** 形式のみ確定。

**セーブはシステムの機能ではない。** メインメニューにセーブ／ロードの項目は置かず、
**シナリオ作者が [`save` / `load`](#save--load) コマンドで組む**。
スロット選択の画面も `choice` で自作する。

これにより、作者が「セーブできる場所」を物語の都合で決められる
（宿屋でだけ保存できる、章の切れ目で自動保存する、など）。

`scenarios/<id>/saves/slotNN.json`（NN は 01〜99）と `auto.json`。

```json
{
  "format_version": 1,
  "scenario_id": "ayame_001",
  "scenario_version": "1.0.0",
  "saved_at": "2026-08-02T14:30:00",
  "slot": 1,
  "label": "第2章 街へ",
  "scene": "chapter2",
  "command_index": 5,
  "variables": {
    "met_ayame": true,
    "affection": 3,
    "route": "a"
  },
  "read_scenes": [ "opening", "chapter1", "talk" ],
  "endings": [ "good_end" ]
}
```

| キー | 型 | 内容 |
|---|---|---|
| `format_version` | int | スキーマ版 |
| `scenario_id` | string | シナリオ ID。フォルダとの整合確認用 |
| `scenario_version` | string | **`meta.version` と照合**。不一致なら警告 |
| `saved_at` | string | 保存日時（ISO 8601） |
| `slot` | int | スロット番号。`auto.json` は `0` |
| `label` | string | 一覧に出す見出し。省略時はシーン ID |
| `scene` | string | 再開するシーン ID |
| `command_index` | int | シーン内の再開位置（`commands` の添字） |
| `variables` | object | 変数の全スナップショット |
| `read_scenes` | array | 既読シーン ID。既読スキップに使う |
| `endings` | array | 到達済みエンディング識別子 |

### 6.1 バージョン不一致時の扱い

`scenario_version` が `meta.version` と異なる場合、シーン ID やコマンド位置が
ずれている可能性がある。ローダは次のように振る舞う。

1. 警告を表示し、続行するかユーザーに確認する
2. `scene` が存在しなければ `start` から開始する
3. `command_index` が範囲外なら `0` に丸める
4. `variables` のうち `variables` 宣言に無いキーは捨てる

### 6.2 実装上の前提

**SD の書き込み API は実装済み**（本体設定の保存を作る際に先取りした）。

```
bool SDCardWrapper::writeFileFromBuffer(const char* path, const void* data, size_t len);
```

残っているのは**状態のシリアライズと `save`/`load` コマンドの処理**だけ。

**USB MSC が有効な間は保存できない**（PC 側と同時に書くとファイルシステムが壊れるため）。
`save` はその場合に失敗するが、**再生は止めない**。

---

## 7. システム設定

`system/settings.json`。シナリオに属さない本体設定。

```json
{
  "format_version": 1,
  "last_scenario": "ayame_001",
  "text_direction": "VERTICAL",
  "font_size": 16,
  "text_speed": 0,
  "auto_refresh_scenes": 10,
  "epd_mode": "epd_text"
}
```

| キー | 型 | 内容 |
|---|---|---|
| `last_scenario` | string | 最後に遊んだシナリオ ID |
| `text_direction` | string | 既定の文字方向。シナリオの `meta` より優先度は低い |
| `font_size` | int | 文字サイズ |
| `text_speed` | int | 文字送り速度（ms/字）。**❌ 未実装** |
| `auto_refresh_scenes` | int | この数のシーンを進むごとに自動フルリフレッシュ。`0` で無効 |
| `epd_mode` | string | 通常時の走査品質 |

---

## 8. 実装状況

### 8.1 実装済み

| 機能 | 実装 |
|---|---|
| 縦書き / 横書き / 揃え / 折り返し | `TypoWrite`（`TextDirection`, `TextAlignment`） |
| **本文のページ送り** | `TypoWrite::drawTextPaged()`。描き切れなかった位置を返す |
| **ルビ** | `TypoWrite::setRubyEnabled(true)`。半角 `\|漢字<かんじ>` / 全角 `｜漢字《かんじ》` |
| 画面遷移 10 種 | `SimpleTransition`（`SimpleTransitionType`） |
| 画像表示 PNG/JPEG/BMP/QOI | `display.drawPngFile(&SD, path, x, y, ...)` |
| フルリフレッシュ / 残像消去 | `SimpleTransition::refreshScreen()` / `clearGhosting()` |
| **JSON の読み込みと検証** | `ScenarioLoader`。cJSON を PSRAM へ向けて解析 |
| **シーンとコマンドの実行** | `ScenarioPlayer` |
| **変数・条件式・分岐** | `set` / `if` / `choice`。`if` は入れ子可 |
| **立ち絵** | `chara`。背景と重ねて描き直す（`renderStage()`） |
| **セーブ・ロード** | `save` / `load`。変数と画面の状態を保存する |
| **ブザー音** | `Buzzer`（GPIO21 / LEDC）。単音・音列・消音 |
| **電池残量** | `Power`（ADC1 ch2 / 分圧比 2.0） |
| **電源 OFF** | `Power::powerOff()`（GPIO44 のパルス列） |
| **SD の読み書き** | `readFileToBuffer()` / `readFilePrefix()` / `writeFileFromBuffer()` |
| **本体設定** | `Settings`（`system/settings.json`） |
| シナリオ一覧・選択 | `SystemMenu`。サムネイル、日本語タイトル、ページ送り |

### 8.2 未実装

**シナリオから書ける機能はすべて実装済み。** 以下はシステム側の未実装項目。

| 機能 | 状況 |
|---|---|
| バックログ・既読スキップ | 既読の記録から必要 |
| 設定画面（文字サイズ・EPD品質） | `system/settings.json` の器はある |
| Wi-Fi 配信 | `REQUIRES` にも入っていない |

未実装のものも `format_version` 1 に**記法だけ予約**してある。
書いても無視され警告が出るだけで再生は止まらず、実装された時点でそのまま動く。

---

## 9. 設計を縛る制約

ローダを実装する際に必ず踏む制約。**シナリオ設計にも影響する。**

### 9.1 同時に開けるファイルは 1 つだけ

`SDCardWrapper` はグローバル単一インスタンス `SD` で、内部に `FILE*` を **1 本しか持たない**。

- **画像を描画している最中に JSON は読めない**
- ローダは「JSON を読む → 閉じる → 画像を描く」の順を守る
- `drawPngFile()` は内部でストリーミング読み込みするため、その間 `SD` を占有する

### 9.2 USB MSC 有効中はファイル操作が全て失敗する

`enableUSBMSC()` 中は `open` / `exists` / `mkdir` / `remove` / `size` / `listDir` が
**即座に false / 0 / nullptr を返す**。

- システムメニューは MSC 状態を確認してからシナリオ一覧を出す
- MSC を無効化したらシナリオ一覧を再読み込みする（PC 側で追加された可能性がある）
- ゲーム進行中に MSC へ入る場合、先にオートセーブする

### 9.3 パス長 255 文字

`buildFullPath()` は `char[256]` に `snprintf` する。超えると失敗する。

`/sdcard/scenarios/<id>/images/chara/<file>` で既に 40 文字前後を消費するため、
**シナリオ ID は 32 文字以内、ファイル名は 64 文字以内**を推奨。

### 9.4 listDir は名前のみ・再帰なし

`listDir()` が返す `FileInfo.name` は**ファイル名のみ**（フルパスではない）。
再帰列挙・ソート・拡張子フィルタは無いため、必要なら呼び出し側で実装する。
`isDirectory` でディレクトリ判別は可能。

### 9.5 ファイル名は ASCII

FAT でマウントされているため、日本語ファイル名は避ける（[2.3](#23-命名の制約)）。

### 9.6 長いファイル名（LFN）が必須

ESP-IDF の FATFS は**既定で 8.3 形式しか扱えない**（`CONFIG_FATFS_LFN_NONE`）。
ベース 8 文字 + 拡張子 3 文字を超えると `stat()` も `fopen()` も失敗する。

本仕様のパスはどれも 8.3 に収まらない。

| 名前 | 8.3 |
|---|---|
| `scenarios` | ✗ ベース 9 文字 |
| `sample_001` | ✗ ベース 10 文字 |
| `scenario.json` | ✗ **拡張子 4 文字** |

そのため `sdkconfig.defaults` で LFN を有効にしてある。

```
CONFIG_FATFS_LFN_HEAP=y
CONFIG_FATFS_MAX_LFN=255
```

`STACK` ではなく `HEAP` を選ぶのは、LFN の作業バッファ（最大 255×2 バイト）が
タスクスタックに載るとスタック不足を招きやすいため。

> **`sdkconfig.defaults` は ASCII のみで書くこと。**
> `kconfgen` がこのファイルをシステムのコードページ（日本語 Windows なら cp932）で
> 読むため、日本語コメントを入れると `UnicodeDecodeError` でビルドが落ちる。

### 9.7 メモリ上限

**結論: 数万文字の長編は余裕で作れる。実用上の天井は約 35 万文字。**

#### 使えるメモリ

| 項目 | 実測値 | 出典 |
|---|---|---|
| PSRAM プール全体 | 6784 KB | 起動ログ `Adding pool of 6784K` |
| SimpleTransition キャンバス | −1012 KB | `PSRAM check - Required: 1012 KB` |
| **シナリオが使える空き** | **約 4762 KB** | |

#### 展開コスト

`sizeof(cJSON)` は **40 バイト**（Xtensa 32bit。`double valuedouble` の 8 バイト境界により
36→40 にパディング）。オブジェクトのキーと文字列値は個別に確保される。

本文 L 文字（日本語 UTF-8 = 3 バイト/字）の `text` コマンド 1 個あたり:

| 内訳 | バイト |
|---|---|
| コマンドのオブジェクトノード | 48 |
| `"type"` のノード + キー + 値 | 74 |
| `"body"` のノード + キー + 本文 | 70 + 3L |
| **ツリー合計** | **192 + 3L** |
| （ファイル上の同じ記述） | 30 + 3L |

**解析中は原文バッファとツリーが同時に生きる**ため、ピークは両者の和になる。
総文字数 C、コマンド数 N として:

```
ピーク ≒ 222N + 6C バイト
```

#### 見積もり（1 コマンド平均 100 文字の場合）

| 本文の総文字数 | ピーク使用量 | 空き 4762KB に対して |
|---|---|---|
| 1 万字 | 約 82 KB | 1.7 % |
| **5 万字** | **約 410 KB** | **8.6 %** |
| 10 万字 | 約 820 KB | 17 % |
| 30 万字 | 約 2.4 MB | 52 % |
| **実用上の天井** | **約 35 万字** | 余裕を見て 3MB まで使う想定 |

解析後は原文バッファを解放するので、**常時使用はピークの約 6 割**。

#### 書く側が気にすべきこと

- **文字数よりコマンド数が効く。** 1 コマンドあたり 192 バイトの固定費がかかるので、
  短い `text` を大量に並べるより、ある程度まとめた方がメモリ効率は良い
  （ページ送りがあるので、長い `body` を書いても表示は破綻しない）
- 足りなくなったら、まず **キャンバスの 1012 KB** を遷移中だけ確保する形に変えるのが効く。
  単独では最大の消費者
- それでも足りなければ章ごとの分割を検討する

#### 実測の仕方

読み込み時に実際の使用量がログに出る。見積もり式の検証にも使える。

```
I (xxx) SCENARIO: Parsed 1520 bytes. Tree: 2432 bytes in PSRAM (x1.60 of source), internal delta: 0 bytes
```

- `internal delta` が大きい → `initAllocator()` の呼び忘れ。PSRAM フックが効いていない
- 見積もりが空き PSRAM を超えそうな場合は警告が出るが、**読み込みは試行される**

---

## 10. 完全なサンプル

全コマンドを 1 回以上使い、`start` から `end` まで辿れる最小の例。

```json
{
  "format_version": 1,

  "meta": {
    "id": "sample_001",
    "title": "サンプルシナリオ",
    "author": "ayame_sys",
    "version": "1.0.0",
    "description": "全コマンドの動作例",
    "text_direction": "VERTICAL",
    "update_url": null
  },

  "assets": {
    "backgrounds": {
      "room":   "images/bg/room.png",
      "street": "images/bg/street.png"
    },
    "characters": {
      "ayame": {
        "normal": "images/chara/ayame_normal.png",
        "smile":  "images/chara/ayame_smile.png"
      }
    }
  },

  "variables": {
    "met_ayame": false,
    "affection": 0,
    "route": "none"
  },

  "start": "opening",

  "scenes": {

    "opening": {
      "commands": [
        { "type": "clear", "color": "BLACK" },
        { "type": "bg", "image": "room", "transition": "FADE_IN" },
        { "type": "text", "body": "静かな部屋だ。" },
        { "type": "beep", "freq": 880, "duration": 120 },
        { "type": "wait", "ms": 500 },
        { "type": "chara", "id": "ayame", "expression": "normal", "x": 60, "y": 400 },
        { "type": "text", "speaker": "あやめ", "body": "|今日<きょう>はいい天気ね。" },
        { "type": "set", "var": "met_ayame", "value": true }
      ],
      "next": "choice_scene"
    },

    "choice_scene": {
      "commands": [
        {
          "type": "choice",
          "prompt": "どうする？",
          "options": [
            { "label": "話しかける", "next": "talk" },
            { "label": "黙っている", "next": "silent" },
            { "label": "名前を呼ぶ", "next": "call",
              "cond": { "var": "met_ayame", "op": "==", "value": true } }
          ]
        }
      ]
    },

    "talk": {
      "commands": [
        { "type": "chara", "id": "ayame", "expression": "smile" },
        { "type": "text", "speaker": "あやめ", "body": "うん、話しかけてくれて嬉しい。" },
        { "type": "set", "var": "affection", "op": "+=", "value": 2 },
        { "type": "set", "var": "route", "value": "a" }
      ],
      "next": "ending_check"
    },

    "silent": {
      "commands": [
        { "type": "text", "body": "何も言えなかった。" },
        { "type": "set", "var": "affection", "op": "-=", "value": 1 }
      ],
      "next": "ending_check"
    },

    "call": {
      "commands": [
        { "type": "text", "speaker": "あやめ", "body": "名前、覚えていてくれたんだ。" },
        { "type": "set", "var": "affection", "op": "+=", "value": 3 },
        { "type": "set", "var": "route", "value": "a" }
      ],
      "next": "ending_check"
    },

    "ending_check": {
      "commands": [
        { "type": "chara", "id": "ayame", "visible": false },
        { "type": "bg", "image": "street", "transition": "WIPE_HORIZONTAL" },
        { "type": "refresh", "mode": "epd_quality" },
        {
          "type": "if",
          "cond": {
            "all": [
              { "var": "affection", "op": ">=", "value": 2 },
              { "var": "route", "op": "==", "value": "a" }
            ]
          },
          "then": [
            { "type": "text", "body": "二人で街へ出かけた。" },
            { "type": "jump", "next": "good_end" }
          ],
          "else": [
            { "type": "text", "body": "一人で街へ出た。" },
            { "type": "jump", "next": "normal_end" }
          ]
        }
      ]
    },

    "good_end": {
      "commands": [
        {
          "type": "beep",
          "melody": [
            { "freq": 523, "duration": 150 },
            { "freq": 659, "duration": 150 },
            { "freq": 784, "duration": 300 }
          ]
        },
        { "type": "refresh", "mode": "epd_quality", "clear_ghost": true },
        { "type": "end", "ending": "good_end", "message": "GOOD END" }
      ]
    },

    "normal_end": {
      "commands": [
        { "type": "end", "ending": "normal_end", "message": "NORMAL END" }
      ]
    }
  }
}
```

---

## 11. 拡張提案

本仕様の範囲外だが、実装する価値がある機能。優先度順。

### 11.1 電子ペーパー特有（この機種ならではの価値）

| # | 機能 | 内容 |
|---|---|---|
| 1 | **`refresh` コマンド**（本仕様に採用済み） | 残像対策をシナリオ作者が制御。場面転換の区切りに置く |
| 2 | **`epd_mode` の使い分け** | 会話は `epd_fast`、一枚絵は `epd_quality`。テンポと画質を両立 |
| 3 | **自動リフレッシュ間隔** | N シーンごとに自動でフルリフレッシュ（`settings.json` の `auto_refresh_scenes`） |

### 11.2 アドベンチャーゲームとして

| # | 機能 | 内容 |
|---|---|---|
| 4 | **バックログ** | 直近 N 件のテキストを遡る。電子ペーパーは残像が残るため紙のように読み返せると相性が良い |
| 5 | **既読スキップ** | `read_scenes` を使い既読シーンを高速送り。周回プレイに必須 |
| 6 | **セーブ時サムネイル** | その時点の画面を縮小して保存。要 SD 書き込み API |
| 7 | **エンディング一覧・回収率** | `endings` を集計して達成度を表示 |

### 11.3 運用・堅牢性

| # | 機能 | 内容 |
|---|---|---|
| 8 | **起動時の整合性チェック** | 参照画像の存在、未定義シーンへの `jump`、`start` の妥当性、未宣言変数の使用を検証してログに出す。**SD 上の手書き JSON は必ず壊れる**ので優先度は高い |
| 9 | **低電池時のオートセーブ** | 電池残量の実装後。電子ペーパーは電源断でも表示が残るため、状態との乖離が起きやすい |
| 10 | **デバッグモード** | シーン直接ジャンプ、変数ダンプ、条件式の評価結果表示。シナリオ制作の効率に直結する |

### 11.4 将来（Wi-Fi 前提、記法のみ予約）

| # | 機能 | 内容 |
|---|---|---|
| 11 | **シナリオのサーバ配信** | `meta.update_url` を予約済み |
| 12 | **Wi-Fi 接続チェック・電池残量表示** | システムメニュー側の機能 |

---

## 12. 用語集

| 用語 | 意味 |
|---|---|
| **シナリオ** | 1 つの物語。`scenarios/<id>/` フォルダ 1 つに対応 |
| **シーン** | シナリオを構成する単位。コマンドの並びを持つ |
| **コマンド** | シーン内の 1 命令。`type` で種類を表す |
| **論理名** | `assets` で定義する画像の別名。コマンドはこれで参照する |
| **スロット** | セーブの保存枠。`slot01.json` 〜 と `auto.json` |
| **フルリフレッシュ** | 全画素に波形を掛け直し、薄くなった表示のコントラストを戻す操作 |
| **残像（ゴースト）** | 電子ペーパーに前の像が薄く残る現象 |
| **LUT ステップ** | 電子ペーパーの走査回数。多いほど濃く、遅い |
