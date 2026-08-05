#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""make_image.py - AYAME 用に画像を 16 階調へ落とす

M5PaperS3 の電子ペーパーは **常に 16 階調（4bpp）** で表示する。
それより多い階調の画像を置いても、実機側で勝手に丸められるだけで
出来上がりを事前に確認できない。このツールで先に落としておくと、
PC の画面で見たものがそのまま実機に出る。

    python tools/make_image.py 元画像.png -o microsd_sample/scenarios/xx/images/bg/room.png

## 何をするか

1. グレースケールにする（電子ペーパーに色は無い）
2. 指定サイズに収める（既定は 540x960。`--size` で変更）
3. **16 階調に量子化する**（0, 17, 34, ... 255 の 16 段）
4. 8bit グレースケールの PNG として書く

## ディザ

写真やグラデーションは `--dither` を付けると滑らかに見える。
線画・文字・アイコンは付けないこと。輪郭がざらつく。

    python tools/make_image.py photo.jpg --dither          # 写真
    python tools/make_image.py logo.png                    # 線画（既定）

`--levels 2` にすると白黒 2 値になる。UI のアイコンなど、
`epd_fast` / `epd_fastest` で表示するものはこちらが向く
（この2つのモードは実機側でどのみち 2 値へ落とされるため）。

## サイズ

| 用途 | 指定 |
|---|---|
| 背景（縦長） | `--size 540x960`（既定） |
| 背景（横長） | `--size 960x540` |
| サムネイル | `--thumb`（56x56。一覧に出る絵） |
| 立ち絵・一枚絵 | `--size none`（元の大きさのまま） |

`--size` を付けた場合、**縦横比は保ったまま**内側に収める。
余った部分は `--pad` の色で埋める（既定は白）。
`--fit cover` にすると、はみ出す分を切って全面を埋める。

## 透過

透過 PNG は**白の上に重ねてから**落とす。
電子ペーパーに透明は無いので、透過のまま渡すと実機側の合成に委ねることになり、
結果が読めない。`--pad black` を付ければ黒の上に重ねる。
"""
import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow が要る: pip install Pillow")


# 電子ペーパーの階調。Panel_EPD は 4bpp なので 16 段。
EPD_LEVELS = 16

# 既定の画面サイズ（縦長）
DEFAULT_SIZE = (540, 960)


def parse_size(text):
    """'540x960' / 'none' を解釈する"""
    if text is None:
        return DEFAULT_SIZE
    if text.lower() in ("none", "orig", "original"):
        return None
    try:
        w, h = text.lower().split("x")
        return (int(w), int(h))
    except Exception:
        raise argparse.ArgumentTypeError(
            "サイズは 540x960 の形式か none で指定する: %r" % text)


def flatten(img, background):
    """透過を単色の上に重ねて潰す。

    電子ペーパーに透明は無い。透過を残したまま実機へ渡すと
    描画時の合成に結果が左右され、PC で見た絵と一致しなくなる。
    """
    if img.mode in ("RGBA", "LA") or "transparency" in img.info:
        img = img.convert("RGBA")
        base = Image.new("RGBA", img.size, background + (255,))
        img = Image.alpha_composite(base, img)
    return img.convert("L")


def resize(img, size, mode, pad):
    """縦横比を保って size に収める"""
    if size is None:
        return img

    tw, th = size
    sw, sh = img.size
    if (sw, sh) == (tw, th):
        return img

    if mode == "cover":
        # はみ出す分を切って全面を埋める
        scale = max(tw / sw, th / sh)
    else:
        # 内側に収める（余白ができる）
        scale = min(tw / sw, th / sh)

    nw = max(1, round(sw * scale))
    nh = max(1, round(sh * scale))
    # LANCZOS は縮小時にいちばん破綻が少ない
    img = img.resize((nw, nh), Image.LANCZOS)

    canvas = Image.new("L", (tw, th), pad)
    canvas.paste(img, ((tw - nw) // 2, (th - nh) // 2))
    return canvas


def quantize(img, levels, dither):
    """指定段数へ量子化する。

    Pillow の quantize() はパレット画像を作ってしまい、
    そのまま PNG にすると 8bit インデックスカラーになる。
    M5GFX の PNG デコーダはパレットも読めるが、
    **グレースケールのまま置いた方が中身を確かめやすい**ので、
    量子化した値を書き戻して 'L' を保つ。
    """
    if levels < 2:
        raise ValueError("levels は 2 以上")

    # 量子化後に取りうる値（0 と 255 を必ず含む等間隔）
    palette = [round(i * 255 / (levels - 1)) for i in range(levels)]

    if dither:
        # 誤差拡散は Pillow のパレット変換に任せる。
        # 自前で書くより速く、結果も安定している。
        pal_img = Image.new("P", (1, 1))
        pal = []
        for v in palette:
            pal += [v, v, v]
        pal += [0, 0, 0] * (256 - levels)
        pal_img.putpalette(pal)

        conv = img.convert("RGB").quantize(
            palette=pal_img, dither=Image.FLOYDSTEINBERG)
        return conv.convert("L")

    # ディザなし。いちばん近い段へ丸める。
    step = 255.0 / (levels - 1)
    table = [palette[min(levels - 1, int(v / step + 0.5))] for v in range(256)]
    return img.point(table)


def count_levels(img):
    return len(set(img.getdata()))


def main():
    p = argparse.ArgumentParser(
        description="AYAME 用に画像を 16 階調へ落とす",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
例:
  背景（縦長 540x960）
    python tools/make_image.py room.jpg -o images/bg/room.png

  背景（横長。meta.rotation が 1 か 3 のシナリオ用）
    python tools/make_image.py wide.jpg --size 960x540 -o images/bg/wide.png

  写真をディザ付きで
    python tools/make_image.py photo.jpg --dither -o images/bg/photo.png

  立ち絵（大きさはそのまま、黒地に合成）
    python tools/make_image.py chara.png --size none --pad black -o images/chara/a.png

  サムネイル
    python tools/make_image.py key.png --thumb -o thumbnail.png

  UI アイコン（白黒 2 値）
    python tools/make_image.py icon.png --levels 2 --size none -o icon.png
""")
    p.add_argument("input", help="元画像（PNG / JPEG / BMP など）")
    p.add_argument("-o", "--output", help="出力 PNG（省略時は 元名_epd.png）")
    p.add_argument("--size", type=parse_size, default=DEFAULT_SIZE,
                   help="出力サイズ 'WxH'。'none' で元のまま（既定 540x960）")
    p.add_argument("--thumb", action="store_true",
                   help="サムネイル（56x56）として出す")
    p.add_argument("--fit", choices=("contain", "cover"), default="contain",
                   help="contain=全体を収める（余白）/ cover=全面を埋める（切る）")
    p.add_argument("--pad", choices=("white", "black"), default="white",
                   help="余白と透過の下地の色（既定 white）")
    p.add_argument("--levels", type=int, default=EPD_LEVELS,
                   help="階調数（既定 16。2 で白黒）")
    p.add_argument("--dither", action="store_true",
                   help="誤差拡散ディザ。写真向け。線画には使わない")
    p.add_argument("--invert", action="store_true", help="白黒を反転する")
    args = p.parse_args()

    if not os.path.isfile(args.input):
        sys.exit("入力が見つからない: %s" % args.input)

    # --thumb は --size より優先する（一覧の枠が 56x56 固定のため）
    size = (56, 56) if args.thumb else args.size

    out = args.output
    if not out:
        base, _ = os.path.splitext(args.input)
        out = base + "_epd.png"

    pad = 255 if args.pad == "white" else 0

    src = Image.open(args.input)
    print("入力  : %s  %s %s" % (args.input, src.size, src.mode))

    img = flatten(src, (pad, pad, pad))
    before = count_levels(img)

    img = resize(img, size, args.fit, pad)
    if args.invert:
        from PIL import ImageOps
        img = ImageOps.invert(img)

    img = quantize(img, args.levels, args.dither)
    after = count_levels(img)

    outdir = os.path.dirname(os.path.abspath(out))
    if outdir and not os.path.isdir(outdir):
        os.makedirs(outdir, exist_ok=True)
    img.save(out, "PNG", optimize=True)

    print("出力  : %s  %s L" % (out, img.size))
    print("階調  : %d 段 -> %d 段（上限 %d%s）"
          % (before, after, args.levels, "、ディザあり" if args.dither else ""))
    print("サイズ: %.1f KB" % (os.path.getsize(out) / 1024))

    if args.levels > EPD_LEVELS:
        print("注意  : %d 段を指定したが、実機は 16 階調までしか出せない"
              % args.levels)
    if args.dither and args.levels <= 2:
        print("注意  : 2 値でディザを掛けると線画がざらつく")


if __name__ == "__main__":
    main()
