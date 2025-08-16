// main/TypoWrite.cpp - エラー修正版
#include "TypoWrite.hpp"
#include "esp_log.h"
#include <cstring>

static const char *TAG = "TypoWrite";

// ========================================
// 固定微調整値の定数定義
// ========================================

namespace TypoWriteConstants
{
    // 小文字（ひらがな・カタカナ）の調整値
    namespace SmallChar
    {
        constexpr float WIDTH_SCALE = 0.75f;  // 幅75%
        constexpr float HEIGHT_SCALE = 0.75f; // 高さ75%
        constexpr int SPACING_OFFSET = -1;    // 字間-1px
        constexpr int VERTICAL_OFFSET = 2;    // 縦位置+2px
        constexpr int HORIZONTAL_OFFSET = 3;  // 横位置+3px
    }

    // 句読点の調整値
    namespace Punctuation
    {
        constexpr float WIDTH_SCALE = 0.7f;  // 幅70%
        constexpr float HEIGHT_SCALE = 1.0f; // 高さそのまま
        constexpr int SPACING_OFFSET = -2;   // 字間-2px
        constexpr int VERTICAL_OFFSET = 0;   // 縦位置そのまま
        constexpr int HORIZONTAL_OFFSET = 4; // 横位置+4px
    }

    // 括弧類の調整値
    namespace Bracket
    {
        constexpr float WIDTH_SCALE = 0.85f; // 幅85%
        constexpr float HEIGHT_SCALE = 1.0f; // 高さそのまま
        constexpr int SPACING_OFFSET = -1;   // 字間-1px
        constexpr int VERTICAL_OFFSET = 0;   // 縦位置そのまま
        constexpr int HORIZONTAL_OFFSET = 1; // 横位置+1px
    }

    // 横棒・長音記号の調整値
    namespace HorizontalBar
    {
        constexpr float WIDTH_SCALE = 1.0f;  // 幅そのまま
        constexpr float HEIGHT_SCALE = 0.8f; // 高さ80%
        constexpr int SPACING_OFFSET = 0;    // 字間そのまま
        constexpr int VERTICAL_OFFSET = 3;   // 縦位置+3px
        constexpr int HORIZONTAL_OFFSET = 0; // 横位置そのまま
    }

    // 通常文字（調整なし）
    namespace Normal
    {
        constexpr float WIDTH_SCALE = 1.0f;  // 幅そのまま
        constexpr float HEIGHT_SCALE = 1.0f; // 高さそのまま
        constexpr int SPACING_OFFSET = 0;    // 字間そのまま
        constexpr int VERTICAL_OFFSET = 0;   // 縦位置そのまま
        constexpr int HORIZONTAL_OFFSET = 0; // 横位置そのまま
    }

    // 枠線表示のデフォルト設定
    namespace Border
    {
        constexpr bool DEFAULT_SHOW = false;        // デフォルトでは非表示
        constexpr uint16_t DEFAULT_COLOR = TFT_RED; // デフォルト色は赤
        constexpr int MARK_SIZE = 5;                // 角マークのサイズ
    }
}

// ========================================
// コンストラクタ・デストラクタ
// ========================================
TypoWrite::TypoWrite(M5GFX *display)
    : _display(display),
      _drawTarget(nullptr),
      _charSprite(nullptr),
      _direction(TextDirection::HORIZONTAL),
      _alignment(TextAlignment::LEFT),
      _x(0),
      _y(0),
      _width(100),
      _height(100),
      _color(TFT_WHITE),
      _bgColor(TFT_BLACK),
      _transparentBg(false),
      _wrap(true),
      _fontSize(1.0f),
      _font(&fonts::lgfxJapanGothic_16),
      _vlwFont(nullptr),
      _isCustomFont(false),
      _vlwParser(nullptr),
      _useVLWParser(false),
      _lineSpacing(2),
      _charSpacing(0),
      _columnSpacing(0),
      _currentX(0),
      _currentY(0),
      _enableSmallCharHandling(true),
      _smallCharSettings(SmallCharSettings::getDefault()),
      _showBorder(TypoWriteConstants::Border::DEFAULT_SHOW),
      _borderColor(TypoWriteConstants::Border::DEFAULT_COLOR),
      _enableCharAdjustment(true)
{
    // マッピングテーブルの初期化
    initializeSmallCharMap();
    initializeVerticalGlyphMap();

    // 固定値による文字種別調整テーブルの初期化
    initializeFixedCharTypeAdjustments();

    // 文字描画用スプライトを事前作成（再利用用）
    _charSprite = new lgfx::LGFX_Sprite(display);

    ESP_LOGI(TAG, "TypoWrite initialized with fixed adjustment values");
    ESP_LOGI(TAG, "Small char: w=%.2f, h=%.2f, s=%d, v=%d, h=%d",
             TypoWriteConstants::SmallChar::WIDTH_SCALE,
             TypoWriteConstants::SmallChar::HEIGHT_SCALE,
             TypoWriteConstants::SmallChar::SPACING_OFFSET,
             TypoWriteConstants::SmallChar::VERTICAL_OFFSET,
             TypoWriteConstants::SmallChar::HORIZONTAL_OFFSET);
    ESP_LOGI(TAG, "Punctuation: w=%.2f, h=%.2f, s=%d, v=%d, h=%d",
             TypoWriteConstants::Punctuation::WIDTH_SCALE,
             TypoWriteConstants::Punctuation::HEIGHT_SCALE,
             TypoWriteConstants::Punctuation::SPACING_OFFSET,
             TypoWriteConstants::Punctuation::VERTICAL_OFFSET,
             TypoWriteConstants::Punctuation::HORIZONTAL_OFFSET);
}

TypoWrite::~TypoWrite()
{
    if (_charSprite)
    {
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
        {0x3041, 0x3042}, // ぁ → あ
        {0x3043, 0x3044}, // ぃ → い
        {0x3045, 0x3046}, // ぅ → う
        {0x3047, 0x3048}, // ぇ → え
        {0x3049, 0x304A}, // ぉ → お
        {0x3063, 0x3064}, // っ → つ
        {0x3083, 0x3084}, // ゃ → や
        {0x3085, 0x3086}, // ゅ → ゆ
        {0x3087, 0x3088}, // ょ → よ
        {0x308E, 0x308F}, // ゎ → わ

        // カタカナ小文字マッピング
        {0x30A1, 0x30A2}, // ァ → ア
        {0x30A3, 0x30A4}, // ィ → イ
        {0x30A5, 0x30A6}, // ゥ → ウ
        {0x30A7, 0x30A8}, // ェ → エ
        {0x30A9, 0x30AA}, // ォ → オ
        {0x30C3, 0x30C4}, // ッ → ツ
        {0x30E3, 0x30E4}, // ャ → ヤ
        {0x30E5, 0x30E6}, // ュ → ユ
        {0x30E7, 0x30E8}, // ョ → ヨ
        {0x30EE, 0x30EF}, // ヮ → ワ
        {0x30F5, 0x30AB}, // ヵ → カ
        {0x30F6, 0x30B1}  // ヶ → ケ
    };

    ESP_LOGI(TAG, "Small character map: %d entries", _smallToLargeMap.size());
}

// ========================================
// 文字種別調整テーブル初期化
// ========================================
void TypoWrite::initializeCharTypeAdjustments()
{
    // ひらがな小文字の調整
    CharTypeAdjustment hiraganaSmall = {0.0f, 0.0f, 0, 0, 0};
    _charAdjustments[CharCategory::SMALL_CHAR] = hiraganaSmall;

    // 句読点の調整
    CharTypeAdjustment punctuation = {0.0f, 0.0f, 0, 0, 0};
    _charAdjustments[CharCategory::PUNCTUATION] = punctuation;

    // 括弧類の調整
    CharTypeAdjustment bracket = {0.0f, 0.0f, 0, 0, 0};
    _charAdjustments[CharCategory::BRACKET] = bracket;

    // 横棒・長音記号の調整
    CharTypeAdjustment horizontalBar = {0.0f, 0.0f, 0, 0, 0};
    _charAdjustments[CharCategory::HORIZONTAL_BAR] = horizontalBar;

    // 通常文字（デフォルト）
    CharTypeAdjustment normal = {0.0f, 0.0f, 0, 0, 0};
    _charAdjustments[CharCategory::NORMAL] = normal;

    ESP_LOGI(TAG, "Character type adjustments initialized");
}

// ========================================
// 固定値による文字種別調整テーブル初期化
// ========================================
void TypoWrite::initializeFixedCharTypeAdjustments()
{
    // 小文字の調整（固定値）
    _charAdjustments[CharCategory::SMALL_CHAR] = {
        TypoWriteConstants::SmallChar::WIDTH_SCALE,
        TypoWriteConstants::SmallChar::HEIGHT_SCALE,
        TypoWriteConstants::SmallChar::SPACING_OFFSET,
        TypoWriteConstants::SmallChar::VERTICAL_OFFSET,
        TypoWriteConstants::SmallChar::HORIZONTAL_OFFSET};

    // 句読点の調整（固定値）
    _charAdjustments[CharCategory::PUNCTUATION] = {
        TypoWriteConstants::Punctuation::WIDTH_SCALE,
        TypoWriteConstants::Punctuation::HEIGHT_SCALE,
        TypoWriteConstants::Punctuation::SPACING_OFFSET,
        TypoWriteConstants::Punctuation::VERTICAL_OFFSET,
        TypoWriteConstants::Punctuation::HORIZONTAL_OFFSET};

    // 括弧類の調整（固定値）
    _charAdjustments[CharCategory::BRACKET] = {
        TypoWriteConstants::Bracket::WIDTH_SCALE,
        TypoWriteConstants::Bracket::HEIGHT_SCALE,
        TypoWriteConstants::Bracket::SPACING_OFFSET,
        TypoWriteConstants::Bracket::VERTICAL_OFFSET,
        TypoWriteConstants::Bracket::HORIZONTAL_OFFSET};

    // 横棒・長音記号の調整（固定値）
    _charAdjustments[CharCategory::HORIZONTAL_BAR] = {
        TypoWriteConstants::HorizontalBar::WIDTH_SCALE,
        TypoWriteConstants::HorizontalBar::HEIGHT_SCALE,
        TypoWriteConstants::HorizontalBar::SPACING_OFFSET,
        TypoWriteConstants::HorizontalBar::VERTICAL_OFFSET,
        TypoWriteConstants::HorizontalBar::HORIZONTAL_OFFSET};

    // 通常文字（調整なし）
    _charAdjustments[CharCategory::NORMAL] = {
        TypoWriteConstants::Normal::WIDTH_SCALE,
        TypoWriteConstants::Normal::HEIGHT_SCALE,
        TypoWriteConstants::Normal::SPACING_OFFSET,
        TypoWriteConstants::Normal::VERTICAL_OFFSET,
        TypoWriteConstants::Normal::HORIZONTAL_OFFSET};

    ESP_LOGI(TAG, "Fixed character type adjustments initialized");
}

// ========================================
// 枠線表示設定
// ========================================
void TypoWrite::setBorderDisplay(bool show, uint16_t color)
{
    _showBorder = show;
    _borderColor = color;
    ESP_LOGI(TAG, "Border display: %s, color: 0x%04X",
             show ? "enabled" : "disabled", color);
}

// ========================================
// 文字調整機能設定
// ========================================
void TypoWrite::setCharacterAdjustment(bool enable)
{
    _enableCharAdjustment = enable;
    ESP_LOGI(TAG, "Character adjustment: %s", enable ? "enabled" : "disabled");
}

void TypoWrite::initializeVerticalGlyphMap()
{
    ESP_LOGI(TAG, "Building vertical glyph mapping table...");

    // 縦書き用グリフマッピング（unordered_map化）
    _verticalGlyphMap = {
        // 句読点
        {0x3001, 0xFE11}, // 、→ ︑
        {0x3002, 0xFE12}, // 。→ ︒

        // 日本語括弧
        {0x300C, 0xFE41}, // 「→ ﹁
        {0x300D, 0xFE42}, // 」→ ﹂
        {0x300E, 0xFE43}, // 『→ ﹃
        {0x300F, 0xFE44}, // 』→ ﹄

        // 半角括弧類
        {0x0028, 0xFE35}, // ( → ︵
        {0x0029, 0xFE36}, // ) → ︶
        {0x005B, 0xFE47}, // [ → ﹇
        {0x005D, 0xFE48}, // ] → ﹈
        {0x007B, 0xFE37}, // { → ︷
        {0x007D, 0xFE38}, // } → ︸

        // 山括弧類
        {0x3008, 0xFE3F}, // 〈→ ︿
        {0x3009, 0xFE40}, // 〉→ ﹀
        {0x300A, 0xFE3D}, // 《→ ︽
        {0x300B, 0xFE3E}, // 》→ ︾

        // その他括弧
        {0x3010, 0xFE3B}, // 【→ ︻
        {0x3011, 0xFE3C}, // 】→ ︼
        {0x3014, 0xFE39}, // 〔→ ︹
        {0x3015, 0xFE3A}, // 〕→ ︺

        // ダッシュ・区切り線類
        {0x2014, 0xFE31}, // — → ︱
        {0x2013, 0xFE32}, // – → ︲
        {0x2015, 0xFE31}, // ― → ︱
        {0x005F, 0xFE33}, // _ → ︳
        {0x2025, 0xFE30}, // ‥ → ︰
        {0x2026, 0xFE19}, // … → ︙

        // 全角ダッシュ・記号
        {0xFF0D, 0xFE32}, // － → ︲
        {0x30FC, 0xFE31}  // ー → ︱
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
    if (it != _metricsCache.end())
    {
        return it->second;
    }

    CharMetrics metrics;

    // VLWParser優先
    if (_useVLWParser && _vlwParser && _vlwParser->isInitialized())
    {
        metrics = {
            static_cast<int32_t>(_vlwParser->getCharWidth(unicode_char) * _fontSize),
            static_cast<int32_t>(_vlwParser->getCharHeight(unicode_char) * _fontSize),
            static_cast<int32_t>(_vlwParser->getCharSetWidth(unicode_char) * _fontSize),
            0 // baseline（必要に応じて追加）
        };
    }
    else
    {
        // M5GFXフォールバック
        lgfx::FontMetrics fm;
        _display->setFont(_font);
        _display->setTextSize(_fontSize);
        _font->updateFontMetric(&fm, unicode_char);

        metrics = {
            static_cast<int32_t>(fm.x_advance * _fontSize),
            static_cast<int32_t>(fm.y_advance * _fontSize),
            static_cast<int32_t>(fm.x_advance * _fontSize), // 送り幅
            static_cast<int32_t>(fm.baseline * _fontSize)};
    }

    // キャッシュに保存（よく使う文字のみ）
    if (_metricsCache.size() < 256)
    { // キャッシュサイズ制限
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
    if (scale == 1.0f && rotation == 0.0f)
    {
        drawDirectCharacter(unicode_char, draw_x, draw_y);
        return;
    }

    // スケールまたは回転が必要な場合はスプライト描画
    drawSpriteCharacter(unicode_char, draw_x, draw_y, scale, rotation);
}

void TypoWrite::drawEnhancedCharacter(uint16_t unicode_char, int x, int y,
                                      float widthScale, float heightScale)
{
    // スケール調整が不要な場合は直接描画
    if (widthScale == 1.0f && heightScale == 1.0f)
    {
        drawDirectCharacter(unicode_char, x, y);
        return;
    }

    // スケール調整が必要な場合はスプライト描画
    drawScaledCharacter(unicode_char, x, y, widthScale, heightScale);
}

// 直接描画（最速パス）
void TypoWrite::drawDirectCharacter(uint16_t unicode_char, int x, int y)
{
    std::string utf8_char = unicodeToUtf8(unicode_char);

    // 描画先の選択（明示的なキャストを追加）
    lgfx::LovyanGFX *target = _drawTarget ? static_cast<lgfx::LovyanGFX *>(_drawTarget) : static_cast<lgfx::LovyanGFX *>(_display);

    // フォント設定
    if (_isCustomFont && _vlwFont)
    {
        target->loadFont(_vlwFont);
    }
    else if (_font)
    {
        target->setFont(_font);
    }

    target->setTextSize(_fontSize);
    target->setTextColor(_color, _bgColor);
    target->drawString(utf8_char.c_str(), _x + x, _y + y);

    if (_isCustomFont && _vlwFont)
    {
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
    if (_charSprite->width() < sprite_width || _charSprite->height() < sprite_height)
    {
        _charSprite->deleteSprite();
        _charSprite->createSprite(sprite_width, sprite_height);
    }

    // 背景をクリア
    _charSprite->fillSprite(_transparentBg ? TFT_TRANSPARENT : _bgColor);

    // フォント設定と描画
    if (_isCustomFont && _vlwFont)
    {
        _charSprite->loadFont(_vlwFont);
    }
    else if (_font)
    {
        _charSprite->setFont(_font);
    }

    _charSprite->setTextSize(_fontSize);
    _charSprite->setTextColor(_color, _bgColor);

    std::string utf8_char = unicodeToUtf8(unicode_char);
    _charSprite->drawString(utf8_char.c_str(), 10, 10);

    if (_isCustomFont && _vlwFont)
    {
        _charSprite->unloadFont();
    }

    // 描画先を選択（明示的なキャストを追加）
    lgfx::LovyanGFX *target = _drawTarget ? static_cast<lgfx::LovyanGFX *>(_drawTarget) : static_cast<lgfx::LovyanGFX *>(_display);

    // 回転・スケール描画
    int center_x = _x + x + sprite_width / 2;
    int center_y = _y + y + sprite_height / 2;

    _charSprite->pushRotateZoom(target, center_x, center_y,
                                rotation, scale, scale,
                                _transparentBg ? TFT_TRANSPARENT : _bgColor);
}

void TypoWrite::drawEnhancedCharacterWithRotation(uint16_t unicode_char, int x, int y,
                                                  float widthScale, float heightScale,
                                                  float rotation)
{
    // 回転もスケールも不要な場合は直接描画
    if (widthScale == 1.0f && heightScale == 1.0f && rotation == 0.0f)
    {
        drawDirectCharacter(unicode_char, x, y);
        return;
    }

    // スケールまたは回転が必要な場合はスプライト描画
    drawScaledCharacterWithRotation(unicode_char, x, y, widthScale, heightScale, rotation);
}

void TypoWrite::drawScaledCharacterWithRotation(uint16_t unicode_char, int x, int y,
                                                float widthScale, float heightScale,
                                                float rotation)
{
    // 回転とスケールの組み合わせ実装（簡易版）
    float avgScale = (widthScale + heightScale) / 2.0f;
    drawUnifiedCharacter(unicode_char, x, y, avgScale, rotation, 0, 0);
}

// 回転とスケールを適用して描画
void TypoWrite::drawScaledCharacter(uint16_t unicode_char, int x, int y,
                                    float widthScale, float heightScale)
{
    CharMetrics metrics = getCharMetrics(unicode_char);

    // スプライトサイズを計算
    int sprite_width = static_cast<int>(metrics.width * widthScale + 4);
    int sprite_height = static_cast<int>(metrics.height * heightScale + 4);

    // 再利用スプライトのサイズ調整
    if (_charSprite->width() < sprite_width || _charSprite->height() < sprite_height)
    {
        _charSprite->deleteSprite();
        _charSprite->createSprite(sprite_width, sprite_height);
    }

    // 背景をクリア
    _charSprite->fillSprite(_transparentBg ? TFT_TRANSPARENT : _bgColor);

    // 文字を描画
    std::string utf8_char = unicodeToUtf8(unicode_char);

    if (_isCustomFont && _vlwFont)
    {
        _charSprite->loadFont(_vlwFont);
    }
    else if (_font)
    {
        _charSprite->setFont(_font);
    }

    _charSprite->setTextSize(_fontSize);
    _charSprite->setTextColor(_color, _bgColor);
    _charSprite->drawString(utf8_char.c_str(), 2, 2);

    if (_isCustomFont && _vlwFont)
    {
        _charSprite->unloadFont();
    }

    // 描画先を取得
    lgfx::LovyanGFX *target = _drawTarget ? static_cast<lgfx::LovyanGFX *>(_drawTarget) : static_cast<lgfx::LovyanGFX *>(_display);

    // スケール適用して描画
    _charSprite->pushSprite(target, _x + x, _y + y, _bgColor);
}

// ========================================
// 横書きテキスト描画
// ========================================
void TypoWrite::drawHorizontalTextEnhanced(const std::string &text)
{
    std::vector<uint16_t> unicode_chars = utf8ToUnicode(text);

    // 描画開始位置を設定
    _currentX = 0;
    _currentY = 0;

    for (size_t i = 0; i < unicode_chars.size(); i++)
    {
        uint16_t unicode_char = unicode_chars[i];

        // 改行処理
        if (unicode_char == '\n')
        {
            _currentX = 0;
            _currentY += getLineHeight() + _lineSpacing;
            continue;
        }

        // 文字メトリクスを取得
        CharMetrics metrics = getCharMetrics(unicode_char);

        // 固定値による文字種別調整を適用
        CharTypeAdjustment adjustment = getFixedCharAdjustment(unicode_char);

        // 調整後のメトリクス計算
        int adjusted_width = static_cast<int>(metrics.width * adjustment.widthScale);
        int adjusted_height = static_cast<int>(metrics.height * adjustment.heightScale);
        int char_spacing = _charSpacing + adjustment.spacingOffset;

        // 折り返し処理
        if (_wrap && (_currentX + adjusted_width > _width))
        {
            _currentX = 0;
            _currentY += getLineHeight() + _lineSpacing;
        }

        // 描画範囲チェック
        if (_currentY + adjusted_height > _height)
        {
            break;
        }

        // 調整された位置で文字描画
        int draw_x = _currentX + adjustment.horizontalOffset;
        int draw_y = _currentY + adjustment.verticalOffset;

        drawEnhancedCharacter(unicode_char, draw_x, draw_y,
                              adjustment.widthScale, adjustment.heightScale);

        // 次の文字位置へ（調整された字間を適用）
        _currentX += adjusted_width + char_spacing;
    }
}

// ========================================
// 縦書きテキスト描画（簡略化版）
// ========================================
void TypoWrite::drawVerticalTextEnhanced(const std::string &text)
{
    std::vector<uint16_t> unicode_chars = utf8ToUnicode(text);

    // 縦書きの開始位置（右上から）
    _currentX = _width - getMaxCharWidth();
    _currentY = 0;

    for (size_t i = 0; i < unicode_chars.size(); i++)
    {
        uint16_t unicode_char = unicode_chars[i];

        // 改行処理（新しい列）
        if (unicode_char == '\n')
        {
            _currentX -= getMaxCharWidth() + _columnSpacing + _lineSpacing;
            _currentY = 0;
            continue;
        }

        // 縦書き用の文字変換
        uint16_t display_char = convertToVerticalGlyph(unicode_char);

        // 文字メトリクスを取得
        CharMetrics metrics = getCharMetrics(display_char);

        // 固定値による文字種別調整を適用
        CharTypeAdjustment adjustment = getFixedCharAdjustment(unicode_char);

        // 回転が必要な文字の処理
        float rotation = shouldRotateInVertical(unicode_char) ? 90.0f : 0.0f;

        // 調整後のメトリクス計算
        int adjusted_width = static_cast<int>(metrics.width * adjustment.widthScale);
        int adjusted_height = static_cast<int>(metrics.height * adjustment.heightScale);
        int char_spacing = _charSpacing + adjustment.spacingOffset;

        // 回転する文字は幅と高さが入れ替わる
        if (rotation != 0.0f)
        {
            std::swap(adjusted_width, adjusted_height);
        }

        // 折り返し処理（新しい列）
        if (_wrap && (_currentY + adjusted_height > _height))
        {
            _currentX -= getMaxCharWidth() + _columnSpacing + _lineSpacing;
            _currentY = 0;
        }

        // 描画範囲チェック
        if (_currentX < 0)
        {
            break;
        }

        // 調整された位置で文字描画
        int draw_x = _currentX + adjustment.horizontalOffset;
        int draw_y = _currentY + adjustment.verticalOffset;

        drawEnhancedCharacterWithRotation(display_char, draw_x, draw_y,
                                          adjustment.widthScale, adjustment.heightScale,
                                          rotation);

        // 次の文字位置へ（調整された字間を適用）
        _currentY += adjusted_height + char_spacing;
    }
}

// ========================================
// 固定値による文字種別調整取得
// ========================================
CharTypeAdjustment TypoWrite::getFixedCharAdjustment(uint16_t unicode_char)
{
    if (!_enableCharAdjustment)
    {
        // 調整機能が無効な場合はデフォルト値を返す
        return {
            TypoWriteConstants::Normal::WIDTH_SCALE,
            TypoWriteConstants::Normal::HEIGHT_SCALE,
            TypoWriteConstants::Normal::SPACING_OFFSET,
            TypoWriteConstants::Normal::VERTICAL_OFFSET,
            TypoWriteConstants::Normal::HORIZONTAL_OFFSET};
    }

    CharCategory category = getCharCategory(unicode_char);

    auto it = _charAdjustments.find(category);
    if (it != _charAdjustments.end())
    {
        return it->second;
    }

    // 見つからない場合は通常文字として扱う
    return {
        TypoWriteConstants::Normal::WIDTH_SCALE,
        TypoWriteConstants::Normal::HEIGHT_SCALE,
        TypoWriteConstants::Normal::SPACING_OFFSET,
        TypoWriteConstants::Normal::VERTICAL_OFFSET,
        TypoWriteConstants::Normal::HORIZONTAL_OFFSET};
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

// ヘルパー関数
uint16_t TypoWrite::convertToVerticalGlyph(uint16_t unicode_char)
{
    auto it = _verticalGlyphMap.find(unicode_char);
    if (it != _verticalGlyphMap.end())
    {
        return it->second;
    }
    return unicode_char; // 変換なし
}

bool TypoWrite::isSmallChar(uint16_t unicode_char) const
{
    return _smallToLargeMap.find(unicode_char) != _smallToLargeMap.end();
}

uint16_t TypoWrite::getCorrespondingLargeChar(uint16_t small_char) const
{
    auto it = _smallToLargeMap.find(small_char);
    if (it != _smallToLargeMap.end())
    {
        return it->second;
    }
    return small_char; // 見つからない場合は元の文字
}

bool TypoWrite::shouldRotateInVertical(uint16_t unicode_char)
{
    // 縦書き用グリフが存在する文字は回転させない
    if (_verticalGlyphMap.find(unicode_char) != _verticalGlyphMap.end())
    {
        return false;
    }

    // ASCII文字（半角英数字）は回転
    if (unicode_char >= 0x0020 && unicode_char <= 0x007E)
    {
        return true;
    }

    // 半角カナも回転
    if (unicode_char >= 0xFF61 && unicode_char <= 0xFF9F)
    {
        return true;
    }

    return false;
}

CharTypeAdjustment TypoWrite::getCharAdjustment(uint16_t unicode_char)
{
    if (!_enableCharAdjustment)
    {
        return _charAdjustments[CharCategory::NORMAL];
    }

    CharCategory category = getCharCategory(unicode_char);

    auto it = _charAdjustments.find(category);
    if (it != _charAdjustments.end())
    {
        return it->second;
    }

    return _charAdjustments[CharCategory::NORMAL];
}

// 行の高さを取得
int TypoWrite::getLineHeight()
{
    CharMetrics metrics = getCharMetrics(0x3000); // 全角スペースで代表
    return metrics.height;
}

// 最大文字幅を取得（縦書き用）
int TypoWrite::getMaxCharWidth()
{
    CharMetrics metrics = getCharMetrics(0x3000); // 全角スペースで代表
    return metrics.width;
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
void TypoWrite::setVLWParser(VLWFontParser *parser)
{
    _vlwParser = parser;
    _useVLWParser = (parser != nullptr && parser->isInitialized());

    if (_useVLWParser)
    {
        ESP_LOGI(TAG, "VLW parser enabled for font metrics");
    }
    else
    {
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
    if (!_display)
    {
        ESP_LOGE(TAG, "Display not initialized");
        return false;
    }

    // フォントを読み込み
    bool result = _display->loadFont(fontArray);
    if (result)
    {
        // 読み込んだフォントを現在のフォントとして設定
        _font = nullptr; // VLWフォントの場合はIFontを使わない
        _isCustomFont = true;
        _vlwFont = fontArray;
        // メトリクスキャッシュをクリア
        _metricsCache.clear();
        ESP_LOGI(TAG, "Font loaded successfully from array");
    }
    else
    {
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
    lgfx::LovyanGFX *target = _drawTarget ? static_cast<lgfx::LovyanGFX *>(_drawTarget) : static_cast<lgfx::LovyanGFX *>(_display);

    target->setClipRect(_x, _y, _width, _height);

    // 背景をクリア（透明でない場合）
    if (!_transparentBg)
    {
        target->fillRect(_x, _y, _width, _height, _bgColor);
    }

    // 新機能: 枠線描画
    if (_showBorder)
    {
        drawAreaBorder();
    }

    // テキスト描画
    if (_direction == TextDirection::HORIZONTAL)
    {
        drawHorizontalTextEnhanced(text);
    }
    else
    {
        drawVerticalTextEnhanced(text);
    }

    // クリッピング領域を解除
    target->clearClipRect();
}

// ========================================
// 新機能: 枠線描画
// ========================================
void TypoWrite::drawAreaBorder()
{
    lgfx::LovyanGFX *target = _drawTarget ? static_cast<lgfx::LovyanGFX *>(_drawTarget) : static_cast<lgfx::LovyanGFX *>(_display);

    // 外枠を描画
    target->drawRect(_x, _y, _width, _height, _borderColor);

    // デバッグ用: 角に小さな十字マークを描画
    int markSize = 5;
    // 左上
    target->drawLine(_x - markSize, _y, _x + markSize, _y, _borderColor);
    target->drawLine(_x, _y - markSize, _x, _y + markSize, _borderColor);

    // 右上
    target->drawLine(_x + _width - markSize, _y, _x + _width + markSize, _y, _borderColor);
    target->drawLine(_x + _width, _y - markSize, _x + _width, _y + markSize, _borderColor);

    // 左下
    target->drawLine(_x - markSize, _y + _height, _x + markSize, _y + _height, _borderColor);
    target->drawLine(_x, _y + _height - markSize, _x, _y + _height + markSize, _borderColor);

    // 右下
    target->drawLine(_x + _width - markSize, _y + _height,
                     _x + _width + markSize, _y + _height, _borderColor);
    target->drawLine(_x + _width, _y + _height - markSize,
                     _x + _width, _y + _height + markSize, _borderColor);
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
                CharMetrics metrics = getCharMetrics(ch);
                current_line_width += metrics.setWidth + _charSpacing;
            }
        }

        width = std::max(width, current_line_width);
        height = line_count * (getCharMetrics(0x3000).height + _lineSpacing) - _lineSpacing;
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
                CharMetrics metrics = getCharMetrics(ch);
                int char_height = metrics.height;
                int char_width = metrics.width;

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

// ========== 描画領域管理 ==========

// 描画領域のクリア
void TypoWrite::clearArea(uint16_t color)
{
    lgfx::LovyanGFX *target = _drawTarget ? static_cast<lgfx::LovyanGFX *>(_drawTarget) : static_cast<lgfx::LovyanGFX *>(_display);

    target->fillRect(_x, _y, _width, _height, color);
}

// ========== 文字カテゴリ判定 ==========

CharCategory TypoWrite::getCharCategory(uint16_t unicode_char)
{
    // 最初に小文字判定をチェック（最優先）
    if (isSmallChar(unicode_char))
    {
        return CharCategory::SMALL_CHAR;
    }

    // 句読点
    if (unicode_char == 0x3001 || unicode_char == 0x3002)
    {
        return CharCategory::PUNCTUATION;
    }

    // 括弧類
    if ((unicode_char >= 0x3008 && unicode_char <= 0x3011) ||
        (unicode_char >= 0x300C && unicode_char <= 0x300F) ||
        unicode_char == 0x0028 || unicode_char == 0x0029 ||
        unicode_char == 0x005B || unicode_char == 0x005D ||
        unicode_char == 0x007B || unicode_char == 0x007D)
    {
        return CharCategory::BRACKET;
    }

    // 横棒・長音記号類
    if (unicode_char == 0x30FC || unicode_char == 0x2014 ||
        unicode_char == 0x2013 || unicode_char == 0x2015 ||
        unicode_char == 0xFF0D || unicode_char == 0x005F)
    {
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
    for (const auto &pair : _smallToLargeMap)
    {
        if (pair.first >= 0x3041 && pair.first <= 0x308E)
        {
            ESP_LOGI(TAG, "  U+%04X -> U+%04X", pair.first, pair.second);
        }
    }

    ESP_LOGI(TAG, "Katakana mappings:");
    for (const auto &pair : _smallToLargeMap)
    {
        if (pair.first >= 0x30A1 && pair.first <= 0x30F6)
        {
            ESP_LOGI(TAG, "  U+%04X -> U+%04X", pair.first, pair.second);
        }
    }

    ESP_LOGI(TAG, "Total: %d mappings", _smallToLargeMap.size());
    ESP_LOGI(TAG, "=====================================");
}

// 文字列内の小文字分析
void TypoWrite::debugAnalyzeSmallChars(const std::string &text)
{
    std::vector<uint16_t> unicode_chars = utf8ToUnicode(text);

    ESP_LOGI(TAG, "=== Small Character Analysis ===");
    ESP_LOGI(TAG, "Text: %s", text.c_str());

    int small_count = 0;
    for (uint16_t ch : unicode_chars)
    {
        if (isSmallChar(ch))
        {
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

void TypoWrite::debugShowCharAdjustments()
{
    ESP_LOGI(TAG, "=== Character Type Adjustments ===");
    ESP_LOGI(TAG, "Character adjustment: %s",
             _enableCharAdjustment ? "ENABLED" : "DISABLED");

    const char *categoryNames[] = {
        "NORMAL", "BRACKET", "HORIZONTAL_BAR",
        "PUNCTUATION", "SMALL_CHAR", "OTHER_SPECIAL"};

    for (const auto &pair : _charAdjustments)
    {
        CharCategory cat = pair.first;
        CharTypeAdjustment adj = pair.second;

        ESP_LOGI(TAG, "%s: w=%.2f, h=%.2f, s=%d, v=%d, h=%d",
                 categoryNames[static_cast<int>(cat)],
                 adj.widthScale, adj.heightScale,
                 adj.spacingOffset, adj.verticalOffset, adj.horizontalOffset);
    }
    ESP_LOGI(TAG, "==================================");
}

void TypoWrite::debugShowFixedAdjustments()
{
    ESP_LOGI(TAG, "=== Fixed Character Type Adjustments ===");
    ESP_LOGI(TAG, "Character adjustment: %s",
             _enableCharAdjustment ? "ENABLED" : "DISABLED");

    ESP_LOGI(TAG, "SMALL_CHAR: w=%.2f, h=%.2f, s=%d, v=%d, h=%d",
             TypoWriteConstants::SmallChar::WIDTH_SCALE,
             TypoWriteConstants::SmallChar::HEIGHT_SCALE,
             TypoWriteConstants::SmallChar::SPACING_OFFSET,
             TypoWriteConstants::SmallChar::VERTICAL_OFFSET,
             TypoWriteConstants::SmallChar::HORIZONTAL_OFFSET);

    ESP_LOGI(TAG, "PUNCTUATION: w=%.2f, h=%.2f, s=%d, v=%d, h=%d",
             TypoWriteConstants::Punctuation::WIDTH_SCALE,
             TypoWriteConstants::Punctuation::HEIGHT_SCALE,
             TypoWriteConstants::Punctuation::SPACING_OFFSET,
             TypoWriteConstants::Punctuation::VERTICAL_OFFSET,
             TypoWriteConstants::Punctuation::HORIZONTAL_OFFSET);

    ESP_LOGI(TAG, "BRACKET: w=%.2f, h=%.2f, s=%d, v=%d, h=%d",
             TypoWriteConstants::Bracket::WIDTH_SCALE,
             TypoWriteConstants::Bracket::HEIGHT_SCALE,
             TypoWriteConstants::Bracket::SPACING_OFFSET,
             TypoWriteConstants::Bracket::VERTICAL_OFFSET,
             TypoWriteConstants::Bracket::HORIZONTAL_OFFSET);

    ESP_LOGI(TAG, "HORIZONTAL_BAR: w=%.2f, h=%.2f, s=%d, v=%d, h=%d",
             TypoWriteConstants::HorizontalBar::WIDTH_SCALE,
             TypoWriteConstants::HorizontalBar::HEIGHT_SCALE,
             TypoWriteConstants::HorizontalBar::SPACING_OFFSET,
             TypoWriteConstants::HorizontalBar::VERTICAL_OFFSET,
             TypoWriteConstants::HorizontalBar::HORIZONTAL_OFFSET);

    ESP_LOGI(TAG, "NORMAL: w=%.2f, h=%.2f, s=%d, v=%d, h=%d",
             TypoWriteConstants::Normal::WIDTH_SCALE,
             TypoWriteConstants::Normal::HEIGHT_SCALE,
             TypoWriteConstants::Normal::SPACING_OFFSET,
             TypoWriteConstants::Normal::VERTICAL_OFFSET,
             TypoWriteConstants::Normal::HORIZONTAL_OFFSET);

    ESP_LOGI(TAG, "========================================");
}
