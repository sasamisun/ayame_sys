# ayame_sys

M5Paper S3（ESP32-S3 + 電子ペーパー 540×960）向けの、
**日本語縦書き表示を備えたアドベンチャーゲーム基盤**。

現状は各サブシステムの動作検証段階で、シナリオ機能はまだ実装していない。
`main/hello_world_main.cpp` はアプリ本体というより各機能のデモ兼テストハーネスである。

---

## ドキュメント

| ファイル | 内容 |
|---|---|
| **[PROGRAM_SPEC.md](PROGRAM_SPEC.md)** | **プログラム仕様書。まずこれを読む** |
| [REFACTORING_LOG.md](REFACTORING_LOG.md) | 改修記録（不具合とその修正経緯、初版仕様書の訂正） |

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
idf.py -p COM7 flash monitor
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
│   ├── hello_world_main.cpp    初期化・シーン描画・メインループ
│   ├── SDcard.*                SDカード / USB MSC
│   ├── TouchHandler.*          タッチ入力のイベント化
│   ├── Button.*                ボタンUI（Button / ButtonManager）
│   ├── VLWFontParser.*         VLWフォントのメトリクス解析
│   ├── TypoWrite.*             日本語の縦書き・横書き組版
│   ├── SimpleTransition.*      画面遷移エフェクト
│   ├── CanvasTest.*            PSRAM・描画性能の検証（テスト専用）
│   └── fonts/
│       └── shippori_16.h       使用中のVLWフォント（4414グリフ）
├── components/
│   └── M5GFX/                  ※git管理外。消すと復元できない
├── managed_components/         espressif/tinyusb, espressif/esp_tinyusb
├── append/font/                フォント生成用の作業ディレクトリ（ビルド対象外）
├── PROGRAM_SPEC.md             プログラム仕様書
└── REFACTORING_LOG.md          改修記録
```

---

## 注意事項

- `components/M5GFX` は **git 管理外**（untracked かつ `.gitignore` にも無い）。
  `git clean` や `git stash -u` で消えると復元できないので、操作前に退避すること。
- 実機の表示は8bitグレースケール。`display.setColorDepth(1)` は
  `Panel_EPD` 側で無視されるため効果がない。ソース中の `TFT_BLUE` などの
  カラー指定もグレーに変換されるため、意図した色にはならない。
- `M5Canvas` は親の色深度を継承せず rgb565（16bpp）で作られる。
  540×960 で1枚あたり約1MB。
- 電子ペーパーでは `fillScreen()` が最も重い操作。ループ内で呼ばないこと。
- `TouchHandler::update()` は破壊的メソッド。1ループにつき1回だけ呼ぶこと。

その他のハマりどころは [PROGRAM_SPEC.md](PROGRAM_SPEC.md) の §7 にまとめてある。
