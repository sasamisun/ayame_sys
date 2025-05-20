// main/TypoWrite.cpp
#include "TypoWrite.hpp"
#include "esp_log.h"

// ログタグ
static const char *TAG = "TYPO_WRITE";

// コンストラクタ
TypoWrite::TypoWrite(M5GFX *display) : _display(display),
                                       _sprite(nullptr),
                                       _spriteCreated(false),
                                       _direction(TextDirection::HORIZONTAL),
                                       _alignment(TextAlignment::LEFT),
                                       _x(0),
                                       _y(0),
                                       _width(display ? display->width() : 0),
                                       _height(display ? display->height() : 0),
                                       _color(TFT_WHITE),
                                       _bgColor(0),
                                       _fontSize(1.0f),
                                       _font(nullptr),
                                       _isCustomFont(false),
                                       _lineSpacing(4),
                                       _charSpacing(2),
                                       _wrap(true),
                                       _transparentBg(true)
{
    // デフォルトフォントを設定
    if (display)
    {
        _font = display->getFont();
        _isCustomFont = false; // デフォルトフォントなのでfalse
    }
    // メトリクス初期化（デフォルト値をゼロに）
    memset(&_metrics, 0, sizeof(_metrics));
        // スプライト作成
    if (display)
    {
        createSprite(_width, _height);
    }
}

// デストラクタ
TypoWrite::~TypoWrite()
{
    deleteSprite();
}

// スプライト作成
bool TypoWrite::createSprite(int width, int height)
{
    // すでに作成されている場合は一旦削除
    deleteSprite();
    
    if (!_display) return false;
    
    // スプライト作成
    _sprite = new lgfx::LGFX_Sprite(_display);
    if (!_sprite) {
        ESP_LOGE(TAG, "Failed to allocate sprite");
        return false;
    }
    
    // スプライト初期化
    if (!_sprite->createSprite(width, height)) {
        ESP_LOGE(TAG, "Failed to create sprite with size %dx%d", width, height);
        delete _sprite;
        _sprite = nullptr;
        return false;
    }
    
    // 作成成功
    _spriteCreated = true;
    
    // 背景色で初期化
    _sprite->fillScreen(_bgColor);
    
    // フォント設定を適用
    _sprite->setFont(_font);
    _sprite->setTextSize(_fontSize);
    
    ESP_LOGI(TAG, "Created sprite with size %dx%d", width, height);
    return true;
}

// スプライト削除
void TypoWrite::deleteSprite()
{
    if (_sprite) {
        _sprite->deleteSprite();
        delete _sprite;
        _sprite = nullptr;
        _spriteCreated = false;
        ESP_LOGI(TAG, "Deleted sprite");
    }
}

// スプライトクリア
void TypoWrite::clearSprite()
{
    if (_sprite) {
        _sprite->fillScreen(_bgColor);
        ESP_LOGI(TAG, "Cleared sprite");
    }
}

// スプライトをディスプレイに転送
void TypoWrite::updateDisplay()
{
    if (_sprite && _spriteCreated) {
        _sprite->pushSprite(_display, _x, _y);
        ESP_LOGI(TAG, "Updated display from sprite at (%d,%d)", _x, _y);
    }
}

// テキスト方向を設定
void TypoWrite::setDirection(TextDirection direction)
{
    _direction = direction;
}

// テキスト揃えを設定
void TypoWrite::setAlignment(TextAlignment alignment)
{
    _alignment = alignment;
}

// 描画位置を設定
void TypoWrite::setPosition(int x, int y)
{
    _x = x;
    _y = y;
}

// 描画領域を設定
void TypoWrite::setArea(int width, int height)
{
    _width = width;
    _height = height;
    // 描画領域が変わったのでスプライトを再作成
    if (_display && (width > 0 && height > 0)) {
        createSprite(width, height);
    }

}

// テキスト色を設定
void TypoWrite::setColor(uint16_t color)
{
    _color = color;
    if (_sprite) {
        if (_transparentBg) {
            _sprite->setTextColor(_color); 
        } else {
            _sprite->setTextColor(_color, _bgColor);
        }
    }
}

// 背景色を設定
void TypoWrite::setBackgroundColor(uint16_t bgColor)
{
    _bgColor = bgColor;
    if (_sprite) {
        if (_transparentBg) {
            _sprite->setTextColor(_color);
        } else {
            _sprite->setTextColor(_color, _bgColor);
        }
        // スプライトの背景色も更新
        clearSprite();
    }
}
// 背景の透明設定
void TypoWrite::setTransparentBackground(bool transparent)
{
    _transparentBg = transparent;
    if (_sprite) {
        if (transparent) {
            _sprite->setTextColor(_color);
        } else {
            _sprite->setTextColor(_color, _bgColor);
        }
    }
}
// フォントサイズを設定
void TypoWrite::setFontSize(float size)
{
    _fontSize = size;
    if (_sprite) {
        _sprite->setTextSize(_fontSize);
    }
}

// フォントを設定するメソッドの修正
void TypoWrite::setFont(const lgfx::IFont *font)
{
    _font = font;
    // フォントがデフォルトフォント (fonts::Font0) かどうかをチェック
    if (_font == &fonts::Font0)
    {
        _isCustomFont = false;
    }
    // 他のデフォルトフォントもチェック
    else if (_font == &fonts::Font2 ||
             _font == &fonts::Font4 ||
             _font == &fonts::Font6 ||
             _font == &fonts::Font7 ||
             _font == &fonts::Font8 ||
             _font == &fonts::DejaVu9 ||
             _font == &fonts::DejaVu12 ||
             _font == &fonts::DejaVu18 ||
             _font == &fonts::DejaVu24 ||
             _font == &fonts::DejaVu40 ||
             _font == &fonts::DejaVu56 ||
             _font == &fonts::DejaVu72 ||
             _font == &fonts::TomThumb ||
             _font == &fonts::lgfxJapanMincho_8 ||
             _font == &fonts::lgfxJapanMincho_12 ||
             _font == &fonts::lgfxJapanMincho_16 ||
             _font == &fonts::lgfxJapanMincho_20 ||
             _font == &fonts::lgfxJapanMincho_24 ||
             _font == &fonts::lgfxJapanMincho_28 ||
             _font == &fonts::lgfxJapanMincho_32 ||
             _font == &fonts::lgfxJapanMincho_36 ||
             _font == &fonts::lgfxJapanMincho_40 ||
             _font == &fonts::lgfxJapanGothic_8 ||
             _font == &fonts::lgfxJapanGothic_12 ||
             _font == &fonts::lgfxJapanGothic_16 ||
             _font == &fonts::lgfxJapanGothic_20 ||
             _font == &fonts::lgfxJapanGothic_24 ||
             _font == &fonts::lgfxJapanGothic_28 ||
             _font == &fonts::lgfxJapanGothic_32 ||
             _font == &fonts::lgfxJapanGothic_36 ||
             _font == &fonts::lgfxJapanGothic_40)
    {
        _isCustomFont = false;
    }
    else
    {
        // 標準フォント以外はカスタムフォントとみなす
        _isCustomFont = true;
    }
}

// 行間を設定
void TypoWrite::setLineSpacing(int spacing)
{
    _lineSpacing = spacing;
}

// 文字間を設定
void TypoWrite::setCharSpacing(int spacing)
{
    _charSpacing = spacing;
}

// テキスト折り返しを設定
void TypoWrite::setWrap(bool wrap)
{
    _wrap = wrap;
}

// フォントをバイト配列から読み込む
bool TypoWrite::loadFontFromArray(const uint8_t *fontArray)
{
    if (!_display)
        return false;

    // M5GFXにフォントを読み込む
    bool result = _display->loadFont(fontArray);
    if (result)
    {
        // 読み込んだフォントを現在のフォントとして設定
        _font = _display->getFont();
        _isCustomFont = true; // カスタムフォントとして設定
        ESP_LOGI(TAG, "Font loaded successfully from array");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to load font from array");
    }

    return result;
}

// テキスト描画（メイン関数）
void TypoWrite::drawText(const std::string &text)
{
    if (!_display || text.empty() || !_font)
    {
        return;
    }

    // 方向に応じて描画処理を切り替え
    if (_direction == TextDirection::HORIZONTAL)
    {
        drawHorizontalText(text, _x, _y);
    }
    else
    {
        drawVerticalText(text, _x, _y);
    }
}

// 中央揃えでテキスト描画
void TypoWrite::drawTextCentered(const std::string &text)
{
    if (!_display || text.empty() || !_font)
    {
        return;
    }

    // テキストのサイズを計算
    int textWidth = 0;
    int textHeight = 0;
    calculateTextSize(text, textWidth, textHeight);

    // 中央位置を計算
    int centerX = _x;
    int centerY = _y;

    if (_direction == TextDirection::HORIZONTAL)
    {
        // 横書きの場合は横方向に中央揃え
        centerX = _x + (_width - textWidth) / 2;
        drawHorizontalText(text, centerX, centerY);
    }
    else
    {
        // 縦書きの場合は縦方向に中央揃え
        centerY = _y + (_height - textHeight) / 2;
        drawVerticalText(text, centerX, centerY);
    }
}

// テキスト描画のためのディスプレイ設定を行う
void TypoWrite::setupDisplay()
{
    if (!_display || !_font)
        return;

    _display->setFont(_font);
    if (_transparentBg)
    {
        _display->setTextColor(_color);
    }
    else
    {
        _display->setTextColor(_color, _bgColor);
    }
    _display->setTextSize(_fontSize);
}

// スプライト操作を安全に行うヘルパーメソッド
void TypoWrite::withSprite(int width, int height, std::function<void(lgfx::LGFX_Sprite *)> operation)
{
    lgfx::LGFX_Sprite *sprite = new lgfx::LGFX_Sprite(_display);
    if (sprite->createSprite(width, height))
    {
        // スプライトの設定
        if (_transparentBg)
        {
            sprite->fillScreen(0);
            sprite->setTextColor(_color);
        }
        else
        {
            sprite->fillScreen(_bgColor);
            sprite->setTextColor(_color, _bgColor);
        }
        sprite->setFont(_font);
        sprite->setTextSize(_fontSize);

        // 操作実行
        operation(sprite);

        // スプライト削除
        sprite->deleteSprite();
    }
    delete sprite;
}

// 横書きテキスト描画
void TypoWrite::drawHorizontalText(const std::string &text, int x, int y, bool measure_only)
{
    if (!_display || !_font)
        return;

    // 現在の描画設定を保存
    if (!measure_only)
    {
        setupDisplay();
    }

    // UTF-8文字列をUnicodeコードポイントに変換
    std::vector<uint16_t> unicode_chars = utf8ToUnicode(text);

    int current_x = x;
    int current_y = y;
    int line_height = getFontHeight() + _lineSpacing;

    for (size_t i = 0; i < unicode_chars.size(); i++)
    {
        uint16_t unicode_char = unicode_chars[i];

        // 改行文字の処理
        if (unicode_char == '\n')
        {
            current_x = x;
            current_y += line_height;
            continue;
        }

        // 文字の幅を取得
        int char_width = getCharacterWidth(unicode_char);

        // 折り返しの処理
        if (_wrap && current_x + char_width > x + _width)
        {
            current_x = x;
            current_y += line_height;
        }

        // 描画処理（measure_onlyでない場合のみ）の部分を修正
        if (!measure_only)
        {
            if (getCharCategory(unicode_char) != CharCategory::NORMAL)
            {
                drawSpecialChar(unicode_char, current_x, current_y);
            }
            else
            {
                // 通常の文字描画
                std::string utf8_str = unicodeToUtf8(unicode_char);
                _display->drawString(utf8_str.c_str(), current_x, current_y, _font);
            }
        }

        // 次の文字位置へ
        current_x += char_width + _charSpacing;
    }
}

// 縦書きテキスト描画
void TypoWrite::drawVerticalText(const std::string &text, int x, int y, bool measure_only)
{
    if (!_display || !_font)
        return;

    // 現在の描画設定を保存
    if (!measure_only)
    {
        _display->setFont(_font);
        if (_transparentBg)
        {
            _display->setTextColor(_color); // 背景色省略で透明に
        }
        else
        {
            _display->setTextColor(_color, _bgColor);
        }
        _display->setTextSize(_fontSize);
    }

    // UTF-8文字列をUnicodeコードポイントに変換
    std::vector<uint16_t> unicode_chars = utf8ToUnicode(text);

    // 基本列幅の設定
    int base_column_width = getFontWidth();

    // 最初の列の文字の幅を先に計算（可能であれば）
    int first_column_width = base_column_width;
    if (!unicode_chars.empty())
    {
        uint16_t first_char = unicode_chars[0];
        if (first_char != '\n')
        {
            updateMetricsForChar(first_char);
            if (_metrics.width > 0)
            {
                first_column_width = _metrics.width * _fontSize;
            }
        }
    }

    // 描画位置の修正 - 最初の列を描画領域内に収める
    // 列の中心が描画領域の右端から列間隔/2だけ内側に来るように設定
    int current_x = x + _width - (first_column_width) - (_lineSpacing);
    int current_y = y;

    // デバッグログ - 描画開始位置の確認
    ESP_LOGI(TAG, "Vertical text start position: x=%d, y=%d (area: x=%d, width=%d)",
             current_x, current_y, x, _width);

    // 現在の列で最大の文字幅を追跡
    int current_column_max_width = first_column_width;

    for (size_t i = 0; i < unicode_chars.size(); i++)
    {
        uint16_t unicode_char = unicode_chars[i];

        // 改行文字の処理（縦書きの場合は次の列に移動）
        if (unicode_char == '\n')
        {
            // 現在の列の最大幅 + 行間隔で次の列位置を計算
            int column_width = current_column_max_width + _lineSpacing;
            current_x -= column_width;    // 次の列（左側）に移動
            current_y = y;                // Y座標をリセット
            current_column_max_width = 0; // 新しい列の最大幅をリセット
            continue;
        }

        // 文字のメトリクスを取得
        updateMetricsForChar(unicode_char);

        // 文字の幅と高さを取得
        int char_width = (_metrics.width > 0) ? _metrics.width * _fontSize : base_column_width;
        int char_height = (_metrics.height) * _fontSize;
        //int char_height = getCharacterHeight(unicode_char) * _fontSize;

        // 現在の列の最大幅を更新
        current_column_max_width = std::max(current_column_max_width, char_width);

        // 折り返しの処理
        if (_wrap && current_y + char_height > y + _height)
        {
            // 現在の列の最大幅 + 行間隔で次の列位置を計算
            int column_width = current_column_max_width + _lineSpacing;
            current_x -= column_width;             // 次の列（左側）に移動
            current_y = y;                         // Y座標をリセット
            current_column_max_width = char_width; // 新しい列の最大幅を初期化
        }

        // 描画処理（measure_onlyでない場合のみ）
        CharCategory category = getCharCategory(unicode_char);
        if (!measure_only)
        {
            bool needRotation = shouldRotateInVertical(unicode_char);
            std::string utf8_str = unicodeToUtf8(unicode_char);
            if (_isCustomFont)
            {
                if (category == CharCategory::BRACKET || category == CharCategory::HORIZONTAL_BAR || category == CharCategory::PUNCTUATION)
                {
                    // 特殊文字の描画
                    drawSpecialChar(unicode_char, current_x, current_y);
                }
                else if (category == CharCategory::OTHER_SPECIAL || needRotation)
                {
                    // 回転が必要な文字（英数字など）
                    drawRotatedCharacter(utf8_str, current_x, current_y, char_height);
                }
                else
                {
                    // 回転が不要な文字（漢字・ひらがな・カタカナなど）
                    _display->drawString(utf8_str.c_str(), current_x, current_y, _font);
                }
            }
            else
            {
                if (category == CharCategory::BRACKET || category == CharCategory::HORIZONTAL_BAR || needRotation)
                {
                    // 回転が必要な文字（英数字など）
                    drawRotatedCharacter(utf8_str, current_x, current_y, char_height);
                }
                else
                {
                    // 回転が不要な文字（漢字・ひらがな・カタカナなど）
                    _display->drawString(utf8_str.c_str(), current_x, current_y, _font);
                }
            }
        }

        // 次の文字位置へ
        current_y += char_height + _charSpacing;
    }
}

// Unicode文字をUTF-8文字列に変換する
std::string TypoWrite::unicodeToUtf8(uint16_t unicode_char)
{
    char utf8_buf[5] = {0};

    if (unicode_char < 0x80)
    {
        // ASCII文字 (1バイト)
        utf8_buf[0] = (char)unicode_char;
    }
    else if (unicode_char < 0x800)
    {
        // 2バイト文字
        utf8_buf[0] = 0xC0 | ((unicode_char >> 6) & 0x1F);
        utf8_buf[1] = 0x80 | (unicode_char & 0x3F);
    }
    else
    {
        // 3バイト文字
        utf8_buf[0] = 0xE0 | ((unicode_char >> 12) & 0x0F);
        utf8_buf[1] = 0x80 | ((unicode_char >> 6) & 0x3F);
        utf8_buf[2] = 0x80 | (unicode_char & 0x3F);
    }

    return std::string(utf8_buf);
}

// UTF-8文字列をUnicodeコードポイントに変換
std::vector<uint16_t> TypoWrite::utf8ToUnicode(const std::string &utf8_string)
{
    std::vector<uint16_t> unicode_chars;

    for (size_t i = 0; i < utf8_string.length(); i++)
    {
        uint8_t c = utf8_string[i];

        if (c < 0x80)
        {
            // ASCII文字
            unicode_chars.push_back(c);
        }
        else if (c < 0xE0)
        {
            // 2バイト文字
            if (i + 1 < utf8_string.length())
            {
                uint16_t unicode = ((c & 0x1F) << 6) | (utf8_string[i + 1] & 0x3F);
                unicode_chars.push_back(unicode);
                i++;
            }
        }
        else if (c < 0xF0)
        {
            // 3バイト文字
            if (i + 2 < utf8_string.length())
            {
                uint16_t unicode = ((c & 0x0F) << 12) | ((utf8_string[i + 1] & 0x3F) << 6) | (utf8_string[i + 2] & 0x3F);
                unicode_chars.push_back(unicode);
                i += 2;
            }
        }
        else
        {
            // 4バイト文字 (UTF-16では2つのコードポイントになるため、ここではスキップ)
            i += 3;
        }
    }

    return unicode_chars;
}

// 特定の文字のメトリクス情報を更新するヘルパー関数
bool TypoWrite::updateMetricsForChar(uint16_t unicode_char) const
{
    if (!_font)
        return false;

    // デフォルトのメトリクスを取得
    _font->getDefaultMetric(&_metrics);

    // 指定された文字のメトリクスを更新
    return _font->updateFontMetric(&_metrics, unicode_char);
}

// フォントの標準幅を取得する関数の修正
int32_t TypoWrite::getFontWidth()
{
    if (!_font)
        return 0;

    // 空白文字（スペース）のメトリクスを取得
    if (updateMetricsForChar(' '))
    {
        return _metrics.width * _fontSize;
    }

    // スペースのメトリクスが取得できない場合は高さと同等と仮定
    return getFontHeight();
}

// フォントの高さを取得する関数の修正
int32_t TypoWrite::getFontHeight()
{
    if (!_font)
        return 0;

    // フォントの標準的なメトリクスを取得
    _font->getDefaultMetric(&_metrics);
    return (_metrics.height) * _fontSize;
    //return 2;
}

// 文字の幅を取得する関数の修正
int32_t TypoWrite::getCharacterWidth(uint16_t unicode_char)
{
    if (!_font)
        return 0;

    // 改行文字の場合は幅0
    if (unicode_char == '\n')
        return 0;

    // 特殊文字の場合は個別に幅を計算
    /*
    if (isSpecialChar(unicode_char)) {
        // 特殊文字も一旦メトリクスを取得してみる
        if (updateMetricsForChar(unicode_char)) {
            return _metrics.width * _fontSize;
        }
        // 取得できない場合はフォント標準幅を返す
        return getFontWidth();
    }
    */

    // 一般的な文字のメトリクスを取得
    if (updateMetricsForChar(unicode_char))
    {
        // x_advanceを優先（実際に次の文字が配置される位置）
        if (_metrics.x_advance > 0)
        {
            return (_metrics.x_advance + _metrics.x_offset) * _fontSize;
        }
        return _metrics.width * _fontSize;
    }

    // メトリクス取得失敗時はフォント標準幅を返す
    ESP_LOGW(TAG, "Failed to get metrics for character U+%04X", unicode_char);
    return getFontWidth();
}

// 文字の高さを取得する関数の修正
int32_t TypoWrite::getCharacterHeight(uint16_t unicode_char)
{
    if (!_font)
        return 0;

    // 改行文字の場合は高さ0
    if (unicode_char == '\n')
        return 0;

    // 文字のメトリクスを取得
    if (updateMetricsForChar(unicode_char))
    {
        if (_metrics.y_advance > 0)
        {
            return (_metrics.y_advance + _metrics.y_offset) * _fontSize;
        }
        return _metrics.height * _fontSize;
    }

    // メトリクス取得失敗時はフォント標準高さを返す
    return getFontHeight();
}

// 文字のカテゴリーを判定
CharCategory TypoWrite::getCharCategory(uint16_t unicode_char)
{
    // 括弧類
    static const uint16_t brackets[] = {
        '(', ')', '[', ']', '{', '}', '<', '>',
        0xFF08, 0xFF09, // （）
        0x300C, 0x300D, // 「」
        0x300E, 0x300F, // 『』
        0x3010, 0x3011  // 【】
    };

    // 横棒・長音記号類
    static const uint16_t horizontal_bars[] = {
        0x2014, 0x2015, 0xFF0D, 0x30FC // ダッシュ、全角ハイフン、長音記号
    };

    // 句読点類
    static const uint16_t punctuations[] = {
        0x3001, 0x3002,           // 読点、句点
        '!', '?', 0xFF01, 0xFF1F, // 感嘆符、疑問符
        ':', ';', 0xFF1A, 0xFF1B  // コロン、セミコロン
    };

    // その他の特殊文字類
    static const uint16_t other_special_chars[] = {
        // 追加の特殊文字があればここに追加
        '@', '#', '$', '%', '&', '*', '+', '=', '<', '>', '/', '\\', '-'};

    // 各カテゴリーをチェック
    for (uint16_t ch : brackets)
    {
        if (unicode_char == ch)
            return CharCategory::BRACKET;
    }

    for (uint16_t ch : horizontal_bars)
    {
        if (unicode_char == ch)
            return CharCategory::HORIZONTAL_BAR;
    }

    for (uint16_t ch : punctuations)
    {
        if (unicode_char == ch)
            return CharCategory::PUNCTUATION;
    }

    // その他の特殊文字
    for (uint16_t ch : other_special_chars)
    {
        if (unicode_char == ch)
            return CharCategory::OTHER_SPECIAL;
    }
    return CharCategory::NORMAL;
}

// 特殊文字の描画
void TypoWrite::drawSpecialChar(uint16_t unicode_char, int x, int y)
{
    // 縦書きモードか横書きモードかで処理を分ける
    if (_direction == TextDirection::VERTICAL)
    {
        uint16_t vertical_code = unicode_char;
        switch (unicode_char)
        {
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
        _display->drawString(unicodeToUtf8(vertical_code).c_str(), x, y, _font);
    }
    else
    {
        // 横書きモードでは通常描画
        _display->drawString(unicodeToUtf8(unicode_char).c_str(), x, y, _font);
    }
}

// 縦書きで回転が必要な文字かどうかを判定
bool TypoWrite::shouldRotateInVertical(uint16_t unicode_char)
{
    // ASCII文字（英数字記号）は回転する
    if (unicode_char < 0x80)
    {
        return true;
    }

    // 全角英数字も回転する
    if ((unicode_char >= 0xFF01 && unicode_char <= 0xFF5E) ||
        (unicode_char >= 0xFF61 && unicode_char <= 0xFF9F))
    {
        return true;
    }

    // 日本語文字（漢字・ひらがな・カタカナ）は回転しない
    if ((unicode_char >= 0x3040 && unicode_char <= 0x30FF) || // ひらがな・カタカナ
        (unicode_char >= 0x4E00 && unicode_char <= 0x9FFF))
    { // 漢字
        return false;
    }

    // デフォルトでは回転しない
    return false;
}

// テキスト描画サイズの計算
void TypoWrite::calculateTextSize(const std::string &text, int &width, int &height)
{
    width = 0;
    height = 0;

    if (_direction == TextDirection::HORIZONTAL)
    {
        // 横書きの場合
        drawHorizontalText(text, 0, 0, true);
        // 横書きテキストの幅と高さを計算
        std::vector<uint16_t> unicode_chars = utf8ToUnicode(text);

        int line_width = 0;
        int max_width = 0;
        int line_count = 1;

        for (size_t i = 0; i < unicode_chars.size(); i++)
        {
            uint16_t unicode_char = unicode_chars[i];

            if (unicode_char == '\n')
            {
                // 改行文字の処理
                max_width = std::max(max_width, line_width);
                line_width = 0;
                line_count++;
                continue;
            }

            // 文字の幅を取得
            int char_width = getCharacterWidth(unicode_char);

            // 折り返しの処理
            if (_wrap && _width > 0 && line_width + char_width > _width)
            {
                max_width = std::max(max_width, line_width);
                line_width = 0;
                line_count++;
            }

            line_width += char_width + _charSpacing;
        }

        max_width = std::max(max_width, line_width);

        width = max_width;
        height = line_count * (getFontHeight() + _lineSpacing);
    }
    else
    {
        // 縦書きの場合
        drawVerticalText(text, 0, 0, true);
        // 縦書きテキストの幅と高さを計算
        std::vector<uint16_t> unicode_chars = utf8ToUnicode(text);

        int column_height = 0;
        int max_height = 0;
        int column_count = 1;

        for (size_t i = 0; i < unicode_chars.size(); i++)
        {
            uint16_t unicode_char = unicode_chars[i];

            if (unicode_char == '\n')
            {
                // 改行文字の処理（縦書きの場合は次の列に移動）
                max_height = std::max(max_height, column_height);
                column_height = 0;
                column_count++;
                continue;
            }

            // 文字の高さを取得
            //            int char_height = getCharacterHeight(unicode_char);
            int char_height = getCharacterHeight(unicode_char);

            // 折り返しの処理
            if (_wrap && _height > 0 && column_height + char_height > _height)
            {
                max_height = std::max(max_height, column_height);
                column_height = 0;
                column_count++;
            }

            column_height += char_height + _charSpacing;
        }

        max_height = std::max(max_height, column_height);

        width = column_count * (getFontWidth() + _lineSpacing);
        height = max_height;
    }
}

// 縦書きで回転が必要な文字を描画する
void TypoWrite::drawRotatedCharacter(const std::string &utf8_str, int x, int y, int char_height)
{
    // 文字の幅を計算
    int charWidth = _display->textWidth(utf8_str.c_str());

    // スプライトサイズを決定（文字の幅と高さの大きい方+余白）
    int charSize = (int)((char_height > charWidth ? char_height : charWidth) + 4);

    // スプライトを使用して回転描画
    withSprite(charSize, charSize, [this, &utf8_str, x, y, char_height, charWidth, charSize](lgfx::LGFX_Sprite *sprite)
               {
        // スプライトの中央に文字を描画
        int cx = (charSize - charWidth) / 2;
        int cy = (charSize - char_height) / 2;
        sprite->drawString(utf8_str.c_str(), cx, cy);

        //sprite->setPivot(charWidth/2, char_height/2);
        // スプライトを90度回転して描画
        sprite->pushRotateZoom(_display, x + char_height/2, y + char_height/2,
                               90, 1.0, 1.0, _bgColor); });
}
