# append/font/ — フォント作成の作業場（旧）

> **ここのスクリプトは使わないこと。**
> フォントを作るなら [`tools/make_font.py`](../../tools/make_font.py) を使う
> （理由は下記「なぜ置き換えたか」）。
>
> このフォルダは**素材（TTF）と過去の生成物を置いておくため**に残してある。
> ビルド対象ではない。

---

## 中身の棚卸し

### 素材（これは今も使う）

| ファイル | 内容 |
|---|---|
| `ShipporiMincho-Regular.ttf` | **現在使用中のフォントの元**。明朝体 |
| `Mplus2-Light.ttf` | M PLUS 2 Light。ゴシック |
| `ipaexg.ttf` / `ipaexm.ttf` | IPAex ゴシック / 明朝。ライセンスは `IPA_Font_License_Agreement_v1.0.txt` |
| `SourceHanSansJP-Light.otf` | 源ノ角ゴシック |
| `x12y16pxMaruMonica.ttf` | 小さいドット系 |
| `x12y12pxMaruMinya.ttf` | 同上 |
| `Readme_IPAexfont00401.txt` | IPAex の配布物に付いていた説明 |

### 文字リスト

| ファイル | 字数 | 内容 |
|---|---|---|
| `joyo_ghost_griflist.txt` | 4416 | **現在の標準セットの元。** `tools/charset_ja.txt` にコピー済み |
| `joyo_joyogai_jinmei_griflist_griflist_filtered.txt` | 4383 | 上を `font_glyph_checker.py` で絞ったもの |
| `griflist_nekonoba.txt` | 160 | 用途不明の小さいリスト |

`tools/make_font.py` は `tools/charset_ja.txt` を見るので、
**ここのリストを直しても反映されない。**

### 過去の生成物（使わないこと）

| ファイル | 名前の pt | 宣言 pt | 全角の実送り |
|---|---|---|---|
| `shippori.h` | — | 18 | 18px |
| `genshin.h` | — | 18 | 18px |
| `mplus2_18.h` | 18 | 18 | 18px |
| `mplus2_16.h` | **16** | **32** | **17px** |
| `mplus2_32.h` | **32** | **38** | **35px** |
| `mplus2.h` | — | 12 | 13px |
| `marumoni.h` | — | 16 | 13px |
| `Mplus2-Light-12.vlw` | 12 | 12 | 13px |
| `x12y16pxMaruMonica-12.vlw` | 12 | 12 | 10px |
| `x12y16pxMaruMonica-16.vlw` | 16 | 16 | 13px |

**ファイル名・宣言サイズ・実際の送り幅が三者ばらばら。**
`mplus2_16.h` は名前が 16、ヘッダの宣言が 32、全角の送りが 17px。

次の3つは **VLW ですらない**。中身は Adafruit GFX 形式のビットマップで、
本プロジェクトの `VLWFontParser` では読めない。

- `maruminya_mini.h`（`x12y12pxMaruMinya5pt8bBitmaps`）
- `marumiya_mini.h`（`x12y12pxMaruMinya10pt8bBitmaps`）
- `myfont.h`

`*_missing.txt` は生成時に「フォントに無い」と判定された文字の記録。
ただし旧ツールの判定は当てにならない（下記）。
`convLog.log` は過去の変換ログ。

---

## なぜ置き換えたか

### 1. 送り幅が本来の値ではなかった

`ttf2vlw.py` は送り幅（`setWidth`）をこう出していた。

```python
set_width = font.getbbox(char) の幅 + int(font_size * 0.1)
```

`getbbox()` が返すのは字面の外接矩形ではなく**レイアウト矩形**
（原点から送り幅まで）。全角ならその幅はすでに `font_size` なので、

```
16 (レイアウト幅) + 1 (16 * 0.1) = 17px
```

となり、**16pt と名乗るフォントの全角送りが 17px** になっていた。
半角も `A` が本来 12px のところ 14px。

1 文字あたり 1〜2px ずつ広いので、1 行 30 字なら 30〜60px ぶん字数が減る。

正しい送り幅は `font.getlength(char)`。`tools/make_font.py` はこちらを使い、
生成後に「全角の送り幅が宣言サイズと一致するか」を必ず検証する。

### 2. ビットマップがレイアウト矩形のままだった

読点 `、` は字面が 5×4 しかないのに 16×4 を格納していた。差分は全部透明。
VLW 本来の持ち方は「字面だけを持ち、位置は `leftExtent` / `topExtent` で表す」。

直した結果、同じ文字数で **1.12MB → 1.00MB** になった。

### 3. 収録判定が画像比較だった

フォントにその文字が入っているかを、**描いた結果を豆腐（□）の見本と
pHash / ORB / SSIM で比べて**判定していた。
OpenCV・imagehash・scikit-image を任意依存で読み込む作りになっているのもこのため。

`cmap` を引けば曖昧さなく分かる。実際、現在の `main/fonts/shippori_16.h` には
**豆腐が 3 つ混入している**（U+FE30 `︰` / U+FE32 `︲` / U+FE33 `︳`。
Shippori Mincho はこの 3 つを収録していない）。

### 4. 空白文字を落としていた

`U+0020` は字面が無いので、ビットマップが全て 0 になる。
旧ツールはそれを「フォントにこの文字は無い」と判定して落としていた。

その結果**半角スペースが縦線として描かれる**不具合が出た
（`TypoWrite` が収録なしのフォールバックへ入り、代替の字形を描いていた）。
今は `TypoWrite` 側にも空白の判定を入れてあるが、
フォントに入れておくのが本来。

---

## 各スクリプトが何だったか

| ファイル | 内容 | 代替 |
|---|---|---|
| `ttf2vlw.py` | TTF → VLW。上記の問題がある | **`tools/make_font.py`** |
| `bin2header.py` | `.vlw` → C ヘッダ | `tools/make_font.py --header` |
| `f2d.py` | TTF → **独自形式**のヘッダ（`FontCharInfo` / `FontInfo`）。VLW ではない。この構造体を読むコードは本プロジェクトに無い | 無し（不要） |
| `txt2griflist.py` | テキスト → 重複を除いた文字リスト | `make_font.py --charset ファイル` が直接テキストを読む |
| `font_glyph_checker.py` | 文字リストから、TTF に無い文字を除く | `make_font.py` が cmap で自動判定する |
| `export-griflist.py` | TTF の収録文字を一覧に出す | `make_font.py --charset all` |
| `font_glyph_extractor.py` | TTF から指定文字だけ抜いた TTF を作る | 代替なし。**これだけは今も使える** |
| `f2d_usage_old.md` | `f2d.py` の使用例。ただし本文中のコマンド名は `font_converter.py` で、**実在しないファイルを指している** | 本ファイル |

---

## 今フォントを作る手順

```bash
python tools/make_font.py append/font/ShipporiMincho-Regular.ttf \
    --size 16 --header main/fonts/shippori_16.h --symbol shippori
```

詳細は [`tools/README.md`](../../tools/README.md)。
