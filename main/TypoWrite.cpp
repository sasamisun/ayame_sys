// main/TypoWrite.cpp
#include "TypoWrite.hpp"
#include "esp_log.h"

// ログタグ
static const char *TAG = "TYPO_WRITE";

// コンストラクタ
TypoWrite::TypoWrite(M5GFX *display) : _display(display),
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
                                       _lineSpacing(4),
                                       _charSpacing(2),
                                       _wrap(true),
                                       _transparentBg(true),
                                       _isCustomFont(false)
{
    // デフォルトフォントを設定
    if (display)
    {
        _font = display->getFont();
        _isCustomFont = false;
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
}

// テキスト色を設定
void TypoWrite::setColor(uint16_t color)
{
    _color = color;
}

// 背景色を設定
void TypoWrite::setBackgroundColor(uint16_t bgColor)
{
    _bgColor = bgColor;
}

// フォントサイズを設定
void TypoWrite::setFontSize(float size)
{
    _fontSize = size;
}

// フォントを設定
void TypoWrite::setFont(const lgfx::IFont *font)
{
    _font = font;
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

// 背景色透明？
void TypoWrite::setTransparentBg(bool transparent)
{
    _transparentBg = transparent;
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

// 横書きテキスト描画
void TypoWrite::drawHorizontalText(const std::string &text, int x, int y, bool measure_only)
{
    if (!_display || !_font)
        return;

    // 現在の描画設定を保存
    if (!measure_only)
    {
        _display->setFont(_font);
        // 背景色を透明にするか設定に応じて切り替え
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

        // 特殊文字の処理
        if (isSpecialChar(unicode_char))
        {
            if (!measure_only)
            {
                drawSpecialChar(unicode_char, current_x, current_y);
            }
        }
        else
        {
            // 通常の文字描画
            if (!measure_only)
            {
                char utf8_buf[5] = {0};
                // Unicodeコードポイントを一時的なUTF-8文字列に変換
                // 簡易的な変換なので、実際はUTF-8エンコード関数を使用するべき
                if (unicode_char < 0x80)
                {
                    utf8_buf[0] = (char)unicode_char;
                }
                else if (unicode_char < 0x800)
                {
                    utf8_buf[0] = 0xC0 | ((unicode_char >> 6) & 0x1F);
                    utf8_buf[1] = 0x80 | (unicode_char & 0x3F);
                }
                else
                {
                    utf8_buf[0] = 0xE0 | ((unicode_char >> 12) & 0x0F);
                    utf8_buf[1] = 0x80 | ((unicode_char >> 6) & 0x3F);
                    utf8_buf[2] = 0x80 | (unicode_char & 0x3F);
                }

                _display->drawString(utf8_buf, current_x, current_y);
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
        // 背景色を透明にするか設定に応じて切り替え
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

    int column_width = getFontWidth() + _lineSpacing;
    int current_x = x + _width - column_width;
    int current_y = y;

    for (size_t i = 0; i < unicode_chars.size(); i++)
    {
        uint16_t unicode_char = unicode_chars[i];

        // 改行文字の処理（縦書きの場合は次の列に移動）
        if (unicode_char == '\n')
        {
            current_x -= column_width;
            current_y = y;
            continue;
        }

        // 文字の高さを取得
        int char_height = getCharacterHeight(unicode_char);

        // 折り返しの処理
        if (_wrap && current_y + char_height > y + _height)
        {
            current_x -= column_width;
            current_y = y;
        }

        // 特殊文字の処理（縦書き用）
        if (isSpecialChar(unicode_char))
        {
            if (!measure_only)
            {
                // 縦書き用の特殊文字描画
                // 例: ハイフン、括弧などは90度回転させるなど
                drawSpecialChar(unicode_char, current_x, current_y);
            }
        }
        else
        {
            // 通常の文字描画（90度回転）
            if (!measure_only)
            {
                char utf8_buf[5] = {0};
                // Unicodeコードポイントを一時的なUTF-8文字列に変換
                if (unicode_char < 0x80)
                {
                    utf8_buf[0] = (char)unicode_char;
                }
                else if (unicode_char < 0x800)
                {
                    utf8_buf[0] = 0xC0 | ((unicode_char >> 6) & 0x1F);
                    utf8_buf[1] = 0x80 | (unicode_char & 0x3F);
                }
                else
                {
                    utf8_buf[0] = 0xE0 | ((unicode_char >> 12) & 0x0F);
                    utf8_buf[1] = 0x80 | ((unicode_char >> 6) & 0x3F);
                    utf8_buf[2] = 0x80 | (unicode_char & 0x3F);
                }

                // 文字種によって処理を分ける
                bool needRotation = shouldRotateInVertical(unicode_char);

                if (needRotation)
                {
                    // 回転が必要な文字（英数字など）
                    // スプライトを使用して回転表示
                    int32_t charWidth = getCharacterWidth(unicode_char);
                    int charSize = (int)((char_height > charWidth ? char_height : charWidth) + 4);
                    lgfx::LGFX_Sprite *charSprite = new lgfx::LGFX_Sprite(_display);
                    if (charSprite->createSprite(charSize, charSize))
                    {

                        charSprite->fillScreen(_bgColor);
                        charSprite->setTextColor(_color, _bgColor);
                        charSprite->setFont(_font);
                        charSprite->setTextSize(_fontSize);

                        // スプライトの中央に文字を描画
                        int cx = (charSize - charWidth) / 2;
                        int cy = (charSize - char_height) / 2;
                        charSprite->drawString(utf8_buf, cx, cy);

                        // スプライトを90度回転して描画
                        charSprite->pushRotateZoom(_display, current_x + char_height / 2, current_y + char_height / 2,
                                                   90, 1.0, 1.0, _bgColor);

                        // スプライトを解放
                        charSprite->deleteSprite();
                    }
                    delete charSprite;
                }
                else
                {
                    // 回転が不要な文字（漢字・ひらがな・カタカナなど）
                    // 直接描画
                    _display->drawString(utf8_buf, current_x, current_y);
                }
            }
        }

        // 次の文字位置へ
        current_y += char_height + _charSpacing;
    }
}

// フォントの標準幅を取得
int32_t TypoWrite::getFontWidth()
{
    if (!_font)
        return 0;

    // フォントの標準幅を見積もる（実際のフォントによって異なるため近似値）
    // 漢字の場合は高さと同じくらいの幅になることが多い
    return getFontHeight();
}

// フォントの高さを取得
int32_t TypoWrite::getFontHeight()
{
    if (!_font)
        return 0;

    // M5GFXのフォント高さを取得
    // テキスト描画前に一時的にディスプレイに設定して高さを取得
    _display->setFont(_font);
    _display->setTextSize(_fontSize);
    return _display->fontHeight();
}

// 文字の幅を取得
int32_t TypoWrite::getCharacterWidth(uint16_t unicode_char)
{
    if (!_font)
        return 0;

    // 改行文字の場合は幅0
    if (unicode_char == '\n')
        return 0;

    // 特殊文字の場合は個別に幅を計算
    /*
    if (isSpecialChar(unicode_char))
    {
        // 特殊文字の幅を返す実装
        // 仮の実装として標準的な文字幅を返す
        return getFontWidth();
    }
    //*/

    // フォントから文字の幅を取得
    _display->setFont(_font);
    _display->setTextSize(_fontSize);

    // UTF-8に変換して幅を取得
    char utf8_buf[5] = {0};
    if (unicode_char < 0x80)
    {
        utf8_buf[0] = (char)unicode_char;
    }
    else if (unicode_char < 0x800)
    {
        utf8_buf[0] = 0xC0 | ((unicode_char >> 6) & 0x1F);
        utf8_buf[1] = 0x80 | (unicode_char & 0x3F);
    }
    else
    {
        utf8_buf[0] = 0xE0 | ((unicode_char >> 12) & 0x0F);
        utf8_buf[1] = 0x80 | ((unicode_char >> 6) & 0x3F);
        utf8_buf[2] = 0x80 | (unicode_char & 0x3F);
    }

    return _display->textWidth(utf8_buf) + 1; // 端数を考慮して+1
}

// 文字の高さを取得
int32_t TypoWrite::getCharacterHeight(uint16_t unicode_char)
{
    if (!_font)
        return 0;

    // 改行文字の場合は高さ0
    if (unicode_char == '\n')
        return 0;

    // フォントから文字の高さを取得
    return getFontHeight();
}

// 特殊文字かどうかを判定
bool TypoWrite::isSpecialChar(uint16_t unicode_char)
{
    // 縦書きで特別な処理が必要な文字のリスト
    // 括弧、句読点、記号など
    static const uint16_t special_chars[] = {
        '(', ')', '[', ']', '{', '}',
        0x300C, 0x300D,                      // 「」
        0x300E, 0x300F,                      // 『』
        0x3010, 0x3011,                      // 【】
        '-', 0x2014, 0x2015, 0xFF0D, 0x30FC, // ハイフン、ダッシュ、全角ハイフン、長音記号
        0x3001, 0x3002,                      // 読点、句点
        '!', '?', 0xFF01, 0xFF1F,            // 感嘆符、疑問符
        ':', ';', 0xFF1A, 0xFF1B             // コロン、セミコロン
    };

    const size_t special_char_count = sizeof(special_chars) / sizeof(special_chars[0]);

    for (size_t i = 0; i < special_char_count; i++)
    {
        if (unicode_char == special_chars[i])
        {
            return true;
        }
    }

    return false;
}

// 特殊文字の描画
void TypoWrite::drawSpecialChar(uint16_t unicode_char, int x, int y)
{
    // 縦書きモードかどうかで処理を分ける
    if (_direction == TextDirection::VERTICAL)
    {
        // 縦書きモードでの特殊文字描画

        // 文字の幅と高さを取得
        int char_width = getFontWidth();
        int char_height = getFontHeight();

        // スプライトを作成して文字を描画
        lgfx::LGFX_Sprite *charSprite = new lgfx::LGFX_Sprite(_display);
        if (charSprite->createSprite(char_width, char_height))
        {
            // スプライトの背景を透明にする場合は、透明色を設定
            if (_transparentBg)
            {
                // スプライトの透明色を設定
                charSprite->fillScreen(0);        // 透明色として扱う背景色
                charSprite->setTextColor(_color); // 背景色省略で透明に
            }
            else
            {
                charSprite->fillScreen(_bgColor);
                charSprite->setTextColor(_color);
            }
            charSprite->setFont(_font);
            charSprite->setTextSize(_fontSize);

            // 文字コードに応じて回転や位置調整を行う
            switch (unicode_char)
            {

            case '(':    // 左括弧 → 上括弧
            case ')':    // 右括弧 → 下括弧
            case '[':    // 左角括弧 → 上角括弧
            case ']':    // 右角括弧 → 下角括弧
            case '{':    // 左波括弧 → 上波括弧
            case '}':    // 右波括弧 → 下波括弧
            case 0x300E: // 『
            case 0x300F: // 』
            case 0x3010: // 【
            case 0x3011: // 】
            {
                // 括弧類は90度回転
                // UTF-8文字列に変換
                char utf8_buf[4] = {0};
                if (unicode_char < 0x80)
                {
                    utf8_buf[0] = (char)unicode_char;
                }
                else if (unicode_char < 0x800)
                {
                    utf8_buf[0] = 0xC0 | ((unicode_char >> 6) & 0x1F);
                    utf8_buf[1] = 0x80 | (unicode_char & 0x3F);
                }
                else
                {
                    utf8_buf[0] = 0xE0 | ((unicode_char >> 12) & 0x0F);
                    utf8_buf[1] = 0x80 | ((unicode_char >> 6) & 0x3F);
                    utf8_buf[2] = 0x80 | (unicode_char & 0x3F);
                }

                // 文字の正確なサイズを取得
                int32_t actual_char_width = getCharacterWidth(unicode_char);
                int32_t actual_char_height = getCharacterHeight(unicode_char);

                // スプライトサイズを文字サイズより大きく設定
                int sprite_size = std::max(actual_char_width, actual_char_height);

                int32_t baseline = (sprite_size) / 4; // 75%の位置
                int32_t baseline_offset = _isCustomFont ? baseline : 0;

                lgfx::LGFX_Sprite *charSprite = new lgfx::LGFX_Sprite(_display);
                if (charSprite->createSprite(sprite_size, sprite_size))
                {
                    // スプライトの背景を透明にする場合は、透明色を設定
                    if (_transparentBg)
                    {
                        charSprite->fillScreen(0);        // 透明色として扱う背景色
                        charSprite->setTextColor(_color); // 背景色省略で透明に
                    }
                    else
                    {
                        charSprite->fillScreen(_bgColor);
                        charSprite->setTextColor(_color);
                    }
                    charSprite->setFont(_font);
                    charSprite->setTextSize(_fontSize);

                    // 文字の実際のバウンディングボックスを考慮した描画位置
                    // 文字をスプライト中央に配置
                    int start_x = (sprite_size - actual_char_width) / 2;
                    int start_y = (sprite_size - actual_char_height) / 2;

                    // 文字によってはベースライン以下に描画される部分があるので、
                    // より安全な位置に描画する
                    // start_x = 0<start_x ? start_x:0;
                    // start_y = 0<start_y ? start_y:0;

                    // start_x+=10;
                    // start_y+=10;
                    // 文字を描画
                    charSprite->drawString(utf8_buf, start_x, start_y);

                    // スプライトを90度回転して描画
                    // 回転中心を文字の中心に設定
                    int dest_x = x + actual_char_height / 2;
                    int dest_y = y + actual_char_height / 2;
                    charSprite->setPivot(actual_char_height / 2, actual_char_height / 2);
                    charSprite->pushRotateZoom(_display, x, y,
                                               0, 1.0, 1.0, _bgColor);

                    // スプライトを解放
                    charSprite->deleteSprite();
                }
                delete charSprite;
            }
            break;

            case 0x300C: // 「
            case 0x300D: // 」
            {
                uint16_t vertical_code = 0;
                if (unicode_char == 0x300C)
                    vertical_code = 0xFE41;
                if (unicode_char == 0x300D)
                    vertical_code = 0xFE42;
                // UTF-8文字列に変換
                char utf8_buf[4] = {0};
                if (vertical_code < 0x80)
                {
                    utf8_buf[0] = (char)vertical_code;
                }
                else if (vertical_code < 0x800)
                {
                    utf8_buf[0] = 0xC0 | ((vertical_code >> 6) & 0x1F);
                    utf8_buf[1] = 0x80 | (vertical_code & 0x3F);
                }
                else
                {
                    utf8_buf[0] = 0xE0 | ((vertical_code >> 12) & 0x0F);
                    utf8_buf[1] = 0x80 | ((vertical_code >> 6) & 0x3F);
                    utf8_buf[2] = 0x80 | (vertical_code & 0x3F);
                }
                _display->drawString(utf8_buf, x + char_width / 4, y);
            }
            break;

            case '-': // ハイフン
            case 0x2015:
            case 0x2014: // ダッシュ
            case 0xFF0D: // 全角ハイフン
            case 0x30FC: // 長音記号
            {
                // 横棒は縦棒に変換
                // 縦線を描画
                int lineX = x + char_width / 2;
                _display->drawFastVLine(lineX, y, char_height, _color);
            }
            break;

            case 0x3002: // 句点
            case 0x3001: // 読点
            {
                // 句読点は右にずらして描画
                char utf8_buf[4] = {0};
                if (unicode_char == 0x3002)
                { // 句点
                    utf8_buf[0] = 0xE3;
                    utf8_buf[1] = 0x80;
                    utf8_buf[2] = 0x82;
                }
                else
                { // 読点（、）
                    utf8_buf[0] = 0xE3;
                    utf8_buf[1] = 0x80;
                    utf8_buf[2] = 0x81;
                }

                _display->drawString(utf8_buf, x + char_width / 4, y);
            }
            break;

            default:
            {
                // その他の特殊文字は通常描画
                char utf8_buf[4] = {0};
                if (unicode_char > 0x7F)
                {
                    // マルチバイト文字の場合
                    if (unicode_char < 0x800)
                    {
                        utf8_buf[0] = 0xC0 | ((unicode_char >> 6) & 0x1F);
                        utf8_buf[1] = 0x80 | (unicode_char & 0x3F);
                    }
                    else
                    {
                        utf8_buf[0] = 0xE0 | ((unicode_char >> 12) & 0x0F);
                        utf8_buf[1] = 0x80 | ((unicode_char >> 6) & 0x3F);
                        utf8_buf[2] = 0x80 | (unicode_char & 0x3F);
                    }
                }
                else
                {
                    utf8_buf[0] = (char)unicode_char;
                }
                _display->drawString(utf8_buf, x, y);
            }
            break;
            }

            // スプライトを解放
            charSprite->deleteSprite();
        }
        delete charSprite;
    }
    else
    {
        // 横書きモードでは通常描画
        char utf8_buf[4] = {0};
        if (unicode_char > 0x7F)
        {
            // マルチバイト文字の場合
            if (unicode_char < 0x800)
            {
                utf8_buf[0] = 0xC0 | ((unicode_char >> 6) & 0x1F);
                utf8_buf[1] = 0x80 | (unicode_char & 0x3F);
            }
            else
            {
                utf8_buf[0] = 0xE0 | ((unicode_char >> 12) & 0x0F);
                utf8_buf[1] = 0x80 | ((unicode_char >> 6) & 0x3F);
                utf8_buf[2] = 0x80 | (unicode_char & 0x3F);
            }
        }
        else
        {
            utf8_buf[0] = (char)unicode_char;
        }
        _display->drawString(utf8_buf, x, y);
    }
}
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

// テキスト幅の取得
int TypoWrite::getTextWidth(const std::string &text)
{
    int width = 0;
    int height = 0;
    calculateTextSize(text, width, height);
    return width;
}

// テキスト高さの取得
int TypoWrite::getTextHeight(const std::string &text)
{
    int width = 0;
    int height = 0;
    calculateTextSize(text, width, height);
    return height;
}

// TypoWrite.cpp に追加する実装
bool TypoWrite::loadFontFromArray(const uint8_t *font_data)
{
    if (!_display || !font_data)
    {
        ESP_LOGE(TAG, "Display or font data is null");
        return false;
    }

    // M5GFXのloadFontメソッドを使用してフォントを読み込む
    if (!_display->loadFont(font_data))
    {
        ESP_LOGE(TAG, "Failed to load font from array");
        return false;
    }

    // 読み込み成功したら、現在のフォントとして設定
    _font = _display->getFont();
    _isCustomFont = true; // カスタムフォントフラグを設定

    ESP_LOGI(TAG, "Custom font loaded successfully from array");
    return true;
}

void TypoWrite::unloadCustomFont()
{
    if (!_display)
    {
        return;
    }

    // M5GFXのunloadFontメソッドを使用してフォントをアンロードし、
    // デフォルトフォントに戻す
    _display->unloadFont();
    _font = _display->getFont();
    _isCustomFont = false; // デフォルトフォントに戻した

    ESP_LOGI(TAG, "Custom font unloaded, back to default font");
}
