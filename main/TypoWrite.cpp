// main/TypoWrite.cpp - エラー修正版
#include "TypoWrite.hpp"
#include "esp_log.h"
#include <cstring>

static const char *TAG = "TypoWrite";

// ========================================
// コンストラクタ・デストラクタ
// ========================================
TypoWrite::TypoWrite(M5GFX *display)
    : _display(display),
      _drawTarget(nullptr),
      _charSprite(nullptr),  // ヘッダーの宣言順序に合わせる
      _direction(TextDirection::HORIZONTAL),
      _alignment(TextAlignment::LEFT),
      _x(0), 
      _y(0),
      _width(100), 
      _height(100),
      _color(TFT_WHITE),
      _bgColor(TFT_BLACK),
      _transparentBg(false),  // _wrapより前に移動
      _wrap(true),
      _fontSize(1.0f),
      _font(&fonts::lgfxJapanGothic_16),
      _vlwFont(nullptr),
      _isCustomFont(false),
      _vlwParser(nullptr),  // _currentX/Yより前に移動
      _useVLWParser(false),
      _lineSpacing(2),
      _charSpacing(0),
      _columnSpacing(0),
      _currentX(0), 
      _currentY(0),
      _enableSmallCharHandling(true),
      _smallCharSettings(SmallCharSettings::getDefault())
{
    // マッピングテーブルの初期化
    initializeSmallCharMap();
    initializeVerticalGlyphMap();
    
    // 文字描画用スプライトを事前作成（再利用用）
    _charSprite = new lgfx::LGFX_Sprite(display);
    
    ESP_LOGI(TAG, "TypoWrite initialized with optimizations");
}

TypoWrite::~TypoWrite()
{
    if (_charSprite) {
        _charSprite->deleteSprite();
        delete _charSprite;
    }
    _vlwFont = nullptr;
    _drawTarget = nullptr;
    ESP_LOGI(TAG, "TypoWrite destroyed");
}

// ========================================
// マッピングテーブル初期化
// ========================================
void TypoWrite::initializeSmallCharMap()
{
    ESP_LOGI(TAG, "Building small character mapping table...");
    
    // ひらがな小文字マッピング
    _smallToLargeMap = {
        {0x3041, 0x3042},  // ぁ → あ
        {0x3043, 0x3044},  // ぃ → い
        {0x3045, 0x3046},  // ぅ → う
        {0x3047, 0x3048},  // ぇ → え
        {0x3049, 0x304A},  // ぉ → お
        {0x3063, 0x3064},  // っ → つ
        {0x3083, 0x3084},  // ゃ → や
        {0x3085, 0x3086},  // ゅ → ゆ
        {0x3087, 0x3088},  // ょ → よ
        {0x308E, 0x308F},  // ゎ → わ
        
        // カタカナ小文字マッピング
        {0x30A1, 0x30A2},  // ァ → ア
        {0x30A3, 0x30A4},  // ィ → イ
        {0x30A5, 0x30A6},  // ゥ → ウ
        {0x30A7, 0x30A8},  // ェ → エ
        {0x30A9, 0x30AA},  // ォ → オ
        {0x30C3, 0x30C4},  // ッ → ツ
        {0x30E3, 0x30E4},  // ャ → ヤ
        {0x30E5, 0x30E6},  // ュ → ユ
        {0x30E7, 0x30E8},  // ョ → ヨ
        {0x30EE, 0x30EF},  // ヮ → ワ
        {0x30F5, 0x30AB},  // ヵ → カ
        {0x30F6, 0x30B1}   // ヶ → ケ
    };
    
    ESP_LOGI(TAG, "Small character map: %d entries", _smallToLargeMap.size());
}

void TypoWrite::initializeVerticalGlyphMap()
{
    ESP_LOGI(TAG, "Building vertical glyph mapping table...");
    
    // 縦書き用グリフマッピング（unordered_map化）
    _verticalGlyphMap = {
        // 句読点
        {0x3001, 0xFE11},  // 、→ ︑
        {0x3002, 0xFE12},  // 。→ ︒
        
        // 日本語括弧
        {0x300C, 0xFE41},  // 「→ ﹁
        {0x300D, 0xFE42},  // 」→ ﹂
        {0x300E, 0xFE43},  // 『→ ﹃
        {0x300F, 0xFE44},  // 』→ ﹄
        
        // 半角括弧類
        {0x0028, 0xFE35},  // ( → ︵
        {0x0029, 0xFE36},  // ) → ︶
        {0x005B, 0xFE47},  // [ → ﹇
        {0x005D, 0xFE48},  // ] → ﹈
        {0x007B, 0xFE37},  // { → ︷
        {0x007D, 0xFE38},  // } → ︸
        
        // 山括弧類
        {0x3008, 0xFE3F},  // 〈→ ︿
        {0x3009, 0xFE40},  // 〉→ ﹀
        {0x300A, 0xFE3D},  // 《→ ︽
        {0x300B, 0xFE3E},  // 》→ ︾
        
        // その他括弧
        {0x3010, 0xFE3B},  // 【→ ︻
        {0x3011, 0xFE3C},  // 】→ ︼
        {0x3014, 0xFE39},  // 〔→ ︹
        {0x3015, 0xFE3A},  // 〕→ ︺
        
        // ダッシュ・区切り線類
        {0x2014, 0xFE31},  // — → ︱
        {0x2013, 0xFE32},  // – → ︲
        {0x2015, 0xFE31},  // ― → ︱
        {0x005F, 0xFE33},  // _ → ︳
        {0x2025, 0xFE30},  // ‥ → ︰
        {0x2026, 0xFE19},  // … → ︙
        
        // 全角ダッシュ・記号
        {0xFF0D, 0xFE32},  // － → ︲
        {0x30FC, 0xFE31}   // ー → ︱
    };
    
    ESP_LOGI(TAG, "Vertical glyph map: %d entries", _verticalGlyphMap.size());
}

// ========================================
// 統一メトリクス取得
// ========================================
CharMetrics TypoWrite::getCharMetrics(uint16_t unicode_char)
{
    // キャッシュチェック
    auto it = _metricsCache.find(unicode_char);
    if (it != _metricsCache.end()) {
        return it->second;
    }
    
    CharMetrics metrics;
    
    // VLWParser優先
    if (_useVLWParser && _vlwParser && _vlwParser->isInitialized()) {
        metrics = {
            static_cast<int32_t>(_vlwParser->getCharWidth(unicode_char) * _fontSize),
            static_cast<int32_t>(_vlwParser->getCharHeight(unicode_char) * _fontSize),
            static_cast<int32_t>(_vlwParser->getCharSetWidth(unicode_char) * _fontSize),
            0  // baseline（必要に応じて追加）
        };
    }
    else {
        // M5GFXフォールバック
        lgfx::FontMetrics fm;
        _display->setFont(_font);
        _display->setTextSize(_fontSize);
        _font->updateFontMetric(&fm, unicode_char);
        
        metrics = {
            static_cast<int32_t>(fm.x_advance * _fontSize),
            static_cast<int32_t>(fm.y_advance * _fontSize),
            static_cast<int32_t>(fm.x_advance * _fontSize),  // 送り幅
            static_cast<int32_t>(fm.baseline * _fontSize)
        };
    }
    
    // キャッシュに保存（よく使う文字のみ）
    if (_metricsCache.size() < 256) {  // キャッシュサイズ制限
        _metricsCache[unicode_char] = metrics;
    }
    
    return metrics;
}

// ========================================
// 統一文字描画関数（すべての描画を一元化）
// ========================================
void TypoWrite::drawUnifiedCharacter(uint16_t unicode_char, int x, int y, 
                                     float scale, float rotation,
                                     float offsetX, float offsetY)
{
    // 実際の描画位置を計算
    int draw_x = x + static_cast<int>(offsetX);
    int draw_y = y + static_cast<int>(offsetY);
    
    // スケール1.0かつ回転なしの場合は直接描画（最速パス）
    if (scale == 1.0f && rotation == 0.0f) {
        drawDirectCharacter(unicode_char, draw_x, draw_y);
        return;
    }
    
    // スケールまたは回転が必要な場合はスプライト描画
    drawSpriteCharacter(unicode_char, draw_x, draw_y, scale, rotation);
}

// 直接描画（最速パス）
void TypoWrite::drawDirectCharacter(uint16_t unicode_char, int x, int y)
{
    std::string utf8_char = unicodeToUtf8(unicode_char);
    
    // 描画先の選択（明示的なキャストを追加）
    lgfx::LovyanGFX* target = _drawTarget ? 
        static_cast<lgfx::LovyanGFX*>(_drawTarget) : 
        static_cast<lgfx::LovyanGFX*>(_display);
    
    // フォント設定
    if (_isCustomFont && _vlwFont) {
        target->loadFont(_vlwFont);
    } else if (_font) {
        target->setFont(_font);
    }
    
    target->setTextSize(_fontSize);
    target->setTextColor(_color, _bgColor);
    target->drawString(utf8_char.c_str(), _x + x, _y + y);
    
    if (_isCustomFont && _vlwFont) {
        target->unloadFont();
    }
}

// スプライトを使った描画（スケール・回転対応）
void TypoWrite::drawSpriteCharacter(uint16_t unicode_char, int x, int y,
                                    float scale, float rotation)
{
    CharMetrics metrics = getCharMetrics(unicode_char);
    
    // スプライトサイズを計算（余裕を持たせる）
    int sprite_width = static_cast<int>(metrics.width * scale + 20);
    int sprite_height = static_cast<int>(metrics.height * scale + 20);
    
    // 再利用スプライトのサイズ調整
    if (_charSprite->width() < sprite_width || _charSprite->height() < sprite_height) {
        _charSprite->deleteSprite();
        _charSprite->createSprite(sprite_width, sprite_height);
    }
    
    // 背景をクリア
    _charSprite->fillSprite(_transparentBg ? TFT_TRANSPARENT : _bgColor);
    
    // フォント設定と描画
    if (_isCustomFont && _vlwFont) {
        _charSprite->loadFont(_vlwFont);
    } else if (_font) {
        _charSprite->setFont(_font);
    }
    
    _charSprite->setTextSize(_fontSize);
    _charSprite->setTextColor(_color, _bgColor);
    
    std::string utf8_char = unicodeToUtf8(unicode_char);
    _charSprite->drawString(utf8_char.c_str(), 10, 10);
    
    if (_isCustomFont && _vlwFont) {
        _charSprite->unloadFont();
    }
    
    // 描画先を選択（明示的なキャストを追加）
    lgfx::LovyanGFX* target = _drawTarget ? 
        static_cast<lgfx::LovyanGFX*>(_drawTarget) : 
        static_cast<lgfx::LovyanGFX*>(_display);
    
    // 回転・スケール描画
    int center_x = _x + x + sprite_width / 2;
    int center_y = _y + y + sprite_height / 2;
    
    _charSprite->pushRotateZoom(target, center_x, center_y,
                                rotation, scale, scale,
                                _transparentBg ? TFT_TRANSPARENT : _bgColor);
}

// ========================================
// 横書きテキスト描画（簡略化版）
// ========================================
void TypoWrite::drawHorizontalText(const std::string &text)
{
    std::vector<uint16_t> unicode_chars = utf8ToUnicode(text);
    
    _currentX = 0;
    _currentY = 0;
    
    for (uint16_t ch : unicode_chars) {
        // 改行処理
        if (ch == '\n') {
            _currentX = 0;
            _currentY += getCharMetrics(0x3000).height + _lineSpacing;  // 全角スペース基準
            continue;
        }
        
        // 描画パラメータの決定
        float scale = 1.0f;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        uint16_t display_char = ch;
        
        // 小文字処理
        if (_enableSmallCharHandling && isSmallChar(ch)) {
            display_char = getCorrespondingLargeChar(ch);
            scale = _smallCharSettings.scale;
            offsetY = getCharMetrics(display_char).height * 0.3f;
        }
        
        CharMetrics metrics = getCharMetrics(display_char);
        
        // 折り返し処理
        if (_wrap && (_currentX + metrics.width > _width)) {
            _currentX = 0;
            _currentY += metrics.height + _lineSpacing;
        }
        
        // 描画範囲チェック
        if (_currentY + metrics.height > _height) {
            break;
        }
        
        // 統一描画関数を直接呼ぶ（コールスタック削減）
        drawUnifiedCharacter(display_char, _currentX, _currentY, 
                           scale, 0.0f, offsetX, offsetY);
        
        // 次の文字位置へ
        _currentX += static_cast<int>(metrics.setWidth * scale) + _charSpacing;
    }
}

// ========================================
// 縦書きテキスト描画（簡略化版）
// ========================================
void TypoWrite::drawVerticalText(const std::string &text)
{
    std::vector<uint16_t> unicode_chars = utf8ToUnicode(text);
    
    // 右端から開始
    int column_width = getCharMetrics(0x3000).height;
    _currentX = _width - column_width - 10;
    _currentY = 0;
    
    ESP_LOGI(TAG, "Starting vertical text at X=%d", _currentX);
    
    for (uint16_t ch : unicode_chars) {
        // 改行処理（左に移動）
        if (ch == '\n') {
            _currentX -= column_width + _columnSpacing + _lineSpacing;
            _currentY = 0;
            continue;
        }
        
        // 縦書き用グリフ変換
        uint16_t display_char = convertToVerticalGlyph(ch);
        
        // 描画パラメータの決定
        float scale = 1.1f;
        float rotation = 0.0f;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        
        // 小文字処理（最優先）
        if (_enableSmallCharHandling && isSmallChar(ch)) {
            display_char = getCorrespondingLargeChar(ch);
            scale = _smallCharSettings.scale;
            offsetX = getCharMetrics(display_char).width * _smallCharSettings.offsetX;
            offsetY = getCharMetrics(display_char).height * _smallCharSettings.offsetY;
        }
        // 回転が必要な文字
        else if (shouldRotateInVertical(display_char)) {
            rotation = 90.0f;
        }
        
        CharMetrics metrics = getCharMetrics(display_char);
        int char_height = (rotation != 0.0f) ? metrics.width : metrics.height;
        
        // 折り返し処理
        if (_wrap && (_currentY + char_height > _height)) {
            _currentX -= column_width + _columnSpacing + _lineSpacing;
            _currentY = 0;
        }
        
        // 描画範囲チェック
        if (_currentX < 0) {
            break;
        }
        
        // 統一描画関数を直接呼ぶ
        drawUnifiedCharacter(display_char, _currentX, _currentY,
                           scale, rotation, offsetX, offsetY);
        
        // 次の文字位置へ
        _currentY += static_cast<int>(char_height * scale) + _charSpacing;
    }
}

// ========================================
// その他の既存メソッド（必要最小限のみ実装）
// ========================================

// 文字変換ヘルパー
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
        }
        else if ((str[i] & 0xE0) == 0xC0) {
            // 2バイト文字
            unicode_char = ((str[i] & 0x1F) << 6) | (str[i + 1] & 0x3F);
            i += 2;
        }
        else if ((str[i] & 0xF0) == 0xE0) {
            // 3バイト文字
            unicode_char = ((str[i] & 0x0F) << 12) | ((str[i + 1] & 0x3F) << 6) | (str[i + 2] & 0x3F);
            i += 3;
        }
        else {
            // 4バイト文字は16ビットに収まらないのでスキップ
            i += 4;
            continue;
        }

        unicode_chars.push_back(unicode_char);
    }

    return unicode_chars;
}

std::string TypoWrite::unicodeToUtf8(uint16_t unicode_char)
{
    std::string utf8_str;

    if (unicode_char <= 0x7F) {
        // 1バイト文字
        utf8_str += (char)unicode_char;
    }
    else if (unicode_char <= 0x7FF) {
        // 2バイト文字
        utf8_str += (char)(0xC0 | (unicode_char >> 6));
        utf8_str += (char)(0x80 | (unicode_char & 0x3F));
    }
    else {
        // 3バイト文字
        utf8_str += (char)(0xE0 | (unicode_char >> 12));
        utf8_str += (char)(0x80 | ((unicode_char >> 6) & 0x3F));
        utf8_str += (char)(0x80 | (unicode_char & 0x3F));
    }

    return utf8_str;
}

// ヘルパー関数
uint16_t TypoWrite::convertToVerticalGlyph(uint16_t unicode_char)
{
    auto it = _verticalGlyphMap.find(unicode_char);
    if (it != _verticalGlyphMap.end()) {
        return it->second;
    }
    return unicode_char;  // 変換なし
}

bool TypoWrite::isSmallChar(uint16_t unicode_char) const
{
    return _smallToLargeMap.find(unicode_char) != _smallToLargeMap.end();
}

uint16_t TypoWrite::getCorrespondingLargeChar(uint16_t small_char) const
{
    auto it = _smallToLargeMap.find(small_char);
    if (it != _smallToLargeMap.end()) {
        return it->second;
    }
    return small_char;  // 見つからない場合は元の文字
}

bool TypoWrite::shouldRotateInVertical(uint16_t unicode_char)
{
    // 縦書き用グリフが存在する文字は回転させない
    if (_verticalGlyphMap.find(unicode_char) != _verticalGlyphMap.end()) {
        return false;
    }
    
    // ASCII文字（半角英数字）は回転
    if (unicode_char >= 0x0020 && unicode_char <= 0x007E) {
        return true;
    }
    
    // 半角カナも回転
    if (unicode_char >= 0xFF61 && unicode_char <= 0xFF9F) {
        return true;
    }
    
    return false;
}

// ========================================
// 公開設定メソッドの実装
// ========================================

// 描画先の設定
void TypoWrite::setDrawTarget(lgfx::LGFX_Sprite *sprite) 
{
    _drawTarget = sprite;
    ESP_LOGI(TAG, "Draw target set to %s", sprite ? "sprite" : "display");
}

// VLWパーサーの設定
void TypoWrite::setVLWParser(VLWFontParser* parser) 
{
    _vlwParser = parser;
    _useVLWParser = (parser != nullptr && parser->isInitialized());
    
    if (_useVLWParser) {
        ESP_LOGI(TAG, "VLW parser enabled for font metrics");
    } else {
        ESP_LOGI(TAG, "VLW parser disabled, using M5GFX metrics");
    }
}

// ========== 基本設定メソッド ==========

// テキスト方向の設定
void TypoWrite::setDirection(TextDirection direction)
{
    _direction = direction;
    ESP_LOGD(TAG, "Text direction set to %s", 
             direction == TextDirection::HORIZONTAL ? "HORIZONTAL" : "VERTICAL");
}

// テキスト揃えの設定
void TypoWrite::setAlignment(TextAlignment alignment)
{
    _alignment = alignment;
    ESP_LOGD(TAG, "Text alignment set");
}

// 描画位置の設定
void TypoWrite::setPosition(int x, int y)
{
    _x = x;
    _y = y;
    ESP_LOGD(TAG, "Position set to (%d, %d)", x, y);
}

// 描画領域の設定
void TypoWrite::setArea(int width, int height)
{
    _width = width;
    _height = height;
    ESP_LOGD(TAG, "Area set to %dx%d", width, height);
}

// テキスト色の設定
void TypoWrite::setColor(uint16_t color)
{
    _color = color;
    ESP_LOGD(TAG, "Text color set to 0x%04X", color);
}

// 背景色の設定
void TypoWrite::setBackgroundColor(uint16_t bgColor)
{
    _bgColor = bgColor;
    ESP_LOGD(TAG, "Background color set to 0x%04X", bgColor);
}

// 折り返しの設定
void TypoWrite::setWrap(bool wrap)
{
    _wrap = wrap;
    ESP_LOGD(TAG, "Text wrap set to %s", wrap ? "enabled" : "disabled");
}

// ========== フォント設定メソッド ==========

// フォントサイズの設定
void TypoWrite::setFontSize(float size)
{
    _fontSize = size;
    // メトリクスキャッシュをクリア（サイズが変わったため）
    _metricsCache.clear();
    ESP_LOGD(TAG, "Font size set to %.2f", size);
}

// フォントの設定
void TypoWrite::setFont(const lgfx::IFont *font)
{
    _font = font;
    _isCustomFont = false;
    _vlwFont = nullptr;
    // メトリクスキャッシュをクリア（フォントが変わったため）
    _metricsCache.clear();
    ESP_LOGD(TAG, "Font changed to built-in font");
}

// カスタムフォントの読み込み
bool TypoWrite::loadFontFromArray(const uint8_t *fontArray)
{
    if (!_display) {
        ESP_LOGE(TAG, "Display not initialized");
        return false;
    }

    // フォントを読み込み
    bool result = _display->loadFont(fontArray);
    if (result) {
        // 読み込んだフォントを現在のフォントとして設定
        _font = nullptr;  // VLWフォントの場合はIFontを使わない
        _isCustomFont = true;
        _vlwFont = fontArray;
        // メトリクスキャッシュをクリア
        _metricsCache.clear();
        ESP_LOGI(TAG, "Font loaded successfully from array");
    } else {
        ESP_LOGE(TAG, "Failed to load font from array");
    }

    return result;
}

// ========== スペーシング設定 ==========

// 行間の設定
void TypoWrite::setLineSpacing(int spacing)
{
    _lineSpacing = spacing;
    ESP_LOGD(TAG, "Line spacing set to %d pixels", spacing);
}

// 文字間の設定
void TypoWrite::setCharSpacing(int spacing)
{
    _charSpacing = spacing;
    ESP_LOGD(TAG, "Character spacing set to %d pixels", spacing);
}

// ========== メインテキスト描画メソッド ==========

// テキスト描画
void TypoWrite::drawText(const std::string &text)
{
    // クリッピング領域を設定
    lgfx::LovyanGFX* target = _drawTarget ? 
        static_cast<lgfx::LovyanGFX*>(_drawTarget) : 
        static_cast<lgfx::LovyanGFX*>(_display);
    
    target->setClipRect(_x, _y, _width, _height);
    
    // 背景をクリア（透明でない場合）
    if (!_transparentBg) {
        target->fillRect(_x, _y, _width, _height, _bgColor);
    }
    
    // テキスト描画
    if (_direction == TextDirection::HORIZONTAL) {
        drawHorizontalText(text);
    } else {
        drawVerticalText(text);
    }
    
    // クリッピング領域を解除
    target->clearClipRect();
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

// ========== サイズ計算メソッド ==========

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

// テキストサイズの計算（内部メソッド）
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
                CharMetrics metrics = getCharMetrics(ch);
                current_line_width += metrics.setWidth + _charSpacing;
            }
        }
        
        width = std::max(width, current_line_width);
        height = line_count * (getCharMetrics(0x3000).height + _lineSpacing) - _lineSpacing;
    } 
    else {  // VERTICAL
        int current_column_height = 0;
        int column_count = 1;
        int max_char_width = 0;
        
        for (uint16_t ch : unicode_chars) {
            if (ch == '\n') {
                height = std::max(height, current_column_height);
                current_column_height = 0;
                column_count++;
            } else {
                CharMetrics metrics = getCharMetrics(ch);
                int char_height = metrics.height;
                int char_width = metrics.width;
                
                // 回転する文字は幅と高さが入れ替わる
                if (shouldRotateInVertical(ch)) {
                    std::swap(char_width, char_height);
                }
                
                current_column_height += char_height + _charSpacing;
                max_char_width = std::max(max_char_width, char_width);
            }
        }
        
        height = std::max(height, current_column_height);
        // 列数 × (最大文字幅 + 列間隔)
        width = column_count * (max_char_width + _columnSpacing + _lineSpacing) - _lineSpacing;
    }
}

// ========== 描画領域管理 ==========

// 描画領域のクリア
void TypoWrite::clearArea(uint16_t color)
{
    lgfx::LovyanGFX* target = _drawTarget ? 
        static_cast<lgfx::LovyanGFX*>(_drawTarget) : 
        static_cast<lgfx::LovyanGFX*>(_display);
    
    target->fillRect(_x, _y, _width, _height, color);
}

// ========== 文字カテゴリ判定 ==========

CharCategory TypoWrite::getCharCategory(uint16_t unicode_char)
{
    // 最初に小文字判定をチェック（最優先）
    if (isSmallChar(unicode_char)) {
        return CharCategory::SMALL_CHAR;
    }
    
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

// ========== デバッグメソッド ==========

// 小文字マッピングテーブルのデバッグ表示
void TypoWrite::debugPrintSmallCharMap() const
{
    ESP_LOGI(TAG, "=== Small Character Mapping Table ===");
    ESP_LOGI(TAG, "Small char handling: %s", 
             _enableSmallCharHandling ? "ENABLED" : "DISABLED");
    ESP_LOGI(TAG, "Settings: scale=%.2f, offsetX=%.2f, offsetY=%.2f",
             _smallCharSettings.scale, 
             _smallCharSettings.offsetX, 
             _smallCharSettings.offsetY);
    
    ESP_LOGI(TAG, "Hiragana mappings:");
    for (const auto& pair : _smallToLargeMap) {
        if (pair.first >= 0x3041 && pair.first <= 0x308E) {
            ESP_LOGI(TAG, "  U+%04X -> U+%04X", pair.first, pair.second);
        }
    }
    
    ESP_LOGI(TAG, "Katakana mappings:");
    for (const auto& pair : _smallToLargeMap) {
        if (pair.first >= 0x30A1 && pair.first <= 0x30F6) {
            ESP_LOGI(TAG, "  U+%04X -> U+%04X", pair.first, pair.second);
        }
    }
    
    ESP_LOGI(TAG, "Total: %d mappings", _smallToLargeMap.size());
    ESP_LOGI(TAG, "=====================================");
}

// 文字列内の小文字分析
void TypoWrite::debugAnalyzeSmallChars(const std::string& text)
{
    std::vector<uint16_t> unicode_chars = utf8ToUnicode(text);
    
    ESP_LOGI(TAG, "=== Small Character Analysis ===");
    ESP_LOGI(TAG, "Text: %s", text.c_str());
    
    int small_count = 0;
    for (uint16_t ch : unicode_chars) {
        if (isSmallChar(ch)) {
            uint16_t large = getCorrespondingLargeChar(ch);
            std::string char_str = unicodeToUtf8(ch);
            std::string large_str = unicodeToUtf8(large);
            ESP_LOGI(TAG, "  Small char found: %s (U+%04X) -> %s (U+%04X)", 
                     char_str.c_str(), ch, large_str.c_str(), large);
            small_count++;
        }
    }
    
    ESP_LOGI(TAG, "Total small characters: %d", small_count);
    ESP_LOGI(TAG, "================================");
}

// 縦書きグリフマッピングのデバッグ表示
void TypoWrite::debugPrintVerticalGlyphMap() const
{
    ESP_LOGI(TAG, "=== Vertical Glyph Mapping Table ===");
    ESP_LOGI(TAG, "Total mappings: %d", _verticalGlyphMap.size());
    
    ESP_LOGI(TAG, "Punctuation mappings:");
    for (const auto& pair : _verticalGlyphMap) {
        if (pair.first == 0x3001 || pair.first == 0x3002) {
            ESP_LOGI(TAG, "  U+%04X -> U+%04X", pair.first, pair.second);
        }
    }
    
    ESP_LOGI(TAG, "Bracket mappings:");
    for (const auto& pair : _verticalGlyphMap) {
        if ((pair.first >= 0x3008 && pair.first <= 0x3015) ||
            (pair.first >= 0x0028 && pair.first <= 0x0029)) {
            ESP_LOGI(TAG, "  U+%04X -> U+%04X", pair.first, pair.second);
        }
    }
    
    ESP_LOGI(TAG, "=====================================");
}

// メトリクスキャッシュのデバッグ表示
void TypoWrite::debugPrintMetricsCache() const
{
    ESP_LOGI(TAG, "=== Metrics Cache Status ===");
    ESP_LOGI(TAG, "Cached characters: %d", _metricsCache.size());
    
    if (!_metricsCache.empty()) {
        ESP_LOGI(TAG, "Sample cached metrics:");
        int count = 0;
        for (const auto& pair : _metricsCache) {
            if (count++ >= 5) break;  // 最初の5個だけ表示
            ESP_LOGI(TAG, "  U+%04X: w=%ld, h=%ld, sw=%ld", 
                     pair.first, 
                     pair.second.width, 
                     pair.second.height, 
                     pair.second.setWidth);
        }
    }
    
    ESP_LOGI(TAG, "============================");
}