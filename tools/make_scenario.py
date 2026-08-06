#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""make_scenario.py - 青空文庫の XHTML を AYAME のシナリオへ変換する

    python tools/make_scenario.py 464_19941.html -o microsd_sample/scenarios/02_nekonojimusyo

青空文庫の HTML は XHTML 1.1 準拠で、ルビも `<ruby><rb>…</rb><rt>…</rt></ruby>`
として構造で持っている。ここを機械的に読めば、本文を手で JSON へ
流し込む作業がまるごと要らなくなる。

## 何をするか

1. `<div class="main_text">` を取り出す
2. ルビを AYAME の記法（`｜漢字《かんじ》`）へ直す
3. 傍点を圏点のルビに直す（`｜か《﹅》｜ま《﹅》`）
4. **1画面に入る量で区切る**（下記）
5. ページごとに `save` を挟んだ `scenario.json` を書く
6. サムネイルを作る

## 1画面ぶんの計算

**単純な字数では切らない。** 段落は必ず改行から始まるので、
字数で切ると列の余りが捨てられて実際より詰め込みすぎになる。

出力するテキストボックスの寸法とフォントの全角送り（em）から、
1列（1行）の字数と1ページの列数を求め、段落ごとに列を積む。

    列の幅        = em + ルビ帯(em/2) + line_spacing
    1ページの列数 = (本文幅 - 列の幅) / 列の幅+間隔 + 1
    1列の字数     = (本文高 + char_spacing) / (em + char_spacing)

ルビ帯は**ルビの有無にかかわらず全列に確保される**（TypoWrite の仕様）ので、
本文にルビが無くても引いておく。

## 栞

1ページごとに `save` を置く。`run()` は「実行 → 添字を進める」順なので、
`save` が記録する位置は **`save` 自身**を指す。読み込むとその `save` から
再実行され、続けて同じページが出る。`checkpoint` は要らない。

`save` はシステムの栞（`system/settings.json`）を触らない。触るのは
`suspend` と電池切れの自動保存だけ。そのため**タイトル画面**を置き、
「つづきから」で `load` するようにしてある。

## 対応していないもの

- 青空文庫の**テキスト版**（`.txt`）。HTML のほうが構造が明確
- 挿絵。画像を持つ作品は別途 `make_image.py` を通すこと
- 傍線・割注・縦中横。AYAME に表現手段が無い
"""
import argparse
import html as html_mod
import json
import math
import os
import re
import sys

# ---------------------------------------------------------------- 既定値

DEFAULT_SIZE = (540, 960)
DEFAULT_MARGIN = 20
DEFAULT_EM = 18            # 内蔵フォント ipaexg_18 の全角送り
DEFAULT_LINE_SPACING = 10
DEFAULT_CHAR_SPACING = 2   # 縦書きの既定（TextSystem.hpp と同じ）
DEFAULT_PADDING = 16
DEFAULT_SCENE_PAGES = 20

RUBY_SCALE = 0.5           # TypoWrite の既定。ルビ帯の幅に効く

SESAME_CHARS = {
    "﹅": "﹅",        # SESAME DOT。内蔵フォントに 6x6 の実体がある
    "・": "・",        # 中点。4x2 と小さいが確実にどのフォントにもある
}


class ConvertError(Exception):
    pass


# ---------------------------------------------------------------- フォント

def load_codepoints(path):
    """VLW（`.vlw` か生成済みの `.h`）に入っている文字を集める。

    **収録されていない字は豆腐にすらならず、何も描かれない。**
    青空文庫の作品には常用漢字の外がふつうに出てくるので、
    変換の時点で知らせないと実機で初めて気づくことになる。
    """
    if path.endswith(".h"):
        text = open(path, encoding="utf-8", errors="replace").read()
        body = text[text.index("{"):text.rindex("}")]
        data = bytes(int(x, 16)
                     for x in re.findall(r"0[xX]([0-9a-fA-F]{2})", body))
    else:
        with open(path, "rb") as f:
            data = f.read()

    if len(data) < 24:
        raise ConvertError("VLW として読めない: %s" % path)

    count = int.from_bytes(data[0:4], "big")
    cps = set()
    off = 24
    for _ in range(count):
        if off + 28 > len(data):
            break
        cps.add(int.from_bytes(data[off:off + 4], "big"))
        off += 28
    return cps


def resolve_builtin_font(root):
    """`main/fonts/active_font.h` が指しているヘッダのパスを返す"""
    active = os.path.join(root, "main", "fonts", "active_font.h")
    if not os.path.isfile(active):
        return None
    m = re.search(r'#include\s+"([^"]+\.h)"',
                  open(active, encoding="utf-8", errors="replace").read())
    if not m:
        return None
    path = os.path.join(root, "main", "fonts", m.group(1))
    return path if os.path.isfile(path) else None


# ---------------------------------------------------------------- 読み込み

def detect_encoding(raw):
    """`<?xml encoding="…"?>` と `<meta charset=…>` から文字コードを見る"""
    head = raw[:1024].decode("ascii", errors="replace")
    m = re.search(r'encoding=["\']([\w-]+)["\']', head)
    if not m:
        m = re.search(r'charset=([\w-]+)', head)
    if not m:
        return "cp932"      # 青空文庫の既定
    name = m.group(1).lower()
    # Shift_JIS は名乗っていても機種依存文字を含むことがある。
    # cp932 のほうが広く、Shift_JIS で読めるものは全て読める。
    if name in ("shift_jis", "shift-jis", "sjis", "x-sjis"):
        return "cp932"
    return name


def load_html(path, encoding=None):
    with open(path, "rb") as f:
        raw = f.read()
    enc = encoding or detect_encoding(raw)
    try:
        return raw.decode(enc), enc
    except UnicodeDecodeError as e:
        raise ConvertError("%s として読めなかった: %s" % (enc, e))


# ---------------------------------------------------------------- 解析

def take(text, pattern, flags=re.S):
    m = re.search(pattern, text, flags)
    return m.group(1) if m else ""


def strip_tags(fragment):
    """タグを全部落として実体参照を戻す。見出しや奥付の1行に使う"""
    t = re.sub(r"<[^>]+>", "", fragment)
    return html_mod.unescape(t).strip()


def parse_header(doc):
    """題名・副題・著者を取る"""
    return {
        "title": strip_tags(take(doc, r'<h1 class="title">(.*?)</h1>')),
        "subtitle": strip_tags(take(doc, r'<h2 class="subtitle">(.*?)</h2>')),
        "author": strip_tags(take(doc, r'<h2 class="author">(.*?)</h2>')),
    }


def parse_colophon(doc):
    """奥付（底本・入力者・校正者）を行の配列で返す"""
    frag = take(doc, r'<div class="bibliographical_information">(.*?)</div>')
    if not frag:
        return []
    frag = re.sub(r"<hr\s*/?>", "", frag)
    lines = []
    for part in re.split(r"<br\s*/?>", frag):
        line = strip_tags(part)
        if line:
            lines.append(line)
    return lines


# ---------------------------------------------------------------- 本文の変換

RUBY_RE = re.compile(
    r"<ruby>\s*<rb>(.*?)</rb>\s*(?:<rp>.*?</rp>)?\s*<rt>(.*?)</rt>\s*"
    r"(?:<rp>.*?</rp>)?\s*</ruby>", re.S)
# <rb> を省いた書き方（<ruby>親<rt>ルビ</rt></ruby>）にも当てる
RUBY_SHORT_RE = re.compile(
    r"<ruby>((?:(?!<rt>|</ruby>).)*?)(?:<rp>.*?</rp>)?<rt>(.*?)</rt>"
    r"(?:<rp>.*?</rp>)?</ruby>", re.S)
SESAME_RE = re.compile(
    r'<(?:strong|em)[^>]*class="[^"]*(?:SESAME|sesame|side_dot|futoji)[^"]*"'
    r'[^>]*>(.*?)</(?:strong|em)>', re.S)
GAIJI_RE = re.compile(r'<img[^>]*\balt="([^"]*)"[^>]*/?>', re.S)
JISAGE_RE = re.compile(r'<div[^>]*class="jisage_(\d+)"[^>]*>(.*?)</div>', re.S)
HEADING_RE = re.compile(r'<h([34])[^>]*>(.*?)</h\1>', re.S)
NOTES_RE = re.compile(r'<span[^>]*class="notes"[^>]*>.*?</span>', re.S)
CHUKI_RE = re.compile(r"［＃[^］]*］")

# ルビ記法として使う文字。本文に紛れると誤検出のもとになる
RUBY_MARKS = "｜|"


class Stats(object):
    def __init__(self):
        self.ruby = 0
        self.sesame = 0
        self.sesame_dots = 0
        self.gaiji = []
        self.chuki = 0
        self.jisage = 0
        self.unknown_tags = []
        self.bare_marks = 0
        self.braces = 0
        self.missing = []       # フォントに無い文字
        self.missing_vertical = []   # 縦書き用字形が無い文字（元の文字のほう）
        self.font_name = ""


def convert_ruby(fragment, stats):
    def repl(m):
        stats.ruby += 1
        base = html_mod.unescape(strip_tags(m.group(1)))
        ruby = html_mod.unescape(strip_tags(m.group(2)))
        if not base or not ruby:
            return base
        return "｜%s《%s》" % (base, ruby)

    fragment = RUBY_RE.sub(repl, fragment)
    return RUBY_SHORT_RE.sub(repl, fragment)


def convert_sesame(fragment, stats, style):
    """傍点を圏点のルビに直す。

    **1文字ずつ範囲を切る。** ルビは範囲の中央に揃えて描かれるので、
    `｜かま《﹅﹅》` と書くと点2つが2文字ぶんの幅の中央へ寄ってしまう。
    1文字ずつなら各文字の真上（縦書きなら右）に1つずつ乗る。
    """
    def repl(m):
        text = html_mod.unescape(strip_tags(m.group(1)))
        if not text:
            return ""
        stats.sesame += 1
        if style == "none":
            return text
        if style == "bracket":
            return "「%s」" % text
        dot = SESAME_CHARS[style]
        stats.sesame_dots += len(text)
        return "".join("｜%s《%s》" % (c, dot) for c in text)

    return SESAME_RE.sub(repl, fragment)


def convert_gaiji(fragment, stats):
    def repl(m):
        alt = html_mod.unescape(m.group(1))
        stats.gaiji.append(alt)
        # ※［＃…］ の形。注記の部分は落として ※ だけ残す
        base = CHUKI_RE.sub("", alt).strip()
        return base or "※"
    return GAIJI_RE.sub(repl, fragment)


def parse_body(doc, stats, sesame_style):
    """本文を「見出し」と「段落」の並びへ畳む。

    @return [("heading", 文字列) | ("para", 文字列), ...]
    """
    frag = take(doc,
                r'<div class="main_text">(.*?)'
                r'(?:<div class="bibliographical_information">|</body>)')
    if not frag:
        raise ConvertError('<div class="main_text"> が見つからない。'
                           '青空文庫の XHTML か確認すること')

    # 記号系を先に畳む。タグの入れ子を壊さない順で行う。
    frag = NOTES_RE.sub("", frag)
    frag = convert_gaiji(frag, stats)
    frag = convert_ruby(frag, stats)
    frag = convert_sesame(frag, stats, sesame_style)

    # 字下げ。中の各行の頭に全角空白を足す
    def jisage(m):
        stats.jisage += 1
        indent = "　" * int(m.group(1))
        inner = m.group(2)
        parts = re.split(r"(<br\s*/?>)", inner)
        out = []
        for part in parts:
            if re.match(r"<br\s*/?>", part):
                out.append(part)
            elif part.strip():
                out.append(indent + part.lstrip())
            else:
                out.append(part)
        return "".join(out)

    frag = JISAGE_RE.sub(jisage, frag)

    # 見出しは区切りとして残すため、印を付けてから br と同列に扱う
    frag = HEADING_RE.sub(
        lambda m: "<br />\x00H\x00" + strip_tags(m.group(2)) + "<br />", frag)

    # 残りの div / span は入れ物なので外側だけ落とす。
    #
    # **タグ名の直後が英字でないことを確かめる。**
    # `b` を単純に並べると `<br />` に食いつき、段落の区切りが消える。
    frag = re.sub(r"</?(?:div|span|p)(?![a-zA-Z])[^>]*>", "", frag)
    # 強調のうち圏点でないもの（傍線など）は文字だけ残す
    frag = re.sub(r"</?(?:strong|em|b|i|sub|sup)(?![a-zA-Z])[^>]*>", "", frag)
    frag = re.sub(r"<hr\s*/?>", "", frag)

    lines = re.split(r"<br\s*/?>", frag)

    blocks = []
    for line in lines:
        # 畳み残したタグが無いか見る。黙って消すと本文が欠ける
        for tag in re.findall(r"<\s*/?\s*([a-zA-Z0-9]+)", line):
            if tag.lower() not in stats.unknown_tags:
                stats.unknown_tags.append(tag.lower())
        line = re.sub(r"<[^>]+>", "", line)
        line = html_mod.unescape(line)

        before = len(CHUKI_RE.findall(line))
        if before:
            stats.chuki += before
            line = CHUKI_RE.sub("", line)

        # **段落の途中の改行は畳む。**
        # 青空文庫の HTML は読みやすさのために原稿を折り返しているだけで、
        # 実際の改行は `<br />` だけ。そのまま残すと本文に無い改行が入る。
        # 欧文の語間だけは1つ空ける（詰めると単語がくっつく）。
        line = re.sub(r"[ \t]*[\r\n]+[ \t]*",
                      lambda m: " " if _ascii_word_join(line, m) else "",
                      line)

        # 行末の空白を落とす。**行頭の全角空白は残す**
        # （段落の字下げなので、消すと組版が変わる）
        line = line.rstrip("\t ")

        if not line.strip():
            continue

        if line.startswith("\x00H\x00"):
            blocks.append(("heading", line[3:].strip()))
        else:
            blocks.append(("para", line))

    return blocks


def _ascii_word_join(line, m):
    """改行の前後がどちらも欧文の語なら、詰めずに1つ空ける"""
    before = line[m.start() - 1] if m.start() > 0 else ""
    after = line[m.end()] if m.end() < len(line) else ""
    return bool(before) and bool(after) and \
        before.isascii() and before.isalnum() and \
        after.isascii() and after.isalnum()


def escape_for_ayame(text, stats):
    """AYAME が記法として読む文字を逃がす"""
    # `{変数名}` の展開。`{{` `}}` でそのものになる
    if "{" in text or "}" in text:
        stats.braces += text.count("{") + text.count("}")
        text = text.replace("{", "{{").replace("}", "}}")
    return text


def count_bare_marks(text):
    """ルビ記法を取り除いたうえで残る `｜` `|` を数える"""
    stripped = re.sub(r"[｜|][^《<＜\n]+[《<＜][^》>＞\n]*[》>＞]", "", text)
    return sum(stripped.count(c) for c in RUBY_MARKS)


# ---------------------------------------------------------------- 版面

def base_length(text):
    """ルビ記法を除いた、版面を占める文字数"""
    return len(re.sub(r"[｜|]|[《<＜][^》>＞\n]*[》>＞]", "", text))


class Page(object):
    """1画面に入る量を数える"""

    def __init__(self, box, em, vertical):
        pad = box["padding"]
        text_w = box["w"] - pad * 2
        text_h = box["h"] - pad * 2
        ls = box["line_spacing"]
        cs = box.get("char_spacing", DEFAULT_CHAR_SPACING if vertical else 0)

        # ルビ帯はルビの有無によらず全列（全行）に確保される
        strip = int(em * RUBY_SCALE)
        pitch = em + strip            # 列（行）1本ぶんの太さ

        if vertical:
            span, depth = text_w, text_h
        else:
            span, depth = text_h, text_w

        self.lines = max(1, (span - pitch) // (pitch + ls) + 1)
        self.per_line = max(1, (depth + cs) // (em + cs))
        self.vertical = vertical

    @property
    def capacity(self):
        return self.lines * self.per_line

    def rows(self, text):
        """この段落が何列（何行）占めるか"""
        n = base_length(text)
        return max(1, math.ceil(n / self.per_line))


def pack(blocks, page, limit_rows):
    """段落を1画面ぶんずつ束ねる。

    @return [(見出し or None, [段落, ...]), ...]
    """
    pages = []
    current = []
    rows = 0
    heading = None
    pending_heading = None

    def flush():
        nonlocal current, rows, heading
        if current:
            pages.append((heading, current))
            heading = None
        current = []
        rows = 0

    for kind, text in blocks:
        if kind == "heading":
            flush()
            pending_heading = text
            continue

        need = page.rows(text)
        if current and rows + need > limit_rows:
            flush()
        if not current and pending_heading is not None:
            heading = pending_heading
            pending_heading = None
        current.append(text)
        rows += need

    flush()
    if pending_heading is not None and pages:
        # 見出しだけで本文が続かなかった。最後の頁に付けておく
        pages.append((pending_heading, []))
    return pages


# ---------------------------------------------------------------- 組み立て

def box_height(em, rows, line_spacing, padding, font_size=1.0):
    """横書きで `rows` 行が収まる外枠の高さ。

    行の高さは「フォントの高さ＋ルビ帯」。**ルビ帯はルビの有無によらず
    全行に確保される**ので、字の高さだけで見積もると足りなくなる。
    フォントの高さは em より1〜2px 高いことがあるので余裕を持たせる。
    """
    line = int((em + 2 + em * RUBY_SCALE) * font_size)
    return padding * 2 + rows * line + max(0, rows - 1) * line_spacing


def build_boxes(size, margin, vertical, em, with_heading):
    """テキストボックスを組む。

    **見出しの枠は、見出しがある作品のときだけ作る。**
    枠が重なると、あとから描いたほうが下地で塗りつぶして先のものを消す。
    本文を画面いっぱいに取ったうえで見出しの帯も置くことはできない。

    見出しを使わない作品で帯のぶんを空けておくと、
    毎ページ1割ほど字数が減るだけで何の得も無い。
    """
    w, h = size
    head_h = box_height(em, 1, DEFAULT_LINE_SPACING, DEFAULT_PADDING, 2.0)
    body_y = margin + head_h + 8 if with_heading else margin

    box = {
        "x": margin,
        "y": body_y,
        "w": w - margin * 2,
        "h": h - margin - body_y,
        "direction": "VERTICAL" if vertical else "HORIZONTAL",
        "line_spacing": DEFAULT_LINE_SPACING,
        "padding": DEFAULT_PADDING,
        "background_color": "WHITE",
        "text_color": "BLACK",
    }
    # 題名・副題・著者の3行。**入りきる高さを数えてから置く。**
    # 適当な倍数にすると著者の行が切られて出ない。
    title_h = box_height(em, 3, em, DEFAULT_PADDING)
    title = {
        "x": margin,
        "y": max(margin, (h - title_h) // 3),
        "w": w - margin * 2,
        "h": title_h,
        "direction": "HORIZONTAL",
        "align": "CENTER",
        "line_spacing": em,
        "padding": DEFAULT_PADDING,
        "background_color": "WHITE",
        "text_color": "BLACK",
    }
    boxes = {"body": box, "title": title}
    if with_heading:
        boxes["heading"] = {
            "x": margin,
            "y": margin,
            "w": w - margin * 2,
            "h": head_h,
            "direction": "HORIZONTAL",
            "align": "CENTER",
            "font_size": 2.0,
            "line_spacing": DEFAULT_LINE_SPACING,
            "padding": DEFAULT_PADDING,
            "background_color": "WHITE",
            "text_color": "BLACK",
        }
    return boxes


def build_scenario(header, pages, colophon, opts, boxes, stats, page=None):
    scene_ids = []
    scenes = {}

    # --- 本文のシーン ---
    per_scene = opts.scene_pages
    groups = [pages[i:i + per_scene] for i in range(0, len(pages), per_scene)]
    if not groups:
        raise ConvertError("本文が1ページも取れなかった")

    for gi, group in enumerate(groups):
        sid = "p%02d" % (gi + 1)
        scene_ids.append(sid)
        cmds = []
        for heading, paras in group:
            if heading and "heading" in boxes:
                cmds.append({"type": "text", "box": "heading",
                             "body": escape_for_ayame(heading, stats),
                             "wait": False})
            if not paras:
                continue
            if not opts.no_save:
                cmds.append({"type": "save", "slot": 1})
            body = "\n".join(escape_for_ayame(p, stats) for p in paras)
            cmds.append({"type": "text", "box": "body", "body": body})
        scenes[sid] = {"commands": cmds}

    for i, sid in enumerate(scene_ids):
        scenes[sid]["next"] = scene_ids[i + 1] if i + 1 < len(scene_ids) \
            else "colophon"

    # --- タイトル ---
    lines = [header["title"]]
    if header["subtitle"]:
        lines.append(header["subtitle"])
    if header["author"]:
        lines.append(header["author"])
    scenes["title"] = {"commands": [
        {"type": "clear", "color": "BLACK"},
        {"type": "text", "box": "title",
         "body": escape_for_ayame("\n".join(lines), stats), "wait": False},
        {"type": "choice", "prompt": "どちらから読みますか", "options": [
            {"label": "はじめから", "next": scene_ids[0]},
            {"label": "つづきから", "next": "resume"},
        ]},
    ]}

    # --- つづきから ---
    #
    # load は成功すると Jumped を返してその場で飛ぶ。
    # **後ろのコマンドは失敗したときしか動かない。**
    scenes["resume"] = {"commands": [
        {"type": "load", "slot": 1},
        {"type": "text", "box": "body",
         "body": "まだ栞がありません。\n「はじめから」を選んでください。"},
        {"type": "jump", "next": "title"},
    ]}

    # --- 奥付 ---
    #
    # 見出しの枠が無い作品では「おわり」も本文の枠へ入れる。
    # 別の枠に出すと、続けて描く本文が下地でそれを塗りつぶす。
    tail = []
    if "heading" in boxes:
        tail.append({"type": "text", "box": "heading",
                     "body": "おわり", "wait": False})
        colo = list(colophon)
    else:
        colo = ["おわり", "　"] + list(colophon)

    # 奥付も本文と同じように1画面ぶんずつ区切る。
    # まとめて渡すと本体がページ送りしてくれるが、
    # 切れ目を作者（ここではツール）が決めたほうが読みやすい。
    if colo:
        groups = pack([("para", c) for c in colo], page, page.lines) \
            if page else [(None, colo)]
        for _, lines in groups:
            tail.append({"type": "text", "box": "body",
                         "body": escape_for_ayame("\n".join(lines), stats)})
    tail.append({"type": "end", "ending": "read"})
    scenes["colophon"] = {"commands": tail}

    meta = {
        "id": opts.scenario_id,
        "title": header["title"] or opts.scenario_id,
        "author": header["author"] or "",
        "version": "1.0.0",
        "description": header["subtitle"] or ("%s（青空文庫より）"
                                              % (header["author"] or "")),
        "text_direction": boxes["body"]["direction"],
        "update_url": None,
    }
    if opts.rotation is not None:
        meta["rotation"] = opts.rotation
    if opts.font:
        meta["font"] = opts.font
    if not opts.no_back_swipe:
        # 読み物なので既定で入れる。読み違えたときに戻れないと困る
        meta["back_swipe"] = True

    return {
        "format_version": 1,
        "meta": meta,
        "assets": {"backgrounds": {}, "characters": {}},
        "textboxes": boxes,
        "variables": {},
        "start": "title",
        "scenes": scenes,
    }


# ---------------------------------------------------------------- サムネイル

def make_thumbnail(title, out_path, font_path):
    """題名の先頭1〜2文字を 56x56 に描く"""
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError:
        return "Pillow が無いのでサムネイルを飛ばした"

    if not font_path or not os.path.isfile(font_path):
        return "サムネイル用の TTF が見つからない: %s" % font_path

    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import make_image

    text = (title or "?")[:2]
    size = 40 if len(text) == 1 else 26
    font = ImageFont.truetype(font_path, size)

    img = Image.new("L", (56, 56), 255)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, 55, 55], outline=0, width=2)

    if len(text) == 1:
        chars = [(text, (28, 30))]
    else:
        chars = [(text[0], (28, 18)), (text[1], (28, 40))]
    for ch, pos in chars:
        d.text(pos, ch, fill=0, font=font, anchor="mm")

    make_image.quantize(img, 16, False).save(out_path)
    return None


# ---------------------------------------------------------------- 出力

def report(path, enc, header, blocks, pages, page, stats, scenes, opts):
    paras = [t for k, t in blocks if k == "para"]
    chars = sum(base_length(t) for t in paras)
    texts = sum(1 for _, ps in pages if ps)

    print("入力  : %s  (%s)" % (os.path.basename(path), enc))
    print("題名  : %s%s" % (header["title"],
                            " ／ " + header["author"] if header["author"] else ""))
    if header["subtitle"]:
        print("副題  : %s" % header["subtitle"])
    print("本文  : %d 段落 %s 字" % (len(paras), format(chars, ",")))
    print("ルビ  : %d 箇所" % stats.ruby)
    if stats.sesame:
        print("傍点  : %d 箇所 -> 圏点ルビ %d 文字" % (stats.sesame, stats.sesame_dots))
    if stats.jisage:
        print("字下げ: %d 箇所" % stats.jisage)
    unit = "列" if page.vertical else "行"
    print("1画面 : %d %s × %d 字 = %d 字（%s %dx%d / em %d）"
          % (page.lines, unit, page.per_line, page.capacity,
             "縦書き" if page.vertical else "横書き",
             opts.box["w"], opts.box["h"], opts.em))
    if opts.limit_rows != page.lines:
        print("        --chars %d の指定により %d %s で区切る"
              % (opts.chars, opts.limit_rows, unit))
    print("出力  : text %d 個 / シーン %d 個 / %d タップ"
          % (texts, len(scenes), texts))

    warns = []
    if stats.unknown_tags:
        warns.append("畳めなかったタグ: %s" % " ".join(stats.unknown_tags))
    if stats.gaiji:
        warns.append("外字 %d 個を代替文字にした: %s"
                     % (len(stats.gaiji), " ".join(stats.gaiji[:3])))
    if stats.chuki:
        warns.append("注記 ［＃…］ を %d 個落とした" % stats.chuki)
    if stats.braces:
        warns.append("{ } を %d 個逃がした（変数記法と衝突するため）" % stats.braces)
    if stats.bare_marks:
        warns.append("本文に ｜ か | が %d 個ある。"
                     "後ろに 《 》 が続くとルビとして誤読される" % stats.bare_marks)
    if stats.missing:
        warns.append("%s に無い字が %d 種ある: %s"
                     % (stats.font_name, len(stats.missing),
                        "".join(stats.missing)))
        warns.append("    収録されていない字は何も描かれない（豆腐にもならない）。")

    if stats.missing_vertical:
        warns.append("%s に縦書き用字形が無い字が %d 種ある: %s"
                     % (stats.font_name, len(stats.missing_vertical),
                        "".join(stats.missing_vertical)))
        warns.append("    縦書きなのに横書きの向き・位置のまま出る。")

    if stats.missing or stats.missing_vertical:
        rel = os.path.relpath(opts.out_dir).replace("\\", "/")
        warns.append("    この作品用のフォントを作り、--font で指し直すこと"
                     "（縦書き用字形は自動で入る）:")
        # **ゴシックを勧める。** 明朝はこの寸法だと横線が落ちる
        # （tools/README.md の make_font.py の節を参照）。
        warns.append("      python tools/make_font.py append/font/ipaexg.ttf "
                     "--size %d \\" % opts.em)
        warns.append("          --charset %s/scenario.json "
                     "-o %s/fonts/book.vlw" % (rel, rel))
        warns.append("      python tools/make_scenario.py %s \\"
                     % os.path.relpath(opts.input).replace("\\", "/"))
        warns.append("          -o %s --font fonts/book.vlw" % rel)

    if warns:
        print("警告  :")
        for w in warns:
            print(w if w.startswith("    ") else "  - " + w)
    else:
        print("警告  : なし")


def main():
    p = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description="青空文庫の XHTML を AYAME のシナリオへ変換する",
        epilog="""
例:
  そのまま変換（入力と同じフォルダへ書く）
    python tools/make_scenario.py 464_19941.html

  出力先を指定（フォルダ名がシナリオ ID になる）
    python tools/make_scenario.py 464_19941.html \\
        -o microsd_sample/scenarios/02_nekonojimusyo

  横書き・横向きの画面で
    python tools/make_scenario.py in.html --direction HORIZONTAL \\
        --rotation 1 --size 960x540

  独自フォントを使う（em も合わせること）
    python tools/make_scenario.py in.html --font fonts/shippori_16.vlw --em 16

  書かずに集計だけ見る
    python tools/make_scenario.py in.html --dry-run
""")
    p.add_argument("input", help="青空文庫からダウンロードした HTML")
    p.add_argument("-o", "--output", help="出力先フォルダ（既定は入力と同じ場所）")
    p.add_argument("--encoding", help="文字コードを明示する（既定は自動判別）")
    p.add_argument("--direction", choices=("VERTICAL", "HORIZONTAL"),
                   default="VERTICAL", help="本文の向き（既定 VERTICAL）")
    p.add_argument("--rotation", type=int, choices=(0, 1, 2, 3),
                   help="meta.rotation。横向きにするとき")
    p.add_argument("--size", default="540x960",
                   help="画面の大きさ 'WxH'（既定 540x960）")
    p.add_argument("--margin", type=int, default=DEFAULT_MARGIN,
                   help="画面の縁からの余白（既定 20）")
    p.add_argument("--em", type=int, default=DEFAULT_EM,
                   help="フォントの全角送り。1画面の量の計算に使う（既定 18）")
    p.add_argument("--font", help="meta.font。指定したら --em も合わせること")
    p.add_argument("--chars", type=int,
                   help="1画面の字数を直接指定する（自動計算を使わない）")
    p.add_argument("--scene-pages", type=int, default=DEFAULT_SCENE_PAGES,
                   help="見出しが無いときのシーン1つあたりのページ数（既定 20）")
    p.add_argument("--sesame", choices=("﹅", "・", "bracket", "none"),
                   default="﹅", help="傍点の出し方（既定 ﹅）")
    p.add_argument("--no-save", action="store_true",
                   help="ページごとの save を入れない")
    p.add_argument("--no-back-swipe", action="store_true",
                   help="横スワイプで前の画面へ戻る機能を入れない")
    p.add_argument("--no-thumb", action="store_true", help="サムネイルを作らない")
    p.add_argument("--thumb-font", help="サムネイルに使う TTF")
    p.add_argument("--check-font",
                   help="収録字の照合に使う VLW（既定は本体の内蔵フォント）")
    p.add_argument("--no-check-font", action="store_true",
                   help="収録字の照合をしない")
    p.add_argument("--dry-run", action="store_true", help="書かずに集計だけ出す")
    opts = p.parse_args()

    if not os.path.isfile(opts.input):
        sys.exit("入力が見つからない: %s" % opts.input)

    try:
        w, h = (int(v) for v in opts.size.lower().split("x"))
    except Exception:
        sys.exit("--size は 540x960 の形式で指定する: %r" % opts.size)

    out_dir = opts.output or os.path.dirname(os.path.abspath(opts.input))
    opts.scenario_id = os.path.basename(os.path.normpath(out_dir))

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if opts.thumb_font is None:
        # 明朝ではなくゴシック。細い横線は 16 階調に落とすと消える
        opts.thumb_font = os.path.join(root, "append", "font", "ipaexg.ttf")

    try:
        doc, enc = load_html(opts.input, opts.encoding)
        stats = Stats()
        header = parse_header(doc)
        blocks = parse_body(doc, stats, opts.sesame)
        colophon = parse_colophon(doc)
    except ConvertError as e:
        sys.exit("変換できない: %s" % e)

    stats.bare_marks = sum(count_bare_marks(t) for _, t in blocks)

    vertical = opts.direction == "VERTICAL"
    has_heading = any(kind == "heading" for kind, _ in blocks)
    boxes = build_boxes((w, h), opts.margin, vertical, opts.em, has_heading)
    opts.box = boxes["body"]
    page = Page(boxes["body"], opts.em, vertical)

    if opts.chars:
        # 字数を直接指定された。1列の字数はそのままに、列数だけ合わせる
        opts.limit_rows = max(1, opts.chars // page.per_line)
    else:
        opts.limit_rows = page.lines

    pages = pack(blocks, page, opts.limit_rows)
    opts.out_dir = out_dir
    scenario = build_scenario(header, pages, colophon, opts, boxes, stats, page)

    # --- 収録字の照合 ---
    #
    # **無い字は豆腐にすらならず、何も描かれない。**
    # 青空文庫の作品には常用漢字の外がふつうに出てくるので、
    # ここで知らせないと実機で初めて気づくことになる。
    if not opts.no_check_font:
        font_path = opts.check_font
        if font_path is None and opts.font:
            cand = os.path.join(out_dir, opts.font)
            font_path = cand if os.path.isfile(cand) else None
        if font_path is None:
            font_path = resolve_builtin_font(root)
        if font_path:
            try:
                cps = load_codepoints(font_path)
                used = set()
                for scene in scenario["scenes"].values():
                    for cmd in scene["commands"]:
                        used.update(cmd.get("body", ""))
                        for opt in cmd.get("options", []):
                            used.update(opt.get("label", ""))
                stats.missing = sorted(
                    ch for ch in used
                    if ord(ch) not in cps and ch not in "\n\r\t")
                stats.font_name = os.path.basename(font_path)

                # **縦書き用字形も見る。**
                #
                # 句読点と括弧は縦書きだと別のコードポイントで描かれる
                # （、U+3001 -> ︑ U+FE11）。本文には literal で現れないので、
                # 使う文字だけを数えていると素通りしてしまう。
                # 無いと横書きの向き・位置のまま出る。
                if vertical:
                    import make_font
                    stats.missing_vertical = sorted(
                        ch for ch in used
                        if ord(ch) in make_font.VERTICAL_SOURCE
                        and make_font.VERTICAL_SOURCE[ord(ch)] not in cps)
            except (ConvertError, OSError, ValueError) as e:
                print("収録字を照合できなかった: %s" % e, file=sys.stderr)

    report(opts.input, enc, header, blocks, pages, page, stats,
           scenario["scenes"], opts)

    if opts.dry_run:
        print()
        print("--dry-run のため書き出していない")
        return

    os.makedirs(out_dir, exist_ok=True)
    out_json = os.path.join(out_dir, "scenario.json")
    with open(out_json, "w", encoding="utf-8", newline="\n") as f:
        json.dump(scenario, f, ensure_ascii=False, indent=2)
        f.write("\n")

    print()
    print("書き出し: %s  (%.1f KB)"
          % (out_json, os.path.getsize(out_json) / 1024))

    if not opts.no_thumb:
        err = make_thumbnail(header["title"],
                             os.path.join(out_dir, "thumbnail.png"),
                             opts.thumb_font)
        if err:
            print("          サムネイルなし: %s" % err)
        else:
            print("          %s" % os.path.join(out_dir, "thumbnail.png"))


if __name__ == "__main__":
    main()
