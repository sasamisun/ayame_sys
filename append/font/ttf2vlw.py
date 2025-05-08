#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ttf2vlw.py - TTFフォントをProcessing VLW形式に変換するスクリプト

import argparse
import os
import struct
import sys
from pathlib import Path
from typing import List, Dict, Tuple

import numpy as np
from fontTools import ttLib
from PIL import Image, ImageDraw, ImageFont

def main():
    parser = argparse.ArgumentParser(description='Convert TTF fonts to Processing VLW format')
    parser.add_argument('--input', '-i', required=True, help='Input TTF font file')
    parser.add_argument('--output', '-o', required=True, help='Output VLW font file')
    parser.add_argument('--size', '-s', type=int, default=12, help='Font size in points (default: 12)')
    parser.add_argument('--antialias', '-a', action='store_true', help='Enable antialiasing (smoothing)')
    parser.add_argument('--chars', '-c', default='basic', 
                      help='Character set to include: "basic" (ASCII 32-126), "extended" (ASCII 32-255), or "all"')
    
    args = parser.parse_args()
    
    # 入力ファイルの存在確認
    if not os.path.exists(args.input):
        print(f"エラー: 入力ファイル '{args.input}' が見つかりません。", file=sys.stderr)
        return 1
    
    # 文字セットの決定
    charset = get_character_set(args.chars)
    
    try:
        # TTFからVLWへの変換
        convert_ttf_to_vlw(args.input, args.output, args.size, charset, args.antialias)
        print(f"変換成功: '{args.input}' → '{args.output}'")
        print(f"フォントサイズ: {args.size}pt, 文字数: {len(charset)}, アンチエイリアス: {'有効' if args.antialias else '無効'}")
        
    except Exception as e:
        print(f"エラー: 変換に失敗しました: {str(e)}", file=sys.stderr)
        return 1
    
    return 0

def get_character_set(charset_name: str) -> List[int]:
    """文字セット名に基づいて含める文字コードのリストを返す"""
    if charset_name == 'basic':
        # ASCII基本文字 (スペース ~ チルダ)
        return list(range(32, 127))
    elif charset_name == 'extended':
        # 拡張ASCII文字
        return list(range(32, 256))
    elif charset_name == 'all':
        # 注意: これは全Unicodeを意味せず、一般的な文字を含む範囲
        # ラテン文字、ひらがな、カタカナ、一部の漢字など
        ranges = [
            (32, 127),     # 基本ASCII
            (160, 256),    # 拡張ラテン文字
            (0x3040, 0x309F),  # ひらがな
            (0x30A0, 0x30FF),  # カタカナ
            (0x4E00, 0x9FFF)   # 基本漢字 (CJK統合漢字)
        ]
        chars = []
        for start, end in ranges:
            chars.extend(range(start, end))
        return chars
    else:
        # デフォルトはASCII基本文字
        return list(range(32, 127))

def convert_ttf_to_vlw(ttf_path: str, vlw_path: str, font_size: int, 
                      char_codes: List[int], antialias: bool = True) -> None:
    """TTFフォントをVLW形式に変換する"""
    # TTFファイルを開く
    ttf = ttLib.TTFont(ttf_path)
    
    # フォント名を取得
    font_name = get_font_name(ttf)
    
    # PILのImageFont作成
    pil_font = ImageFont.truetype(ttf_path, font_size)
    
    # フォントのメトリクス取得
    ascent, descent = get_font_metrics(ttf, font_size)
    
    # 各文字のビットマップとメトリクスを生成
    glyph_data = {}
    valid_chars = []
    
    for char_code in char_codes:
        try:
            char = chr(char_code)
            # 文字がフォント内に存在するか確認
            if not has_glyph(ttf, char_code):
                # 文字がない場合はスキップ
                continue
                
            bitmap, width, height, advance, bearing_x, bearing_y = render_glyph(
                pil_font, char, antialias)
            
            if bitmap is not None:
                glyph_data[char_code] = {
                    'bitmap': bitmap,
                    'width': width,
                    'height': height,
                    'advance': advance,
                    'bearing_x': bearing_x,
                    'bearing_y': bearing_y
                }
                valid_chars.append(char_code)
        except Exception as e:
            # 問題のある文字はスキップ
            print(f"警告: 文字 U+{char_code:04X} ({chr(char_code) if char_code < 0x10000 else '?'}) をスキップしました: {e}")
    
    # VLWファイルを作成
    write_vlw_file(vlw_path, glyph_data, valid_chars, font_size, ascent, descent, font_name, antialias)

def get_font_name(ttf: ttLib.TTFont) -> str:
    """TTFからフォント名を取得"""
    name_record = None
    
    # 'name'テーブルからフォント名を取得
    if 'name' in ttf:
        for record in ttf['name'].names:
            if record.nameID == 4:  # Full font name
                try:
                    if record.isUnicode():
                        name_record = record.toUnicode()
                    else:
                        name_record = record.string.decode('latin-1')
                    break
                except:
                    pass
    
    if name_record:
        return name_record
    else:
        # フォント名が見つからない場合はファイル名を使用
        return os.path.basename(ttf.reader.file.name)

def get_font_metrics(ttf: ttLib.TTFont, font_size: int) -> Tuple[float, float]:
    """フォントのアセントとディセントを取得"""
    ascent = 0
    descent = 0
    
    # 'hhea'テーブルからメトリクスを取得
    if 'hhea' in ttf:
        units_per_em = ttf['head'].unitsPerEm
        ascent = ttf['hhea'].ascent * font_size / units_per_em
        descent = -ttf['hhea'].descent * font_size / units_per_em  # デセントは通常負の値
    
    return ascent, descent

def has_glyph(ttf: ttLib.TTFont, char_code: int) -> bool:
    """フォントに指定された文字のグリフが含まれているか確認"""
    cmap = ttf.getBestCmap()
    return char_code in cmap

def render_glyph(font: ImageFont.FreeTypeFont, char: str, antialias: bool) -> Tuple:
    """文字をレンダリングしてビットマップとメトリクスを返す"""
    # 文字の大きさを取得
    try:
        left, top, right, bottom = font.getbbox(char)
        width = right - left
        height = bottom - top
    except:
        # PILのフォントメトリクス互換性のための代替手段
        width, height = font.getsize(char)
        left, top = 0, 0
    
    # サイズが0の場合（スペースなど）
    if width == 0 or height == 0:
        width = max(width, 1)
        height = max(height, 1)
    
    # 余白を追加
    pad = 2
    width += pad * 2
    height += pad * 2
    
    # ビットマップ作成
    img = Image.new('L', (width, height), 0)
    draw = ImageDraw.Draw(img)
    
    # 文字描画
    draw.text((pad - left, pad - top), char, font=font, fill=255)
    
    # アンチエイリアスが無効な場合は2値化
    if not antialias:
        img = img.point(lambda x: 0 if x < 128 else 255, '1')
    
    # ビットマップデータ取得
    bitmap = np.array(img)
    
    # フォントメトリクス取得
    try:
        metrics = font.getmetrics()
        ascent, descent = metrics
    except:
        ascent, descent = height, 0
    
    # アドバンス（次の文字までの幅）
    try:
        advance = font.getlength(char)
    except:
        # 代替手段
        advance = width - pad * 2
    
    # ベアリング（文字の位置調整）
    bearing_x = left
    bearing_y = ascent - top
    
    return bitmap, width, height, advance, bearing_x, bearing_y

def write_vlw_file(vlw_path: str, glyph_data: Dict, char_codes: List[int], 
                 font_size: float, ascent: float, descent: float, 
                 font_name: str, antialias: bool) -> None:
    """VLWファイルを作成する"""
    with open(vlw_path, 'wb') as f:
        # ヘッダーの書き込み
        glyph_count = len(char_codes)
        version = 11  # VLWのバージョン
        reserved = 0  # 予約値
        
        # ヘッダー書き込み
        f.write(struct.pack('>H', glyph_count))  # グリフ数 (2バイト、big endian)
        f.write(struct.pack('>B', version))      # バージョン (1バイト)
        f.write(struct.pack('>f', font_size))    # フォントサイズ (4バイト、float)
        f.write(struct.pack('>B', reserved))     # 予約値 (1バイト)
        f.write(struct.pack('>f', ascent))       # アセント (4バイト、float)
        f.write(struct.pack('>f', descent))      # ディセント (4バイト、float)
        
        # 各グリフのヘッダー書き込み
        glyph_offsets = []
        current_offset = f.tell() + (8 * glyph_count)  # グリフヘッダーの後の位置
        
        for char_code in char_codes:
            glyph = glyph_data[char_code]
            width = glyph['width']
            height = glyph['height']
            
            # グリフヘッダー
            f.write(struct.pack('>I', char_code))     # 文字コード (4バイト)
            f.write(struct.pack('>I', current_offset))  # ビットマップデータへのオフセット (4バイト)
            
            bitmap_size = width * height
            current_offset += bitmap_size + 20  # ビットマップ + メタデータ
        
        # 各グリフのビットマップデータ書き込み
        for char_code in char_codes:
            glyph = glyph_data[char_code]
            bitmap = glyph['bitmap']
            width = glyph['width']
            height = glyph['height']
            advance = glyph['advance']
            bearing_x = glyph['bearing_x']
            bearing_y = glyph['bearing_y']
            
            # グリフメタデータ
            f.write(struct.pack('>H', width))        # 幅 (2バイト)
            f.write(struct.pack('>H', height))       # 高さ (2バイト)
            f.write(struct.pack('>f', advance))      # アドバンス (4バイト、float)
            f.write(struct.pack('>f', bearing_x))    # X方向ベアリング (4バイト、float)
            f.write(struct.pack('>f', bearing_y))    # Y方向ベアリング (4バイト、float)
            
            # ビットマップデータ
            for y in range(height):
                for x in range(width):
                    f.write(struct.pack('>B', bitmap[y, x]))  # ピクセル値 (1バイト)
        
        # フォント名
        font_name_bytes = font_name.encode('utf-8')
        f.write(struct.pack('>H', len(font_name_bytes)))  # 文字列長 (2バイト)
        f.write(font_name_bytes)  # フォント名
        
        # スムージングフラグ
        f.write(struct.pack('>?', antialias))  # ブール値 (1バイト)

if __name__ == "__main__":
    sys.exit(main())