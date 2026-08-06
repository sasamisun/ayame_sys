# tools/ — 素材を作るツール

AYAME 用の画像とフォントを用意するスクリプト。すべて Python 3 + Pillow。

```
pip install Pillow fonttools
```

`fonttools` は `make_font.py` だけが使う（無くても動くが、
フォントに無い文字が豆腐として埋め込まれる）。

| ツール | 何をするか | 出力先 |
|---|---|---|
| [`make_image.py`](#make_imagepy) | 画像を **16 階調**に落とす | SD カードへ置く PNG |
| [`make_font.py`](#make_fontpy) | TTF/OTF から **VLW フォント**を作る | `.vlw` と `main/fonts/*.h` |
| [`make_icons.py`](#make_iconspy) | 画像をファームウェアに埋め込む | `main/icons/*.h` |

| その他のファイル | 内容 |
|---|---|
| `charset_ja.txt` | `make_font.py` の標準の文字集合（4416 字） |
| `icons/` | メニューのボタン画像の素材（108×80） |

古い作業用スクリプトは [`append/font/`](../append/font/README.md) にある。
**そちらは使わないこと**（理由はそのフォルダの README）。

---

## make_image.py

**SD カードに置く画像を、実機と同じ 16 階調に落とす。**

M5PaperS3 の電子ペーパーは常に 16 階調（4bpp）。
それより多い階調の画像を置いても実機側で丸められるだけなので、
先に落としておけば **PC で見たものがそのまま実機に出る**。

```bash
# 背景（縦長 540x960）
python tools/make_image.py room.jpg -o microsd_sample/scenarios/xx/images/bg/room.png

# 背景（横長。meta.rotation が 1 か 3 のシナリオ用）
python tools/make_image.py wide.jpg --size 960x540 -o images/bg/wide.png

# 写真はディザ付きで
python tools/make_image.py photo.jpg --dither -o images/bg/photo.png

# 立ち絵（大きさそのまま、黒地に合成）
python tools/make_image.py chara.png --size none --pad black -o images/chara/a.png

# 立ち絵（透過を残す。レイヤーで重ねる素材はこちら）
python tools/make_image.py eye.png --size none --keep-alpha -o images/chara/eye.png

# 40% に縮める
python tools/make_image.py chara.png --scale 40% --size none -o images/chara/a.png

# 左右反転（右目用の素材を作る）
python tools/make_image.py eye.png --flip h --size none -o images/chara/eye_r.png

# サムネイル（一覧に出る 56x56）
python tools/make_image.py key.png --thumb -o thumbnail.png
```

| オプション | 既定 | 内容 |
|---|---|---|
| `--size WxH` | `540x960` | 出力サイズ。`none` で元のまま |
| `--thumb` | | 56×56。`--size` より優先 |
| `--scale N%` | | 倍率でリサイズ。**`--size` より先に効く** |
| `--smooth` | 切 | 拡大を滑らかにする（既定は NEAREST） |
| `--fit contain\|cover` | `contain` | `contain`=全体を収める（余白）/ `cover`=全面を埋める（切る） |
| `--pad white\|black` | `white` | 余白と透過の下地 |
| `--levels N` | `16` | 階調数。`2` で白黒 |
| `--dither` | 切 | 誤差拡散ディザ |
| `--flip h\|v\|hv` | | 反転。h=左右 / v=上下 / hv=両方 |
| `--keep-alpha` | 切 | 透過を残す（下記） |
| `--invert` | 切 | 白黒反転 |

### ディザを使う場面

| 素材 | ディザ |
|---|---|
| 写真、グラデーション | **付ける** |
| 線画、文字、アイコン | **付けない**（輪郭がざらつく） |

`epd_fast` / `epd_fastest` で表示するものは、実機側でどのみち 2 値へ
落とされる。そういう画像は `--levels 2` で作った方が結果を確かめやすい。

### 透過を残すか潰すか

| 使い方 | 指定 |
|---|---|
| 背景・一枚絵・サムネイル | **潰す**（既定）。`--pad` の色の上に重ねてから落とす |
| 立ち絵・レイヤーで重ねる部品 | **残す**（`--keep-alpha`） |

背景として使う絵で透過を残すと、実機の合成任せになって
PC で見た絵と一致しない。電子ペーパーに透明は無いため。

一方、[立ち絵のレイヤー](../SCENARIO_SPEC.md#33-assets)は
背景の上に重ねるのが前提なので透過が要る。
`--keep-alpha` を付けると**明るさだけを 16 階調へ落とし、
アルファはそのまま残す**（アルファを量子化すると縁がぎざつく）。

```bash
python tools/make_image.py body.png --size none --keep-alpha -o body.png
```

> **`--keep-alpha` と `--dither` は併用しない。**
> 縁の半透明とディザの網点が重なって汚くなる。

---

## make_font.py

**TTF/OTF から VLW フォントを作る。**

```bash
# 標準の文字集合で 16pt。本体組み込み用のヘッダまで作る
python tools/make_font.py append/font/ShipporiMincho-Regular.ttf \
    --size 16 --header main/fonts/shippori_16.h --symbol shippori

# シナリオの本文に出てくる文字だけ（軽いフォントになる）
python tools/make_font.py font.ttf --size 20 --charset story.txt

# フォントの全収録グリフ（巨大になる）
python tools/make_font.py font.ttf --size 16 --charset all
```

| オプション | 既定 | 内容 |
|---|---|---|
| `--size N` | `16` | フォントサイズ（px） |
| `--charset` | `ja` | `ja` / `ascii` / `all` / テキストファイルのパス |
| `-o` | `<font>-<size>.vlw` | VLW の出力先 |
| `--header` | | C ヘッダも出す場合の出力先 |
| `--symbol` | ヘッダ名 | ヘッダ内の配列名 |
| `--no-antialias` | 切 | アンチエイリアスを切る |

### 生成後に必ず検証が走る

```
--- 検証 ---
  グリフ数     : 4249
  宣言サイズ   : 16pt
  全角の送り幅 : [16]  OK（宣言サイズと一致）
  昇順・重複なし: OK（二分探索が効く）
  半角スペース : 収録あり（送り 4px）
  全角スペース : 収録あり（送り 16px）
```

見るべきは次の4つ。

| 項目 | なぜ見るか |
|---|---|
| **全角の送り幅** | 宣言サイズと一致しないと、指定した pt 数どおりに組まれない |
| **昇順・重複なし** | `VLWFontParser` は昇順のときだけ二分探索を使う。崩れると線形探索に落ちる |
| **半角/全角スペース** | 落ちると空白が縦線として描かれる |
| width / advance の範囲 | M5GFX は `uint8_t` で読む。255 を超えると壊れる |

### 文字集合

| 指定 | 内容 |
|---|---|
| `ja`（既定） | `charset_ja.txt`。ASCII / かな / 漢字約 3700 / **縦書き用字形**（U+FE10〜FE48） |
| `ascii` | 印字可能な ASCII のみ |
| `all` | TTF の cmap にある全グリフ |
| ファイルパス | そのテキストに出てくる文字だけ。行頭 `#` はコメント |

**縦書き用字形を落とさないこと。** `TypoWrite` が縦書きで句読点・括弧を
差し替えるのに使う。無いと「、」が横書きの位置に出る。

### 大きさの目安（`charset_ja.txt` / Shippori Mincho で実測）

| サイズ | ファイル |
|---|---|
| 16pt | 1.00 MB |
| 20pt | 1.46 MB |
| 24pt | 2.05 MB |

pt の約 1.56 乗で増える（pt² ではない。グリフの多くが小さく、
ビットマップが正方形にならないため）。
アプリ領域は 10.5MB、現在の使用は約 2MB。

### 旧 `append/font/ttf2vlw.py` との違い

**送り幅の出し方が違う。** 旧ツールは

```python
set_width = getbbox(char) の幅 + int(font_size * 0.1)
```

としていた。`getbbox()` が返すのは字面の外接矩形ではなく**レイアウト矩形**
（原点から送り幅まで）なので、全角では `16 + 1 = 17px` になり、
**16pt と名乗るフォントの全角送りが 17px** になっていた。
半角も `A` が本来 12px のところ 14px。

本ツールは `font.getlength()` を使う。これが本来の送り幅で、
全角はちょうど指定サイズになる。

他に3点直してある。

| 項目 | 旧 | 本ツール |
|---|---|---|
| ビットマップ | レイアウト矩形のまま（`、` は字面 5×4 なのに 16×4 を格納） | 字面だけ切り出す |
| 収録判定 | 描いた画像を pHash / ORB / SSIM で豆腐と比較 | **cmap を引く**（曖昧さが無い） |
| 空白文字 | 字面が空 → 「フォントに無い」と誤判定して落とす | 送り幅だけのグリフとして残す |

---

## make_icons.py

**UI の画像をファームウェアに埋め込む。** 引数は取らない。

```bash
python tools/make_icons.py
```

| 入力 | 出力 | 用途 |
|---|---|---|
| `tools/icons/*.png` | `main/icons/menu_icons.h` | メニュー下段のボタン（108×80 固定） |
| `append/image/*.png` | `main/icons/images.h` | ロゴなど（寸法は自由） |

SD に置かずに埋め込むのは、**USB MSC が有効な間は SD が読めない**ため。
その状態でメニューのボタンが真っ白になると、
「USB を切る」ボタンすら見えなくなって操作不能になる。

| 素材 | 変換 |
|---|---|
| 透過なし（ボタン） | 明るさで**2値化**する。背景まで含めて描くこと |
| 透過あり（ロゴ） | 白地に合成し、**階調はそのまま**。反転も2値化もしない |

透過ありを2値化しないのは、素材の明暗を推測して補正しないため。
**画面に出したい濃さでそのまま描くこと。**

---

## よくある手順

新しいシナリオの素材を一式そろえる場合。

```bash
S=microsd_sample/scenarios/my_story

# 背景
python tools/make_image.py src/room.jpg  -o $S/images/bg/room.png
python tools/make_image.py src/night.jpg --dither -o $S/images/bg/night.png

# 立ち絵（原寸のまま、透過は白へ潰す）
python tools/make_image.py src/ayame.png --size none -o $S/images/chara/ayame_normal.png

# 立ち絵をレイヤーに分ける場合は透過を残し、40% に縮める
python tools/make_image.py src/body.png --scale 40% --size none --keep-alpha \
    -o $S/images/chara/ayame_body.png

# 一覧に出るサムネイル
python tools/make_image.py src/key.png --thumb -o $S/thumbnail.png
```

フォントを差し替える場合は、生成したヘッダを `main/fonts/` に置き、
`main/fonts/active_font.h` の `#include` と `AYAME_FONT_DATA` を書き換える。
シナリオ側だけで使うなら [`meta.font`](../SCENARIO_SPEC.md#32-meta) で
`.vlw` を直接指せる（本体の再ビルドが要らない）。
**送り幅が変わると1行に入る字数が変わる**ので、
`textboxes` の寸法は実機で見て調整すること。
