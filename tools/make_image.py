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

## 拡大縮小（`--scale`）

パーセントで指定する。`--size` より**先に**掛かるので、
「半分にしてから 540x960 へ収める」と読める。

    python tools/make_image.py chara.png --scale 50% --size none

**拡大は既定で NEAREST。** ドット絵として作られた素材を滑らかに拡大すると、
輪郭がぼやけて 16 階調では汚く見える。写真なら `--smooth`。
縮小は常に LANCZOS。

## 反転（`--flip`）

**実機には画像を反転させる仕組みが無い。** ここで反転済みの素材を作る。

`drawPngFile()` は拡大率と原点しか取らず、回転も反転も持たない。
スプライトへ展開して `pushRotateZoom()` を通せばできるが、
**透過がアルファ合成から「色キー1色」に落ちる**ため、
16階調しかない画面では縁が汚くなる。
立ち絵のパーツ（目・口）は小さいので、反転済みを1枚足すほうが確実。

## 透過

透過 PNG は**白の上に重ねてから**落とす。
電子ペーパーに透明は無いので、透過のまま渡すと実機側の合成に委ねることになり、
結果が読めない。`--pad black` を付ければ黒の上に重ねる。

**立ち絵だけは例外。** `--keep-alpha` を付けると透過を残す。
背景の上に重ねる素材なので潰してはいけない。
実機の `drawPngFile()` がアルファを見て背景と混ぜてくれる。

    python tools/make_image.py chara.png --keep-alpha --scale 40% --size none

アルファは量子化しない。中間の透明度が縁のなめらかさを作っているため。
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


def parse_scale(text):
    """'50%' / '150' / '1.5' を倍率へ直す"""
    if text is None:
        return None
    t = text.strip().rstrip("%")
    try:
        v = float(t)
    except ValueError:
        raise argparse.ArgumentTypeError("倍率は 50%% のように指定する: %r" % text)
    # % を付けても付けなくても、1 より大きければパーセントとみなす。
    # 「--scale 50」を 50 倍と解釈すると事故になるため。
    ratio = v / 100.0 if v > 3.0 else v
    if ratio <= 0:
        raise argparse.ArgumentTypeError("倍率は 0 より大きいこと: %r" % text)
    return ratio


def rescale(img, ratio, smooth):
    """倍率でリサイズする。

    **拡大は既定で NEAREST。** ドット絵として作られた素材を
    滑らかに拡大すると、輪郭がぼやけて 16 階調では汚く見える。
    写真を拡大したい場合だけ --smooth を付ける。
    """
    if ratio is None or ratio == 1.0:
        return img
    w = max(1, round(img.width * ratio))
    h = max(1, round(img.height * ratio))
    if smooth or ratio < 1.0:
        # 縮小は LANCZOS がいちばん破綻が少ない
        return img.resize((w, h), Image.LANCZOS)
    return img.resize((w, h), Image.NEAREST)


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


def split_alpha(img):
    """透過を残したまま扱うため、明るさとアルファに分ける。

    立ち絵は背景の上に重ねるので、透過を潰してはいけない。
    実機の `drawPngFile()` はアルファを見て背景と混ぜてくれる。
    """
    rgba = img.convert("RGBA")
    return rgba.convert("L"), rgba.getchannel("A")


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

  立ち絵の左右反転（右目用の素材を作る）
    python tools/make_image.py eye.png --flip h --size none -o eye_r.png

  立ち絵を半分の大きさに
    python tools/make_image.py chara.png --scale 50% --size none -o small.png
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
    p.add_argument("--scale", type=parse_scale,
                   help="倍率でリサイズする（例 50%% / 150%%）。--size より先に効く")
    p.add_argument("--smooth", action="store_true",
                   help="拡大を滑らかにする（既定は NEAREST。写真向け）")
    p.add_argument("--flip", choices=("h", "v", "hv"),
                   help="画像を反転する。h=左右 / v=上下 / hv=両方")
    p.add_argument("--keep-alpha", action="store_true",
                   help="透過を残す（立ち絵など、背景に重ねる素材向け）")
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

    # 透過の扱いは2通り。
    #   既定        … 単色の上へ重ねて潰す（背景・一枚絵）
    #   --keep-alpha … 残す（立ち絵。背景に重ねるため）
    alpha = None
    if args.keep_alpha:
        img, alpha = split_alpha(src)
    else:
        img = flatten(src, (pad, pad, pad))
    before = count_levels(img)

    # 倍率は --size より先に掛ける。
    # 「半分にしてから 540x960 へ収める」と読めるようにするため。
    # **アルファにも同じ変形を掛ける。**
    # 片方だけリサイズすると、字面と抜きがずれて縁が汚くなる。
    img = rescale(img, args.scale, args.smooth)
    if alpha is not None:
        alpha = rescale(alpha, args.scale, args.smooth)

    # 反転はリサイズより前。
    # 後にすると余白の入り方まで一緒に反転してしまう。
    if args.flip:
        if "h" in args.flip:
            img = img.transpose(Image.FLIP_LEFT_RIGHT)
            if alpha is not None:
                alpha = alpha.transpose(Image.FLIP_LEFT_RIGHT)
        if "v" in args.flip:
            img = img.transpose(Image.FLIP_TOP_BOTTOM)
            if alpha is not None:
                alpha = alpha.transpose(Image.FLIP_TOP_BOTTOM)

    img = resize(img, size, args.fit, pad)
    if alpha is not None:
        # 余白は透明で埋める（下地の色ではない）
        alpha = resize(alpha, size, args.fit, 0)

    if args.invert:
        from PIL import ImageOps
        img = ImageOps.invert(img)

    img = quantize(img, args.levels, args.dither)
    after = count_levels(img)

    if alpha is not None:
        # アルファは量子化しない。中間の透明度が縁のなめらかさを作っている。
        img = Image.merge("LA", (img, alpha))

    outdir = os.path.dirname(os.path.abspath(out))
    if outdir and not os.path.isdir(outdir):
        os.makedirs(outdir, exist_ok=True)
    img.save(out, "PNG", optimize=True)

    print("出力  : %s  %s %s" % (out, img.size, img.mode))
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
