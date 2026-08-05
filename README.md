# ayame_sys

M5Paper S3（ESP32-S3 + 電子ペーパー 540×960）向けの、
**日本語縦書き表示を備えたアドベンチャーゲーム基盤**。

SD カードに置いた JSON のシナリオを読んで再生する。
起動するとシステムメニューが出て、`scenarios/` にあるシナリオを選べる。

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

21 本のサンプルが入っており、各機能の書き方と動作を確かめられる。

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
│   ├── hello_world_main.cpp    旧デモ。ビルド対象外
│   └── fonts/
│       └── shippori_16.h       使用中のVLWフォント（4414グリフ / 約1.12MB）
├── microsd_sample/             SDカードに入れる中身のサンプル（21本）
├── tools/                      素材を作るツール（画像の減色・フォント生成・アイコン埋め込み）
│   ├── make_image.py           画像を16階調に落とす
│   ├── make_font.py            TTF/OTF から VLW フォントを作る
│   ├── make_icons.py           UI画像をファームウェアに埋め込む
│   └── charset_ja.txt          フォントの標準文字集合（4416字）
├── components/
│   └── M5GFX/                  ※git管理外。消すと復元できない
├── managed_components/         espressif/tinyusb, espressif/esp_tinyusb
├── append/                     素材の置き場（ビルド対象外）
│   ├── font/                   TTFと過去の生成物。棚卸しは append/font/README.md
│   └── image/                  ロゴなど、埋め込み用の画像素材
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
