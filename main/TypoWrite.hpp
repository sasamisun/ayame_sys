// main/TypoWrite.hpp
#ifndef _TYPO_WRITE_HPP_
#define _TYPO_WRITE_HPP_

#include <M5GFX.h>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include "VLWFontParser.hpp"

// テキスト方向の列挙型
enum class TextDirection
{
    HORIZONTAL, // 横書き（左から右）
    VERTICAL    // 縦書き（上から下）
};

// テキスト揃えの列挙型
enum class TextAlignment
{
    LEFT,   // 左揃え（縦書きの場合は上揃え）
    CENTER, // 中央揃え
    RIGHT   // 右揃え（縦書きの場合は下揃え）
};

// 文字カテゴリの列挙型
enum class CharCategory
{
    NORMAL,         // 通常文字
    BRACKET,        // 括弧類
    HORIZONTAL_BAR, // 横棒・長音記号類
    PUNCTUATION,    // 句読点類
    SMALL_CHAR,     // 小文字（ひらがな・カタカナ）
    OTHER_SPECIAL   // その他の特殊文字
};

// 小文字の描画設定構造体（シンプル版）
struct SmallCharSettings
{
    float scale;        // 縮小率（0.7～0.8推奨）
    float offsetX;      // X方向オフセット（文字幅に対する比率）
    float offsetY;      // Y方向オフセット（文字高に対する比率）
    
    // デフォルト設定（実測で調整済み）
    static SmallCharSettings getDefault() {
        return {
            0.75f,  // 75%に縮小
            0.35f,  // 右に35%オフセット（縦書き用）
            -0.2f   // 上に20%オフセット（縦書き用）
        };
    }
};

// TypoWrite - 縦書き/横書き対応テキスト描画クラス
class TypoWrite
{
private:
    M5GFX *_display;                    // 描画先のディスプレイ（デフォルト）
    lgfx::LGFX_Sprite *_drawTarget;     // 描画先スプライト（外部から設定可能）
    TextDirection _direction;           // テキスト方向
    TextAlignment _alignment;           // テキスト揃え
    int _x;                             // 描画開始X座標
    int _y;                             // 描画開始Y座標
    int _width;                         // 描画領域の幅
    int _height;                        // 描画領域の高さ
    uint16_t _color;                    // テキスト色
    uint16_t _bgColor;                  // 背景色
    float _fontSize;                    // フォントサイズ倍率
    const lgfx::IFont *_font;           // 使用フォント
    const uint8_t *_vlwFont;            // VLWフォントデータ
    bool _isCustomFont;                 // カスタムフォントを使用しているかどうか
    int _lineSpacing;                   // 行間（ピクセル）
    int _charSpacing;                   // 文字間（ピクセル）
    bool _wrap;                         // テキストを折り返すか
    bool _transparentBg;                // 背景色透明
    mutable lgfx::FontMetrics _metrics; // メトリクス情報をキャッシュするための変数
    int _columnSpacing;                 // 縦書き時の列間隔


    // 現在の描画位置（描画領域内の相対位置）
    int _currentX;
    int _currentY;

    VLWFontParser *_vlwParser; // VLWパーサー（独自管理）
    bool _useVLWParser;                 // VLWパーサーを使用するかどうか


    // 内部メソッド
    void drawHorizontalText(const std::string &text);
    void drawVerticalText(const std::string &text);

    // 一文字描画メソッド
    void drawCharacterHorizontal(uint16_t unicode_char, int x, int y);
    void drawCharacterVertical(uint16_t unicode_char, int x, int y);

    // 特殊文字の回転描画
    void drawRotatedCharacter(uint16_t unicode_char, int x, int y);

    // 文字サイズ計算
    int32_t getCharacterWidth(uint16_t unicode_char);
    int32_t getCharacterHeight(uint16_t unicode_char);

    // フォント関連のヘルパーメソッド
    int32_t getFontWidth();
    int32_t getFontHeight();


    // 特殊文字の処理
    CharCategory getCharCategory(uint16_t unicode_char);

    // 縦書きで回転が必要な文字かどうかを判定
    bool shouldRotateInVertical(uint16_t unicode_char);

    // 特定の文字のメトリクス情報を更新するヘルパー関数
    bool updateMetricsForChar(uint16_t unicode_char) const;

    // テキストサイズ計算
    void calculateTextSize(const std::string &text, int &width, int &height);

    // 小文字システム関連メンバ
    bool _enableSmallCharHandling;                            // 小文字特別処理を有効にするか
    SmallCharSettings _smallCharSettings;                     // 小文字描画設定
    std::unordered_map<uint16_t, uint16_t> _smallToLargeMap; // 小文字→大文字マッピング

    // 小文字システム内部メソッド
    /**
     * @brief 小文字マッピングテーブルを初期化する
     * ひらがな・カタカナの小文字を対応する大文字にマッピング
     */
    void initializeSmallCharMap();
    
    /**
     * @brief 指定された文字が小文字かどうかを判定する
     * @param unicode_char 判定したいUnicode文字コード
     * @return 小文字ならtrue、そうでなければfalse
     */
    bool isSmallChar(uint16_t unicode_char) const;
    
    /**
     * @brief 小文字に対応する大文字を取得する
     * @param small_char 小文字のUnicode文字コード
     * @return 対応する大文字のUnicode文字コード（見つからない場合は元の文字）
     */
    uint16_t getCorrespondingLargeChar(uint16_t small_char) const;

    /**
     * @brief 縦書き用小文字描画（縮小+位置オフセット）
     * @param small_char 小文字のUnicode文字コード
     * @param x X座標（描画領域内の相対位置）
     * @param y Y座標（描画領域内の相対位置）
     */
    void drawSmallCharacterVertical(uint16_t small_char, int x, int y);
    
    /**
     * @brief 横書き用小文字描画（縮小+位置オフセット）
     * @param small_char 小文字のUnicode文字コード
     * @param x X座標（描画領域内の相対位置）
     * @param y Y座標（描画領域内の相対位置）
     */
    void drawSmallCharacterHorizontal(uint16_t small_char, int x, int y);

    /**
     * @brief 文字の縮小描画（一時スプライト使用）
     * @param unicode_char 描画する文字のUnicode文字コード
     * @param x X座標（描画領域内の相対位置）
     * @param y Y座標（描画領域内の相対位置）
     * @param scale 縮小率（1.0=原寸、0.5=50%など）
     * @param offsetX 追加X方向オフセット（通常は0）
     * @param offsetY 追加Y方向オフセット（通常は0）
     */
    void drawScaledCharacter(uint16_t unicode_char, int x, int y, 
                           float scale, float offsetX, float offsetY);
public:
    // コンストラクタ
    TypoWrite(M5GFX *display);
    // デストラクタ
    ~TypoWrite();

    // 文字変換ヘルパーメソッド
    std::vector<uint16_t> utf8ToUnicode(const std::string &utf8_string);
    std::string unicodeToUtf8(uint16_t unicode_char);

    // 描画先の設定
    void setDrawTarget(lgfx::LGFX_Sprite *sprite); // nullptrでディスプレイに直接描画

    // 設定メソッド - 基本設定
    void setDirection(TextDirection direction);
    void setAlignment(TextAlignment alignment);
    void setPosition(int x, int y);
    void setArea(int width, int height);
    void setColor(uint16_t color);
    void setBackgroundColor(uint16_t bgColor);
    void setTransparentBg(bool transparent) { _transparentBg = transparent; }

    // 設定メソッド - フォント関連
    void setFontSize(float size);
    void setFont(const lgfx::IFont *font);
    void setIsCustomFont(bool isCustom) { _isCustomFont = isCustom; }
    bool loadFontFromArray(const uint8_t *fontArray);

    // 設定メソッド - スペーシング
    void setLineSpacing(int spacing);
    void setCharSpacing(int spacing);
    void setColumnSpacing(int spacing) { _columnSpacing = spacing; }
    void setWrap(bool wrap);

    // テキスト描画メソッド
    void drawText(const std::string &text);
    void drawTextCentered(const std::string &text);

    // サイズ計算メソッド
    int getTextWidth(const std::string &text);
    int getTextHeight(const std::string &text);

    // 描画位置取得メソッド
    int getCurrentX() const { return _x; }
    int getCurrentY() const { return _y; }

    // フォント情報取得メソッド
    bool isCustomFont() const { return _isCustomFont; }

    // 描画領域のクリア
    void clearArea(uint16_t color = 0);

    /**
     * @brief VLWフォントパーサーを設定する
     * @param parser VLWフォントパーサーのポインタ
     */
    void setVLWParser(VLWFontParser *parser);

    /**
     * @brief VLWパーサーを使用してフォント高を取得する
     * @return フォント高（ピクセル）
     */
    int32_t getFontHeightVLW();

    /**
     * @brief VLWパーサーを使用してフォント幅を取得する
     * @return フォント幅（ピクセル）
     */
    int32_t getFontWidthVLW();

    /**
     * @brief VLWパーサーを使用して指定文字の幅を取得する
     * @param unicode_char Unicode文字コード
     * @return 文字幅（ピクセル）
     */
    int32_t getCharacterWidthVLW(uint16_t unicode_char);

    /**
     * @brief VLWパーサーを使用して指定文字の高さを取得する
     * @param unicode_char Unicode文字コード
     * @return 文字高さ（ピクセル）
     */
    int32_t getCharacterHeightVLW(uint16_t unicode_char);

    /**
     * @brief VLWパーサーを使用して文字の送り幅を取得する
     * @param unicode_char Unicode文字コード
     * @return 送り幅（ピクセル）
     */
    int32_t getCharacterSetWidthVLW(uint16_t unicode_char);

    // 小文字システム公開メソッド
    // 🌟 小文字システム公開メソッド（追加のみ）
    void setSmallCharHandling(bool enable);
    bool isSmallCharHandlingEnabled() const;
    void setSmallCharSettings(const SmallCharSettings& settings);
    const SmallCharSettings& getSmallCharSettings() const;
    
    // 🌟 デバッグメソッド（追加のみ）
    void debugPrintSmallCharMap() const;
    void debugAnalyzeSmallChars(const std::string& text);
};

#endif // _TYPO_WRITE_HPP_