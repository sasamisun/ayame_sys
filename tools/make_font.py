#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""make_font.py - TTF/OTF から AYAME 用の VLW フォントを作る

    python tools/make_font.py append/font/ShipporiMincho-Regular.ttf --size 16 \
        --charset joyo --header main/fonts/shippori_16.h --symbol shippori

`.vlw` と、それを配列にした C ヘッダを出す。
ヘッダを `main/fonts/` に置き、`TextSystem.cpp` で `#include` すれば使える。

## append/font/ttf2vlw.py との違い

**送り幅（setWidth）の出し方が違う。** 旧ツールは

    set_width = getbbox(char) の幅 + int(font_size * 0.1)

としていた。`getbbox()` が返すのは字面の外接矩形ではなく
**レイアウト矩形**（原点から送り幅まで）なので、全角では

    16 (レイアウト幅) + 1 (16 * 0.1) = 17px

となり、**16pt と名乗るフォントの全角送りが 17px** になっていた。
半角も `A` が本来 12px のところ 14px。1 文字あたり 1〜2px ずつ広く、
1 行 30 字なら 30〜60px ぶん字数が減る計算になる。

本ツールは `font.getlength(char)` を使う。これが本来の送り幅で、
全角はちょうど指定サイズ（16pt なら 16px）になる。

**ビットマップの切り出しも違う。** 旧ツールはレイアウト矩形のまま格納していた。
読点 `、` は字面が 5x4 なのに 16x4 を持っていて、差分は全部透明。
本ツールは字面だけを切り出し、位置は `leftExtent` / `topExtent` で表す
（これが VLW 本来の持ち方で、M5GFX の描画もこれを前提にしている）。

## 文字集合

| `--charset` | 内容 |
|---|---|
| `joyo` | ASCII + かな + 常用漢字 + 記号（既定。約 3000 字） |
| `all` | TTF に入っている全グリフ。**巨大になる** |
| ファイルパス | そのテキストに出てくる文字だけ |

シナリオ専用の軽いフォントを作るなら、本文を1つのテキストにまとめて渡す。

    python tools/make_font.py font.ttf --charset scenario_text.txt --size 20

## 空白文字

`U+0020`（半角スペース）と `U+3000`（全角スペース）は字面が無いので、
素朴に作ると「フォントに無い文字」として落ちる。実際に落ちていて、
**半角スペースが縦線として描かれる不具合**が出たことがある。
本ツールは字面が空でも**送り幅だけ持つグリフ**として必ず入れる。

## 大きさの目安

VLW のサイズは pt の約 1.56 乗で増える（実測）。

| サイズ | 常用漢字セット |
|---|---|
| 16pt | 約 1.1 MB |
| 20pt | 約 1.5 MB |
| 24pt | 約 2.1 MB |

アプリ領域は 10.5MB、現在の使用は約 2MB。
"""
import argparse
import os
import struct
import sys
import unicodedata

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("Pillow が要る: pip install Pillow")


# ---------------------------------------------------------------- 文字集合

def charset_ascii():
    """印字可能な ASCII（スペースを含む）"""
    return [chr(c) for c in range(0x20, 0x7F)]


def charset_kana():
    """ひらがな・カタカナ・全角記号・全角英数"""
    out = []
    out += [chr(c) for c in range(0x3000, 0x303F + 1)]   # 記号（全角スペース含む）
    out += [chr(c) for c in range(0x3041, 0x309F + 1)]   # ひらがな
    out += [chr(c) for c in range(0x30A0, 0x30FF + 1)]   # カタカナ
    out += [chr(c) for c in range(0xFF01, 0xFF9F + 1)]   # 全角英数・半角カナ
    return out


def font_codepoints(ttf_path):
    """TTF の cmap に載っているコードポイント。取れなければ None

    **収録判定はこれで行う。** 描いた結果を見て判断すると、
    フォントが返す .notdef（□の豆腐）を字形と区別できない。
    旧ツールは画像の類似度で豆腐を判定していたが、
    cmap を引けば曖昧さなく分かる。
    """
    try:
        from fontTools.ttLib import TTFont
    except ImportError:
        print("注意: fontTools が無いので収録判定を省く"
              "（フォントに無い文字が豆腐として入る）")
        print("      pip install fonttools")
        return None

    f = TTFont(ttf_path, fontNumber=0, lazy=True)
    cps = set()
    for table in f["cmap"].tables:
        cps.update(table.cmap.keys())
    f.close()
    # 基本多言語面のみ。VLW のコードポイントは M5GFX 側が 16bit で読む。
    return {c for c in cps if c <= 0xFFFF}


def charset_from_font(ttf_path):
    """TTF に収録されている全コードポイント"""
    cps = font_codepoints(ttf_path)
    if cps is None:
        sys.exit("--charset all には fontTools が要る: pip install fonttools")
    return [chr(c) for c in sorted(cps)]


# 同梱の標準セット。このスクリプトと同じ場所に置いてある。
DEFAULT_CHARSET = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               "charset_ja.txt")


def charset_from_text(path):
    """テキストに出てくる文字を集める。

    行頭 `#` はコメント。改行・タブ・制御文字は落とす。
    """
    out = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#"):
                continue
            for ch in line:
                if ch in ("\n", "\r", "\t"):
                    continue
                if unicodedata.category(ch).startswith("C"):
                    continue        # 制御文字
                out.append(ch)
    return out


# ---------------------------------------------------------------- グリフ

class Glyph:
    __slots__ = ("cp", "w", "h", "advance", "top", "left", "bitmap")

    def __init__(self, cp, w, h, advance, top, left, bitmap):
        self.cp = cp
        self.w = w
        self.h = h
        self.advance = advance
        self.top = top
        self.left = left
        self.bitmap = bitmap


# 字面が無くても送り幅だけ持たせる文字。
# 落とすと TypoWrite が「収録なし」の分岐へ入り、
# 半角スペースが縦線として描かれるなどの不具合になる。
BLANK_CHARS = {0x0020, 0x3000, 0x00A0}


def build_glyph(font, ch, ascent, antialias=True):
    """1文字ぶんのグリフを作る。作れなければ None

    @param antialias False にするとアルファを 0/255 の2値へ落とす。
                     ドット絵として設計されたフォントは、中間調が入ると
                     かえって輪郭がぼやける。
    """
    cp = ord(ch)

    # 送り幅。**これが本来の advance。**
    # getbbox() の幅ではないことに注意（そちらはレイアウト矩形）。
    advance = int(round(font.getlength(ch)))

    # 位置を出すのに2つの座標系を経由する。**混ぜると字が上下にずれる。**
    #
    #   getbbox() … レイアウト矩形。原点は「左端 x アセント線」（anchor='la'）
    #   getmask() … その矩形の大きさのビットマップ。座標は矩形の左上が原点
    #
    # つまり字面の位置は「レイアウト矩形の位置 + 矩形内での字面の位置」。
    # 片方だけで済ませると、、や。のように下に寄る字が上端へ跳ね上がる。
    try:
        box = font.getbbox(ch)
        mask = font.getmask(ch, mode="L")
    except Exception:
        return None

    ink = mask.getbbox() if mask is not None else None

    if ink is None or box is None:
        # 字面が無い。空白なら送り幅だけのグリフとして残す。
        if cp in BLANK_CHARS:
            if advance <= 0:
                return None
            return Glyph(cp, 0, 0, advance, 0, 0, b"")
        return None

    ix0, iy0, ix1, iy1 = ink
    w = ix1 - ix0
    h = iy1 - iy0
    if w <= 0 or h <= 0:
        return None

    # 字面だけを切り出す（余白を持たないのが VLW 本来の形）
    src = Image.frombytes("L", mask.size, bytes(mask))
    cropped = src.crop((ix0, iy0, ix1, iy1))
    if not antialias:
        cropped = cropped.point(lambda v: 255 if v >= 128 else 0)
    bitmap = cropped.tobytes()

    # アセント線から字面の上端までの距離
    from_ascent = box[1] + iy0

    # VLW の topExtent は「ベースラインから字面の上端まで」
    top = ascent - from_ascent
    left = box[0] + ix0

    return Glyph(cp, w, h, advance, top, left, bitmap)


# ---------------------------------------------------------------- 出力

def write_vlw(path, glyphs, size, ascent, descent):
    """VLW を書く。

    グリフは**コードポイント昇順**に並べる。
    `VLWFontParser` は昇順のときだけ二分探索を使い、
    崩れていると線形探索へ落ちて描画が遅くなる。
    """
    glyphs = sorted(glyphs, key=lambda g: g.cp)

    # シナリオ用のフォントは scenarios/<id>/fonts/ に置くのが定石で、
    # そのフォルダはまだ無いことが多い。作ってから書く。
    parent = os.path.dirname(os.path.abspath(path))
    if parent:
        os.makedirs(parent, exist_ok=True)

    with open(path, "wb") as f:
        f.write(struct.pack(">6i", len(glyphs), 11, size, 0, ascent, -abs(descent)))
        for g in glyphs:
            f.write(struct.pack(">7i", g.cp, g.h, g.w, g.advance, g.top, g.left, 0))
        for g in glyphs:
            if g.bitmap:
                f.write(g.bitmap)
    return path


def write_header(path, vlw_path, symbol):
    """VLW を C の配列にする"""
    data = open(vlw_path, "rb").read()
    guard = "_%s_H_" % symbol.upper()

    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("#ifndef %s\n#define %s\n\n" % (guard, guard))
        f.write("#include <stdint.h>\n\n")
        f.write("// tools/make_font.py が生成\n")
        f.write("// 元ファイル: %s\n" % os.path.basename(vlw_path))
        f.write("// データサイズ: %d バイト\n\n" % len(data))
        f.write("static const uint8_t %s[]"
                "  __attribute__((section(\".rodata.font\"))) = {\n" % symbol)
        for i in range(0, len(data), 16):
            row = ", ".join("0x%02x" % b for b in data[i:i + 16])
            f.write("    %s,\n" % row)
        f.write("};\n\n#endif // %s\n" % guard)
    return path


# ---------------------------------------------------------------- 検証

def verify(vlw_path, size):
    """書いたものを読み直して、宣言サイズと実測が合うか確かめる"""
    data = open(vlw_path, "rb").read()
    n, ver, fsize, _pad, asc, desc = struct.unpack(">6i", data[:24])

    glyphs = []
    off = 24
    for _ in range(n):
        cp, h, w, adv, top, left, _p = struct.unpack(">7i", data[off:off + 28])
        glyphs.append((cp, h, w, adv, top, left))
        off += 28

    print()
    print("--- 検証 ---")
    print("  グリフ数     : %d" % n)
    print("  宣言サイズ   : %dpt" % fsize)
    print("  ascent/descent: %d / %d  (行の高さ %d)" % (asc, desc, asc - desc))

    # 濁点・半濁点（U+3099〜309C）は送り 0 が正しいので除く。
    # 前の字に重ねる記号で、独立した幅を持たない。
    def is_wide(cp):
        if 0x3099 <= cp <= 0x309C:
            return False
        return (0x3041 <= cp <= 0x30FF) or (0x4E00 <= cp <= 0x9FFF)

    wide = [g for g in glyphs if is_wide(g[0])]
    if wide and size == 0:
        # `.vlw` からヘッダを作り直しただけの場合。
        # 宣言サイズは引数で渡っていないので、ヘッダの値と突き合わせる。
        size = fsize
    if wide:
        uniq = sorted(set(g[3] for g in wide))
        ok = uniq == [size]
        print("  全角の送り幅 : %s  %s"
              % (uniq, "OK（宣言サイズと一致）" if ok
                 else "*** 宣言サイズ %d と違う ***" % size))
        if not ok:
            for g in wide:
                if g[3] != size:
                    print("      U+%04X %r -> %dpx" % (g[0], chr(g[0]), g[3]))

    # uint8 に収まるか。M5GFX は width / xAdvance を uint8、gdX を int8 で読む。
    bad_w = [g for g in glyphs if not 0 <= g[2] <= 255]
    bad_a = [g for g in glyphs if not 0 <= g[3] <= 255]
    bad_l = [g for g in glyphs if not -128 <= g[5] <= 127]
    for name, bad in (("width", bad_w), ("advance", bad_a), ("leftExtent", bad_l)):
        if bad:
            print("  *** %s が範囲外: %d 件（先頭 U+%04X）***"
                  % (name, len(bad), bad[0][0]))

    order_ok = all(glyphs[i][0] < glyphs[i + 1][0] for i in range(len(glyphs) - 1))
    print("  昇順・重複なし: %s" % ("OK（二分探索が効く）" if order_ok else "*** 崩れている ***"))

    for cp, label in ((0x0020, "半角スペース"), (0x3000, "全角スペース")):
        g = next((x for x in glyphs if x[0] == cp), None)
        print("  %s : %s" % (label,
                             "収録あり（送り %dpx）" % g[3] if g else "*** 欠落 ***"))

    print("  最大グリフ   : %d x %d"
          % (max(g[2] for g in glyphs), max(g[1] for g in glyphs)))
    print("  ファイル     : %.2f MB" % (len(data) / 1024 / 1024))


# ---------------------------------------------------------------- main

def main():
    p = argparse.ArgumentParser(
        description="TTF/OTF から AYAME 用の VLW フォントを作る",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
例:
  常用漢字セットで 16pt（本体組み込み用のヘッダまで作る）
    python tools/make_font.py append/font/ShipporiMincho-Regular.ttf \\
        --size 16 --header main/fonts/shippori_16.h --symbol shippori

  シナリオの本文に出てくる文字だけ
    python tools/make_font.py font.ttf --size 20 --charset story.txt

  フォントの全収録グリフ（巨大になる）
    python tools/make_font.py font.ttf --size 16 --charset all
""")
    p.add_argument("ttf", help="TTF / OTF ファイル、または既存の .vlw")
    p.add_argument("-s", "--size", type=int, default=16,
                   help="フォントサイズ（px。既定 16）")
    p.add_argument("-c", "--charset", default="ja",
                   help="ja / ascii / all / テキストファイルのパス（既定 ja）")
    p.add_argument("-o", "--output", help="出力 VLW（既定 <font>-<size>.vlw）")
    p.add_argument("--header", help="C ヘッダも出す場合の出力先")
    p.add_argument("--symbol", help="ヘッダ内の配列名（既定はヘッダのファイル名）")
    p.add_argument("--no-antialias", action="store_true",
                   help="アンチエイリアスを切る（2値化）")
    args = p.parse_args()

    if not os.path.isfile(args.ttf):
        sys.exit("フォントが見つからない: %s" % args.ttf)

    # 既に .vlw があるならヘッダを作るだけ。
    #
    # 変換は数分かかるので、書体を選び直すたびに再レンダリングしたくない。
    # `tools/font/` に .vlw だけ残しておけば、ここから .h を戻せる。
    if args.ttf.lower().endswith(".vlw"):
        if not args.header:
            sys.exit("`.vlw` を入力にする場合は --header が要る")
        symbol = args.symbol or os.path.splitext(
            os.path.basename(args.header))[0]
        write_header(args.header, args.ttf, symbol)
        print("ヘッダ   : %s （配列名 %s、元 %s）"
              % (args.header, symbol, args.ttf))
        verify(args.ttf, 0)          # size=0 は「宣言サイズとの照合をしない」
        return

    base = os.path.splitext(os.path.basename(args.ttf))[0]
    out = args.output or "%s-%d.vlw" % (base, args.size)

    font = ImageFont.truetype(args.ttf, args.size)
    ascent, descent = font.getmetrics()

    print("フォント : %s" % args.ttf)
    print("サイズ   : %dpx （ascent %d / descent %d → 行の高さ %d）"
          % (args.size, ascent, descent, ascent + descent))

    # --- 文字集合 ---
    if args.charset == "ja":
        if not os.path.isfile(DEFAULT_CHARSET):
            sys.exit("標準の文字集合が見つからない: %s" % DEFAULT_CHARSET)
        chars = charset_from_text(DEFAULT_CHARSET)
    elif args.charset == "ascii":
        chars = charset_ascii()
    elif args.charset == "all":
        chars = charset_from_font(args.ttf)
    elif os.path.isfile(args.charset):
        chars = charset_from_text(args.charset)
    else:
        sys.exit("--charset は ja / ascii / all / 実在するテキストファイル: %s"
                 % args.charset)

    # 空白は必ず入れる（字面が無いので落ちやすい）
    chars = sorted(set(chars) | {chr(c) for c in BLANK_CHARS}, key=ord)
    print("文字集合 : %s （%d 字）" % (args.charset, len(chars)))

    # フォントに無い文字を先に除く。
    # 残すとフォントが返す .notdef（□）がそのまま埋め込まれ、
    # 実機に豆腐が並ぶうえ容量も食う。
    available = font_codepoints(args.ttf)

    # --- グリフ化 ---
    glyphs = []
    missing = []
    for i, ch in enumerate(chars):
        if available is not None and ord(ch) not in available:
            missing.append(ch)
            continue
        g = build_glyph(font, ch, ascent, not args.no_antialias)
        if g is None:
            missing.append(ch)
        else:
            glyphs.append(g)
        if (i + 1) % 500 == 0:
            print("  %d / %d ..." % (i + 1, len(chars)))

    if not glyphs:
        sys.exit("グリフが1つも作れなかった")

    print("収録     : %d 字（このフォントに無い文字 %d 字）"
          % (len(glyphs), len(missing)))
    if missing:
        rep = "".join(missing[:20])
        print("  例: %s%s" % (rep, " ..." if len(missing) > 20 else ""))
        mpath = os.path.splitext(out)[0] + "_missing.txt"
        with open(mpath, "w", encoding="utf-8") as f:
            for ch in missing:
                f.write("U+%04X %s\n" % (ord(ch), ch))
        print("  一覧: %s" % mpath)

    write_vlw(out, glyphs, args.size, ascent, descent)
    print("出力     : %s" % out)

    if args.header:
        symbol = args.symbol or os.path.splitext(os.path.basename(args.header))[0]
        write_header(args.header, out, symbol)
        print("ヘッダ   : %s （配列名 %s）" % (args.header, symbol))

    verify(out, args.size)


if __name__ == "__main__":
    main()
