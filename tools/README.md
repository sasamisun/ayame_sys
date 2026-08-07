# tools/ — 素材を作るツール

AYAME 用の画像とフォントを用意するスクリプト。すべて Python 3 + Pillow。

```
pip install Pillow fonttools
```

`fonttools` は `make_font.py` だけが使う（無くても動くが、
フォントに無い文字が豆腐として埋め込まれる）。

| ツール | 何をするか | 引数 | 出力先 |
|---|---|---|---|
| [`make_scenario.py`](#make_scenariopy) | 青空文庫の HTML を**シナリオ**に変換する | HTML 1つ | `scenarios/<id>/scenario.json` |
| [`make_image.py`](#make_imagepy) | 画像を **16 階調**に落とす | 画像 1枚 | SD カードへ置く PNG |
| [`make_font.py`](#make_fontpy) | TTF/OTF から **VLW フォント**を作る | TTF 1つ | `.vlw` と `main/fonts/*.h` |
| [`make_icons.py`](#make_iconspy) | 画像をファームウェアに埋め込む | **取らない** | `main/icons/*.h` |

> **画像を1枚変換したいなら `make_image.py`。**
> `make_icons.py` はフォルダの中身をまとめてヘッダにするだけのもので、
> 拡大縮小も減色もしない。名前が似ているので取り違えやすい。

| その他のファイル | 内容 |
|---|---|
| `charset_ja.txt` | `make_font.py` の標準の文字集合（4415 字） |
| `icons/` | メニューのボタン画像の素材（108×80） |
| `images/` | ロゴなど、寸法の決まっていない埋め込み画像 |
| `font/` | 変換済みの VLW と、その C ヘッダ |

**TTF/OTF はリポジトリに同梱していない。** 書体ごとに再配布の条件が違うため、
使う人が配布元から入手すること。以下の例で `<ゴシック>.ttf` と書いてあるのは
そのつもり。本文に使うならゴシックを選ぶこと（理由は
[make_font.py の節](#明朝はこの寸法では使えない)）。

---

## make_scenario.py

**青空文庫の HTML を、そのまま読めるシナリオに変換する。**

```bash
python tools/make_scenario.py 464_19941.html \
    -o microsd_sample/scenarios/02_nekonojimusyo
```

青空文庫の HTML は XHTML 1.1 準拠で、ルビも
`<ruby><rb>親字</rb><rt>ルビ</rt></ruby>` として構造で持っている。
だから機械的に読める。**本文を手で JSON へ流し込む作業がまるごと要らない。**

出力は `scenario.json` と `thumbnail.png`。

| 元 | 変換後 |
|---|---|
| `<ruby><rb>繻子</rb>…<rt>しゆす</rt>…</ruby>` | `｜繻子《しゆす》` |
| `<strong class="SESAME_DOT">かま</strong>`（傍点） | `｜か《﹅》｜ま《﹅》` |
| `<br />` | 段落の区切り |
| `<div class="jisage_2">` | 各行の頭に全角空白2つ |
| `<h3>` / `<h4>`（見出し） | シーンの区切り＋見出しの枠へ |
| `<img alt="※［＃…］">`（外字） | 代替文字にして**警告** |
| 底本・入力者・校正者 | 最後の「奥付」シーン |

**畳んだあとにタグが残っていないか検査する。** 正規表現で黙って消すと
本文が欠けるので、知らないタグが残っていたら名前を挙げて警告する。

### 1タップの分量は画面から計算する

**単純な字数では切らない。** 段落は必ず改行から始まるので、
字数で切ると列の余りが捨てられて詰め込みすぎになる。

出力するテキストボックスの寸法とフォントの全角送り（`--em`）から、
1列の字数と1ページの列数を求め、段落ごとに列を積んで区切る。

```
1画面 : 12 列 × 44 字 = 528 字（縦書き 500x920 / em 18）
出力  : text 18 個 / シーン 2 個 / 18 タップ
```

「猫の事務所」（6,266 字・111 段落）で 18 タップ。
字数だけで切ると 12 個になるが、実際には入りきらない。

### 栞は1ページごとの `save`

各ページの前に `{ "type": "save", "slot": 1 }` を置く。
`run()` は「実行 → 添字を進める」順なので、`save` が記録する位置は
**`save` 自身**を指す。読み込むとその `save` から再実行され、同じページが出る。

`save` はシステムの栞（`system/settings.json`）を触らない。触るのは
`suspend` と電池切れの自動保存だけ。そのため**タイトル画面**を置き、
「つづきから」で `load` するようにしてある。

> 1ページごとに SD へ 400 バイト前後を書く。
> 電子ペーパーの書き換えが 117〜351ms なので埋もれる程度だが、
> 気になれば `--no-save` で切れる。

### 収録されていない字を知らせる

**VLW に無い字は豆腐にすらならず、何も描かれない。**
青空文庫の作品には常用漢字の外がふつうに出てくる。

縦書きなら**縦書き用字形**（`、`→`︑` など）も見る。
無いと句読点や括弧が横書きの向き・位置のまま出る。

変換のたびにフォントと突き合わせ、足りなければ直し方まで出す。

```
警告  :
  - gothic_18.h に無い字が 3 種ある: 炯竃繻
    収録されていない字は何も描かれない（豆腐にもならない）。
  - book.vlw に縦書き用字形が無い字が 8 種ある: …、。《》「」ー
    縦書きなのに横書きの向き・位置のまま出る。
    この作品用のフォントを作り、--font で指し直すこと（縦書き用字形は自動で入る）:
      python tools/make_font.py <ゴシック>.ttf --size 18 \
          --charset .../scenario.json -o .../fonts/book.vlw
      python tools/make_scenario.py .../464_19941.html \
          -o ... --font fonts/book.vlw
```

**`scenario.json` をそのまま `--charset` に渡せる。**
本文に出てくる字だけのフォントになるので軽い
（「猫の事務所」は 611 字で 0.16MB。標準の文字集合なら 1.3MB）。

> **元にする TTF はゴシックにすること。**
> 明朝はこの寸法だと横線が落ちて読めない（[make_font.py の節](#明朝はこの寸法では使えない)）。

### 前の画面へ戻れるようにする

`meta.back_swipe` を**既定で入れる**。読み物なので、読み違えたときに
戻れないと困る。横スワイプで1画面戻り、逆向きで進む
（[SCENARIO_SPEC.md](../SCENARIO_SPEC.md#32-meta)）。

`--no-back-swipe` で外せる。

### オプション

| オプション | 既定 | 内容 |
|---|---|---|
| `-o` | 入力と同じ場所 | 出力先フォルダ。**フォルダ名がシナリオ ID になる** |
| `--encoding` | 自動 | 文字コード。`<?xml encoding=…?>` から判別する |
| `--direction` | `VERTICAL` | 本文の向き |
| `--rotation` | なし | `meta.rotation`。横向きにするとき |
| `--size WxH` | `540x960` | 画面の大きさ。ボックスをこれに合わせる |
| `--margin N` | `20` | 画面の縁からの余白 |
| `--em N` | `18` | フォントの全角送り。1画面の量の計算に使う |
| `--font PATH` | なし | `meta.font`。**指定したら `--em` も合わせること** |
| `--chars N` | 自動 | 1画面の字数を直接指定する |
| `--scene-pages N` | `20` | 見出しが無いときのシーン1つあたりのページ数 |
| `--sesame` | `﹅` | 傍点の出し方。`﹅` / `・` / `bracket` / `none` |
| `--no-save` | 切 | ページごとの `save` を入れない |
| `--no-back-swipe` | 切 | 横スワイプで戻る機能を入れない |
| `--no-thumb` | 切 | サムネイルを作らない |
| `--thumb-font` | 手元から探す | サムネイルに使う TTF。見つからなければ飛ばす |
| `--check-font` | 内蔵フォント | 収録字の照合に使う VLW |
| `--no-check-font` | 切 | 収録字の照合をしない |
| `--dry-run` | 切 | 書かずに集計だけ出す |

### 対応していないもの

| 項目 | 理由 |
|---|---|
| 青空文庫の**テキスト版**（`.txt`） | HTML のほうが構造が明確で、注記も markup へ畳まれている |
| 挿絵 | 画像は別途 `make_image.py` を通し、手で `bg` を足すこと |
| 傍線・割注・縦中横 | AYAME に表現手段が無い |
| ダウンロード | ファイルは利用者が用意する |

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
python tools/make_font.py <明朝>.ttf \
    --size 16 --header main/fonts/gothic_16.h --symbol font_gothic_16

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
| `--no-vertical` | 切 | 縦書き用字形（U+FE19〜FE48）を入れない |

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

### 縦書き用字形は必ず入る

**縦書きの句読点・括弧は別のコードポイントで描かれる。**

```
、 U+3001 → ︑ U+FE11        「 U+300C → ﹁ U+FE41
。 U+3002 → ︒ U+FE12        」 U+300D → ﹂ U+FE42
```

差し替えるのは `TypoWrite::convertToVerticalGlyph()`。
**差し替え先がフォントに無ければ元の文字に戻す**（豆腐にしないため）。
戻された文字は回転もされないので、**縦書きなのに横向きのまま出る。**

これらは本文に literal では現れないので、`--charset 本文.txt` のように
「出てくる文字だけ」を集めると1つも入らない。
そこで**どの `--charset` でも 25 字を必ず足す**（数 KB しか増えない）。

欧文専用のフォントを作るときは `--no-vertical` で外せる。

検証に収録数が出る。

```
  縦書き用字形 : 25/25 収録
```

**元の TTF に字が無ければ足しようがない。** そのときはこう出る。

```
  縦書き用字形 : 0/25 収録  *** 縦書きで 、。「」 が横向きになる ***
      横向きになる文字: 、 。 … ‥ ー － _ ( ) { } 〔 〕 【 】 《 》 〈 〉 「 」 『 』 [ ]
      元のフォントにこれらの字が無い。縦書きで使うなら別の書体にすること。
```

よく使う書体の収録状況（実測）:

| TTF | 縦書き用字形 |
|---|---|
| `ipaexg.ttf` / `ipaexm.ttf` | 25/25 |
| `ShipporiMincho-Regular.ttf` | 22/25 |
| `Mplus2-Light.ttf` | 10/25 |
| `x12y12pxMaruMinya.ttf` | **0/25**（縦書きには使えない） |

### 明朝はこの寸法では使えない

**IPAex 明朝を 18px でラスタライズすると、細い横線が丸ごと落ちる。**


| サイズ | 「三」の横線 3 本 |
|---|---|
| 18px | 上が消える |
| 20px | 出る |
| 22px | 中央が消える |
| 24px | **3 本とも消える** |

ヒンティングがヘアラインを 0 へ丸めているので、ツール側では直せない。
4倍で描いて縮めれば形は戻るが、今度は全部が中間の灰色になり、
電子ペーパーではやはり薄い。

**本文にはゴシックを使うこと。** 横線も縦線も 1〜2px の真っ黒で出る。
明朝の見た目が要る場合は、実機で確かめてから決めること。

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

### 旧ツール（`ttf2vlw.py`）との違い

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

**UI の画像をファームウェアに埋め込む。**

**引数は取らない。入出力はフォルダで決まっている。**
素材を差し替えてから、引数なしで回す。

```bash
python tools/make_icons.py
```

| 入力 | 出力 | 用途 |
|---|---|---|
| `tools/icons/*.png` | `main/icons/menu_icons.h` | メニュー下段のボタン（108×80 固定） |
| `tools/images/*.png` | `main/icons/images.h` | ロゴなど（寸法は自由） |

**拡大縮小も減色もしない。** 貼る前に整えておくこと（`make_image.py`）。
引数を渡すとエラーになり、`make_image.py` の使い方を出す。

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

### 青空文庫の作品を1本入れる

```bash
S=microsd_sample/scenarios/my_book

# 1. まず変換して、収録されていない字を出させる
python tools/make_scenario.py 手元のファイル.html -o $S

# 2. 警告が出たら、この作品用のフォントを作る
python tools/make_font.py <ゴシック>.ttf --size 18 \
    --charset $S/scenario.json -o $S/fonts/book.vlw

# 3. そのフォントを指して作り直す（警告が消えれば完成）
python tools/make_scenario.py 手元のファイル.html -o $S --font fonts/book.vlw
```

### シナリオの素材を一式そろえる

```bash
S=microsd_sample/scenarios/my_story

# 背景
python tools/make_image.py src/room.jpg  -o $S/images/bg/room.png
python tools/make_image.py src/night.jpg --dither -o $S/images/bg/night.png

# 立ち絵（原寸のまま、透過は白へ潰す）
python tools/make_image.py src/ayame.png --size none -o $S/images/chara/ayame_normal.png

# 立ち絵をレイヤーに分ける場合は透過を残し、40% に縮める
python tools/make_image.py src/body.png --scale 40% --size none --keep-alpha -o $S/images/chara/ayame_body.png

# 一覧に出るサムネイル
python tools/make_image.py src/key.png --thumb -o $S/thumbnail.png
```

フォントを差し替える場合は、生成したヘッダを `main/fonts/` に置き、
`main/fonts/active_font.h` の `#include` と `AYAME_FONT_DATA` を書き換える。
シナリオ側だけで使うなら [`meta.font`](../SCENARIO_SPEC.md#32-meta) で
`.vlw` を直接指せる（本体の再ビルドが要らない）。
**送り幅が変わると1行に入る字数が変わる**ので、
`textboxes` の寸法は実機で見て調整すること。
