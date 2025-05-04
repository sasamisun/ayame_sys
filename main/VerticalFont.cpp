// main/VerticalFont.cpp
#include "VerticalFont.hpp"
#include "esp_log.h"

static const char* TAG = "VerticalFont";

// コンストラクタ
VerticalFont::VerticalFont(M5GFX* display, const lgfx::IFont* font) 
    : _display(display), _font(font), _textColor(TFT_WHITE), _bgColor(TFT_BLACK), 
      _textSize(1.0f), _fillBg(false), _specialCharAdjust(true) {
    
    // フォントが指定されていない場合はデフォルトフォントを使用
    if (_font == nullptr && _display != nullptr) {
        _font = _display->getFont();
    }
}

// フォント設定
void VerticalFont::setFont(const lgfx::IFont* font) {
    _font = font;
}

// フォントの幅を取得する補助メソッド
int32_t VerticalFont::getFontWidth() const {
    if (!_font) return 8; // デフォルト値
    
    // M5GFXのFontMetricsを使って幅を取得
    lgfx::FontMetrics metrics;
    _font->getDefaultMetric(&metrics);
    return metrics.width; // または適切なメトリック値
}

// フォントの高さを取得する補助メソッド
int32_t VerticalFont::getFontHeight() const {
    if (!_font) return 16; // デフォルト値
    
    // M5GFXのFontMetricsを使って高さを取得
    lgfx::FontMetrics metrics;
    _font->getDefaultMetric(&metrics);
    return metrics.height; // または適切なメトリック値
}

// UTF-8文字列をUnicodeコードポイントに変換
std::vector<uint16_t> VerticalFont::utf8ToUnicode(const std::string& text) const {
    std::vector<uint16_t> unicode;
    
    for (size_t i = 0; i < text.length();) {
        uint16_t codepoint = 0;
        uint8_t c = static_cast<uint8_t>(text[i++]);
        
        // UTF-8デコード
        if (c < 0x80) {  // 1バイト文字
            codepoint = c;
        } else if ((c & 0xE0) == 0xC0) {  // 2バイト文字
            if (i < text.length()) {
                uint8_t c2 = static_cast<uint8_t>(text[i++]);
                codepoint = ((c & 0x1F) << 6) | (c2 & 0x3F);
            }
        } else if ((c & 0xF0) == 0xE0) {  // 3バイト文字
            if (i + 1 < text.length()) {
                uint8_t c2 = static_cast<uint8_t>(text[i++]);
                uint8_t c3 = static_cast<uint8_t>(text[i++]);
                codepoint = ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
            }
        } else if ((c & 0xF8) == 0xF0) {  // 4バイト文字（サロゲートペア）
            // 現在のM5GFXでは16bit Unicodeまでしかサポートしていないため、
            // サロゲートペアはスキップ
            i += 3;
            codepoint = 0xFFFD;  // 代替文字
        }
        
        unicode.push_back(codepoint);
    }
    
    return unicode;
}

// 特殊文字の判定
bool VerticalFont::isSpecialChar(uint16_t codepoint) const {
    // 句読点、括弧などの特殊文字の判定
    // 日本語の句読点
    if (codepoint == 0x3001 ||  // 、
        codepoint == 0x3002 ||  // 。
        codepoint == 0xFF0C ||  // ，
        codepoint == 0xFF0E ||  // ．
        codepoint == 0xFF01 ||  // ！
        codepoint == 0xFF1F) {  // ？
        return true;
    }
    
    // 日本語の括弧
    if ((codepoint >= 0x3008 && codepoint <= 0x300F) ||  // 〈〉《》「」『』
        (codepoint >= 0xFF08 && codepoint <= 0xFF09) ||  // （）
        (codepoint >= 0xFF3B && codepoint <= 0xFF3D) ||  // ［］｝
        (codepoint >= 0xFF5B && codepoint <= 0xFF5D)) {  // ｛｝
        return true;
    }
    
    return false;
}

// 特殊文字の描画調整
void VerticalFont::adjustSpecialChar(uint16_t codepoint, int32_t& x, int32_t& y) const {
    if (!_specialCharAdjust) return;
    
    float charWidth = getFontWidth() * _textSize;
    
    // 句読点は右寄せ
    if (codepoint == 0x3001 || codepoint == 0x3002 ||  // 、。
        codepoint == 0xFF0C || codepoint == 0xFF0E ||  // ，．
        codepoint == 0xFF01 || codepoint == 0xFF1F) {  // ！？
        x += charWidth / 4;
    }
    
    // 開き括弧は右寄せ
    if (codepoint == 0x3008 || codepoint == 0x300A ||  // 〈《
        codepoint == 0x300C || codepoint == 0x300E ||  // 「『
        codepoint == 0xFF08 || codepoint == 0xFF3B ||  // （［
        codepoint == 0xFF5B) {                          // ｛
        x += charWidth / 4;
    }
    
    // 閉じ括弧は左寄せ
    if (codepoint == 0x3009 || codepoint == 0x300B ||  // 〉》
        codepoint == 0x300D || codepoint == 0x300F ||  // 」』
        codepoint == 0xFF09 || codepoint == 0xFF3D ||  // ）］
        codepoint == 0xFF5D) {                          // ｝
        x -= charWidth / 4;
    }
}

// 1文字のみ縦書き描画
void VerticalFont::drawVerticalChar(uint16_t codepoint, int32_t x, int32_t y) {
    if (_display == nullptr || _font == nullptr) {
        ESP_LOGE(TAG, "Display or font not set");
        return;
    }
    
    // 文字のサイズ取得
    int32_t w = getFontWidth() * _textSize;
    int32_t h = getFontHeight() * _textSize;
    
    // 文字の回転点は中心
    float pivotX = x + w / 2.0f;
    float pivotY = y + h / 2.0f;
    
    // 一時的にピボット設定を保存
    float oldPivotX = _display->getPivotX();
    float oldPivotY = _display->getPivotY();
    
    // 特殊文字の位置調整
    int32_t adjustedX = x;
    int32_t adjustedY = y;
    if (_specialCharAdjust && isSpecialChar(codepoint)) {
        adjustSpecialChar(codepoint, adjustedX, adjustedY);
    }
    
    // ピボットを設定
    _display->setPivot(pivotX, pivotY);
    
    // 文字の描画（90度回転）
    _display->setTextSize(_textSize);
    
    // 背景色の設定
    if (_fillBg) {
        _display->setTextColor(_textColor, _bgColor);
    } else {
        _display->setTextColor(_textColor);
    }
    
    // 一時的に現在のフォントを保存
    const lgfx::IFont* oldFont = _display->getFont();
    _display->setFont(_font);
    
    // 文字の描画
    char16_t utf16Char = static_cast<char16_t>(codepoint);
    _display->drawChar(utf16Char, adjustedX, adjustedY, 3); // 3 = 270度回転
    
    // 設定を戻す
    _display->setFont(oldFont);
    _display->setPivot(oldPivotX, oldPivotY);
}

// 縦書きテキスト描画
void VerticalFont::drawVerticalText(const std::string& text, int32_t x, int32_t y) {
    if (_display == nullptr || _font == nullptr || text.empty()) {
        ESP_LOGE(TAG, "Display or font not set, or text is empty");
        return;
    }
    
    // テキストをUnicodeコードポイントに変換
    std::vector<uint16_t> unicode = utf8ToUnicode(text);
    
    // 文字の高さを取得
    int32_t charHeight = getFontHeight() * _textSize;
    
    // 文字を縦に描画
    int32_t currentY = y;
    for (uint16_t codepoint : unicode) {
        // 改行コードの処理
        if (codepoint == '\n') {
            // 縦書きの場合は左に移動
            x -= charHeight;
            currentY = y;
            continue;
        }
        
        // 文字の描画
        drawVerticalChar(codepoint, x, currentY);
        
        // 次の文字位置へ
        currentY += charHeight;
    }
}

// 縦書きテキスト描画（複数行）
void VerticalFont::drawVerticalTextColumns(const std::string& text, int32_t x, int32_t y, 
                                         int32_t columnSpacing, int maxColumns) {
    if (_display == nullptr || _font == nullptr || text.empty()) {
        ESP_LOGE(TAG, "Display or font not set, or text is empty");
        return;
    }
    
    // テキストをUnicodeコードポイントに変換
    std::vector<uint16_t> unicode = utf8ToUnicode(text);
    
    // 文字のサイズを取得
    int32_t charHeight = getFontHeight() * _textSize;
    int32_t charWidth = getFontWidth() * _textSize;
    
    // 画面の高さを取得
    int32_t screenHeight = _display->height();
    int32_t maxRowsPerColumn = (screenHeight - y) / charHeight;
    
    // カラム（列）単位の描画
    int32_t currentColumn = 0;
    int32_t currentRow = 0;
    int32_t currentX = x;
    int32_t currentY = y;
    
    for (uint16_t codepoint : unicode) {
        // 改行コードの処理
        if (codepoint == '\n') {
            currentRow = 0;
            currentColumn++;
            currentX = x - (currentColumn * (charWidth + columnSpacing));
            currentY = y;
            
            // 最大カラム数をチェック
            if (maxColumns > 0 && currentColumn >= maxColumns) {
                break;
            }
            
            continue;
        }
        
        // 1カラムの最大行数を超えた場合、次のカラムへ
        if (currentRow >= maxRowsPerColumn) {
            currentRow = 0;
            currentColumn++;
            currentX = x - (currentColumn * (charWidth + columnSpacing));
            currentY = y;
            
            // 最大カラム数をチェック
            if (maxColumns > 0 && currentColumn >= maxColumns) {
                break;
            }
        }
        
        // 文字の描画
        drawVerticalChar(codepoint, currentX, currentY);
        
        // 次の文字位置へ
        currentRow++;
        currentY += charHeight;
    }
}

// テキストの縦方向の長さを計算
int32_t VerticalFont::getVerticalTextHeight(const std::string& text) const {
    if (_font == nullptr || text.empty()) {
        return 0;
    }
    
    // テキストをUnicodeコードポイントに変換
    std::vector<uint16_t> unicode = utf8ToUnicode(text);
    
    // 改行を含まない最大行数を計算
    int32_t maxLines = 1;
    int32_t currentLines = 1;
    
    for (uint16_t codepoint : unicode) {
        if (codepoint == '\n') {
            currentLines = 1;
        } else {
            currentLines++;
            maxLines = std::max(maxLines, currentLines);
        }
    }
    
    // 縦方向の長さを計算
    return maxLines * getFontHeight() * _textSize;
}