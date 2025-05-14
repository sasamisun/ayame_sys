#!/usr/bin/env python3
"""
グリフフィルタリングツール：テキストファイルから、指定されたTTFフォントに
含まれていない文字を除去するスクリプト
"""

import argparse
import os
from fontTools.ttLib import TTFont

def get_supported_chars(ttf_path):
    """TTFファイルに含まれる文字のUnicodeコードポイントのセットを返す"""
    try:
        font = TTFont(ttf_path)
        
        # グリフが含まれるTTFのcmapテーブルを取得
        cmap = {}
        for table in font['cmap'].tables:
            if table.isUnicode():
                cmap.update(table.cmap)
        
        # サロゲートペアを考慮
        supported_chars = set()
        for code_point in cmap.keys():
            if code_point < 0x10000:  # BMP（基本多言語面）の文字
                supported_chars.add(chr(code_point))
            else:  # サロゲートペア
                supported_chars.add(chr(code_point))
                
        return supported_chars
    except Exception as e:
        print(f"フォントファイル読み込みエラー: {e}")
        return set()

def filter_text(text, supported_chars):
    """サポートされていない文字を除去したテキストを返す"""
    filtered_text = ""
    removed_chars = set()
    
    for char in text:
        if char in supported_chars or char.isspace():
            filtered_text += char
        else:
            removed_chars.add(char)
    
    return filtered_text, removed_chars

def process_file(input_path, ttf_path, output_path=None):
    """テキストファイルを処理して結果を保存する"""
    # 出力ファイル名が指定されていない場合、元のファイル名に "_filtered" を追加
    if output_path is None:
        base, ext = os.path.splitext(input_path)
        output_path = f"{base}_filtered{ext}"
    
    try:
        # フォントからサポートされている文字を取得
        supported_chars = get_supported_chars(ttf_path)
        if not supported_chars:
            return False, "フォントから文字を抽出できませんでした"
        
        # テキストファイルを読み込む
        with open(input_path, 'r', encoding='utf-8') as f:
            text = f.read()
        
        # テキストをフィルタリング
        filtered_text, removed_chars = filter_text(text, supported_chars)
        
        # 結果を保存
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(filtered_text)
        
        # 結果の統計
        return True, {
            "total_chars": len(text),
            "filtered_chars": len(text) - len(filtered_text),
            "removed_unique_chars": removed_chars,
            "output_file": output_path
        }
    
    except Exception as e:
        return False, f"処理中にエラーが発生しました: {e}"

def main():
    """メイン関数"""
    parser = argparse.ArgumentParser(description='TTFフォントにない文字をテキストファイルから除去します')
    parser.add_argument('text_file', help='処理するテキストファイルのパス')
    parser.add_argument('ttf_file', help='参照するTTFフォントファイルのパス')
    parser.add_argument('-o', '--output', help='出力ファイルのパス（指定しない場合は元のファイル名に_filteredを付加）')
    
    args = parser.parse_args()
    
    success, result = process_file(args.text_file, args.ttf_file, args.output)
    
    if success:
        print(f"処理完了: {result['total_chars']}文字中、{result['filtered_chars']}文字を除去しました")
        if result['removed_unique_chars']:
            print("除去された文字セット:")
            print(''.join(sorted(result['removed_unique_chars'])))
        print(f"結果は {result['output_file']} に保存されました")
    else:
        print(f"エラー: {result}")

if __name__ == "__main__":
    main()