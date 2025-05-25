// main/TypoWrite.cpp
#include "TypoWrite.hpp"
#include "esp_log.h"
#include <cstring>

// ログタグ
static const char* TAG = "TypoWrite";

// コンストラクタ
TypoWrite::TypoWrite(M5GFX *display)
    : _display(display),
      _sprite(nullptr),
      _charSprite(nullptr),
      _spriteInitialized(false),
      _direction(TextDirection::HORIZONTAL),
      _alignment(TextAlignment::LEFT),
      _x(0),
      _y(0),
      _width(100),
      _height(100),
      _color(TFT_WHITE),
      _bgColor(TFT_BLACK),
      _fontSize(1.0f),
      _font(&fonts::lgfxJapanGothic_16),
      _isCustomFont(false),
      _lineSpacing(2),
      _charSpacing(0),
      _wrap(true),
      _transparentBg(false),
      _columnSpacing(0),
      _charSpriteWidth(64),
      _charSpriteHeight(64),
      _currentX(0),
      _currentY(0)
{
    ESP_LOGI(TAG, "TypoWrite initialized");
}

// デストラクタ
TypoWrite::~TypoWrite()
{
    // スプライトの解放
    if (_charSprite) {
        _charSprite->deleteSprite();
        delete _charSprite;
        _charSprite = nullptr;
    }
    
    if (_sprite) {
        _sprite->deleteSprite();
        delete _sprite;
        _sprite = nullptr;
    }
    
    ESP_LOGI(TAG, "TypoWrite destroyed");
}

// メインスプライトの初期化
bool TypoWrite::initMainSprite()
{
    // 既存のスプライトがあれば削除
    if (_sprite) {
        _sprite->deleteSprite();
        delete _sprite;
    }
    
    // 新しいスプライトを作成
    _sprite = new lgfx::LGFX_Sprite(_display);
    if (!_sprite) {
        ESP_LOGE(TAG, "Failed to allocate sprite");
        return false;
    }
    
    // スプライトのバッファを作成
    _sprite->setColorDepth(_display->getColorDepth());
    if (!_sprite->createSprite(_width, _height)) {
        ESP_LOGE(TAG, "Failed to create sprite buffer");
        delete _sprite;
        _sprite = nullptr;
        return false;
    }
    
    // 背景をクリア
    clearMainSprite();
    
    ESP_LOGI(TAG, "Main sprite initialized: %dx%d", _width, _height);
    return true;
}

// 一文字用スプライトの初期化
bool TypoWrite::initCharSprite()
{
    // 既存のスプライトがあれば削除
    if (_charSprite) {
        _charSprite->deleteSprite();
        delete _charSprite;
    }
    
    // 新しいスプライトを作成
    _charSprite = new lgfx::LGFX_Sprite(_display);
    if (!_charSprite) {
        ESP_LOGE(TAG, "Failed to allocate char sprite");
        return false;
    }
    
    // スプライトのバッファを作成
    _charSprite->setColorDepth(_display->getColorDepth());
    if (!_charSprite->createSprite(_charSpriteWidth, _charSpriteHeight)) {
        ESP_LOGE(TAG, "Failed to create char sprite buffer");
        delete _charSprite;
        _charSprite = nullptr;
        return false;
    }
    
    ESP_LOGI(TAG, "Char sprite initialized: %dx%d", _charSpriteWidth, _charSpriteHeight);
    return true;
}

// メインスプライトのクリア
void TypoWrite::clearMainSprite()
{
    if (_sprite) {
        if (_transparentBg) {
            _sprite->fillSprite(TFT_TRANSPARENT);
        } else {
            _sprite->fillSprite(_bgColor);
        }
    }
}

// 一文字用スプライトのクリア
void TypoWrite::clearCharSprite()
{
    if (_charSprite) {
        if (_transparentBg) {
            _charSprite->fillSprite(TFT_TRANSPARENT);
        } else {
            _charSprite->fillSprite(_bgColor);
        }
    }
}

// UTF-8文字列をUnicodeコードポイントの配列に変換
std::vector<uint16_t> TypoWrite::utf8ToUnicode(const std::string &utf8_string)
{
    std::vector<uint16_t> unicode_chars;
    const uint8_t *str = (const uint8_t *)utf8_string.c_str();
    size_t len = utf8_string.length();
    size_t i = 0;

    while (i < len) {
        uint16_t unicode_char = 0;
        
        if ((str[i] & 0x80) == 0) {
            // 1バイト文字 (ASCII)
            unicode_char = str[i];
            i++;
        } else if ((str[i] & 0xE0) == 0xC0) {
            // 2バイト文字
            unicode_char = ((str[i] & 0x1F) << 6) | (str[i + 1] & 0x3F);
            i += 2;
        } else if ((str[i] & 0xF0) == 0xE0) {
            // 3バイト文字
            unicode_char = ((str[i] & 0x0F) << 12) | ((str[i + 1] & 0x3F) << 6) | (str[i + 2] & 0x3F);
            i += 3;
        } else {
            // 4バイト文字は16ビットに収まらないのでスキップ
            i += 4;
            continue;
        }
        
        unicode_chars.push_back(unicode_char);
    }
    
    return unicode_chars;
}

// UnicodeコードポイントをUTF-8文字列に変換
std::string TypoWrite::unicodeToUtf8(uint16_t unicode_char)
{
    std::string utf8_str;
    
    if (unicode_char <= 0x7F) {
        // 1バイト文字
        utf8_str += (char)unicode_char;
    } else if (unicode_char <= 0x7FF) {
        // 2バイト文字
        utf8_str += (char)(0xC0 | (unicode_char >> 6));
        utf8_str += (char)(0x80 | (unicode_char & 0x3F));
    } else {
        // 3バイト文字
        utf8_str += (char)(0xE0 | (unicode_char >> 12));
        utf8_str += (char)(0x80 | ((unicode_char >> 6) & 0x3F));
        utf8_str += (char)(0x80 | (unicode_char & 0x3F));
    }
    
    return utf8_str;
}

// 縦書き用グリフ変換
uint16_t convertToVerticalGlyph(uint16_t unicode_char)
{
    uint16_t vertical_code = unicode_char;
    
    switch (unicode_char) {
        // 句読点
        case 0x3001: // 、（読点）
            vertical_code = 0xFE11;
            break;
        case 0x3002: // 。（句点）
            vertical_code = 0xFE12;
            break;

        // 日本語の括弧
        case 0x300C: // 「
            vertical_code = 0xFE41;
            break;
        case 0x300D: // 」
            vertical_code = 0xFE42;
            break;
        case 0x300E: // 『
            vertical_code = 0xFE43;
            break;
        case 0x300F: // 』
            vertical_code = 0xFE44;
            break;

        // 半角括弧類
        case 0x0028: // (
            vertical_code = 0xFE35;
            break;
        case 0x0029: // )
            vertical_code = 0xFE36;
            break;
        case 0x005B: // [
            vertical_code = 0xFE47;
            break;
        case 0x005D: // ]
            vertical_code = 0xFE48;
            break;
        case 0x007B: // {
            vertical_code = 0xFE37;
            break;
        case 0x007D: // }
            vertical_code = 0xFE38;
            break;

        // 山括弧類
        case 0x3008: // 〈
            vertical_code = 0xFE3F;
            break;
        case 0x3009: // 〉
            vertical_code = 0xFE40;
            break;
        case 0x300A: // 《
            vertical_code = 0xFE3D;
            break;
        case 0x300B: // 》
            vertical_code = 0xFE3E;
            break;

        // その他の括弧
        case 0x3010: // 【
            vertical_code = 0xFE3B;
            break;
        case 0x3011: // 】
            vertical_code = 0xFE3C;
            break;
        case 0x3014: // 〔 (亀甲括弧)
            vertical_code = 0xFE39;
            break;
        case 0x3015: // 〕
            vertical_code = 0xFE3A;
            break;

        // ダッシュ・区切り線類
        case 0x2014: // —（EMダッシュ）
            vertical_code = 0xFE31;
            break;
        case 0x2013: // –（ENダッシュ）
            vertical_code = 0xFE32;
            break;
        case 0x2015: // ―（水平バー）
            vertical_code = 0xFE31; // EMダッシュの縦書き版に変換
            break;
        case 0x005F: // _（アンダースコア）
            vertical_code = 0xFE33;
            break;
        case 0x2025: // ‥（2ドットリーダー）
            vertical_code = 0xFE30;
            break;
        case 0x2026: // …
            vertical_code = 0xFE19;
            break;

        // 全角ダッシュ・記号
        case 0xFF0D: // －（全角ハイフンマイナス）
            vertical_code = 0xFE32; // ENダッシュの縦書き版に変換
            break;
        case 0x30FC: // ー（長音記号）
            vertical_code = 0xFE31; // EMダッシュの縦書き版に変換（縦棒になる）
            break;

        default:
            break;
    }
    
    return vertical_code;
}

// 縦書きで回転が必要な文字かどうかを判定
bool TypoWrite::shouldRotateInVertical(uint16_t unicode_char)
{
    // 縦書き用グリフが存在する文字は回転させない
    if (convertToVerticalGlyph(unicode_char) != unicode_char) {
        return false;
    }
    
    // ASCII文字（半角英数字）は回転させる
    if (unicode_char >= 0x0020 && unicode_char <= 0x007E) {
        return true;
    }
    
    // その他の半角カナなども回転させる
    if (unicode_char >= 0xFF61 && unicode_char <= 0xFF9F) {
        return true;
    }
    
    return false;
}

// 文字カテゴリの判定
CharCategory TypoWrite::getCharCategory(uint16_t unicode_char)
{
    // 句読点
    if (unicode_char == 0x3001 || unicode_char == 0x3002) {
        return CharCategory::PUNCTUATION;
    }
    
    // 括弧類
    if ((unicode_char >= 0x3008 && unicode_char <= 0x3011) ||
        (unicode_char >= 0x300C && unicode_char <= 0x300F) ||
        unicode_char == 0x0028 || unicode_char == 0x0029 ||
        unicode_char == 0x005B || unicode_char == 0x005D ||
        unicode_char == 0x007B || unicode_char == 0x007D) {
        return CharCategory::BRACKET;
    }
    
    // 横棒・長音記号類
    if (unicode_char == 0x30FC || unicode_char == 0x2014 || 
        unicode_char == 0x2013 || unicode_char == 0x2015 ||
        unicode_char == 0xFF0D || unicode_char == 0x005F) {
        return CharCategory::HORIZONTAL_BAR;
    }
    
    return CharCategory::NORMAL;
}

// 文字の幅を取得
int32_t TypoWrite::getCharacterWidth(uint16_t unicode_char)
{
    if (!_font) return 0;
    
    std::string utf8_char = unicodeToUtf8(unicode_char);
    
    // フォントサイズを一時的に設定
    _display->setFont(_font);
    _display->setTextSize(_fontSize);
    
    // テキスト幅を取得
    int32_t width = _display->textWidth(utf8_char.c_str());
    
    return width;
}

// 文字の高さを取得
int32_t TypoWrite::getCharacterHeight(uint16_t unicode_char)
{
    if (!_font) return 0;
    
    // フォントサイズを一時的に設定
    _display->setFont(_font);
    _display->setTextSize(_fontSize);
    
    // フォントの高さを取得
    return _display->fontHeight();
}

// フォントの幅を取得
int32_t TypoWrite::getFontWidth()
{
    if (!_font) return 0;
    
    _display->setFont(_font);
    _display->setTextSize(_fontSize);
    
    return _display->fontWidth();
}

// フォントの高さを取得
int32_t TypoWrite::getFontHeight()
{
    if (!_font) return 0;
    
    _display->setFont(_font);
    _display->setTextSize(_fontSize);
    
    return _display->fontHeight();
}

// 横書き用の一文字描画
void TypoWrite::drawCharacterHorizontal(uint16_t unicode_char, int x, int y)
{
    if (!_charSprite || !_sprite) return;
    
    // 一文字用スプライトをクリア
    clearCharSprite();
    
    // フォント設定
    _charSprite->setFont(_font);
    _charSprite->setTextSize(_fontSize);
    _charSprite->setTextColor(_color, _bgColor);
    
    // UTF-8に変換して描画
    std::string utf8_char = unicodeToUtf8(unicode_char);
    _charSprite->drawString(utf8_char.c_str(), 0, 0);
    
    // メインスプライトに転写
    _charSprite->pushSprite(_sprite, x, y, _transparentBg ? TFT_TRANSPARENT : _bgColor);
}

// 縦書き用の一文字描画
void TypoWrite::drawCharacterVertical(uint16_t unicode_char, int x, int y)
{
    if (!_charSprite || !_sprite) return;
    
    // 縦書き用グリフに変換
    uint16_t display_char = convertToVerticalGlyph(unicode_char);
    
    // 一文字用スプライトをクリア
    clearCharSprite();
    
    // フォント設定
    _charSprite->setFont(_font);
    _charSprite->setTextSize(_fontSize);
    _charSprite->setTextColor(_color, _bgColor);
    
    // 回転が必要かチェック
    if (shouldRotateInVertical(unicode_char)) {
        // 90度回転して描画
        drawRotatedCharacter(display_char, x, y);
    } else {
        // そのまま描画
        std::string utf8_char = unicodeToUtf8(display_char);
        _charSprite->drawString(utf8_char.c_str(), 0, 0);
        
        // メインスプライトに転写
        _charSprite->pushSprite(_sprite, x, y, _transparentBg ? TFT_TRANSPARENT : _bgColor);
    }
}

// 回転した文字の描画
void TypoWrite::drawRotatedCharacter(uint16_t unicode_char, int x, int y)
{
    if (!_charSprite || !_sprite) return;
    
    // 一時的なスプライトを作成（回転用）
    lgfx::LGFX_Sprite tempSprite(_display);
    tempSprite.setColorDepth(_display->getColorDepth());
    tempSprite.createSprite(_charSpriteHeight, _charSpriteWidth);
    
    // 背景をクリア
    if (_transparentBg) {
        tempSprite.fillSprite(TFT_TRANSPARENT);
    } else {
        tempSprite.fillSprite(_bgColor);
    }
    
    // フォント設定
    tempSprite.setFont(_font);
    tempSprite.setTextSize(_fontSize);
    tempSprite.setTextColor(_color, _bgColor);
    
    // 文字を描画
    std::string utf8_char = unicodeToUtf8(unicode_char);
    tempSprite.drawString(utf8_char.c_str(), 0, 0);
    
    // 90度回転してメインスプライトに描画
    tempSprite.setPivot(tempSprite.width() / 2, tempSprite.height() / 2);
    tempSprite.pushRotateZoom(_sprite, x + _charSpriteWidth / 2, y + _charSpriteHeight / 2, 
                           90, 1.0, 1.0, _transparentBg ? TFT_TRANSPARENT : _bgColor);
    
    // 一時スプライトを削除
    tempSprite.deleteSprite();
}

// 横書きテキストの描画
void TypoWrite::drawHorizontalText(const std::string &text)
{
    // UTF-8をUnicodeに変換
    std::vector<uint16_t> unicode_chars = utf8ToUnicode(text);
    
    _currentX = 0;
    _currentY = 0;
    
    for (uint16_t ch : unicode_chars) {
        // 改行処理
        if (ch == '\n') {
            _currentX = 0;
            _currentY += getFontHeight() + _lineSpacing;
            continue;
        }
        
        // 文字の幅を取得
        int char_width = getCharacterWidth(ch);
        
        // 折り返し処理
        if (_wrap && (_currentX + char_width > _width)) {
            _currentX = 0;
            _currentY += getFontHeight() + _lineSpacing;
        }
        
        // 描画範囲チェック
        if (_currentY + getFontHeight() > _height) {
            break;
        }
        
        // 文字を描画
        drawCharacterHorizontal(ch, _currentX, _currentY);
        
        // 次の文字位置へ
        _currentX += char_width + _charSpacing;
    }
}

// 縦書きテキストの描画
void TypoWrite::drawVerticalText(const std::string &text)
{
    // UTF-8をUnicodeに変換
    std::vector<uint16_t> unicode_chars = utf8ToUnicode(text);
    
    _currentX = _width - getFontWidth(); // 右から開始
    _currentY = 0;
    
    for (uint16_t ch : unicode_chars) {
        // 改行処理（縦書きでは左に移動）
        if (ch == '\n') {
            _currentX -= getFontWidth() + _columnSpacing + _lineSpacing;
            _currentY = 0;
            continue;
        }
        
        // 文字の高さを取得
        int char_height = getFontHeight();
        
        // 折り返し処理
        if (_wrap && (_currentY + char_height > _height)) {
            _currentX -= getFontWidth() + _columnSpacing + _lineSpacing;
            _currentY = 0;
        }
        
        // 描画範囲チェック
        if (_currentX < 0) {
            break;
        }
        
        // 文字を描画
        drawCharacterVertical(ch, _currentX, _currentY);
        
        // 次の文字位置へ
        _currentY += char_height + _charSpacing;
    }
}

// テキストサイズの計算
void TypoWrite::calculateTextSize(const std::string &text, int &width, int &height)
{
    std::vector<uint16_t> unicode_chars = utf8ToUnicode(text);
    
    width = 0;
    height = 0;
    
    if (_direction == TextDirection::HORIZONTAL) {
        int current_line_width = 0;
        int line_count = 1;
        
        for (uint16_t ch : unicode_chars) {
            if (ch == '\n') {
                width = std::max(width, current_line_width);
                current_line_width = 0;
                line_count++;
            } else {
                current_line_width += getCharacterWidth(ch) + _charSpacing;
            }
        }
        
        width = std::max(width, current_line_width);
        height = line_count * (getFontHeight() + _lineSpacing) - _lineSpacing;
        
    } else { // VERTICAL
        int current_column_height = 0;
        int column_count = 1;
        
        for (uint16_t ch : unicode_chars) {
            if (ch == '\n') {
                height = std::max(height, current_column_height);
                current_column_height = 0;
                column_count++;
            } else {
                current_column_height += getFontHeight() + _charSpacing;
            }
        }
        
        height = std::max(height, current_column_height);
        width = column_count * (getFontWidth() + _columnSpacing + _lineSpacing) - _lineSpacing;
    }
}

// 設定メソッドの実装
void TypoWrite::setDirection(TextDirection direction)
{
    _direction = direction;
}

void TypoWrite::setAlignment(TextAlignment alignment)
{
    _alignment = alignment;
}

void TypoWrite::setPosition(int x, int y)
{
    _x = x;
    _y = y;
}

void TypoWrite::setArea(int width, int height)
{
    _width = width;
    _height = height;
    _spriteInitialized = false; // スプライトの再初期化が必要
}

void TypoWrite::setColor(uint16_t color)
{
    _color = color;
}

void TypoWrite::setBackgroundColor(uint16_t bgColor)
{
    _bgColor = bgColor;
}

void TypoWrite::setFontSize(float size)
{
    _fontSize = size;
}

void TypoWrite::setFont(const lgfx::IFont *font)
{
    _font = font;
    _isCustomFont = false;
}

void TypoWrite::setLineSpacing(int spacing)
{
    _lineSpacing = spacing;
}

void TypoWrite::setCharSpacing(int spacing)
{
    _charSpacing = spacing;
}

void TypoWrite::setWrap(bool wrap)
{
    _wrap = wrap;
}

// カスタムフォントの読み込み
bool TypoWrite::loadFontFromArray(const uint8_t *fontArray)
{
    if (!_display) return false;
    
    // フォントを読み込み
    bool result = _display->loadFont(fontArray);
    if (result)
    {
        // 読み込んだフォントを現在のフォントとして設定
        _font = _display->getFont();
        _isCustomFont = true; // カスタムフォントとして設定
        ESP_LOGI(TAG, "Font loaded successfully from array");
    }
    
    ESP_LOGE(TAG, "Failed to load custom font");
    return result;
}

// テキスト描画
void TypoWrite::drawText(const std::string &text)
{
    // スプライトが初期化されていなければ初期化
    if (!_spriteInitialized) {
        if (!initMainSprite() || !initCharSprite()) {
            ESP_LOGE(TAG, "Failed to initialize sprites");
            return;
        }
        _spriteInitialized = true;
    }
    
    // スプライトをクリア
    clearMainSprite();
    
    // テキスト描画
    if (_direction == TextDirection::HORIZONTAL) {
        drawHorizontalText(text);
    } else {
        drawVerticalText(text);
    }
    
    // 画面に表示
    updateDisplay();
}

// 中央揃えでテキスト描画
void TypoWrite::drawTextCentered(const std::string &text)
{
    // テキストサイズを計算
    int text_width, text_height;
    calculateTextSize(text, text_width, text_height);
    
    // 元の位置を保存
    int original_x = _x;
    int original_y = _y;
    TextAlignment original_alignment = _alignment;
    
    // 中央揃えに設定
    _alignment = TextAlignment::CENTER;
    
    // 中央位置を計算
    if (_direction == TextDirection::HORIZONTAL) {
        _x = original_x + (_width - text_width) / 2;
        _y = original_y + (_height - text_height) / 2;
    } else {
        _x = original_x + (_width - text_width) / 2;
        _y = original_y + (_height - text_height) / 2;
    }
    
    // テキスト描画
    drawText(text);
    
    // 設定を元に戻す
    _x = original_x;
    _y = original_y;
    _alignment = original_alignment;
}

// テキスト幅の取得
int TypoWrite::getTextWidth(const std::string &text)
{
    int width, height;
    calculateTextSize(text, width, height);
    return width;
}

// テキスト高さの取得
int TypoWrite::getTextHeight(const std::string &text)
{
    int width, height;
    calculateTextSize(text, width, height);
    return height;
}

// スプライトのクリア
void TypoWrite::clearSprite(uint16_t color)
{
    if (_sprite) {
        _sprite->fillSprite(color);
    }
}

// 画面への表示
void TypoWrite::updateDisplay()
{
    if (_sprite && _display) {
        _sprite->pushSprite(_display, _x, _y, _transparentBg ? TFT_TRANSPARENT : _bgColor);
    }
}

// メトリクス情報の更新
bool TypoWrite::updateMetricsForChar(uint16_t unicode_char) const
{
    if (!_font || !_display) return false;
    
    //std::string utf8_char = unicodeToUtf8(unicode_char);
    
    _display->setFont(_font);
    _display->setTextSize(_fontSize);
    
    return _font->updateFontMetric(&_metrics, unicode_char);
}

