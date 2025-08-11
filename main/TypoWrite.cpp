// main/TypoWrite.cpp
#include "TypoWrite.hpp"
#include "esp_log.h"
#include <cstring>

// ログタグ
static const char *TAG = "TypoWrite";

// コンストラクタ
TypoWrite::TypoWrite(M5GFX *display)
    : _display(display),
      _drawTarget(nullptr), // 描画先（nullptrの場合は_displayに描画）
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
      _vlwFont(nullptr),
      _isCustomFont(false),
      _lineSpacing(2),
      _charSpacing(0),
      _wrap(true),
      _transparentBg(false),
      _columnSpacing(0),
      _currentX(0),
      _currentY(0),
      _vlwParser(nullptr),
      _useVLWParser(false),
      _enableSmallCharHandling(true),
      _smallCharSettings(SmallCharSettings::getDefault())
{
    initializeSmallCharMap();
    ESP_LOGI(TAG, "TypoWrite initialized");
}

// デストラクタ
TypoWrite::~TypoWrite()
{
    _vlwFont = nullptr;
    _drawTarget = nullptr;
    ESP_LOGI(TAG, "TypoWrite destroyed");
}

// 描画先スプライトの設定
void TypoWrite::setDrawTarget(lgfx::LGFX_Sprite *sprite)
{
    _drawTarget = sprite;
    ESP_LOGI(TAG, "Draw target set to %s", sprite ? "sprite" : "display");
}

void TypoWrite::setVLWParser(VLWFontParser* parser) {
    _vlwParser = parser;
    _useVLWParser = (parser != nullptr && parser->isInitialized());
    
    if (_useVLWParser) {
        ESP_LOGI(TAG, "VLW parser enabled for font metrics");
    } else {
        ESP_LOGI(TAG, "VLW parser disabled, using M5GFX metrics");
    }
}

// === 小文字マッピングテーブル構築 ===
void TypoWrite::initializeSmallCharMap()
{
    ESP_LOGI(TAG, "Building small character mapping table...");
    
    // 🔹 ひらがな小文字マッピング
    _smallToLargeMap[0x3041] = 0x3042;  // ぁ → あ
    _smallToLargeMap[0x3043] = 0x3044;  // ぃ → い
    _smallToLargeMap[0x3045] = 0x3046;  // ぅ → う
    _smallToLargeMap[0x3047] = 0x3048;  // ぇ → え
    _smallToLargeMap[0x3049] = 0x304A;  // ぉ → お
    _smallToLargeMap[0x3063] = 0x3064;  // っ → つ
    _smallToLargeMap[0x3083] = 0x3084;  // ゃ → や
    _smallToLargeMap[0x3085] = 0x3086;  // ゅ → ゆ
    _smallToLargeMap[0x3087] = 0x3088;  // ょ → よ
    _smallToLargeMap[0x308E] = 0x308F;  // ゎ → わ

    // 🔹 カタカナ小文字マッピング
    _smallToLargeMap[0x30A1] = 0x30A2;  // ァ → ア
    _smallToLargeMap[0x30A3] = 0x30A4;  // ィ → イ
    _smallToLargeMap[0x30A5] = 0x30A6;  // ゥ → ウ
    _smallToLargeMap[0x30A7] = 0x30A8;  // ェ → エ
    _smallToLargeMap[0x30A9] = 0x30AA;  // ォ → オ
    _smallToLargeMap[0x30C3] = 0x30C4;  // ッ → ツ
    _smallToLargeMap[0x30E3] = 0x30E4;  // ャ → ヤ
    _smallToLargeMap[0x30E5] = 0x30E6;  // ュ → ユ
    _smallToLargeMap[0x30E7] = 0x30E8;  // ョ → ヨ
    _smallToLargeMap[0x30EE] = 0x30EF;  // ヮ → ワ
    _smallToLargeMap[0x30F5] = 0x30AB;  // ヵ → カ
    _smallToLargeMap[0x30F6] = 0x30B1;  // ヶ → ケ

    ESP_LOGI(TAG, "Small character mapping table built successfully!");
    ESP_LOGI(TAG, "  - Hiragana mappings: 10 characters");
    ESP_LOGI(TAG, "  - Katakana mappings: 12 characters");
    ESP_LOGI(TAG, "  - Total mappings: %d characters", _smallToLargeMap.size());
}

// === 小文字判定機能 ===
bool TypoWrite::isSmallChar(uint16_t unicode_char) const
{
    // マッピングテーブルに存在するかをチェック
    bool result = _smallToLargeMap.find(unicode_char) != _smallToLargeMap.end();
    
    ESP_LOGD(TAG, "isSmallChar(U+%04X) = %s", unicode_char, result ? "true" : "false");
    return result;
}

// === 対応大文字取得機能 ===
uint16_t TypoWrite::getCorrespondingLargeChar(uint16_t small_char) const
{
    auto it = _smallToLargeMap.find(small_char);
    
    if (it != _smallToLargeMap.end()) {
        uint16_t large_char = it->second;
        ESP_LOGD(TAG, "getCorrespondingLargeChar(U+%04X) -> U+%04X", small_char, large_char);
        return large_char;
    } else {
        ESP_LOGD(TAG, "getCorrespondingLargeChar(U+%04X) -> not found, returning original", small_char);
        return small_char;  // 見つからない場合は元の文字をそのまま返す
    }
}

void TypoWrite::setSmallCharHandling(bool enable) 
{ 
    _enableSmallCharHandling = enable; 
}

bool TypoWrite::isSmallCharHandlingEnabled() const 
{ 
    return _enableSmallCharHandling; 
}

void TypoWrite::setSmallCharSettings(const SmallCharSettings& settings) 
{ 
    _smallCharSettings = settings; 
}

const SmallCharSettings& TypoWrite::getSmallCharSettings() const 
{ 
    return _smallCharSettings; 
}

// === デバッグ機能 - マッピングテーブル表示 ===
void TypoWrite::debugPrintSmallCharMap() const
{
    ESP_LOGI(TAG, "=== Small Character Mapping Table ===");
    ESP_LOGI(TAG, "Small char handling: %s", _enableSmallCharHandling ? "ENABLED" : "DISABLED");
    ESP_LOGI(TAG, "Settings: scale=%.2f, offsetX=%.2f, offsetY=%.2f",
             _smallCharSettings.scale, _smallCharSettings.offsetX, _smallCharSettings.offsetY);
    ESP_LOGI(TAG, "");
    
    ESP_LOGI(TAG, "📝 Hiragana mappings:");
    // ひらがな小文字を順番に表示
    std::vector<uint16_t> hiragana_small = {
        0x3041, 0x3043, 0x3045, 0x3047, 0x3049,  // ぁぃぅぇぉ
        0x3063, 0x3083, 0x3085, 0x3087, 0x308E   // っゃゅょゎ
    };
    
    for (uint16_t small : hiragana_small) {
        uint16_t large = getCorrespondingLargeChar(small);
        ESP_LOGI(TAG, "  U+%04X (%c) -> U+%04X (%c)", 
                 small, (char)small, large, (char)large);
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "📝 Katakana mappings:");
    // カタカナ小文字を順番に表示
    std::vector<uint16_t> katakana_small = {
        0x30A1, 0x30A3, 0x30A5, 0x30A7, 0x30A9,  // ァィゥェォ
        0x30C3, 0x30E3, 0x30E5, 0x30E7, 0x30EE,  // ッャュョヮ
        0x30F5, 0x30F6                            // ヵヶ
    };
    
    for (uint16_t small : katakana_small) {
        uint16_t large = getCorrespondingLargeChar(small);
        ESP_LOGI(TAG, "  U+%04X (%c) -> U+%04X (%c)", 
                 small, (char)small, large, (char)large);
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Total: %d mappings registered", _smallToLargeMap.size());
    ESP_LOGI(TAG, "=====================================");
}

// === デバッグ機能 - 文字列内小文字分析 ===
void TypoWrite::debugAnalyzeSmallChars(const std::string& text)
{
    std::vector<uint16_t> unicode_chars = utf8ToUnicode(text);  // 既存関数呼び出し
    
    for (uint16_t ch : unicode_chars) {
        bool is_small = isSmallChar(ch);
        if (is_small) {
            uint16_t large = getCorrespondingLargeChar(ch);
            std::string char_str = unicodeToUtf8(ch);      // 既存関数呼び出し
            std::string large_str = unicodeToUtf8(large);  // 既存関数呼び出し
            ESP_LOGI(TAG, "Small char: %s -> %s", char_str.c_str(), large_str.c_str());
        }
    }
}

// === 🌟 Step1: 文字カテゴリ判定の更新（小文字対応） ===
CharCategory TypoWrite::getCharCategory(uint16_t unicode_char)
{
    // 🌟 最初に小文字判定をチェック（最優先）
    if (isSmallChar(unicode_char)) {
        return CharCategory::SMALL_CHAR;
    }

    // 既存の判定処理（句読点、括弧等）
    if (unicode_char == 0x3001 || unicode_char == 0x3002) {
        return CharCategory::PUNCTUATION;
    }

    if ((unicode_char >= 0x3008 && unicode_char <= 0x3011) ||
        (unicode_char >= 0x300C && unicode_char <= 0x300F) ||
        unicode_char == 0x0028 || unicode_char == 0x0029 ||
        unicode_char == 0x005B || unicode_char == 0x005D ||
        unicode_char == 0x007B || unicode_char == 0x007D) {
        return CharCategory::BRACKET;
    }

    if (unicode_char == 0x30FC || unicode_char == 0x2014 ||
        unicode_char == 0x2013 || unicode_char == 0x2015 ||
        unicode_char == 0xFF0D || unicode_char == 0x005F) {
        return CharCategory::HORIZONTAL_BAR;
    }

    return CharCategory::NORMAL;
}

// UTF-8文字列をUnicodeコードポイントの配列に変換
std::vector<uint16_t> TypoWrite::utf8ToUnicode(const std::string &utf8_string)
{
    std::vector<uint16_t> unicode_chars;
    const uint8_t *str = (const uint8_t *)utf8_string.c_str();
    size_t len = utf8_string.length();
    size_t i = 0;

    while (i < len)
    {
        uint16_t unicode_char = 0;

        if ((str[i] & 0x80) == 0)
        {
            // 1バイト文字 (ASCII)
            unicode_char = str[i];
            i++;
        }
        else if ((str[i] & 0xE0) == 0xC0)
        {
            // 2バイト文字
            unicode_char = ((str[i] & 0x1F) << 6) | (str[i + 1] & 0x3F);
            i += 2;
        }
        else if ((str[i] & 0xF0) == 0xE0)
        {
            // 3バイト文字
            unicode_char = ((str[i] & 0x0F) << 12) | ((str[i + 1] & 0x3F) << 6) | (str[i + 2] & 0x3F);
            i += 3;
        }
        else
        {
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

    if (unicode_char <= 0x7F)
    {
        // 1バイト文字
        utf8_str += (char)unicode_char;
    }
    else if (unicode_char <= 0x7FF)
    {
        // 2バイト文字
        utf8_str += (char)(0xC0 | (unicode_char >> 6));
        utf8_str += (char)(0x80 | (unicode_char & 0x3F));
    }
    else
    {
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

    switch (unicode_char)
    {
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
    case 0x2015:                // ―（水平バー）
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
    case 0xFF0D:                // －（全角ハイフンマイナス）
        vertical_code = 0xFE32; // ENダッシュの縦書き版に変換
        break;
    case 0x30FC:                // ー（長音記号）
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
    if (convertToVerticalGlyph(unicode_char) != unicode_char)
    {
        return false;
    }

    // ASCII文字（半角英数字）は回転させる
    if (unicode_char >= 0x0020 && unicode_char <= 0x007E)
    {
        return true;
    }

    // その他の半角カナなども回転させる
    if (unicode_char >= 0xFF61 && unicode_char <= 0xFF9F)
    {
        return true;
    }

    return false;
}

// 文字の幅を取得
int32_t TypoWrite::getCharacterWidth(uint16_t unicode_char) {
    // VLWパーサーが利用可能な場合は優先使用
    if (_useVLWParser && _vlwParser) {
        return getCharacterWidthVLW(unicode_char);
    }
    
    // 既存のM5GFX実装
    lgfx::FontMetrics metrics;
    _display->setFont(_font);
    _display->setTextSize(_fontSize);
    _font->updateFontMetric(&metrics, unicode_char);
    return metrics.x_advance * _fontSize;
}

// 文字の高さを取得
int32_t TypoWrite::getCharacterHeight(uint16_t unicode_char) {
    // VLWパーサーが利用可能な場合は優先使用
    if (_useVLWParser && _vlwParser) {
        return getCharacterHeightVLW(unicode_char);
    }
    
    // 既存のM5GFX実装
    lgfx::FontMetrics metrics;
    _display->setFont(_font);
    _display->setTextSize(_fontSize);
    _font->updateFontMetric(&metrics, unicode_char);
    return metrics.y_advance * _fontSize;
}

// フォントの幅を取得
int32_t TypoWrite::getFontWidth() {
    // VLWパーサーが利用可能な場合は優先使用
    if (_useVLWParser && _vlwParser) {
        return getFontWidthVLW();
    }
    
    // 既存のM5GFX実装
    lgfx::FontMetrics metrics;
    uint16_t full_width_space = 0x3000; // 全角スペース
    _display->setFont(_font);
    _display->setTextSize(_fontSize);
    _font->updateFontMetric(&metrics, full_width_space);
    return metrics.x_advance * _fontSize;
}

// フォントの高さを取得
int32_t TypoWrite::getFontHeight() {
    // VLWパーサーが利用可能な場合は優先使用
    if (_useVLWParser && _vlwParser) {
        return getFontHeightVLW();
    }
    
    // 既存のM5GFX実装
    lgfx::FontMetrics metrics;
    uint16_t full_width_space = 0x3000; // 全角スペース
    _display->setFont(_font);
    _display->setTextSize(_fontSize);
    _font->updateFontMetric(&metrics, full_width_space);
    return metrics.y_advance * _fontSize;
}

int32_t TypoWrite::getFontHeightVLW(){
    if (_useVLWParser && _vlwParser) {
        return static_cast<int32_t>(_vlwParser->getFontHeight() * _fontSize);
    }
    
    // フォールバック：既存のM5GFX方式
    return getFontHeight();
}

int32_t TypoWrite::getFontWidthVLW(){
    if (_useVLWParser && _vlwParser) {
        return static_cast<int32_t>(_vlwParser->getFontWidth() * _fontSize);
    }
    
    // フォールバック：既存のM5GFX方式
    return getFontWidth();
}

int32_t TypoWrite::getCharacterWidthVLW(uint16_t unicode_char){
    if (_useVLWParser && _vlwParser) {
        return static_cast<int32_t>(_vlwParser->getCharWidth(unicode_char) * _fontSize);
    }
    
    // フォールバック：既存のM5GFX方式
    return getCharacterWidth(unicode_char);
}

int32_t TypoWrite::getCharacterHeightVLW(uint16_t unicode_char){
    if (_useVLWParser && _vlwParser) {
        return static_cast<int32_t>(_vlwParser->getCharHeight(unicode_char) * _fontSize);
    }
    
    // フォールバック：既存のM5GFX方式
    return getCharacterHeight(unicode_char);
}

int32_t TypoWrite::getCharacterSetWidthVLW(uint16_t unicode_char){
    if (_useVLWParser && _vlwParser) {
        return static_cast<int32_t>(_vlwParser->getCharSetWidth(unicode_char) * _fontSize);
    }
    
    // フォールバック：既存の方式（通常の文字幅を使用）
    return getCharacterWidthVLW(unicode_char);
}

// 横書き用の一文字描画
void TypoWrite::drawCharacterHorizontal(uint16_t unicode_char, int x, int y)
{
    std::string utf8_char = unicodeToUtf8(unicode_char);

    if (_drawTarget)
    {
        // スプライトに描画
        if (_isCustomFont && _vlwFont)
        {
            _drawTarget->loadFont(_vlwFont);
        }
        else if (_font)
        {
            _drawTarget->setFont(_font);
        }
        _drawTarget->setTextSize(_fontSize);
        _drawTarget->setTextColor(_color, _bgColor);
        _drawTarget->drawString(utf8_char.c_str(), _x + x, _y + y);
        if (_isCustomFont && _vlwFont)
        {
            _drawTarget->unloadFont();
        }
    }
    else
    {
        // ディスプレイに描画
        if (_isCustomFont && _vlwFont)
        {
            _display->loadFont(_vlwFont);
        }
        else if (_font)
        {
            _display->setFont(_font);
        }
        _display->setTextSize(_fontSize);
        _display->setTextColor(_color, _bgColor);
        _display->drawString(utf8_char.c_str(), _x + x, _y + y);
        if (_isCustomFont && _vlwFont)
        {
            _display->unloadFont();
        }
    }
}

// 縦書き用の一文字描画
void TypoWrite::drawCharacterVertical(uint16_t unicode_char, int x, int y)
{
    // 縦書き用グリフに変換
    uint16_t display_char = convertToVerticalGlyph(unicode_char);

    // 回転が必要かチェック
    if (shouldRotateInVertical(unicode_char))
    {
        // 90度回転して描画
        drawRotatedCharacter(display_char, x, y);
    }
    else
    {
        // そのまま描画
        drawCharacterHorizontal(display_char, x, y);
    }
}

// 回転した文字の描画
void TypoWrite::drawRotatedCharacter(uint16_t unicode_char, int x, int y)
{
    // 文字のサイズを取得
    int char_width = getCharacterWidth(unicode_char);
    int char_height = getCharacterHeight(unicode_char);

    // 一時的なスプライトを作成（回転用）
    lgfx::LGFX_Sprite *tempSprite;
    if (_drawTarget)
    {
        tempSprite = new lgfx::LGFX_Sprite(_drawTarget);
        tempSprite->setColorDepth(_drawTarget->getColorDepth());
    }
    else
    {
        tempSprite = new lgfx::LGFX_Sprite(_display);
        tempSprite->setColorDepth(_display->getColorDepth());
    }

    tempSprite->createSprite(char_width + 10, char_height + 10);

    // 背景をクリア
    if (_transparentBg)
    {
        tempSprite->fillSprite(TFT_TRANSPARENT);
    }
    else
    {
        tempSprite->fillSprite(_bgColor);
    }

    // フォント設定と描画
    if (_isCustomFont && _vlwFont)
    {
        tempSprite->loadFont(_vlwFont);
    }
    else if (_font)
    {
        tempSprite->setFont(_font);
    }
    tempSprite->setTextSize(_fontSize);
    tempSprite->setTextColor(_color, _bgColor);
    std::string utf8_char = unicodeToUtf8(unicode_char);
    tempSprite->drawString(utf8_char.c_str(), 5, 5);
    if (_isCustomFont && _vlwFont)
    {
        tempSprite->unloadFont();
    }

    // 90度回転して描画
    tempSprite->setPivot(tempSprite->width() / 2, tempSprite->height() / 2);

    if (_drawTarget)
    {
        tempSprite->pushRotateZoom(_drawTarget, _x + x + char_height / 2, _y + y + char_width / 2,
                                   90, 1.0, 1.0, _transparentBg ? TFT_TRANSPARENT : _bgColor);
    }
    else
    {
        tempSprite->pushRotateZoom(_display, _x + x + char_height / 2, _y + y + char_width / 2,
                                   90, 1.0, 1.0, _transparentBg ? TFT_TRANSPARENT : _bgColor);
    }

    // 一時スプライトを削除
    tempSprite->deleteSprite();
    delete tempSprite;
}

// 横書きテキストの描画
void TypoWrite::drawHorizontalText(const std::string &text)
{
    // UTF-8をUnicodeに変換
    std::vector<uint16_t> unicode_chars = utf8ToUnicode(text);

    _currentX = 0;
    _currentY = 0;

    for (uint16_t ch : unicode_chars)
    {
        // 改行処理
        if (ch == '\n')
        {
            _currentX = 0;
            _currentY += getFontHeight() + _lineSpacing;
            continue;
        }

        // 文字の幅を取得
        int char_width = getCharacterWidth(ch);

        // 折り返し処理
        if (_wrap && (_currentX + char_width > _width))
        {
            _currentX = 0;
            _currentY += getFontHeight() + _lineSpacing;
        }

        // 描画範囲チェック
        if (_currentY + getFontHeight() > _height)
        {
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

    // 最初の列の位置を計算（右端から開始）
    int column_width = getFontHeight();     // 縦書きでは文字の高さが列の幅になる
    _currentX = _width - column_width - 10; // 右端から少し余白を設けて開始
    _currentY = 0;

    ESP_LOGI(TAG, "Starting vertical text at X=%d, width=%d, column_width=%d",
             _currentX, _width, column_width);

    for (uint16_t ch : unicode_chars)
    {
        // 改行処理（縦書きでは左に移動）
        if (ch == '\n')
        {
            _currentX -= column_width + _columnSpacing + _lineSpacing;
            _currentY = 0;
            ESP_LOGI(TAG, "New line at X=%d", _currentX);
            continue;
        }

        // 文字の実際のサイズを取得
        int char_width = getCharacterWidth(ch);
        int char_height = getFontHeight();

        // 縦書きでは高さと幅が入れ替わる可能性がある
        if (shouldRotateInVertical(ch))
        {
            std::swap(char_width, char_height);
        }

        // 折り返し処理
        if (_wrap && (_currentY + char_height > _height))
        {
            _currentX -= column_width + _columnSpacing + _lineSpacing;
            _currentY = 0;
            ESP_LOGI(TAG, "Wrap to new column at X=%d", _currentX);
        }

        // 描画範囲チェック
        if (_currentX < 0)
        {
            ESP_LOGI(TAG, "Out of bounds, stopping at X=%d", _currentX);
            break;
        }

        // 文字を描画
        ESP_LOGD(TAG, "Drawing char at (%d, %d)", _currentX, _currentY);
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

    if (_direction == TextDirection::HORIZONTAL)
    {
        int current_line_width = 0;
        int line_count = 1;

        for (uint16_t ch : unicode_chars)
        {
            if (ch == '\n')
            {
                width = std::max(width, current_line_width);
                current_line_width = 0;
                line_count++;
            }
            else
            {
                current_line_width += getCharacterWidth(ch) + _charSpacing;
            }
        }

        width = std::max(width, current_line_width);
        height = line_count * (getFontHeight() + _lineSpacing) - _lineSpacing;
    }
    else
    { // VERTICAL
        int current_column_height = 0;
        int column_count = 1;
        int max_char_width = 0;

        for (uint16_t ch : unicode_chars)
        {
            if (ch == '\n')
            {
                height = std::max(height, current_column_height);
                current_column_height = 0;
                column_count++;
            }
            else
            {
                // 縦書きでは文字の高さが縦方向のサイズ
                int char_height = getFontHeight();
                int char_width = getCharacterWidth(ch);

                // 回転する文字は幅と高さが入れ替わる
                if (shouldRotateInVertical(ch))
                {
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
    _vlwFont = nullptr;
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
    if (!_display)
        return false;

    // フォントを読み込み
    bool result = _display->loadFont(fontArray);
    if (result)
    {
        // 読み込んだフォントを現在のフォントとして設定
        _font = nullptr; // VLWフォントの場合はIFontを使わない
        _isCustomFont = true;
        _vlwFont = fontArray;
        ESP_LOGI(TAG, "Font loaded successfully from array");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to load font from array");
    }

    return result;
}

// テキスト描画
void TypoWrite::drawText(const std::string &text)
{
    // クリッピング領域を設定
    if (_drawTarget)
    {
        _drawTarget->setClipRect(_x, _y, _width, _height);

        // 背景をクリア（透明でない場合）
        if (!_transparentBg)
        {
            _drawTarget->fillRect(_x, _y, _width, _height, _bgColor);
        }
    }
    else
    {
        _display->setClipRect(_x, _y, _width, _height);

        // 背景をクリア（透明でない場合）
        if (!_transparentBg)
        {
            _display->fillRect(_x, _y, _width, _height, _bgColor);
        }
    }

    // テキスト描画
    if (_direction == TextDirection::HORIZONTAL)
    {
        drawHorizontalText(text);
    }
    else
    {
        drawVerticalText(text);
    }

    // クリッピング領域を解除
    if (_drawTarget)
    {
        _drawTarget->clearClipRect();
    }
    else
    {
        _display->clearClipRect();
    }
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
    if (_direction == TextDirection::HORIZONTAL)
    {
        _x = original_x + (_width - text_width) / 2;
        _y = original_y + (_height - text_height) / 2;
    }
    else
    {
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

// 描画領域のクリア
void TypoWrite::clearArea(uint16_t color)
{
    if (_drawTarget)
    {
        _drawTarget->fillRect(_x, _y, _width, _height, color);
    }
    else
    {
        _display->fillRect(_x, _y, _width, _height, color);
    }
}

// メトリクス情報の更新
bool TypoWrite::updateMetricsForChar(uint16_t unicode_char) const
{
    if (!_font || !_display)
        return false;

    _display->setFont(_font);
    _display->setTextSize(_fontSize);

    return _font->updateFontMetric(&_metrics, unicode_char);
}