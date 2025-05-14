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

# 追加の画像処理ライブラリ（オプション）
try:
    import cv2
    HAS_CV2 = True
except ImportError:
    HAS_CV2 = False
    print("警告: OpenCV (cv2) が見つかりません。基本的な画像比較のみ使用します。")

try:
    import imagehash
    HAS_IMAGEHASH = True
except ImportError:
    HAS_IMAGEHASH = False
    print("警告: imagehash が見つかりません。基本的な画像比較のみ使用します。")

try:
    from skimage.metrics import structural_similarity as ssim
    HAS_SSIM = True
except ImportError:
    HAS_SSIM = False
    print("警告: scikit-image が見つかりません。基本的な画像比較のみ使用します。")

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
    
    def __init__(self, ttf_path, font_size=16, anti_aliasing=True, fallback_fonts=None, tofu_reference=None, tofu_threshold=0.8, use_advanced_comparison=True):
        """
        コンストラクタ
        
        Args:
            ttf_path (str): メインTTFファイルのパス
            font_size (int): フォントサイズ (ポイント単位)
            anti_aliasing (bool): アンチエイリアス処理を行うかどうか
            fallback_fonts (list): 代替フォントパスのリスト
            tofu_reference (int): トーフ文字参照用のUnicodeコードポイント (None=使用しない)
            tofu_threshold (float): トーフ判定の閾値 (0.0-1.0、高いほど厳格)
            use_advanced_comparison (bool): 高度な画像比較手法を使用するか
        """
        self.ttf_path = ttf_path
        self.font_size = font_size
        self.anti_aliasing = anti_aliasing
        self.tofu_threshold = tofu_threshold
        self.use_advanced_comparison = use_advanced_comparison
        
        # フォント名を抽出 (拡張子なし)
        self.font_name = os.path.splitext(os.path.basename(ttf_path))[0]
        
        # PILのImageFontを使用してメインフォントを読み込む
        self.main_font = ImageFont.truetype(ttf_path, font_size)
        
        # 代替フォントのリストを初期化
        self.fallback_fonts = []
        
        # 代替フォントがある場合は読み込む
        if fallback_fonts:
            for fb_path in fallback_fonts:
                if os.path.isfile(fb_path):
                    try:
                        fb_font = ImageFont.truetype(fb_path, font_size)
                        fb_name = os.path.splitext(os.path.basename(fb_path))[0]
                        self.fallback_fonts.append((fb_path, fb_font, fb_name))
                        print(f"代替フォント '{fb_name}' を読み込みました！")
                    except Exception as e:
                        print(f"警告: 代替フォント '{fb_path}' の読み込みに失敗しました: {e}")
        
        # アセントとディセントを取得 (メインフォントから)
        try:
            # PILのメトリクス取得 (新しいバージョンでサポート)
            metrics = self.main_font.getmetrics()
            self.ascent = metrics[0]
            self.descent = -metrics[1]  # VLWでは負の値
        except:
            # フォールバック値
            self.ascent = int(font_size * 0.75)
            self.descent = -int(font_size * 0.25)
        
        # 変換するグリフのリスト
        self.glyphs = []
        
        # 見つからなかったグリフのリスト
        self.missing_glyphs = []
        
        # 代替フォントから取得したグリフのカウント
        self.fallback_glyph_count = 0
        
        # 資本主義的制約に縛られない自由な設定
        self.version = 11  # VLWバージョン (通常は11)
        self.padding = 0   # パディング (通常は0)
        
        # トーフ文字の参照サンプル
        self.tofu_reference_bitmap = None
        self.tofu_reference_image = None
        if tofu_reference is not None:
            try:
                self.tofu_reference_char = chr(tofu_reference)
                print(f"トーフ参照文字 U+{tofu_reference:04X} '{self.tofu_reference_char}' を使用します。")
                
                # トーフ参照文字のビットマップを取得
                bbox = self.main_font.getbbox(self.tofu_reference_char)
                if bbox is not None:
                    left, top, right, bottom = bbox
                    width = right - left
                    height = bottom - top
                    
                    if width > 0 and height > 0:
                        self.tofu_reference_bitmap = self._render_glyph(
                            self.tofu_reference_char, width, height, left, top, self.main_font
                        )
                        # PIL画像形式にも変換（高度な比較用）
                        self.tofu_reference_image = Image.fromarray(self.tofu_reference_bitmap)
                        print(f"トーフ参照ビットマップを取得しました: サイズ {width}x{height}")
            except Exception as e:
                print(f"警告: トーフ参照文字の設定中にエラー: {e}")
                self.tofu_reference_bitmap = None
        
        print(f"革命的メインフォント '{self.font_name}' を読み込みました！")
        print(f"サイズ: {font_size}pt, アセント: {self.ascent}, ディセント: {self.descent}")
        print(f"代替フォント数: {len(self.fallback_fonts)}")
    
    def add_basic_ascii(self):
        """基本的なASCII文字 (32-126) を追加する"""
        for code_point in range(32, 127):
            self.add_glyph(code_point)
        return self
    
    def add_glyph(self, code_point):
        """
        指定したUnicodeコードポイントのグリフを追加
        メインフォントで見つからない場合は代替フォントから検索
        
        Args:
            code_point (int): Unicodeコードポイント
        """
        # コードポイントからUnicode文字を取得
        char = chr(code_point)
        
        # すでに追加済みのグリフかどうかを確認
        if any(g.code_point == code_point for g in self.glyphs):
            print(f"情報: グリフ U+{code_point:04X} '{char}' はすでに追加されています。スキップします。")
            return
        
        print(f"グリフを探索中: U+{code_point:04X} '{char}'")
        
        # まずメインフォントでグリフを探す
        glyph_info = self._find_glyph_in_font(code_point, self.main_font, self.font_name, True)
        
        # メインフォントで見つからなかった場合、代替フォントを順に試す
        if glyph_info is None and self.fallback_fonts:
            for fb_path, fb_font, fb_name in self.fallback_fonts:
                print(f"代替フォント '{fb_name}' でグリフ U+{code_point:04X} '{char}' を探索...")
                glyph_info = self._find_glyph_in_font(code_point, fb_font, fb_name, False)
                if glyph_info is not None:
                    self.fallback_glyph_count += 1
                    print(f"グリフを代替フォント '{fb_name}' から追加: U+{code_point:04X} '{char}'")
                    break
        
        # どのフォントでも見つからなかった場合
        if glyph_info is None:
            self.missing_glyphs.append((code_point, char))
            print(f"警告: グリフ U+{code_point:04X} '{char}' はどのフォントにも見つかりませんでした。")
            return
        
        # グリフ情報をリストに追加
        self.glyphs.append(glyph_info)
    
    def _find_glyph_in_font(self, code_point, font, font_name, tofu_chk):
        """
        指定したフォントから指定したコードポイントのグリフを探す
        
        Args:
            code_point (int): Unicodeコードポイント
            font: PILのImageFontオブジェクト
            font_name (str): フォント名（ログ用）
        
        Returns:
            GlyphInfo: 見つかった場合はグリフ情報、見つからない場合はNone
        """
        char = chr(code_point)
        
        # グリフのサイズを取得
        try:
            bbox = font.getbbox(char)
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
                
                # フォントによっては文字が正しくレンダリングされても
                # 幅や高さが0になることがあるのでチェック
                if width <= 0 or height <= 0:
                    print(f"警告: グリフ U+{code_point:04X} '{char}' のサイズが無効です。幅={width}, 高さ={height}")
                    return None
                
                # 送り幅の計算
                set_width = width + int(self.font_size * 0.1)
                
                # グリフのビットマップを作成
                bitmap = self._render_glyph(char, width, height, left, top, font)
                
                # ビットマップの内容が全て0（透明）の場合は
                # そのフォントに文字がないと判断
                if bitmap.size > 0 and np.max(bitmap) == 0:
                    print(f"警告: グリフ U+{code_point:04X} '{char}' は空のビットマップです。フォント '{font_name}' にこの文字はありません。")
                    return None
                
                # トーフ文字の検出
                # トーフ参照文字が設定されている場合、それとの比較を優先
                if self.tofu_reference_bitmap is not None and code_point != ord(self.tofu_reference_char) and tofu_chk:
                    # トーフ参照は設定されていて、かつ現在の文字はトーフ参照文字自身ではない
                    
                    # PIL画像への変換
                    bitmap_image = Image.fromarray(bitmap)
                    
                    # 高度な比較手法を使用する場合
                    if self.use_advanced_comparison:
                        similarity = self.compare_characters(bitmap_image, self.tofu_reference_image)
                        
                        # デバッグ情報
                        print(f"高度なトーフ比較 (U+{code_point:04X}): 類似度 = {similarity:.3f}, 閾値 = {self.tofu_threshold:.3f}")
                        
                        if similarity >= self.tofu_threshold:
                            print(f"警告: グリフ U+{code_point:04X} '{char}' はトーフ参照文字との類似度が高い ({similarity:.3f}) ためトーフと判定されました。")
                            return None
                
                # 従来のパターン検出（参照文字がない場合のフォールバック）
                elif self.tofu_reference_bitmap is None:
                    if self._is_tofu_glyph_v2(bitmap, width, height, code_point, font_name):
                        print(f"警告: グリフ U+{code_point:04X} '{char}' はパターン分析でトーフ文字として検出されました。フォント '{font_name}' にこの文字はありません。")
                        return None
        
        except Exception as e:
            print(f"警告: フォント '{font_name}' でグリフ U+{code_point:04X} '{char}' の処理中にエラー: {e}")
            return None
        
        # グリフ情報を作成
        glyph_info = GlyphInfo(
            code_point=code_point,
            width=width,
            height=height,
            set_width=set_width,
            top_extent=self.ascent - top,
            left_extent=left,
            bitmap=bitmap
        )
        
        print(f"グリフを追加: U+{code_point:04X} '{char}' (幅: {width}, 高さ: {height}, 送り幅: {set_width}) - '{font_name}' から")
        return glyph_info
    
    def _is_tofu_glyph(self, bitmap, width, height, code_point, font_name):
        """
        ビットマップが「トーフ文字」（フォントに文字がない場合の四角形）かどうかを判定
        
        Args:
            bitmap: ビットマップデータ
            width: ビットマップの幅
            height: ビットマップの高さ
            code_point: Unicodeコードポイント（ログ用）
            font_name: フォント名（ログ用）
            
        Returns:
            bool: トーフ文字と判断された場合はTrue、そうでない場合はFalse
        """
        # 最小サイズ未満は除外（トーフとしてはあまりに小さすぎる）
        if width < 5 or height < 5:
            return False
        
        # 文字に対して明らかに大きすぎるか小さすぎる場合は
        # 異常なグリフとして判断
        expected_size = self.font_size * 0.8  # フォントサイズからの想定サイズ
        if width > expected_size * 2 or height > expected_size * 2:
            print(f"異常なグリフサイズ: U+{code_point:04X} (幅: {width}, 高さ: {height})")
            return True
        
        # トーフ文字の特徴：
        # 1. 外周の輪郭がはっきりしている（高いアルファ値）
        # 2. 中央部が比較的空白（低いアルファ値）、または白黒の模様
        # 3. 矩形に近い形状
        
        # 縦横比が極端に異なる場合は除外
        aspect_ratio = width / height if height > 0 else 0
        if aspect_ratio < 0.5 or aspect_ratio > 2.0:
            return False
        
        # ピクセル値のヒストグラム分析
        # トーフは通常、2値的（輪郭と内側で値が大きく異なる）
        # 一方、通常の文字はより滑らかな分布を持つ
        
        # ビットマップを解析
        if bitmap.size == 0:
            return False
            
        # 単純なパターン検出: 
        # 1. ビットマップの周囲に濃い輪郭があるか
        # 2. 内部と周囲のコントラストが高いか
        
        # エッジ（外周）のピクセルを取得
        edge_pixels = np.concatenate([
            bitmap[0, :],                # 上辺
            bitmap[-1, :],               # 下辺
            bitmap[1:-1, 0],             # 左辺（角を除く）
            bitmap[1:-1, -1]             # 右辺（角を除く）
        ])
        
        # 内部のピクセルを取得（外周を除く）
        if width > 2 and height > 2:
            inner_pixels = bitmap[1:-1, 1:-1]
            
            # エッジと内部の平均値の差
            edge_mean = np.mean(edge_pixels)
            inner_mean = np.mean(inner_pixels)
            contrast = abs(edge_mean - inner_mean)
            
            # ヒストグラム分析：値の分布が2つの大きなピークに集中しているか
            # （輪郭と内部で明確な区別がある）
            hist, _ = np.histogram(bitmap, bins=10, range=(0, 255))
            peaks = [i for i in range(1, 9) if hist[i] > hist[i-1] and hist[i] > hist[i+1]]
            bimodal = len(peaks) <= 2 and contrast > 50
            
            # トーフ文字の特徴：
            # - 内部と輪郭のコントラストが高い
            # - 均一な四角形に近い形状
            # - 周囲のピクセル値が高い（はっきりした輪郭）
            
            if bimodal and edge_mean > 200 and contrast > 70:
                # デバッグ情報
                print(f"トーフ特性(U+{code_point:04X}): コントラスト={contrast:.1f}, エッジ平均={edge_mean:.1f}, 内部平均={inner_mean:.1f}, 双峰性={bimodal}")
                return True
        
        # 特殊なフォント固有のパターン検出
        # （特定のフォントで知られているトーフパターン）
        # 例：非対応文字のパターンがフォントごとに分かっている場合
        
        # 現在の実装ではフォント固有のパターンはなし
        
        return False
    
    def _is_tofu_glyph_v2(self, bitmap, width, height, code_point, font_name):
        """
        改良版トーフ文字検出アルゴリズム - より積極的にトーフを検出
        
        Args:
            bitmap: ビットマップデータ
            width: ビットマップの幅
            height: ビットマップの高さ
            code_point: Unicodeコードポイント（ログ用）
            font_name: フォント名（ログ用）
            
        Returns:
            bool: トーフ文字と判断された場合はTrue、そうでない場合はFalse
        """
        if bitmap.size == 0 or width < 3 or height < 3:
            return False
        
        # 簡易的なフォント固有パターン検出のみに変更
        # IPAフォントの場合
        if "ipa" in font_name.lower():
            # IPAフォントのトーフ検出設定を緩和
            edge_top = bitmap[0, :]
            edge_bottom = bitmap[-1, :]
            edge_left = bitmap[:, 0]
            edge_right = bitmap[:, -1]
            
            # 輪郭の平均値
            edge_mean = np.mean(np.concatenate([edge_top, edge_bottom, edge_left, edge_right]))
            
            # 典型的なIPA系フォントのトーフパターン
            if edge_mean > 230 and width > 10 and height > 10:
                # 典型的なトーフは外周がほぼ真っ白（高輝度）
                inner_region = bitmap[2:-2, 2:-2] if width > 4 and height > 4 else bitmap
                inner_mean = np.mean(inner_region)
                contrast = edge_mean - inner_mean
                
                if contrast > 150:
                    print(f"IPA系フォントのトーフを検出: U+{code_point:04X}, 輪郭平均={edge_mean:.1f}, 内部平均={inner_mean:.1f}, コントラスト={contrast:.1f}")
                    return True
        
        # M+フォントの場合 - 設定を大幅に緩和
        if ("mplus" in font_name.lower() or "m+" in font_name.lower()):
            # 明らかな極端パターンのみを検出
            # M+フォントの特徴的なトーフは外周が極端に明るく内部が極端に暗いパターン
            edge_top = bitmap[0, :]
            edge_bottom = bitmap[-1, :]
            
            # 外周の平均輝度が極端に高いもののみをトーフと判定
            edge_mean = np.mean(np.concatenate([edge_top, edge_bottom]))
            
            # 極端な場合のみトーフと判断
            if edge_mean > 245 and width > 5 and height > 5:
                print(f"M+系フォントの極端なトーフを検出: U+{code_point:04X}, 輪郭平均={edge_mean:.1f}")
                return True
        
        # 標準の字形と明らかに異なるサイズの場合はトーフの可能性
        expected_size = self.font_size * 0.7
        if (width > expected_size * 2 or height > expected_size * 2) and width > 20 and height > 20:
            print(f"異常な大きさのグリフを検出: U+{code_point:04X}, 幅={width}, 高さ={height}")
            return True
        
        # ほとんどの場合はトーフではないと判断
        return False
        
    def compare_characters(self, img1, img2):
        """
        複数の手法を組み合わせた高度な画像比較
        
        Args:
            img1: PIL画像オブジェクト
            img2: PIL画像オブジェクト
        
        Returns:
            float: 類似度（0.0-1.0）、高いほど類似
        """
        # 複数の類似度測定手法を組み合わせる
        similarity_scores = []
        
        # 1. パーセプチュアルハッシュ（pHash）による比較
        phash_score = self.compare_phash(img1, img2)
        similarity_scores.append(("phash", phash_score, 0.1))  # 重み0.4
        
        # 2. ORB特徴点検出による比較
        if HAS_CV2:
            orb_score = self.compare_orb(img1, img2)
            similarity_scores.append(("orb", orb_score, 0.1))  # 重み0.2
        
        # 3. 構造的類似性指標（SSIM）による比較
        if HAS_SSIM:
            ssim_score = self.compare_ssim(img1, img2)
            similarity_scores.append(("ssim", ssim_score, 0.1))  # 重み0.4
        
        # 基本的な類似度計算（常にフォールバックとして使用）
        basic_score = self.compare_simple(img1, img2)
        
        # 高度な手法が一つも使えない場合は基本的な類似度を返す
        if not similarity_scores:
            return basic_score
        
        # デバッグ情報
        print("類似度スコア:")
        print(f"  基本比較: {basic_score:.3f}")
        for name, score, weight in similarity_scores:
            print(f"  {name}: {score:.3f} (重み: {weight:.1f})")
        
        # 重み付け平均を計算
        total_weight = sum(weight for _, _, weight in similarity_scores)
        weighted_sum = sum(score * weight for _, score, weight in similarity_scores)
        
        if total_weight > 0:
            weighted_average = weighted_sum / total_weight
        else:
            weighted_average = basic_score
        
        print(f"  最終類似度: {weighted_average:.3f}")
        
        return weighted_average
    
    def compare_phash(self, img1, img2):
        """
        パーセプチュアルハッシュによる画像比較
        
        Args:
            img1: PIL画像オブジェクト
            img2: PIL画像オブジェクト
        
        Returns:
            float: 類似度（0.0-1.0）、高いほど類似
        """
        if not HAS_IMAGEHASH:
            return 0.5  # ライブラリがない場合は中立値を返す
        
        try:
            # PIL画像に変換
            if not isinstance(img1, Image.Image):
                img1 = Image.fromarray(np.array(img1))
            if not isinstance(img2, Image.Image):
                img2 = Image.fromarray(np.array(img2))
            
            # パーセプチュアルハッシュを計算
            hash1 = imagehash.phash(img1)
            hash2 = imagehash.phash(img2)
            
            # 正規化された類似度（0〜1、高いほど類似）
            max_diff = hash1.hash.size  # 可能な最大差異
            similarity = 1.0 - (hash1 - hash2) / max_diff
            
            return float(similarity)
        except Exception as e:
            print(f"パーセプチュアルハッシュ比較中にエラー: {e}")
            return 0.5
    
    def compare_orb(self, img1, img2):
        """
        ORB特徴点検出による画像比較
        
        Args:
            img1: PIL画像オブジェクト
            img2: PIL画像オブジェクト
        
        Returns:
            float: 類似度（0.0-1.0）、高いほど類似
        """
        if not HAS_CV2:
            return 0.5  # OpenCVがない場合は中立値を返す
        
        try:
            # NumPy配列に変換
            arr1 = np.array(img1)
            arr2 = np.array(img2)
            
            # グレースケールに変換
            if len(arr1.shape) > 2:
                arr1 = cv2.cvtColor(arr1, cv2.COLOR_RGB2GRAY)
            if len(arr2.shape) > 2:
                arr2 = cv2.cvtColor(arr2, cv2.COLOR_RGB2GRAY)
            
            # 画像サイズが極端に小さい場合は比較不能
            if arr1.shape[0] < 8 or arr1.shape[1] < 8 or arr2.shape[0] < 8 or arr2.shape[1] < 8:
                return 0.5
            
            # ORB特徴点検出器
            orb = cv2.ORB_create()
            
            # 特徴点と記述子を検出
            kp1, des1 = orb.detectAndCompute(arr1, None)
            kp2, des2 = orb.detectAndCompute(arr2, None)
            
            # 特徴点がない場合
            if des1 is None or des2 is None or len(des1) == 0 or len(des2) == 0:
                return 0.5
            
            # 特徴点マッチング
            bf = cv2.BFMatcher(cv2.NORM_HAMMING, crossCheck=True)
            matches = bf.match(des1, des2)
            
            # マッチスコアを正規化（0〜1）
            similarity = len(matches) / max(len(kp1), len(kp2))
            
            return float(similarity)
        except Exception as e:
            print(f"ORB特徴点比較中にエラー: {e}")
            return 0.5
    
    def compare_ssim(self, img1, img2):
        """
        構造的類似性指標（SSIM）による画像比較
        
        Args:
            img1: PIL画像オブジェクト
            img2: PIL画像オブジェクト
        
        Returns:
            float: 類似度（0.0-1.0）、高いほど類似
        """
        if not HAS_SSIM:
            return 0.5  # scikit-imageがない場合は中立値を返す
        
        try:
            # NumPy配列に変換
            arr1 = np.array(img1)
            arr2 = np.array(img2)
            
            # 画像サイズを揃える
            min_height = min(arr1.shape[0], arr2.shape[0])
            min_width = min(arr1.shape[1], arr2.shape[1])
            
            if min_height < 5 or min_width < 5:
                return 0.5  # サイズが小さすぎる場合
            
            arr1_resized = arr1[:min_height, :min_width]
            arr2_resized = arr2[:min_height, :min_width]
            
            # グレースケールに変換
            if len(arr1_resized.shape) > 2:
                arr1_resized = cv2.cvtColor(arr1_resized, cv2.COLOR_RGB2GRAY)
            if len(arr2_resized.shape) > 2:
                arr2_resized = cv2.cvtColor(arr2_resized, cv2.COLOR_RGB2GRAY)
            
            # SSIM計算
            similarity, _ = ssim(arr1_resized, arr2_resized, full=True)
            
            # 0〜1に正規化
            similarity = (similarity + 1) / 2
            
            return float(similarity)
        except Exception as e:
            print(f"SSIM比較中にエラー: {e}")
            return 0.5
    
    def compare_simple(self, img1, img2):
        """
        基本的なピクセル比較（外部ライブラリ不要）
        
        Args:
            img1: PIL画像オブジェクト
            img2: PIL画像オブジェクト
        
        Returns:
            float: 類似度（0.0-1.0）、高いほど類似
        """
        try:
            # NumPy配列に変換
            arr1 = np.array(img1)
            arr2 = np.array(img2)
            
            # サイズが違う場合はリサイズ
            if arr1.shape != arr2.shape:
                # 小さい方のサイズにリサイズ
                min_height = min(arr1.shape[0], arr2.shape[0])
                min_width = min(arr1.shape[1], arr2.shape[1])
                
                # サイズが小さすぎる場合
                if min_height < 3 or min_width < 3:
                    return 0.5
                
                img1_resized = img1.resize((min_width, min_height))
                img2_resized = img2.resize((min_width, min_height))
                
                arr1 = np.array(img1_resized)
                arr2 = np.array(img2_resized)
            
            # 平均二乗誤差
            mse = np.mean((arr1.astype(float) - arr2.astype(float)) ** 2)
            
            # 類似度に変換（MSEが小さいほど類似度が高い）
            max_val = 255.0
            similarity = 1.0 - min(1.0, mse / (max_val ** 2))
            
            return float(similarity)
        except Exception as e:
            print(f"基本比較中にエラー: {e}")
            return 0.5
    
    def _render_glyph(self, char, width, height, left, top, font=None):
        """
        グリフのビットマップをレンダリング
        
        Args:
            char (str): レンダリングする文字
            width (int): グリフの幅
            height (int): グリフの高さ
            left (int): 左側拡張
            top (int): 上部拡張
            font: PILのImageFontオブジェクト (None時はメインフォント使用)
        
        Returns:
            numpy.ndarray: アルファ値の2D配列
        """
        if width == 0 or height == 0:
            return np.zeros((0, 0), dtype=np.uint8)
        
        # フォントが指定されていない場合はメインフォントを使用
        if font is None:
            font = self.main_font
        
        # 十分なサイズのイメージを作成 (余裕を持たせる)
        img_width = width + 10
        img_height = height + 10
        img = Image.new('L', (img_width, img_height), 0)
        draw = ImageDraw.Draw(img)
        
        # 文字を描画 (アンチエイリアスの有無で描画メソッドを変更)
        if self.anti_aliasing:
            draw.text((5-left, 5-top), char, font=font, fill=255)
        else:
            # アンチエイリアス無効の場合は単純な2値化
            draw.text((5-left, 5-top), char, font=font, fill=255)
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
        
        # 結果レポートを表示
        print("\n----- 革命的VLWフォント生成レポート -----")
        print(f"出力ファイル: {output_path}")
        print(f"合計グリフ数: {len(self.glyphs)}")
        
        if self.fallback_fonts:
            print(f"代替フォントから追加されたグリフ数: {self.fallback_glyph_count}")
        
        if self.missing_glyphs:
            print(f"見つからなかったグリフ数: {len(self.missing_glyphs)}")
            if len(self.missing_glyphs) <= 10:
                for code_point, char in self.missing_glyphs:
                    print(f"  - U+{code_point:04X} '{char}'")
            else:
                # 多すぎる場合は最初の10個だけ表示
                for code_point, char in self.missing_glyphs[:10]:
                    print(f"  - U+{code_point:04X} '{char}'")
                print(f"  ... 他 {len(self.missing_glyphs) - 10} 文字")
            
            # 見つからなかったグリフの一覧をファイルに出力
            missing_file = f"{os.path.splitext(output_path)[0]}_missing.txt"
            with open(missing_file, 'w', encoding='utf-8') as mf:
                mf.write(f"# {output_path} で見つからなかったグリフの一覧\n")
                mf.write(f"# 合計: {len(self.missing_glyphs)} 文字\n\n")
                
                # コードポイント順にソート
                sorted_missing = sorted(self.missing_glyphs, key=lambda x: x[0])
                
                for code_point, char in sorted_missing:
                    mf.write(f"U+{code_point:04X} '{char}'\n")
                
            print(f"見つからなかったグリフの一覧を {missing_file} に保存しました。")
        
        print("----------------------------------------")
        
        return output_path
    
    def write_missing_report(self, output_path=None):
        """
        見つからなかったグリフの一覧を出力
        
        Args:
            output_path (str, optional): 出力ファイルパス。None指定時は自動生成。
        
        Returns:
            str: 生成されたレポートファイルのパス、または見つからないグリフがない場合はNone
        """
        if not self.missing_glyphs:
            print("見つからなかったグリフはありません。")
            return None
        
        if output_path is None:
            # 出力ファイル名を自動生成
            output_path = f"{self.font_name}-{self.font_size}_missing.txt"
        
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(f"# {self.font_name} フォントで見つからなかったグリフの一覧\n")
            f.write(f"# 合計: {len(self.missing_glyphs)} 文字\n\n")
            
            # コードポイント順にソート
            sorted_missing = sorted(self.missing_glyphs, key=lambda x: x[0])
            
            for code_point, char in sorted_missing:
                f.write(f"U+{code_point:04X} '{char}'\n")
        
        print(f"見つからなかったグリフの一覧を {output_path} に保存しました。")
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
    parser.add_argument('-f', '--fallback', action='append', help='代替フォントのパス (複数指定可能)')
    parser.add_argument('-c', '--chars-file', help='文字リストファイル (1行1文字のテキストファイル)')
    parser.add_argument('--dump-missing', action='store_true', help='見つからなかった文字を別ファイルに出力')
    parser.add_argument('--tofu-reference', type=lambda x: int(x, 0), help='トーフ文字として使用する文字コード (例: 0x3013)')
    parser.add_argument('--tofu-threshold', type=float, default=0.8, help='トーフ判定の閾値 (0.0-1.0、高いほど厳格、デフォルト: 0.8)')
    parser.add_argument('--no-advanced-comparison', action='store_true', help='高度な画像比較手法を使用しない')
    parser.add_argument('--basic-only', action='store_true', help='フォント固有のパターン検出を使用せず、参照文字比較のみを使用')
    
    args = parser.parse_args()
    
    # TTFファイルの存在確認
    if not os.path.isfile(args.ttf_file):
        print(f"エラー: TTFファイル '{args.ttf_file}' が見つかりません！")
        return 1
    
    # 代替フォントの存在確認
    fallback_fonts = []
    if args.fallback:
        for fb_path in args.fallback:
            if not os.path.isfile(fb_path):
                print(f"警告: 代替フォント '{fb_path}' が見つかりません。スキップします。")
            else:
                fallback_fonts.append(fb_path)
    
    # TTF2VLWインスタンスを作成
    converter = TTF2VLW(
        args.ttf_file, 
        args.size, 
        not args.no_antialias, 
        fallback_fonts, 
        args.tofu_reference,
        args.tofu_threshold,
        not args.no_advanced_comparison
    )
    
    # ASCII文字の追加
    if args.ascii:
        converter.add_basic_ascii()
    
    # Unicode範囲の追加
    if args.range:
        try:
            # カンマ区切りで複数の範囲指定をサポート
            ranges = args.range.split(',')
            for range_str in ranges:
                range_parts = range_str.strip().split('-')
                start_code = int(range_parts[0], 0)  # 0xで始まる場合も正しく解析
                end_code = int(range_parts[1], 0) if len(range_parts) > 1 else start_code
                print(f"Unicode範囲 U+{start_code:04X} から U+{end_code:04X} までを追加します...")
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
                    text_content = f.read()
                print(f"テキストファイル '{args.text}' から文字を読み込みます... ({len(set(text_content))} 種類の文字)")
                converter.add_text(text_content)
            except Exception as e:
                print(f"エラー: テキストファイルの読み込みに失敗しました: {e}")
                return 1
        else:
            # 直接テキストとして処理
            print(f"直接指定されたテキストから文字を読み込みます... ({len(set(args.text))} 種類の文字)")
            converter.add_text(args.text)
    
    # 文字リストファイルからの追加
    if args.chars_file:
        if not os.path.isfile(args.chars_file):
            print(f"エラー: 文字リストファイル '{args.chars_file}' が見つかりません！")
            return 1
        
        try:
            # 改行で区切られた文字を読み込む
            with open(args.chars_file, 'r', encoding='utf-8') as f:
                chars = []
                for line_num, line in enumerate(f, 1):
                    line = line.strip()
                    if not line or line.startswith('#'):  # 空行とコメント行をスキップ
                        continue
                    
                    if line.startswith('U+') or line.startswith('u+'):
                        # Unicodeコードポイント表記 (U+XXXX)
                        try:
                            code = int(line[2:], 16)
                            chars.append(chr(code))
                        except ValueError:
                            print(f"警告: 行 {line_num} のUnicodeコードポイントが無効です: {line}")
                    else:
                        # 通常の文字の場合は、行全体を追加（各文字を個別に）
                        for char in line:
                            chars.append(char)
            
            if chars:
                unique_chars = set(chars)  # 重複を除去
                print(f"文字リストファイル '{args.chars_file}' から {len(unique_chars)} 文字を読み込みます...")
                converter.add_text(''.join(unique_chars))
            else:
                print(f"警告: 文字リストファイル '{args.chars_file}' に有効な文字がありません。")
        except Exception as e:
            print(f"エラー: 文字リストファイルの処理中にエラーが発生しました: {e}")
            return 1
    
    # グリフが追加されなかった場合はデフォルトでASCIIを追加
    if not converter.glyphs:
        print("警告: グリフが指定されていません。基本ASCII文字を追加します。")
        converter.add_basic_ascii()
    
    # VLWファイルを生成
    vlw_path = converter.write_vlw(args.output)
    
    # 見つからなかった文字のレポートを出力
    if args.dump_missing and converter.missing_glyphs:
        missing_path = converter.write_missing_report()
    
    return 0

if __name__ == "__main__":
    sys.exit(main())