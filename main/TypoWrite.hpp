// main/TypoWrite.hpp - 固定値微調整版ヘッダー
#ifndef _TYPO_WRITE_HPP_
#define _TYPO_WRITE_HPP_

#include <M5GFX.h>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include "VLWFontParser.hpp"

// ========================================
// 列挙型定義
// ========================================

// テキスト方向の列挙型
enum class TextDirection {
    HORIZONTAL,  // 横書き（左から右）
    VERTICAL     // 縦書き（上から下）
};

// テキスト揃えの列挙型
enum class TextAlignment {
    LEFT,    // 左揃え（縦書きの場合は上揃え）
    CENTER,  // 中央揃え
    RIGHT    // 右揃え（縦書きの場合は下揃え）
};

// 文字カテゴリの列挙型
enum class CharCategory {
    NORMAL,         // 通常文字
    BRACKET,        // 括弧類
    HORIZONTAL_BAR, // 横棒・長音記号類
    PUNCTUATION,    // 句読点類
    SMALL_CHAR,     // 小文字（ひらがな・カタカナ）
    OTHER_SPECIAL   // その他の特殊文字
};

// ========================================
// 構造体定義
// ========================================

// 小文字の描画設定構造体
struct SmallCharSettings {
    float scale;    // 縮小率（0.7～0.8推奨）
    float offsetX;  // X方向オフセット（文字幅に対する比率）
    float offsetY;  // Y方向オフセット（文字高に対する比率）
    
    // デフォルト設定
    static SmallCharSettings getDefault() {
        return {
            0.75f,   // 75%に縮小
            0.35f,   // 右に35%オフセット（縦書き用）
            -0.1f    // 上に20%オフセット（縦書き用）
        };
    }
};

// 文字メトリクス構造体（統一管理用）
struct CharMetrics {
    int32_t width;      // 文字幅（ピクセル）
    int32_t height;     // 文字高さ（ピクセル）
    int32_t setWidth;   // 文字送り幅（ピクセル）
    int32_t baseline;   // ベースライン位置
};

// 文字種別の微調整設定構造体（固定値版）
struct CharTypeAdjustment {
    float widthScale;     // 幅の調整倍率
    float heightScale;    // 高さの調整倍率
    int spacingOffset;    // 字間オフセット（ピクセル）
    int verticalOffset;   // 縦位置オフセット（ピクセル）
    int horizontalOffset; // 横位置オフセット（ピクセル）
};

// ========================================
// TypoWriteクラス定義
// ========================================
class TypoWrite {
private:
    // ========== 基本設定 ==========
    M5GFX* _display;                     // 描画先のディスプレイ（デフォルト）
    lgfx::LGFX_Sprite* _drawTarget;      // 描画先スプライト（外部から設定可能）
    lgfx::LGFX_Sprite* _charSprite;      // 文字描画用再利用スプライト（最適化用）
    
    // ========== 描画設定 ==========
    TextDirection _direction;            // テキスト方向
    TextAlignment _alignment;            // テキスト揃え
    int _x, _y;                         // 描画開始位置
    int _width, _height;                // 描画領域のサイズ
    uint16_t _color;                    // テキスト色
    uint16_t _bgColor;                  // 背景色
    bool _transparentBg;                // 背景透明フラグ
    bool _wrap;                         // 折り返しフラグ
    
    // ========== フォント設定 ==========
    float _fontSize;                    // フォントサイズ
    const lgfx::IFont* _font;          // フォント
    const uint8_t* _vlwFont;           // VLWフォントデータ
    bool _isCustomFont;                // カスタムフォントフラグ
    VLWFontParser* _vlwParser;         // VLWパーサー（外部から設定）
    bool _useVLWParser;                // VLWパーサー使用フラグ
    
    // ========== スペーシング設定 ==========
    int _lineSpacing;                  // 行間
    int _charSpacing;                  // 文字間
    int _columnSpacing;                // 列間（縦書き用）
    
    // ========== 描画状態 ==========
    int _currentX, _currentY;          // 現在の描画位置
    
    // ========== 小文字処理設定 ==========
    bool _enableSmallCharHandling;     // 小文字処理有効フラグ
    SmallCharSettings _smallCharSettings; // 小文字描画設定
    
    // ========== 固定値微調整機能 ==========
    bool _showBorder;                  // 枠線表示フラグ
    uint16_t _borderColor;             // 枠線色
    bool _enableCharAdjustment;        // 文字調整有効フラグ（固定値適用の ON/OFF）
    std::unordered_map<CharCategory, CharTypeAdjustment> _charAdjustments; // 固定値調整テーブル
    
    // ========== マッピングテーブル ==========
    std::unordered_map<uint16_t, uint16_t> _smallToLargeMap;   // 小文字→大文字マッピング
    std::unordered_map<uint16_t, uint16_t> _verticalGlyphMap;  // 縦書き用グリフマッピング
    
    // ========== パフォーマンス最適化 ==========
    std::unordered_map<uint16_t, CharMetrics> _metricsCache;   // メトリクスキャッシュ

public:
    // ========================================
    // コンストラクタ・デストラクタ
    // ========================================
    TypoWrite(M5GFX *display);
    ~TypoWrite();
    
    // ========================================
    // 基本設定メソッド
    // ========================================
    
    // 描画先の設定
    void setDrawTarget(lgfx::LGFX_Sprite *sprite);
    
    // VLWパーサーの設定
    void setVLWParser(VLWFontParser* parser);
    
    // テキスト方向の設定
    void setDirection(TextDirection direction);
    
    // テキスト揃えの設定
    void setAlignment(TextAlignment alignment);
    
    // 描画位置の設定
    void setPosition(int x, int y);
    
    // 描画領域の設定
    void setArea(int width, int height);
    
    // テキスト色の設定
    void setColor(uint16_t color);
    
    // 背景色の設定
    void setBackgroundColor(uint16_t bgColor);
        
    // 折り返しの設定
    void setWrap(bool wrap);


    
    // ========================================
    // 固定値微調整機能設定（簡易版）
    // ========================================
    
    // 枠線表示の簡易切り替え（色は固定値 TFT_RED を使用）
    void setBorderDisplay(bool show, uint16_t color = TFT_WHITE);
    
    // 文字調整機能の簡易切り替え（固定値の適用 ON/OFF）
    void setCharacterAdjustment(bool enable);

    // 固定値による文字種別調整の取得
    CharTypeAdjustment getCharAdjustment(uint16_t unicode_char);
    // ========================================
    // フォント設定メソッド
    // ========================================
    
    // フォントサイズの設定
    void setFontSize(float size);
    
    // フォントの設定
    void setFont(const lgfx::IFont *font);
    
    // カスタムフォントの読み込み
    bool loadFontFromArray(const uint8_t *fontArray);
    
    // ========================================
    // スペーシング設定メソッド
    // ========================================
    
    // 行間の設定
    void setLineSpacing(int spacing);
    
    // 文字間の設定
    void setCharSpacing(int spacing);
    
    // 列間の設定（縦書き用）
    void setColumnSpacing(int spacing);
    
    
    // ========================================
    // メインテキスト描画メソッド
    // ========================================
    
    // テキスト描画（固定値調整適用版）
    void drawText(const std::string &text);
    
    // 中央揃えでテキスト描画
    void drawTextCentered(const std::string &text);
    
    // ========================================
    // サイズ計算メソッド
    // ========================================
    
    // テキスト幅の取得
    int getTextWidth(const std::string &text);
    
    // テキスト高さの取得
    int getTextHeight(const std::string &text);
    
    // ========================================
    // 描画領域管理メソッド
    // ========================================
    
    // 描画領域のクリア
    void clearArea(uint16_t color = TFT_BLACK);
    
    // ========================================
    // デバッグメソッド
    // ========================================
    
    // 小文字マッピングテーブルのデバッグ表示
    void debugPrintSmallCharMap() const;
    
    // 文字列内の小文字分析
    void debugAnalyzeSmallChars(const std::string& text);
    
    // 固定値調整設定のデバッグ表示
    void debugShowFixedAdjustments();


    void debugShowCharAdjustments();

private:
    // ========================================
    // 内部初期化メソッド
    // ========================================
    
    void initializeAllTables();

    // ========================================
    // 内部描画メソッド
    // ========================================
    
    // 横書きテキスト描画（固定値調整適用版）
    void drawHorizontalTextEnhanced(const std::string &text);
    
    // 縦書きテキスト描画（固定値調整適用版）
    void drawVerticalTextEnhanced(const std::string &text);
    
    // 枠線描画
    void drawAreaBorder();
    
    // 文字描画（スケール調整対応）
    void drawEnhancedCharacter(uint16_t unicode_char, int x, int y,
                              float widthScale = 1.0f, float heightScale = 1.0f);
    
    // 回転付き文字描画（スケール調整対応）
    void drawEnhancedCharacterWithRotation(uint16_t unicode_char, int x, int y,
                                          float widthScale, float heightScale,
                                          float rotation);
    
    // スケール調整文字描画
    void drawScaledCharacter(uint16_t unicode_char, int x, int y,
                            float widthScale, float heightScale);
    
    // スケール調整＋回転文字描画
    void drawScaledCharacterWithRotation(uint16_t unicode_char, int x, int y,
                                        float widthScale, float heightScale,
                                        float rotation);
    
    // 直接描画（既存）
    void drawDirectCharacter(uint16_t unicode_char, int x, int y);
    
    
    // ========================================
    // 内部計算・判定メソッド
    // ========================================
    
    // テキストサイズの計算
    void calculateTextSize(const std::string &text, int &width, int &height);
    
    // 文字メトリクスの取得
    CharMetrics getCharMetrics(uint16_t unicode_char);
    
    // 固定値による文字種別調整取得
    CharTypeAdjustment getFixedCharAdjustment(uint16_t unicode_char);
    
    // 文字カテゴリの判定
    CharCategory getCharCategory(uint16_t unicode_char);
    
    // 小文字判定
    bool isSmallChar(uint16_t unicode_char) const;
    
    // 小文字の対応する大文字を取得
    uint16_t getCorrespondingLargeChar(uint16_t small_char) const;
    
    // 縦書き時の回転判定
    bool shouldRotateInVertical(uint16_t unicode_char);
    
    // 縦書き用グリフへの変換
    uint16_t convertToVerticalGlyph(uint16_t unicode_char);
    
    // ========================================
    // ヘルパーメソッド
    // ========================================
    
    // UTF-8からUnicodeへの変換
    std::vector<uint16_t> utf8ToUnicode(const std::string &utf8_string);
    
    // UnicodeからUTF-8への変換
    std::string unicodeToUtf8(uint16_t unicode_char);
    
    // 行の高さを取得
    int getLineHeight();
    
    // 最大文字幅を取得（縦書き用）
    int getMaxCharWidth();
};

#endif // _TYPO_WRITE_HPP_