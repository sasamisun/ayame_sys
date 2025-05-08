'''
使用例：python script.py フォントファイル.ttf
'''

import fontTools.ttLib
import sys
import os.path

def extract_characters_from_ttf(font_path):
    # 出力ファイル名を生成（拡張子を除いたTTFファイル名）
    base_name = os.path.splitext(os.path.basename(font_path))[0]
    output_path = f"{base_name}_chars.txt"
    
    # TTFファイルを開く
    try:
        font = fontTools.ttLib.TTFont(font_path)
    except Exception as e:
        print(f"エラー: フォントファイルを開けませんでした: {e}")
        sys.exit(1)
    
    # サポートされている文字のコードポイントを収集
    try:
        cmap = font.getBestCmap()
    except Exception as e:
        print(f"エラー: フォントからcmapを取得できませんでした: {e}")
        sys.exit(1)
    
    # 文字をUnicodeコードポイントから実際の文字に変換
    characters = []
    for code_point in sorted(cmap.keys()):
        try:
            # コードポイントを文字に変換
            char = chr(code_point)
            characters.append(char)
        except (ValueError, OverflowError):
            # 一部の無効なコードポイントをスキップ
            pass
    
    # 結果をファイルに書き込む
    try:
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(''.join(characters))
        print(f"抽出された文字数: {len(characters)}")
        print(f"結果は {output_path} に保存されました")
    except Exception as e:
        print(f"エラー: ファイルへの書き込み中にエラーが発生しました: {e}")
        sys.exit(1)

def main():
    # コマンドライン引数の確認
    if len(sys.argv) != 2:
        print("使用方法: python script.py <font.ttf>")
        sys.exit(1)
    
    font_path = sys.argv[1]
    
    # TTFファイルの存在確認
    if not os.path.exists(font_path):
        print(f"エラー: ファイル '{font_path}' が見つかりません")
        sys.exit(1)
    
    extract_characters_from_ttf(font_path)

if __name__ == "__main__":
    main()