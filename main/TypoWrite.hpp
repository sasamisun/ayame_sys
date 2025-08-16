// main/TypoWrite.hpp - リファクタリング版ヘッダー
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
            -0.2f    // 上に20%オフセット（縦書き用）
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
    int _x, _y;                         // 描画開始座標
    int _width, _height;                // 描画領域のサイズ
    uint16_t _color;                    // テキスト色
    uint16_t _bgColor;                  // 背景色
    bool _transparentBg;                // 背景透明フラグ
    bool _wrap;                         // テキスト折り返しフラグ
    
    // ========== フォント設定 ==========
    float _fontSize;                    // フォントサイズ倍率
    const lgfx::IFont* _font;           // 使用フォント
    const uint8_t* _vlwFont;            // VLWフォントデータ
    bool _isCustomFont;                 // カスタムフォント使用フラグ
    VLWFontParser* _vlwParser;          // VLWパーサー
    bool _useVLWParser;                 // VLWパーサー使用フラグ
    
    // ========== スペーシング設定 ==========
    int _lineSpacing;                   // 行間（ピクセル）
    int _charSpacing;                   // 文字間（ピクセル）
    int _columnSpacing;                 // 縦書き時の列間隔
    
    // ========== 現在位置管理 ==========
    int _currentX;                      // 現在のX座標（描画領域内の相対位置）
    int _currentY;                      // 現在のY座標（描画領域内の相対位置）
    
    // ========== 小文字システム ==========
    bool _enableSmallCharHandling;     // 小文字特別処理の有効/無効
    SmallCharSettings _smallCharSettings;  // 小文字描画設定
    std::unordered_map<uint16_t, uint16_t> _smallToLargeMap;  // 小文字→大文字マッピング
    
    // ========== 縦書きシステム ==========
    std::unordered_map<uint16_t, uint16_t> _verticalGlyphMap;  // 縦書き用グリフマッピング
    
    // ========== キャッシュシステム ==========
    mutable std::unordered_map<uint16_t, CharMetrics> _metricsCache;  // メトリクスキャッシュ
    
    // ========================================
    // 内部メソッド（プライベート）
    // ========================================
    
    // 初期化メソッド
    void initializeSmallCharMap();      // 小文字マッピング初期化
    void initializeVerticalGlyphMap();  // 縦書きグリフマッピング初期化
    
    // 統一文字描画メソッド（全描画を一元化）
    void drawUnifiedCharacter(uint16_t unicode_char, int x, int y,
                             float scale = 1.0f, float rotation = 0.0f,
                             float offsetX = 0.0f, float offsetY = 0.0f);
    
    // 直接描画メソッド（最速パス、スケール1.0・回転なし専用）
    void drawDirectCharacter(uint16_t unicode_char, int x, int y);
    
    // スプライト描画メソッド（スケール・回転対応）
    void drawSpriteCharacter(uint16_t unicode_char, int x, int y,
                            float scale, float rotation);
    
    // テキスト描画実装
    void drawHorizontalText(const std::string& text);
    void drawVerticalText(const std::string& text);
    
    // 文字判定・変換メソッド
    bool isSmallChar(uint16_t unicode_char) const;
    uint16_t getCorrespondingLargeChar(uint16_t small_char) const;
    uint16_t convertToVerticalGlyph(uint16_t unicode_char);
    bool shouldRotateInVertical(uint16_t unicode_char);
    
    // メトリクス取得（統一化）
    CharMetrics getCharMetrics(uint16_t unicode_char);
    
    // テキストサイズ計算
    void calculateTextSize(const std::string& text, int& width, int& height);
    
    // 文字カテゴリ判定
    CharCategory getCharCategory(uint16_t unicode_char);
    
public:
    // ========================================
    // パブリックインターフェース
    // ========================================
    
    // コンストラクタ・デストラクタ
    TypoWrite(M5GFX* display);
    ~TypoWrite();
    
    // ========== 文字変換ヘルパー ==========
    std::vector<uint16_t> utf8ToUnicode(const std::string& utf8_string);
    std::string unicodeToUtf8(uint16_t unicode_char);
    
    // ========== 描画先設定 ==========
    void setDrawTarget(lgfx::LGFX_Sprite* sprite);  // nullptrでディスプレイに直接描画
    
    // ========== 基本設定メソッド ==========
    void setDirection(TextDirection direction);
    void setAlignment(TextAlignment alignment);
    void setPosition(int x, int y);
    void setArea(int width, int height);
    void setColor(uint16_t color);
    void setBackgroundColor(uint16_t bgColor);
    void setTransparentBg(bool transparent) { _transparentBg = transparent; }
    void setWrap(bool wrap);
    
    // ========== フォント設定メソッド ==========
    void setFontSize(float size);
    void setFont(const lgfx::IFont* font);
    void setIsCustomFont(bool isCustom) { _isCustomFont = isCustom; }
    bool loadFontFromArray(const uint8_t* fontArray);
    void setVLWParser(VLWFontParser* parser);
    
    // ========== スペーシング設定 ==========
    void setLineSpacing(int spacing);
    void setCharSpacing(int spacing);
    void setColumnSpacing(int spacing) { _columnSpacing = spacing; }
    
    // ========== テキスト描画メソッド ==========
    void drawText(const std::string& text);
    void drawTextCentered(const std::string& text);
    
    // ========== サイズ計算メソッド ==========
    int getTextWidth(const std::string& text);
    int getTextHeight(const std::string& text);
    
    // ========== 描画位置取得 ==========
    int getCurrentX() const { return _x; }
    int getCurrentY() const { return _y; }
    
    // ========== フォント情報取得 ==========
    bool isCustomFont() const { return _isCustomFont; }
    
    // ========== 描画領域管理 ==========
    void clearArea(uint16_t color = 0);
    
    // ========== 小文字システム公開メソッド ==========
    void setSmallCharHandling(bool enable) { _enableSmallCharHandling = enable; }
    bool isSmallCharHandlingEnabled() const { return _enableSmallCharHandling; }
    void setSmallCharSettings(const SmallCharSettings& settings) { _smallCharSettings = settings; }
    const SmallCharSettings& getSmallCharSettings() const { return _smallCharSettings; }
    
    // ========== デバッグメソッド ==========
    void debugPrintSmallCharMap() const;
    void debugAnalyzeSmallChars(const std::string& text);
    void debugPrintVerticalGlyphMap() const;
    void debugPrintMetricsCache() const;
};

#endif // _TYPO_WRITE_HPP_