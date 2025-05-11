// main/TypoWrite.hpp
#ifndef _TYPO_WRITE_HPP_
#define _TYPO_WRITE_HPP_

#include <M5GFX.h>
#include <vector>
#include <string>
#include <functional>

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
    OTHER_SPECIAL   // その他の特殊文字
};

// TypoWrite - 縦書き/横書き対応テキスト描画クラス
class TypoWrite
{
private:
    M5GFX *_display;          // 描画先のディスプレイ
    TextDirection _direction; // テキスト方向
    TextAlignment _alignment; // テキスト揃え
    int _x;                   // 描画開始X座標
    int _y;                   // 描画開始Y座標
    int _width;               // 描画領域の幅
    int _height;              // 描画領域の高さ
    uint16_t _color;          // テキスト色
    uint16_t _bgColor;        // 背景色
    float _fontSize;          // フォントサイズ倍率
    const lgfx::IFont *_font; // 使用フォント
    bool _isCustomFont;       // カスタムフォントを使用しているかどうか
    int _lineSpacing;         // 行間（ピクセル）
    int _charSpacing;         // 文字間（ピクセル）
    bool _wrap;               // テキストを折り返すか
    bool _transparentBg;      // 背景色透明
    mutable lgfx::FontMetrics _metrics; // メトリクス情報をキャッシュするための変数

    // 内部メソッド
    void setupDisplay();
    void drawHorizontalText(const std::string &text, int x, int y, bool measure_only = false);
    void drawVerticalText(const std::string &text, int x, int y, bool measure_only = false);

    // 文字サイズ計算
    /**
     * 特定の文字の幅を取得する
     * @param display 表示デバイス
     * @param font フォント
     * @param unicode_char Unicodeコードポイント
     * @return 文字の幅（ピクセル）
     */
    int32_t getCharacterWidth(uint16_t unicode_char);
    /**
     * 特定の文字の高さを取得する
     * @param display 表示デバイス
     * @param font フォント
     * @return 文字の高さ（ピクセル）
     */
    int32_t getCharacterHeight(uint16_t unicode_char);

    // フォント関連のヘルパーメソッド
    int32_t getFontWidth();
    int32_t getFontHeight();

    // 文字変換ヘルパーメソッド
    std::vector<uint16_t> utf8ToUnicode(const std::string &utf8_string);
    std::string unicodeToUtf8(uint16_t unicode_char);

    // スプライト操作ヘルパー
    void withSprite(int width, int height, std::function<void(lgfx::LGFX_Sprite *)> operation);

    // 特殊文字の処理
    CharCategory getCharCategory(uint16_t unicode_char);
    void drawSpecialChar(uint16_t unicode_char, int x, int y);

    // 縦書きで回転が必要な文字かどうかを判定
    bool shouldRotateInVertical(uint16_t unicode_char);

    // テキスト描画サイズの計算
    void calculateTextSize(const std::string &text, int &width, int &height);

    // 縦書きで回転が必要な文字を描画するヘルパーメソッド
    void drawRotatedCharacter(const std::string &utf8_str, int x, int y, int char_height);

    // 特定の文字のメトリクス情報を更新するヘルパー関数
    bool updateMetricsForChar(uint16_t unicode_char) const;

public:
    // コンストラクタ
    TypoWrite(M5GFX *display);

    // 設定メソッド
    void setDirection(TextDirection direction);
    void setAlignment(TextAlignment alignment);
    void setPosition(int x, int y);
    void setArea(int width, int height);
    void setColor(uint16_t color);
    void setBackgroundColor(uint16_t bgColor);
    void setFontSize(float size);
    void setFont(const lgfx::IFont *font);
    void setLineSpacing(int spacing);
    void setCharSpacing(int spacing);
    void setWrap(bool wrap);
    void setTransparentBg(bool transparent) { _transparentBg = transparent; }
    void setTransparentBackground(bool transparent) { _transparentBg = transparent; }
    void setIsCustomFont(bool isCustom) { _isCustomFont = isCustom; }

    // フォント読み込みメソッド
    bool loadFontFromArray(const uint8_t *fontArray);

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
};

#endif // _TYPO_WRITE_HPP_