# ayame_sys

M5PaperS3（ESP32-S3 + 電子ペーパー 540×960）向けの、
**日本語縦書き表示を備えたアドベンチャーゲーム基盤**。

SD カードに置いた JSON のシナリオを読んで再生する。
起動するとシステムメニューが出て、`scenarios/` にあるシナリオを選べる。

**物語を作る人は C++ を書かない。** JSON を SD へ置くだけで、
ビルドもツールチェーンも要らない。

---

## できること

| | |
|---|---|
| **日本語の組版** | 縦書き・ルビ・禁則処理・圏点・縦書き用字形。横書きと同じ画面に置ける |
| **20 個のコマンド** | 本文・背景・立ち絵・選択肢・分岐・変数・セーブ・画面遷移・ブザー |
| **立ち絵のレイヤー合成** | 体・目・口を重ねる。目5種 × 口5種の 25 通りを 11 枚で出せる |
| **中断と再開** | 電子ペーパーは電源を切っても像が残る。**読みかけの画面がそのまま栞になる** |
| **前の画面へ戻る** | 横スワイプ。`meta.back_swipe` で作品ごとに選ぶ |
| **青空文庫の取り込み** | HTML をコマンド1回で読み物にする（[`tools/make_scenario.py`](tools/README.md#make_scenariopy)） |

電子ペーパーは1回の書き換えに 117〜351ms かかる。動きのある演出には向かないが、
**1つの場面ぶんをまとめて1回で出す**ようにしてあるので、読むぶんには待たされない。

---

## ドキュメント

| ファイル | 内容 | 読む人 |
|---|---|---|
| **[SCENARIO_SPEC.md](SCENARIO_SPEC.md)** | **シナリオデータの書き方** | 作品を作る人 |
| **[PROGRAM_SPEC.md](PROGRAM_SPEC.md)** | **プログラムの構成** | 実装する人 |
| [microsd_sample/README.md](microsd_sample/README.md) | SD カードの中身とサンプルの説明 | 両方 |
| [tools/README.md](tools/README.md) | 素材を作るツールの使い方 | 作品を作る人 |
| [REFACTORING_LOG.md](REFACTORING_LOG.md) | 改修記録（不具合とその修正経緯） | |

---

## 試す

1. `microsd_sample/` の**中身**（`scenarios/` と `system/`）を SD のルート直下へコピー
   （`microsd_sample` フォルダごとではない）
2. SD を挿して電源を入れる
3. 一覧からシナリオを選ぶ

27 本のサンプルが入っている。
**まず一覧の先頭「ようこそ AYAME へ」を開くと、何ができるかが一巡で分かる。**
残りは機能ごとの確認用で、各機能の書き方と動作を確かめられる。

---

## ビルド

### ESP-IDF は v5.3.2 を使うこと

**v5.4.3 でビルドすると画面が1ラインおきの白黒縞模様になり、正常に表示できない。**
起動もログも正常だが表示だけが崩れる（ハードウェアの問題ではない）。
原因は未特定。詳細は [PROGRAM_SPEC.md](PROGRAM_SPEC.md) の §2.1 を参照。

VSCode の ESP-IDF 拡張を使う場合は、`.vscode/settings.json` の
`idf.espIdfPathWin` と `idf.currentSetup` が**実在する v5.3.2 のパス**を
指していることを確認する。存在しないパスを指していると拡張が有効化に失敗し、
Visual Studio 同梱の CMake/Ninja にフォールバックしてビルドが壊れる。

### 手順

```
idf.py build
idf.py -p COM5 flash monitor
```

VSCode なら `ESP-IDF: Build your project` を使う。
生の `ninja` タスクを直接叩かないこと（上記のフォールバックを踏む）。

### 起動確認

正常なら起動ログに次が出る。

```
I (xxx) M5GFX: [Autodetect] board_M5PaperS3
```

---

## ディレクトリ構成

```
ayame_sys/
├── main/                       アプリケーション本体
│   ├── main.cpp                入口・画面の切り替え・メインループ
│   ├── SystemMenu.*            シナリオ選択メニュー
│   ├── ScenarioLoader.*        シナリオJSONの読み込み・検証
│   ├── ScenarioPlayer.*        シーンとコマンドの実行
│   ├── TextSystem.*            フォントと描画器の生成・保持
│   ├── TypoWrite.*             日本語の縦書き・横書き組版
│   ├── VLWFontParser.*         VLWフォントのメトリクス解析
│   ├── SDcard.*                SDカード / USB MSC
│   ├── TouchHandler.*          タッチ入力のイベント化
│   ├── Button.*                ボタンUI（Button / ButtonManager）
│   ├── SimpleTransition.*      画面遷移・リフレッシュ・残像消去
│   ├── Buzzer.*                ブザー（LEDC PWM）
│   ├── Power.*                 電源制御と電池残量
│   ├── Settings.*              本体設定（system/settings.json）
│   ├── fonts/
│   │   ├── active_font.h       **使うフォントを1つ選ぶ**（include を書き換える）
│   │   └── gothic_18.h         変換済みVLWフォント（内蔵はこの1書体）
│   └── icons/                  埋め込み画像（メニューのボタン・ロゴ）
├── microsd_sample/             SDカードに入れる中身のサンプル（27本）
├── tools/                      素材を作るツール
│   ├── make_scenario.py        青空文庫の HTML をシナリオに変換する
│   ├── make_image.py           画像を16階調に落とす／拡大縮小／反転する
│   ├── make_font.py            TTF/OTF から VLW フォントを作る
│   ├── make_icons.py           UI画像をファームウェアに埋め込む
│   ├── charset_ja.txt          フォントの標準文字集合（4415字）
│   ├── icons/                  メニューのボタン画像の素材
│   └── font/                   変換済みフォント（.vlw と .h）
├── components/
│   └── M5GFX/                  ※git管理外。消すと復元できない
├── managed_components/         espressif/tinyusb, espressif/esp_tinyusb
├── SCENARIO_SPEC.md            シナリオデータ仕様書
├── PROGRAM_SPEC.md             プログラム仕様書
└── REFACTORING_LOG.md          改修記録
```

---

## 注意事項

- `components/M5GFX` は **git 管理外**（untracked かつ `.gitignore` にも無い）。
  `git clean` や `git stash -u` で消えると復元できないので、操作前に退避すること。
- 実機の表示は**16階調（4bpp）**。白黒2値ではない。
  `display.setColorDepth()` は `Panel_EPD` 側で引数が無視されるため呼ぶ意味がない。
  ソース中の `TFT_BLUE` などのカラー指定もグレーに変換される。
- `M5Canvas` は親の色深度を継承せず rgb565（16bpp）で作られる。
  540×960 で1枚あたり約1MB。
- 電子ペーパーでは `fillScreen()` が最も重い操作。ループ内で呼ばないこと。
- `TouchHandler::update()` は破壊的メソッド。1ループにつき1回だけ呼ぶこと。
- `display.loadFont()` と `unloadFont()` は必ず対にすること。
  載せたままだと `setTextSize()` の基準が変わる。

その他のハマりどころは [PROGRAM_SPEC.md](PROGRAM_SPEC.md) の §7 にまとめてある。

---

## 謝辞とライセンス

**このプロジェクトは、先人の書いたものの上に成り立っている。**
とくに電子ペーパーの駆動と日本語フォントの描画は、
自分で書いたら到底ここまで来られなかった。

### ソフトウェア

| | 作者 | ライセンス | 使い方 |
|---|---|---|---|
| **[M5GFX](https://github.com/m5stack/M5GFX)** | M5Stack | MIT | **画面のすべて。** 電子ペーパーの駆動・16階調の LUT・PNG デコード・VLW フォントの描画 |
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | lovyan03 | FreeBSD | M5GFX の土台。描画とスプライトの実装そのもの |
| [ESP-IDF](https://github.com/espressif/esp-idf) v5.3.2 | Espressif | Apache-2.0 | FreeRTOS・SD/FAT・LEDC・PSRAM |
| [cJSON](https://github.com/DaveGamble/cJSON) | Dave Gamble | MIT | シナリオ JSON の解析（ESP-IDF 同梱） |
| [esp_tinyusb](https://components.espressif.com/components/espressif/esp_tinyusb) 1.7.2 / [tinyusb](https://github.com/hathach/tinyusb) 0.18.0 | Espressif / hathach | Apache-2.0 / MIT | USB マスストレージ（PC から SD を書き換える） |
| [Pillow](https://python-pillow.org/) | | HPND | `tools/` の画像・フォント変換 |
| [fontTools](https://github.com/fonttools/fonttools) | | MIT | `tools/make_font.py` の収録字判定 |

M5GFX は上記のほか TJpgDec（ChaN）、Pngle（kikuchan）なども内包している。
一覧は [`components/M5GFX/README.md`](components/M5GFX/README.md) の License 節を参照。

### フォント

| フォント | 作者 | ライセンス | どこで使っているか |
|---|---|---|---|
| **IPAex ゴシック** | 情報処理推進機構（IPA） | [IPA フォントライセンス v1.0](components/M5GFX/src/lgfx/Fonts/IPA/IPA_Font_License_Agreement_v1.0.txt) | 内蔵フォント `main/fonts/gothic_18.h`、`02_nekonojimusyo` の `fonts/book.vlw` |
| x12y12pxMaruMinya | Fontopo | 配布元の条件による | `22_font` の `fonts/maruminya_18.vlw`（独自フォントの実演用） |

**同梱しているのは元のフォントではなく、`tools/make_font.py` で
VLW 形式へ変換した派生物**（ビットマップとメトリクスだけを抜き出したもの）。

元のフォントに戻したい場合、および別の書体で作り直したい場合は、
配布元から TTF を入手して次を実行する。

```bash
python tools/make_font.py <入手した>.ttf --size 18 \
    -o tools/font/gothic_18.vlw \
    --header main/fonts/gothic_18.h --symbol font_gothic_18
```

IPAex フォントは [IPA のサイト](https://moji.or.jp/ipafont/)から入手できる。
IPA フォントライセンスの全文は M5GFX にも同梱されている（上表のリンク先）。

### 素材

| | 出典 |
|---|---|
| あやめ（`00_ayame_sample` の立ち絵・背景・アイキャッチ） | このプロジェクトのために用意したもの |
| 「猫の事務所」宮沢賢治 | [青空文庫](https://www.aozora.gr.jp/cards/000081/card464.html)（入力: 細川みづ穂 / 校正: 瀬戸さえ子）。本文は著作権保護期間満了 |
| ゆっくり立ち絵（`00_yukkuri_sample`） | 配布元の規約に従うこと（原作: 東方Project / 上海アリス幻樂団）。**紹介シナリオは権利の都合で `00_ayame_sample` へ移した** |

### このリポジトリ

上記の第三者の成果物を除く部分について、`ayame_sys` 自体のライセンスは
まだ決めていない。利用したい場合は連絡してほしい。
