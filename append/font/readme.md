# フォント変換ツールの使用例

このフォント変換ツールを使用するいくつかの例を示します。以下のシナリオでツールがどのように動作するかを説明します。

## 基本的な使用例

### 例1: 基本的なフォント変換

最もシンプルな使用方法です。IPAゴシックフォントを16ピクセルサイズでデフォルト文字セットで変換します。

```bash
python font_converter.py ipaexg.ttf
```

出力：
- ファイル名: `ipaexg_16.h` (自動生成)
- 変数名: `ipaexg_16` (ファイル名から生成)

### 例2: フォントサイズを指定

```bash
python font_converter.py ipaexg.ttf --size 24
```

出力：
- ファイル名: `ipaexg_24.h`
- 変数名: `ipaexg_24`

### 例3: 出力ファイル名を指定

```bash
python font_converter.py NotoSansJP-Regular.otf --output gothic_medium.h
```

出力：
- ファイル名: `gothic_medium.h` (指定した名前)
- 変数名: `gothic_medium` (ファイル名から生成)

## 高度な使用例

### 例4: 文字幅最適化を使用

各文字の実際の幅を検出して、余白を除去します。

```bash
python font_converter.py ipaexg.ttf --size 20 --optimize
```

出力：
- ファイル名: `ipaexg_20.h`
- 各文字の実際の幅が最適化されたデータが生成されます

### 例5: カスタム文字セットファイルを使用

特定の文字だけを含む独自の文字セットを使用します。

1. まず `my_charset.txt` というテキストファイルを作成して、必要な文字を記述します：
   ```
   あいうえお
   漢字表示
   ABCabc123
   記号!?
   ```

2. そのファイルを指定して実行：
   ```bash
   python font_converter.py mplus-1p-regular.ttf --charset my_charset.txt
   ```

出力：
- ファイル名: `mplus-1p-regular_16.h`
- `my_charset.txt` に含まれる文字だけがフォントデータに含まれます

### 例6: 複数サイズのフォントを一度に生成

複数のサイズでフォントデータを一括生成します。

```bash
python font_converter.py NotoSansJP-Regular.otf --multiple-sizes 12,16,24,32
```

出力：
- `NotoSansJP-Regular_12.h`
- `NotoSansJP-Regular_16.h`
- `NotoSansJP-Regular_24.h`
- `NotoSansJP-Regular_32.h`

### 例7: すべてのオプションを組み合わせ

複数のオプションを組み合わせた高度な使用例です。

```bash
python font_converter.py rounded-mplus-1p-bold.ttf --size 20 --charset japanese_common.txt --output ui_font.h --optimize --multiple-sizes 16,20,24
```

出力：
- `ui_font_16.h`
- `ui_font_20.h`
- `ui_font_24.h`
- 各ファイルは、指定した文字セットと最適化された文字幅を持ちます

## 実行結果の例

上記の例1を実行した場合、以下のような出力が得られます：

1. ターミナル出力：
   ```
   フォントデータを ipaexg_16.h に保存しました。
   文字数: 141, データサイズ: 45120 バイト
   ```

2. `ipaexg_16.h` ファイルの内容:
   ```c
   #pragma once

   // 自動生成されたフォントデータ
   // フォント: ipaexg.ttf, サイズ: 16px, スタイル: regular
   // 生成日時: 2025-03-21 14:25:03.456789
   // 文字数: 141, データサイズ: 45120 バイト

   #include "epd_text.h"  // フォント構造体の定義

   const FontCharInfo ipaexg_16_chars[] = {
       { 0x0020, 4, 0, 8, 16 },     // スペース
       { 0x0030, 9, 128, 16, 16 },  // 0
       { 0x0031, 9, 256, 16, 16 },  // 1
       // ...（他の文字情報が続く）
   };

   const uint8_t ipaexg_16_bitmap[] = {
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       // ...（ビットマップデータが続く）
   };

   const FontInfo ipaexg_16 = {
       .size = 16,
       .max_height = 16,
       .style = "regular",
       .chars_count = 141,
       .chars = ipaexg_16_chars,
       .bitmap_data = ipaexg_16_bitmap
   };
   ```

## 生成されたフォントヘッダーファイルの使用方法

生成されたヘッダーファイルは、ESP32プロジェクトに以下のように組み込むことができます：

1. `ipaexg_16.h` をプロジェクトのincludeディレクトリに配置

2. テキスト描画モジュールで読み込み：
   ```c
   #include "epd_text.h"
   #include "ipaexg_16.h"  // 生成されたフォントデータ
   
   void app_main(void) {
       // EPD Wrapper初期化
       EPDWrapper epd;
       epd_wrapper_init(&epd);
       
       // テキスト設定構造体の初期化
       EPDTextConfig text_config;
       epd_text_config_init(&text_config);
       
       // フォント設定
       text_config.font = &ipaexg_16;  // 生成したフォント情報を設定
       
       // テキスト描画
       epd_text_draw_string(&epd, 10, 10, "こんにちは世界！", &text_config);
       
       // 画面更新
       epd_wrapper_update_screen(&epd, MODE_GC16);
   }
   ```

このツールは柔軟な設定が可能で、ESP32プロジェクトの様々なフォントニーズに対応できます。文字セットを最適化することでメモリ使用量を抑え、パフォーマンスを向上させることができます。