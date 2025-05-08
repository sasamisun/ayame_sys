#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
txt2griflist.py - テキストファイルからグリフリストを抽出するスクリプト

使用方法:
    python txt2griflist.py input.txt [--comp comp_file.txt]

説明:
    このスクリプトは入力テキストファイルから一意の文字を抽出し、
    コードポイント順にソートして新しいファイルに保存します。
    --comp オプションを使用すると、抽出された文字と比較ファイルの差分を求めることができます。
"""

import sys
import os
import argparse
import unicodedata

def extract_unique_chars(filename):
    """
    テキストファイルから一意の文字を抽出する
    
    Args:
        filename: 入力テキストファイルのパス
        
    Returns:
        unicodeの文字のセット
    """
    try:
        with open(filename, 'r', encoding='utf-8') as f:
            text = f.read()
        
        # テキストから一意の文字を抽出
        char_set = set(text)
        
        return char_set
    except Exception as e:
        print(f"エラー: ファイル '{filename}' の読み込み中にエラーが発生しました: {e}")
        sys.exit(1)

def save_sorted_chars(chars, output_filename):
    """
    コードポイント順にソートされた文字をファイルに保存する
    
    Args:
        chars: 文字のコレクション
        output_filename: 出力ファイル名
    """
    try:
        # コードポイント順にソート
        sorted_chars = sorted(chars)
        
        # ファイルに保存
        with open(output_filename, 'w', encoding='utf-8') as f:
            f.write(''.join(sorted_chars))
        
        print(f"抽出された文字数: {len(sorted_chars)}")
        print(f"結果は {output_filename} に保存されました")
    except Exception as e:
        print(f"エラー: ファイル '{output_filename}' への書き込み中にエラーが発生しました: {e}")
        sys.exit(1)

def find_missing_chars(comp_chars, main_chars, output_filename):
    """
    比較ファイルにあり、メインファイルにない文字を抽出する
    
    Args:
        comp_chars: メインファイルの文字セット
        main_chars: 比較ファイルの文字セット
        output_filename: 出力ファイル名
    """
    # メインにあって比較ファイルにない文字を抽出
    missing_chars = main_chars - comp_chars
    
    if not missing_chars:
        print("比較ファイルにないメイン文字はありません。")
        return
    
    try:
        # コードポイント順にソート
        sorted_missing = sorted(missing_chars)
        
        # 結果をファイルに保存
        with open(output_filename, 'w', encoding='utf-8') as f:
            f.write(''.join(sorted_missing))
        
        print(f"比較ファイルにない文字数: {len(sorted_missing)}")
        print(f"結果は {output_filename} に保存されました")
    except Exception as e:
        print(f"エラー: ファイル '{output_filename}' への書き込み中にエラーが発生しました: {e}")
        sys.exit(1)

def main():
    # コマンドライン引数のパース
    parser = argparse.ArgumentParser(description='テキストファイルからグリフリストを抽出するスクリプト')
    parser.add_argument('input_file', help='入力テキストファイル')
    parser.add_argument('--comp', help='比較用テキストファイル')
    
    args = parser.parse_args()
    
    # 入力ファイルの存在チェック
    if not os.path.isfile(args.input_file):
        print(f"エラー: 入力ファイル '{args.input_file}' が見つかりません")
        sys.exit(1)
    
    # 比較ファイルが指定されていれば、その存在をチェック
    if args.comp and not os.path.isfile(args.comp):
        print(f"エラー: 比較ファイル '{args.comp}' が見つかりません")
        sys.exit(1)
    
    # 入力ファイルから一意の文字を抽出
    input_chars = extract_unique_chars(args.input_file)
    
    # 出力ファイル名を生成
    filename, ext = os.path.splitext(args.input_file)
    output_filename = f"{filename}_griflist{ext}"
    
    # ソートされた文字をファイルに保存
    save_sorted_chars(input_chars, output_filename)
    
    # 比較オプションが指定されていれば実行
    if args.comp:
        comp_chars = extract_unique_chars(args.comp)
        find_missing_chars(input_chars, comp_chars, "lost_griff.txt")
    
if __name__ == "__main__":
    main()