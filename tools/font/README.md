# tools/font/ — 変換済みフォント

`append/font/` の TTF/OTF を [`tools/make_font.py`](../make_font.py) で
VLW に変換したもの。文字集合は `tools/charset_ja.txt`（4416 字）。

| `.vlw` | 本体。M5GFX が読む形式 |
|---|---|
| `.h` | 上を C の配列にしたもの。`main/fonts/` にコピーして使う |
| `_missing.txt` | そのフォントに入っていなかった文字の一覧 |

**`main/fonts/` に同じ `.h` を配置済み。**
切り替えは [`main/fonts/active_font.h`](../../main/fonts/active_font.h) の
`AYAME_FONT` の数字を変えてビルドし直す。

---

## 一覧

| `AYAME_FONT` | ファイル | 元 | 系統 | 全角の送り | 収録 | 容量 |
|---|---|---|---|---|---|---|
| 0 | `shippori_16` | Shippori Mincho Regular | 明朝 | 16px | 4249 | 1.00 MB |
| 1 | `mplus2_16` | M PLUS 2 Light | ゴシック（細） | 16px | 3918 | 0.92 MB |
| 2 | `ipaexg_16` | IPAex ゴシック | ゴシック | 16px | 4415 | 1.03 MB |
| 3 | `ipaexm_16` | IPAex 明朝 | 明朝 | 16px | 4415 | 1.04 MB |
| 4 | `sourcehan_16` | 源ノ角ゴシック Light | ゴシック（細） | 16px | 4410 | 1.09 MB |
| 5 | `marumonica_16` | x12y16px MaruMonica | ドット（丸） | **漢字 12px / かな 16px** | 4265 | 0.68 MB |
| 6 | `maruminya_12` | x12y12px MaruMinya | ドット（小） | 12px | 4222 | 0.57 MB |
| 7 | `maruminya_16` | x12y12px MaruMinya | ドット | 16px | 4222 | 0.98 MB |
| 8 | `maruminya_18` | x12y12px MaruMinya | ドット | 18px | 4222 | 1.21 MB |

行の高さ（ascent + |descent|）は系統で違う。

| フォント | ascent | descent | 行の高さ |
|---|---|---|---|
| Shippori / M PLUS 2 / 源ノ角 | 19 | −5 | 24 |
| IPAex ゴ / IPAex 明 | 15 | −2 | 17 |
| MaruMonica | 14 | −2 | 16 |
| MaruMinya | 12 | 0 | 12 |

**行の高さが変わると1画面に入る行数が変わる。**
IPAex 系は Shippori より 7px 低いので、同じ箱に 1〜2 行多く入る。

---

## 試すときに見るところ

| 見るもの | どこで分かるか |
|---|---|
| どのフォントで焼いたか | 起動ログ `I (xxx) TEXTSYS: Font: ...` |
| 1 行に入る字数 | `02_pagesend`（長文のページ送り） |
| 縦書きの句読点・括弧 | `01_momotaro` |
| ルビの見え方 | `03_ruby` |
| 拡大したときの粗さ | `18_fontscale` |

---

## 注意

### ドット系（5 / 6）は設計サイズがある

- **MaruMonica** は「12×16px」。**漢字の送りが 12px** なので、
  16px 想定の縦書きでは横に詰まって見える。
  かな・英数は 16px の枠に合う
- **MaruMinya** は 12px 設計。小さいぶん字数は多く入るが、
  ルビ（本文の約半分）はほぼ読めない

どちらもアンチエイリアスを切って変換してある（`--no-antialias`）。
ドット絵として設計されたフォントに中間調を入れると、輪郭がぼやけるため。

### IPAex 明朝（3）は 16px では線が飛ぶ

**16px では細い横画が消え、`字` の上部や `習` が崩れる。**
ヒンティングの都合で、この書体は小さい寸法に耐えない。
PIL で直接描いても同じになるので、変換ツールの問題ではない。

| サイズ | 見え方 |
|---|---|
| 16 / 18 / 20px | 画が飛ぶ。本文には使えない |
| 24px | 実用になる |

**明朝が欲しいなら 0（Shippori Mincho）を使う。**
16px でもきれいに出る。IPAex 明朝を使いたい場合は 24px で作り直すこと。

```bash
python tools/make_font.py append/font/ipaexm.ttf --size 24     -o tools/font/ipaexm_24.vlw --header tools/font/ipaexm_24.h     --symbol font_ipaexm_24
```

同じ理由で、**細いゴシック（1 M PLUS 2 Light / 4 源ノ角 Light）も
16px ではかなり薄い**。電子ペーパーは階調が 16 段しかないので、
細い線はディザに埋もれやすい。濃さが要るなら 2（IPAex ゴシック）。

### ドット系は設計サイズの整数倍・1.5 倍で作ること

MaruMinya は **12px 設計**。拡大率が割り切れないと、
太い画と細い画が混ざって不揃いになる。

| サイズ | 倍率 | 見え方 |
|---|---|---|
| 12px | 1.0 | くっきり（native） |
| **16px** | 1.33 | **線が痩せて不揃い** |
| **18px** | 1.5 | **整う。おすすめ** |
| 24px | 2.0 | 整う |

MaruMonica は 12×16px 設計なので、16px は縦方向が native。
ただし**漢字の送りが 12px** で、かなの 16px と混ざると詰まって見える。

### `mplus2_16` は収録が少ない

4416 字中 3918 字と、7 本の中でいちばん少ない。
M PLUS 2 は旧字体・異体字の収録が薄い。
`mplus2_16_missing.txt` に一覧がある。

逆に IPAex ゴ / 明は 4415 字とほぼ全部入る。

### 作り直す

```bash
python tools/make_font.py append/font/ipaexg.ttf --size 20 \
    -o tools/font/ipaexg_20.vlw --header tools/font/ipaexg_20.h \
    --symbol font_ipaexg_20
```

サイズを変えたら `main/fonts/active_font.h` に分岐を足す。
使い方は [`tools/README.md`](../README.md#make_fontpy)。
