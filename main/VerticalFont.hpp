// main/VerticalFont.hpp
#ifndef _VERTICAL_FONT_HPP_
#define _VERTICAL_FONT_HPP_

#include <M5GFX.h>
#include <string>
#include <vector>

class VerticalFont {
private:
    M5GFX* _display;                  // 描画先のディスプレイ
    const lgfx::IFont* _font;         // 使用するフォント
    uint32_t _textColor;              // テキスト色
    uint32_t _bgColor;                // 背景色
    float _textSize;                  // テキストサイズ
    bool _fillBg;                     // 背景を塗りつぶすかどうか
    bool _specialCharAdjust;          // 特殊文字（句読点など）の位置調整をするか
    
    // 特殊文字の判定
    bool isSpecialChar(uint16_t codepoint) const;
    
    // 特殊文字の描画調整を行う
    void adjustSpecialChar(uint16_t codepoint, int32_t& x, int32_t& y) const;
    
    // UTF-8文字列をUnicodeコードポイントに変換
    std::vector<uint16_t> utf8ToUnicode(const std::string& text) const;
    
    // フォントの幅と高さを取得する補助メソッド
    int32_t getFontWidth() const;
    int32_t getFontHeight() const;
    
public:
    VerticalFont(M5GFX* display, const lgfx::IFont* font = nullptr);
    ~VerticalFont() = default;
    
    // フォント設定
    void setFont(const lgfx::IFont* font);
    const lgfx::IFont* getFont() const { return _font; }
    
    // 色設定
    void setTextColor(uint32_t color) { _textColor = color; _fillBg = false; }
    void setTextColor(uint32_t fgcolor, uint32_t bgcolor) { 
        _textColor = fgcolor; 
        _bgColor = bgcolor;
        _fillBg = true; 
    }
    uint32_t getTextColor() const { return _textColor; }
    
    // テキストサイズ設定
    void setTextSize(float size) { _textSize = size; }
    float getTextSize() const { return _textSize; }
    
    // 特殊文字調整の設定
    void setSpecialCharAdjust(bool adjust) { _specialCharAdjust = adjust; }
    bool getSpecialCharAdjust() const { return _specialCharAdjust; }
    
    // 縦書きテキスト描画
    void drawVerticalText(const std::string& text, int32_t x, int32_t y);
    
    // 縦書きテキスト描画（複数行）
    void drawVerticalTextColumns(const std::string& text, int32_t x, int32_t y, 
                                int32_t columnSpacing, int maxColumns = 0);
    
    // テキストの縦方向の長さを計算
    int32_t getVerticalTextHeight(const std::string& text) const;
    
    // 1文字のみ描画
    void drawVerticalChar(uint16_t codepoint, int32_t x, int32_t y);
};

#endif // _VERTICAL_FONT_HPP_