#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
フォントグリフ抽出ツール (Font Glyph Extractor)

【目的】
TTFフォントファイルから、指定された文字（グリフ）のみを抽出して
新しいTTFフォントファイルを作成するツールだにゃ！

【必要なライブラリ】
pip install fonttools

【引数】
1. input_font: 入力するTTFフォントファイルのパス
2. glyph_list: 抽出したい文字が書かれたテキストファイルのパス
3. output_font: 出力する新しいTTFフォントファイルのパス

【グリフリストファイルの形式】
- 1行に1文字、または連続した文字列を記載
- 例:
  あ
  い
  う
  ABCDEFGあいうえお
  
【使用例】
# 基本的な使い方
python font_glyph_extractor.py input.ttf glyphs.txt output.ttf

# ひらがなのみを抽出
python font_glyph_extractor.py NotoSansJP.ttf hiragana_list.txt hiragana_only.ttf

# Webフォント用に必要な文字だけ抽出
python font_glyph_extractor.py myfont.ttf web_chars.txt webfont_subset.ttf

【特徴】
? 重複文字は自動的に除外されるにゃ
? フォントのメタデータ（名前など）も保持されるにゃ
? 元のフォントの品質を保ったまま抽出できるにゃ
"""

import sys
import argparse
from pathlib import Path
from fontTools.ttLib import TTFont
from fontTools.subset import Subsetter, Options


def read_glyph_list(glyph_list_path):
    """
    グリフリストファイルから文字を読み込む関数だにゃ
    
    Args:
        glyph_list_path (str): グリフリストファイルのパス
        
    Returns:
        set: ユニークな文字のセット
    """
    try:
        with open(glyph_list_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # すべての文字を集めて、重複を除去するにゃ
        unique_chars = set(content.replace('\n', '').replace('\r', '').replace(' ', ''))
        
        # 空白文字を除外するにゃ（必要に応じて）
        unique_chars.discard('')
        
        return unique_chars
    
    except FileNotFoundError:
        print(f"? エラー: グリフリストファイル '{glyph_list_path}' が見つからないにゃ！", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"? エラー: グリフリストの読み込み中に問題が発生したにゃ: {e}", file=sys.stderr)
        sys.exit(1)


def extract_glyphs(input_font_path, glyph_list_path, output_font_path):
    """
    TTFフォントから指定されたグリフのみを抽出する関数だにゃ
    
    Args:
        input_font_path (str): 入力フォントファイルのパス
        glyph_list_path (str): グリフリストファイルのパス
        output_font_path (str): 出力フォントファイルのパス
    """
    # グリフリストを読み込むにゃ
    print(f"?? グリフリストを読み込み中... {glyph_list_path}")
    chars_to_extract = read_glyph_list(glyph_list_path)
    
    if not chars_to_extract:
        print("??  警告: グリフリストが空だにゃ！", file=sys.stderr)
        sys.exit(1)
    
    print(f"? {len(chars_to_extract)} 個のユニークな文字を検出したにゃ！")
    
    # フォントファイルを読み込むにゃ
    print(f"?? フォントファイルを読み込み中... {input_font_path}")
    try:
        font = TTFont(input_font_path)
    except FileNotFoundError:
        print(f"? エラー: フォントファイル '{input_font_path}' が見つからないにゃ！", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"? エラー: フォントファイルの読み込みに失敗したにゃ: {e}", file=sys.stderr)
        sys.exit(1)
    
    # サブセット化のオプションを設定するにゃ
    options = Options()
    options.retain_gids = False  # グリフIDを再割り当てしてファイルサイズを最適化するにゃ
    options.notdef_outline = True  # .notdefグリフを保持するにゃ
    options.recalc_bounds = True  # バウンディングボックスを再計算するにゃ
    options.recalc_timestamp = True  # タイムスタンプを更新するにゃ
    options.drop_tables = []  # テーブルは削除しないにゃ
    
    # Subsetterを作成するにゃ
    subsetter = Subsetter(options=options)
    
    # 文字をUnicodeコードポイントに変換するにゃ
    unicodes = [ord(char) for char in chars_to_extract]
    
    # サブセット化を実行するにゃ
    print(f"??  フォントからグリフを抽出中...")
    subsetter.populate(unicodes=unicodes)
    subsetter.subset(font)
    
    # 出力ディレクトリが存在しない場合は作成するにゃ
    output_path = Path(output_font_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    # 新しいフォントファイルを保存するにゃ
    print(f"?? 新しいフォントを保存中... {output_font_path}")
    font.save(output_font_path)
    
    # ファイルサイズを表示するにゃ
    input_size = Path(input_font_path).stat().st_size
    output_size = output_path.stat().st_size
    reduction = (1 - output_size / input_size) * 100
    
    print(f"\n?? 完了したにゃ！")
    print(f"?? 元のサイズ: {input_size:,} bytes")
    print(f"?? 新しいサイズ: {output_size:,} bytes")
    print(f"?? サイズ削減率: {reduction:.1f}%")
    print(f"? {len(chars_to_extract)} 個の文字を含む新しいフォントが作成されたにゃ！")


def main():
    """
    メイン関数 - コマンドライン引数を処理して実行するにゃ
    """
    # コマンドライン引数のパーサーを設定するにゃ
    parser = argparse.ArgumentParser(
        description='TTFフォントから指定されたグリフのみを抽出するツールだにゃ！',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
使用例:
  %(prog)s input.ttf glyphs.txt output.ttf
  %(prog)s NotoSansJP.ttf hiragana.txt hiragana_only.ttf
        """
    )
    
    parser.add_argument(
        'input_font',
        help='入力するTTFフォントファイルのパス'
    )
    
    parser.add_argument(
        'glyph_list',
        help='抽出したい文字が書かれたテキストファイルのパス'
    )
    
    parser.add_argument(
        'output_font',
        help='出力する新しいTTFフォントファイルのパス'
    )
    
    # 引数をパースするにゃ
    args = parser.parse_args()
    
    # グリフ抽出を実行するにゃ
    print("=" * 60)
    print("?? フォントグリフ抽出ツール ??")
    print("=" * 60)
    print()
    
    extract_glyphs(args.input_font, args.glyph_list, args.output_font)
    
    print()
    print("=" * 60)


if __name__ == '__main__':
    main()