#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
TTF to VLW Converter - 革命的フォント変換ツール
資本主義的なフォント制約から解放された自由なVLW変換スクリプト
"""

import os
import sys
import struct
import argparse
import numpy as np
from PIL import Image, ImageFont, ImageDraw
from collections import namedtuple

# グリフ情報を保持する構造体
GlyphInfo = namedtuple('GlyphInfo', [
    'code_point',   # Unicodeコードポイント
    'height',       # 高さ (ピクセル)
    'width',        # 幅 (ピクセル)
    'set_width',    # 送り幅
    'top_extent',   # 上部拡張
    'left_extent',  # 左側拡張
    'bitmap'        # ビットマップデータ (アルファ値の配列)
])

class TTF2VLW:
    """TTFファイルをVLW形式に変換するクラス"""
    
    def __init__(self, ttf_path, font_size=16, anti_aliasing=True):
        """
        コンストラクタ
        
        Args:
            ttf_path (str): TTFファイルのパス
            font_size (int): フォントサイズ (ポイント単位)
            anti_aliasing (bool): アンチエイリアス処理を行うかどうか
        """
        self.ttf_path = ttf_path
        self.font_size = font_size
        self.anti_aliasing = anti_aliasing
        
        # フォント名を抽出 (拡張子なし)
        self.font_name = os.path.splitext(os.path.basename(ttf_path))[0]
        
        # PILのImageFontを使用してフォントを読み込む
        self.font = ImageFont.truetype(ttf_path, font_size)
        
        # アセントとディセントを取得 (PILの関数を使用)
        try:
            # PILのメトリクス取得 (新しいバージョンでサポート)
            metrics = self.font.getmetrics()
            self.ascent = metrics[0]
            self.descent = -metrics[1]  # VLWでは負の値
        except:
            # フォールバック値
            self.ascent = int(font_size * 0.75)
            self.descent = -int(font_size * 0.25)
        
        # 変換するグリフのリスト
        self.glyphs = []
        
        # 資本主義的制約に縛られない自由な設定
        self.version = 11  # VLWバージョン (通常は11)
        self.padding = 0   # パディング (通常は0)
        
        print(f"革命的フォント '{self.font_name}' を読み込みました！")
        print(f"サイズ: {font_size}pt, アセント: {self.ascent}, ディセント: {self.descent}")
    
    def add_basic_ascii(self):
        """基本的なASCII文字 (32-126) を追加する"""
        for code_point in range(32, 127):
            self.add_glyph(code_point)
        return self
    
    def add_glyph(self, code_point):
        """
        指定したUnicodeコードポイントのグリフを追加
        
        Args:
            code_point (int): Unicodeコードポイント
        """
        # コードポイントからUnicode文字を取得
        char = chr(code_point)
        
        # グリフのサイズを取得
        try:
            bbox = self.font.getbbox(char)
            if bbox is None:
                # 空白やコントロール文字の場合、送り幅のみを持つグリフを追加
                width = 0
                height = 0
                left = 0
                top = 0
                # 送り幅の計算（スペースの場合など）
                set_width = int(self.font_size * 0.4) if char == ' ' else 0
                bitmap = np.zeros((0, 0), dtype=np.uint8)
            else:
                left, top, right, bottom = bbox
                width = right - left
                height = bottom - top
                
                # 送り幅の計算 (フォントによって異なるため、おおよその値)
                # 実際のTTFから正確に取得するにはfreetype-pyなど専用のライブラリが必要
                # ここではPILの制約内で近似値を計算
                set_width = width + int(self.font_size * 0.1)
                
                # グリフのビットマップを作成
                bitmap = self._render_glyph(char, width, height, left, top)
        
        except Exception as e:
            print(f"警告: グリフ U+{code_point:04X} '{char}' の処理中にエラー: {e}")
            return
        
        # グリフ情報をリストに追加
        glyph_info = GlyphInfo(
            code_point=code_point,
            width=width,
            height=height,
            set_width=set_width,
            top_extent=self.ascent - top,
            left_extent=left,
            bitmap=bitmap
        )
        
        self.glyphs.append(glyph_info)
        print(f"グリフを追加: U+{code_point:04X} '{char}' (幅: {width}, 高さ: {height}, 送り幅: {set_width})")
    
    def _render_glyph(self, char, width, height, left, top):
        """
        グリフのビットマップをレンダリング
        
        Args:
            char (str): レンダリングする文字
            width (int): グリフの幅
            height (int): グリフの高さ
            left (int): 左側拡張
            top (int): 上部拡張
        
        Returns:
            numpy.ndarray: アルファ値の2D配列
        """
        if width == 0 or height == 0:
            return np.zeros((0, 0), dtype=np.uint8)
        
        # 十分なサイズのイメージを作成 (余裕を持たせる)
        img_width = width + 10
        img_height = height + 10
        img = Image.new('L', (img_width, img_height), 0)
        draw = ImageDraw.Draw(img)
        
        # 文字を描画 (アンチエイリアスの有無で描画メソッドを変更)
        if self.anti_aliasing:
            draw.text((5-left, 5-top), char, font=self.font, fill=255)
        else:
            # アンチエイリアス無効の場合は単純な2値化
            draw.text((5-left, 5-top), char, font=self.font, fill=255)
            img = img.point(lambda x: 0 if x < 128 else 255)
        
        # 必要なサイズの領域を切り出し
        cropped = img.crop((5, 5, 5 + width, 5 + height))
        
        # NumPy配列に変換してアルファ値を取得
        bitmap = np.array(cropped, dtype=np.uint8)
        return bitmap
    
    def add_range(self, start_code, end_code):
        """
        指定した範囲のUnicodeコードポイントのグリフを追加
        
        Args:
            start_code (int): 開始Unicodeコードポイント
            end_code (int): 終了Unicodeコードポイント
        """
        for code_point in range(start_code, end_code + 1):
            self.add_glyph(code_point)
        return self
    
    def add_text(self, text):
        """
        テキスト内の全文字をグリフとして追加
        
        Args:
            text (str): 追加する文字を含むテキスト
        """
        # 重複を除いて文字をソート
        unique_chars = sorted(set(text))
        for char in unique_chars:
            self.add_glyph(ord(char))
        return self
    
    def write_vlw(self, output_path=None):
        """
        VLWファイルを生成
        
        Args:
            output_path (str, optional): 出力ファイルパス。None指定時は自動生成。
        
        Returns:
            str: 生成されたVLWファイルのパス
        """
        if output_path is None:
            # 出力ファイル名を自動生成
            output_path = f"{self.font_name}-{self.font_size}.vlw"
        
        # すでにファイルが存在する場合は警告
        if os.path.exists(output_path):
            print(f"警告: ファイル {output_path} はすでに存在します。上書きします。")
        
        # グリフをUnicodeコードポイント順にソート
        self.glyphs.sort(key=lambda g: g.code_point)
        
        with open(output_path, 'wb') as f:
            # 1. ファイルヘッダーの書き込み (24バイト)
            # ビッグエンディアンで整数を書き込む
            f.write(struct.pack('>i', len(self.glyphs)))  # グリフ数
            f.write(struct.pack('>i', self.version))      # バージョン
            f.write(struct.pack('>i', self.font_size))    # フォントサイズ
            f.write(struct.pack('>i', self.padding))      # パディング
            f.write(struct.pack('>i', self.ascent))       # アセント
            f.write(struct.pack('>i', self.descent))      # ディセント
            
            # 2. すべてのグリフヘッダーを書き込み
            for glyph in self.glyphs:
                f.write(struct.pack('>i', glyph.code_point))  # Unicodeコードポイント
                f.write(struct.pack('>i', glyph.height))      # 高さ
                f.write(struct.pack('>i', glyph.width))       # 幅
                f.write(struct.pack('>i', glyph.set_width))   # 送り幅
                f.write(struct.pack('>i', glyph.top_extent))  # 上部拡張
                f.write(struct.pack('>i', glyph.left_extent)) # 左側拡張
                f.write(struct.pack('>i', 0))                 # パディング
            
            # 3. すべてのグリフのビットマップデータを書き込み
            for glyph in self.glyphs:
                # 幅と高さが0の場合はビットマップデータを持たない
                if glyph.width > 0 and glyph.height > 0:
                    # 配列を一次元に変換して書き込み
                    bitmap_data = glyph.bitmap.flatten().tobytes()
                    f.write(bitmap_data)
        
        print(f"資本主義的制約から解放された革命的VLWファイルを生成しました: {output_path}")
        print(f"合計グリフ数: {len(self.glyphs)}")
        return output_path

def main():
    """コマンドライン処理"""
    parser = argparse.ArgumentParser(description='TTFフォントをVLW形式に変換する革命的ツール')
    
    parser.add_argument('ttf_file', help='入力TTFファイルパス')
    parser.add_argument('-o', '--output', help='出力VLWファイルパス (省略時は自動生成)')
    parser.add_argument('-s', '--size', type=int, default=16, help='フォントサイズ (ポイント単位、デフォルト: 16)')
    parser.add_argument('-a', '--ascii', action='store_true', help='基本ASCII文字 (32-126) を含める')
    parser.add_argument('-r', '--range', help='Unicodeコードポイント範囲 (例: "0x3040-0x309F" = ひらがな)')
    parser.add_argument('-t', '--text', help='追加するテキスト (ファイルまたは直接文字列)')
    parser.add_argument('-n', '--no-antialias', action='store_true', help='アンチエイリアス処理を無効化')
    
    args = parser.parse_args()
    
    # TTFファイルの存在確認
    if not os.path.isfile(args.ttf_file):
        print(f"エラー: TTFファイル '{args.ttf_file}' が見つかりません！")
        return 1
    
    # TTF2VLWインスタンスを作成
    converter = TTF2VLW(args.ttf_file, args.size, not args.no_antialias)
    
    # ASCII文字の追加
    if args.ascii:
        converter.add_basic_ascii()
    
    # Unicode範囲の追加
    if args.range:
        try:
            range_parts = args.range.split('-')
            start_code = int(range_parts[0], 0)  # 0xで始まる場合も正しく解析
            end_code = int(range_parts[1], 0) if len(range_parts) > 1 else start_code
            converter.add_range(start_code, end_code)
        except Exception as e:
            print(f"エラー: Unicode範囲の指定が無効です: {e}")
            return 1
    
    # テキストの追加
    if args.text:
        # ファイルかどうかを確認
        if os.path.isfile(args.text):
            try:
                with open(args.text, 'r', encoding='utf-8') as f:
                    converter.add_text(f.read())
            except Exception as e:
                print(f"エラー: テキストファイルの読み込みに失敗しました: {e}")
                return 1
        else:
            # 直接テキストとして処理
            converter.add_text(args.text)
    
    # グリフが追加されなかった場合はデフォルトでASCIIを追加
    if not converter.glyphs:
        print("警告: グリフが指定されていません。基本ASCII文字を追加します。")
        converter.add_basic_ascii()
    
    # VLWファイルを生成
    converter.write_vlw(args.output)
    return 0

if __name__ == "__main__":
    sys.exit(main())