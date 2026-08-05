#!/usr/bin/env python3
"""画像をファームウェア埋め込み用の C ヘッダに変換する。

使い方:
    python tools/make_icons.py

変換するもの:

| 入力 | 出力 | 用途 |
|---|---|---|
| `tools/icons/*.png` | `main/icons/menu_icons.h` | メニュー下段のボタン（108x80 固定） |
| `append/image/*.png` | `main/icons/images.h` | ロゴなど（寸法は自由） |

## なぜ埋め込むのか

画像を SD カードに置くと、**USB MSC が有効な間は読めなくなる**
（SDCardWrapper は MSC 中に全てのファイル操作を失敗させる）。
その状態ではメニューのボタンが真っ白になり、
「USB を切る」ボタンすら見えなくなって操作不能になる。
電源OFF時のロゴも、SD 未挿入では出せなくなる。
UI の部品はファームウェア側に持たせる。

## 用意する画像

- ボタンは **108 x 80 ピクセル**（108 x 5 = 540 で画面幅ちょうど。拡大縮小はしない）
- ロゴは任意の寸法。画面は 540 x 960（横向きなら 960 x 540）
- 形式は PNG
- **ボタンは白黒2値で作る。** 下記のとおりこの変換器が明るさで2値化するので、
  中間調を入れても潰れる。アンチエイリアスは切っておくとずれが無い
- **ロゴなど透過のあるものは中間調を使ってよい**（下記）

## 透過の扱い

**透過がある画像（ロゴなど）は、白背景へ合成して階調をそのまま残す。**

透明な部分は白（何も描かない）、不透明な部分は元の明るさのまま。
**反転も2値化もしない。** 素材の明暗を推測して補正することはしない。

したがって、**画面に出したい濃さでそのまま描くこと**。
白い画面の上に置くので、白に近い色で描いた部分は薄く出る。

**この経路は2値化しない。** `Panel_EPD` は常に 16 階調（4bpp）で扱い、
`display.setColorDepth()` は引数を無視するので 1bpp にはならない。
`epd_quality` / `epd_text` なら中間調がそのまま出る
（`epd_fast` / `epd_fastest` はベイヤーディザで2階調になる）。

**透過が無い画像は明るさで2値化する。**
メニューのボタンのように、背景まで含めて描かれているものが該当する。
ボタンは下地が描かれないので、背景も画像に含めること。
"""

import io
import os
import re
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))

ICON_SRC_DIR = os.path.join(HERE, "icons")
ICON_OUT_PATH = os.path.join(ROOT, "main", "icons", "menu_icons.h")

IMAGE_SRC_DIR = os.path.join(ROOT, "append", "image")
IMAGE_OUT_PATH = os.path.join(ROOT, "main", "icons", "images.h")

ICON_WIDTH = 108
ICON_HEIGHT = 80

# 2値化のしきい値。これより明るい画素を白にする
THRESHOLD = 128


def to_identifier(name):
    """ファイル名を C の識別子にする"""
    stem = os.path.splitext(os.path.basename(name))[0]
    ident = re.sub(r"[^0-9a-zA-Z_]", "_", stem)
    if ident and ident[0].isdigit():
        ident = "_" + ident
    return ident


def convert(path, expect_size=None):
    """PNG を 1bit 白黒に正規化し、(PNGバイト列, 幅, 高さ) を返す"""
    img = Image.open(path)

    if expect_size and img.size != expect_size:
        print("  警告: %s は %dx%d です（期待は %dx%d）。そのまま埋め込みます"
              % (os.path.basename(path), img.size[0], img.size[1],
                 expect_size[0], expect_size[1]))

    size = img.size

    has_alpha = (img.mode in ("RGBA", "LA")
                 or (img.mode == "P" and "transparency" in img.info))

    if has_alpha:
        # **透過がある画像は、白背景へ合成して階調をそのまま残す。**
        #
        # 透明な部分は白（＝何も描かない）、不透明な部分は元の明るさのまま。
        # 反転も2値化もしない。
        #
        # ## 2値化しない理由
        #
        # Panel_EPD は常に 16 階調（4bpp）で扱う。
        # `display.setColorDepth()` は引数を無視するので 1bpp にはならない。
        # epd_quality / epd_text なら中間調がそのまま出る。
        # ここで潰すと、元画像のグレーが失われる。
        #
        # ## 反転しない理由（一度やって間違えた）
        #
        # 「不透明部分に明るい画素が多い」ことから
        # 「白インクを透過背景に描いた素材」と誤って判断し、
        # 明るさを反転したことがある。
        # 実際には ayamelogo.png は
        #     透明 23,371px / グレー(明るさ179) 8,085px / 黒(明るさ0) 630px
        # の3領域で、多数派のグレーが閾値より明るかっただけだった。
        # 反転すると 179→76、0→255 となり、**黒い部分が白く抜ける**。
        #
        # 素材の明暗を推測して補正しない。合成するだけにする。
        rgba = img.convert("RGBA")
        alpha = rgba.getchannel("A")
        lum = rgba.convert("L")

        # 透明な部分を白（255）に置き換える。
        # 中途半端なアルファは、その割合ぶんだけ白へ寄る。
        out = Image.new("L", img.size, 255)
        out.paste(lum, (0, 0), alpha)
        img = out
    else:
        # 透過が無い画像は明るさで2値化する。
        # 背景まで含めて描かれているもの（メニューのアイコンなど）が該当する。
        #
        # ディザリングは使わない。
        # 1bpp の電子ペーパーでは、ディザの網点が汚く見えるため。
        img = img.convert("L").point(lambda v: 255 if v >= THRESHOLD else 0, mode="1")

    buf = io.BytesIO()
    img.save(buf, format="PNG", optimize=True)
    return buf.getvalue(), size[0], size[1]


def emit_header(src_dir, out_path, prefix, expect_size, extra_lines, title):
    """src_dir の PNG を1つのヘッダにまとめて書き出す"""
    if not os.path.isdir(src_dir):
        print("エラー: %s がありません" % src_dir)
        return False

    sources = sorted(f for f in os.listdir(src_dir) if f.lower().endswith(".png"))
    if not sources:
        print("警告: %s に PNG がありません。スキップします" % src_dir)
        return True

    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    lines = []
    lines.append("// %s" % os.path.relpath(out_path, ROOT).replace("\\", "/"))
    lines.append("//")
    lines.append("// tools/make_icons.py が自動生成したもの。")
    lines.append("// **直接編集しないこと。** 元画像を差し替えてスクリプトを回すこと。")
    lines.append("//   元: %s" % os.path.relpath(src_dir, ROOT).replace("\\", "/"))
    lines.append("//")
    lines.append("// SD ではなくここに埋め込んでいるのは、")
    lines.append("// USB MSC 有効中は全てのファイル操作が失敗するため。")
    lines.append("// SD 由来だと、その間ボタンやロゴが描けなくなる。")
    lines.append("#pragma once")
    lines.append("")
    lines.append("#include <cstddef>")
    lines.append("#include <cstdint>")
    lines.append("")
    lines.extend(extra_lines)

    print("[%s]" % title)
    total = 0
    for name in sources:
        data, w, h = convert(os.path.join(src_dir, name), expect_size)
        ident = to_identifier(name)
        total += len(data)

        print("  %-22s -> %s%-18s %5d bytes  (%dx%d)"
              % (name, prefix, ident, len(data), w, h))

        lines.append("// %s  (%d x %d)" % (name, w, h))
        lines.append("inline constexpr int %s%s_width  = %d;" % (prefix, ident, w))
        lines.append("inline constexpr int %s%s_height = %d;" % (prefix, ident, h))
        lines.append("inline constexpr uint8_t %s%s[] = {" % (prefix, ident))
        for i in range(0, len(data), 16):
            chunk = data[i:i + 16]
            lines.append("    " + " ".join("0x%02X," % b for b in chunk))
        lines.append("};")
        lines.append("inline constexpr size_t %s%s_len = sizeof(%s%s);"
                     % (prefix, ident, prefix, ident))
        lines.append("")

    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))

    print("  -> %s  (%d バイト / %d 個)"
          % (os.path.relpath(out_path, ROOT).replace("\\", "/"), total, len(sources)))
    print()
    return True


def main():
    ok = emit_header(
        ICON_SRC_DIR, ICON_OUT_PATH, "icon_", (ICON_WIDTH, ICON_HEIGHT),
        ["constexpr int MENU_ICON_WIDTH  = %d;" % ICON_WIDTH,
         "constexpr int MENU_ICON_HEIGHT = %d;" % ICON_HEIGHT,
         ""],
        "メニューアイコン")

    ok = emit_header(
        IMAGE_SRC_DIR, IMAGE_OUT_PATH, "image_", None, [],
        "画像（ロゴなど）") and ok

    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
