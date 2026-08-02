// main/TypoWrite.cpp - エラー修正版
#include "TypoWrite.hpp"
#include "esp_log.h"
#include <cstring>

static const char *TAG = "TypoWrite";

namespace {
/**
 * @brief 字面を持たない空白文字か
 *
 * スペースは「インクを持たない」という定義なので、フォントに専用グリフを
 * 持たないことが多い（shippori_16 は U+0020 と U+3000 のどちらも持っていない。
 * 未収録の ASCII は U+0020 ただ1つ）。
 * その状態で描画へ回すとグリフが見つからず代替字形が描かれ、
 * 縦線のようなゴミが出る。メトリクスと描画の両方でここを見て弾く。
 */
inline bool isBlankChar(uint16_t unicode_char)
{
    return unicode_char == 0x0020    // 半角スペース
        || unicode_char == 0x3000    // 全角スペース
        || unicode_char == 0x0009;   // タブ
}
}  // namespace

// ========================================
// 固定微調整値の定数定義
// ========================================

// 文字種別ごとの微調整値。
//
// 全カテゴリの倍率は 1.0（＝調整なし）を既定とする。
// 以前は全カテゴリが 1.1 だったため、
//   ・カテゴリ分類の仕組みが「全文字を一律1.1倍」以外の意味を持たない
//   ・倍率が 1.0 でないため drawEnhancedCharacter() の高速パス判定が常に外れ、
//     1文字ごとにスプライト生成→塗り→転送という重い経路を通る
//   ・しかも横書きの pushSprite は等倍転送なので拡大されず、
//     縦書き（pushRotateZoom）だけ 1.1倍になりサイズが不一致
// という状態だった。
// 個別の文字種を調整したい場合は該当する名前空間の値だけを変更すること。
namespace TypoWriteConstants
{
    // 小文字（ひらがな・カタカナ）の調整値
    namespace SmallChar
    {
        constexpr float WIDTH_SCALE = 1.0f;
        constexpr float HEIGHT_SCALE = 1.0f;
        constexpr int SPACING_OFFSET = 0;
        constexpr int VERTICAL_OFFSET = 0;
        constexpr int HORIZONTAL_OFFSET = 0;
    }

    // 句読点の調整値
    namespace Punctuation
    {
        constexpr float WIDTH_SCALE = 1.0f;
        constexpr float HEIGHT_SCALE = 1.0f;
        constexpr int SPACING_OFFSET = 0;
        constexpr int VERTICAL_OFFSET = 0;
        constexpr int HORIZONTAL_OFFSET = 0;
    }

    // 括弧類の調整値
    namespace Bracket
    {
        constexpr float WIDTH_SCALE = 1.0f;
        constexpr float HEIGHT_SCALE = 1.0f;
        constexpr int SPACING_OFFSET = 0;
        constexpr int VERTICAL_OFFSET = 0;
        constexpr int HORIZONTAL_OFFSET = 0;
    }

    // 横棒・長音記号の調整値
    namespace HorizontalBar
    {
        constexpr float WIDTH_SCALE = 1.0f;
        constexpr float HEIGHT_SCALE = 1.0f;
        constexpr int SPACING_OFFSET = 0;
        constexpr int VERTICAL_OFFSET = 0;
        constexpr int HORIZONTAL_OFFSET = 0;
    }

    // 通常文字（調整なし）
    namespace Normal
    {
        constexpr float WIDTH_SCALE = 1.0f;
        constexpr float HEIGHT_SCALE = 1.0f;
        constexpr int SPACING_OFFSET = 0;
        constexpr int VERTICAL_OFFSET = 0;
        constexpr int HORIZONTAL_OFFSET = 0;
    }

    // 枠線表示のデフォルト設定
    namespace Border
    {
        constexpr bool DEFAULT_SHOW = false;        // デフォルトでは非表示
        constexpr uint16_t DEFAULT_COLOR = TFT_RED; // デフォルト色は赤
        constexpr int MARK_SIZE = 5;                // 角マークのサイズ
    }
}

void TypoWrite::initializeAllTables()
{
    ESP_LOGI(TAG, "Initializing all TypoWrite tables...");

    // 1. 小文字→大文字マッピングテーブル初期化
    ESP_LOGD(TAG, "Building small character mapping table...");
    _smallToLargeMap = {
        // ひらがな小文字マッピング
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

    // 2. 縦書き用グリフマッピングテーブル初期化
    ESP_LOGD(TAG, "Building vertical glyph mapping table...");
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

    // 3. 文字種別調整テーブル初期化
    ESP_LOGD(TAG, "Building character type adjustment table...");
    _charAdjustments = {
        // 小文字の調整
        {CharCategory::SMALL_CHAR, {
            TypoWriteConstants::SmallChar::WIDTH_SCALE,
            TypoWriteConstants::SmallChar::HEIGHT_SCALE,
            TypoWriteConstants::SmallChar::SPACING_OFFSET,
            TypoWriteConstants::SmallChar::VERTICAL_OFFSET,
            TypoWriteConstants::SmallChar::HORIZONTAL_OFFSET}},

        // 句読点の調整
        {CharCategory::PUNCTUATION, {
            TypoWriteConstants::Punctuation::WIDTH_SCALE,
            TypoWriteConstants::Punctuation::HEIGHT_SCALE,
            TypoWriteConstants::Punctuation::SPACING_OFFSET,
            TypoWriteConstants::Punctuation::VERTICAL_OFFSET,
            TypoWriteConstants::Punctuation::HORIZONTAL_OFFSET}},

        // 括弧類の調整
        {CharCategory::BRACKET, {
            TypoWriteConstants::Bracket::WIDTH_SCALE,
            TypoWriteConstants::Bracket::HEIGHT_SCALE,
            TypoWriteConstants::Bracket::SPACING_OFFSET,
            TypoWriteConstants::Bracket::VERTICAL_OFFSET,
            TypoWriteConstants::Bracket::HORIZONTAL_OFFSET}},

        // 横棒・長音記号の調整
        {CharCategory::HORIZONTAL_BAR, {
            TypoWriteConstants::HorizontalBar::WIDTH_SCALE,
            TypoWriteConstants::HorizontalBar::HEIGHT_SCALE,
            TypoWriteConstants::HorizontalBar::SPACING_OFFSET,
            TypoWriteConstants::HorizontalBar::VERTICAL_OFFSET,
            TypoWriteConstants::HorizontalBar::HORIZONTAL_OFFSET}},

        // 通常文字の調整
        {CharCategory::NORMAL, {
            TypoWriteConstants::Normal::WIDTH_SCALE,
            TypoWriteConstants::Normal::HEIGHT_SCALE,
            TypoWriteConstants::Normal::SPACING_OFFSET,
            TypoWriteConstants::Normal::VERTICAL_OFFSET,
            TypoWriteConstants::Normal::HORIZONTAL_OFFSET}}
    };

    ESP_LOGI(TAG, "All tables initialized successfully:");
    // size_t を %d で出力していたため、明示キャストして %u にする
    ESP_LOGI(TAG, "  Small char mappings: %u entries",
             static_cast<unsigned>(_smallToLargeMap.size()));
    ESP_LOGI(TAG, "  Vertical glyph mappings: %u entries",
             static_cast<unsigned>(_verticalGlyphMap.size()));
    ESP_LOGI(TAG, "  Character adjustments: %u categories",
             static_cast<unsigned>(_charAdjustments.size()));
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
      _x(0), _y(0),
      _width(100), _height(100),
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
      _currentX(0), _currentY(0),
      _enableSmallCharHandling(true),
      _smallCharSettings(SmallCharSettings::getDefault()),
      _showBorder(TypoWriteConstants::Border::DEFAULT_SHOW),
      _borderColor(TypoWriteConstants::Border::DEFAULT_COLOR),
      _enableCharAdjustment(true),
      _rubyEnabled(false),
      _rubyScale(0.5f)
{
    // 全テーブルの一括初期化（シンプル！）
    initializeAllTables();

    // 文字描画用スプライトの初期化
    _charSprite = new lgfx::LGFX_Sprite(_display);
    
    ESP_LOGI(TAG, "TypoWrite constructor completed");
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
        // VLWCharMetrics を1回で取得する。
        // 従来は getCharWidth() / getCharHeight() / getCharSetWidth() を
        // 個別に呼んでおり、内部の findGlyph() が1文字あたり3回走っていた。
        // 各アクセサのフォールバック値は getCharMetrics() と同一
        // （width/setWidth は fontWidth、height は fontHeight）なので、
        // まとめても結果は変わらない。
        const VLWCharMetrics vm = _vlwParser->getCharMetrics(unicode_char);

        metrics = {
            static_cast<int32_t>(vm.width * _fontSize),
            static_cast<int32_t>(vm.height * _fontSize),
            static_cast<int32_t>(vm.setWidth * _fontSize),
            0 // baseline（必要に応じて追加）
        };
    }
    else if (_font)
    {
        // M5GFXフォールバック。
        // メトリクスを問い合わせるだけの処理なので、_display の状態は変更しない。
        // 従来はここで _display->setFont() / setTextSize() を呼んでおり、
        // 「サイズを知りたいだけ」の呼び出しが描画設定を書き換えていた
        // （_drawTarget がスプライトのときでも _display を触っていた）。
        // updateFontMetric() は IFont 自身の情報から算出するため設定は不要。
        lgfx::FontMetrics fm;
        _font->updateFontMetric(&fm, unicode_char);

        metrics = {
            static_cast<int32_t>(fm.x_advance * _fontSize),
            static_cast<int32_t>(fm.y_advance * _fontSize),
            static_cast<int32_t>(fm.x_advance * _fontSize), // 送り幅
            static_cast<int32_t>(fm.baseline * _fontSize)};
    }
    else
    {
        // メトリクスの取得元が無い（VLWパーサ未設定かつ _font == nullptr）。
        // 従来はこの分岐が存在せず、_font->updateFontMetric() で
        // nullptr 参照クラッシュしていた。
        // 0 を返すと送り幅0で描画ループが進まなくなるため、
        // 非退化な既定値を入れて描画を継続させる。
        ESP_LOGW(TAG, "No metrics source for U+%04X (font=null, VLW parser unavailable)",
                 unicode_char);

        int32_t fallback = static_cast<int32_t>(16 * _fontSize);

        if (isBlankChar(unicode_char))
        {
            // 空白は字面を持たないため、フォントに収録されていないことが多い
            // （shippori_16 は U+0020 も U+3000 も持っていない）。
            // 汎用の fallback は 1em なので、そのままでは
            // 半角スペースが全角幅になってしまう。幅を分けて与える。
            const int32_t halfEm = static_cast<int32_t>(8 * _fontSize);
            const int32_t advance = (unicode_char == 0x3000) ? fallback : halfEm;
            // 字面が無いので width/height は 0。送りだけを持たせる。
            metrics = {0, 0, advance, 0};
        }
        else
        {
            metrics = {fallback, fallback, fallback, 0};
        }
    }

    // キャッシュに保存する。
    //
    // 従来は「上限に達したら以後は一切保存しない」実装だったため、
    // 最初に現れた256文字だけが残り、それ以降の文字は毎回ミスし続けていた。
    // 日本語では容易に上限へ達するうえ、追い出し戦略もなかった。
    // 上限に達したら丸ごと捨てて入れ直す（世代的な追い出し）方式に変更し、
    // 後半のテキストでもキャッシュが効くようにする。
    // メモリ使用量の上限は従来どおり保たれる。
    if (_metricsCache.size() >= METRICS_CACHE_LIMIT)
    {
        ESP_LOGD(TAG, "Metrics cache full (%u entries), clearing",
                 static_cast<unsigned>(_metricsCache.size()));
        _metricsCache.clear();
    }
    _metricsCache[unicode_char] = metrics;

    return metrics;
}

void TypoWrite::drawEnhancedCharacter(uint16_t unicode_char, int x, int y,
                                      float widthScale, float heightScale)
{
    // 空白は送りだけ進めればよく、描いてはいけない
    if (isBlankChar(unicode_char))
    {
        return;
    }

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
// 描画先にフォント・サイズ・色をまとめて適用する（文字列描画の前に1回だけ）
void TypoWrite::applyTextStyle(lgfx::LovyanGFX *target)
{
    if (!target) { return; }

    if (_isCustomFont && _vlwFont)
    {
        target->loadFont(_vlwFont);
    }
    else if (_font)
    {
        target->setFont(_font);
    }

    target->setTextSize(_fontSize);

    // 背景透明モードでは1引数版の setTextColor を使う。
    // LovyanGFX は前景色と背景色が同一のとき背景を塗らないため、これで透過になる。
    // 従来は無条件に setTextColor(_color, _bgColor) としており、
    // _bgColor が TFT_TRANSPARENT(0x0120) のときその色の矩形が文字ごとに描かれていた。
    if (_transparentBg)
    {
        target->setTextColor(_color);
    }
    else
    {
        target->setTextColor(_color, _bgColor);
    }
}

// applyTextStyle() で読み込んだフォントを解放する（文字列描画の後に1回だけ）
void TypoWrite::releaseTextStyle(lgfx::LovyanGFX *target)
{
    if (!target) { return; }

    if (_isCustomFont && _vlwFont)
    {
        target->unloadFont();
    }
}

void TypoWrite::drawDirectCharacter(uint16_t unicode_char, int x, int y)
{
    // フォント・サイズ・色の設定は drawText() が描画開始前に
    // applyTextStyle() で1回だけ済ませている。
    // 以前はこのメソッド内で1文字ごとに loadFont()/unloadFont() まで
    // 実行しており、VLWヘッダの解析が文字数分繰り返されていた。
    std::string utf8_char = unicodeToUtf8(unicode_char);

    lgfx::LovyanGFX *target = _drawTarget
        ? static_cast<lgfx::LovyanGFX *>(_drawTarget)
        : static_cast<lgfx::LovyanGFX *>(_display);

    target->drawString(utf8_char.c_str(), _x + x, _y + y);
}

void TypoWrite::drawEnhancedCharacterWithRotation(uint16_t unicode_char, int x, int y,
                                                  float widthScale, float heightScale,
                                                  float rotation)
{
    // 空白は送りだけ進めればよく、描いてはいけない
    if (isBlankChar(unicode_char))
    {
        return;
    }

    // 回転もスケールも不要な場合は直接描画
    if (widthScale == 1.0f && heightScale == 1.0f && rotation == 0.0f)
    {
        drawDirectCharacter(unicode_char, x, y);
        return;
    }

    // スケールまたは回転が必要な場合はスプライト描画
    drawScaledCharacterWithRotation(unicode_char, x, y, widthScale, heightScale, rotation);
}

void TypoWrite::getEmBoxSize(int &emW, int &emH)
{
    // 基本は代表文字（全角スペース相当）のメトリクス。
    // これらは _fontSize を掛けた値になっている。
    emW = getMaxCharWidth();
    emH = getLineHeight();

    // 代表文字より大きなグリフが存在しうるので、実測の最大値でも押し広げる。
    // 例: shippori_16 は代表幅16に対し最大グリフ幅が18ある。
    if (_useVLWParser && _vlwParser && _vlwParser->isInitialized())
    {
        const int maxW = static_cast<int>(_vlwParser->getMaxCharWidth() * _fontSize);
        const int maxH = static_cast<int>(_vlwParser->getMaxCharHeight() * _fontSize);
        if (maxW > emW) { emW = maxW; }
        if (maxH > emH) { emH = maxH; }
    }

    if (emW < 1) { emW = 1; }
    if (emH < 1) { emH = 1; }
}

bool TypoWrite::ensureCharSprite(int w, int h)
{
    if (!_charSprite) { return false; }

    // 既に同じサイズなら作り直さない。
    // emボックスは全文字共通なので、実際に作り直されるのは
    // フォントやフォントサイズを変更したときだけになる。
    if (_charSprite->width() == w && _charSprite->height() == h) { return true; }

    if (_charSprite->width() > 0) { _charSprite->deleteSprite(); }

    if (!_charSprite->createSprite(w, h))
    {
        ESP_LOGE(TAG, "createSprite(%d, %d) failed", w, h);
        return false;
    }
    return true;
}

void TypoWrite::drawScaledCharacterWithRotation(uint16_t unicode_char, int x, int y,
                                                float widthScale, float heightScale,
                                                float rotation)
{
    // 回転・スケールが不要な場合は直接描画（最速パス）
    if (widthScale == 1.0f && heightScale == 1.0f && rotation == 0.0f)
    {
        drawDirectCharacter(unicode_char, x, y);
        return;
    }

    if (!_charSprite)
    {
        ESP_LOGE(TAG, "charSprite is null! Falling back to direct drawing");
        drawDirectCharacter(unicode_char, x, y);
        return;
    }

    // スプライトは「全文字共通の em ボックス」サイズで用意する。
    //
    // 以前はグリフごとに
    //     sprite = (metrics.width * widthScale + 20) x (metrics.height * heightScale + 20)
    // としており、スプライト寸法がグリフのビットマップ高に依存していた。
    // 配置は pushRotateZoom の中心指定なので、
    //     center_y = y + sprite_height / 2
    // がグリフごとに変わってしまい、同じ列の中で基準位置が跳ねていた。
    //   例: あ(h=15) は y+17、ー(h=7) は y+13 と 4px ずれる
    // 一方 drawDirectCharacter() は drawString(x, y) の左上基準で一貫している。
    // つまり通常文字と小文字・回転文字で配置基準が食い違っていた。
    //
    // em ボックス固定にすると全文字が同じ基準になり、
    // かつスプライトの作り直しも起きなくなる。
    int emW = 0;
    int emH = 0;
    getEmBoxSize(emW, emH);

    if (!ensureCharSprite(emW, emH))
    {
        ESP_LOGE(TAG, "Failed to prepare char sprite for U+%04X. Falling back to direct drawing",
                 unicode_char);
        drawDirectCharacter(unicode_char, x, y);
        return;
    }

    // 背景をクリア（塗った色を pushRotateZoom の透過キーに使う）
    const uint16_t fillColor = _transparentBg ? static_cast<uint16_t>(TFT_TRANSPARENT) : _bgColor;
    _charSprite->fillSprite(fillColor);

    // グリフはスプライトの原点に描く。
    // drawDirectCharacter() の drawString(_x + x, _y + y) と同じ左上基準になり、
    // 両経路で配置が一致する。
    applyTextStyle(_charSprite);
    const std::string utf8_char = unicodeToUtf8(unicode_char);
    _charSprite->drawString(utf8_char.c_str(), 0, 0);
    releaseTextStyle(_charSprite);

    // 描画先を選択
    lgfx::LovyanGFX *target = _drawTarget
        ? static_cast<lgfx::LovyanGFX *>(_drawTarget)
        : static_cast<lgfx::LovyanGFX *>(_display);

    // pushRotateZoom はスプライトの中心を指定位置に置く。
    // 拡大後の em ボックス左上が (_x + x, _y + y) に来るよう中心を求めることで、
    // 直接描画と同じ左上基準になる。
    // 回転する場合も em ボックスの中心を軸に回るため、
    // 縦書き中の半角英数が列の中央に収まる。
    const int center_x = _x + x + static_cast<int>(emW * widthScale / 2.0f);
    const int center_y = _y + y + static_cast<int>(emH * heightScale / 2.0f);

    _charSprite->pushRotateZoom(target, center_x, center_y,
                                rotation, widthScale, heightScale,
                                fillColor);
}

// スケールを適用して描画（回転なし）
void TypoWrite::drawScaledCharacter(uint16_t unicode_char, int x, int y,
                                    float widthScale, float heightScale)
{
    // スケール調整が不要な場合は直接描画（最速パス）
    if (widthScale == 1.0f && heightScale == 1.0f)
    {
        drawDirectCharacter(unicode_char, x, y);
        return;
    }

    // 回転なし（0度）として回転付き実装に委譲する。
    //
    // 従来はここに独自のスプライト描画を持っていたが、
    // 最後の転送が pushSprite() による等倍転送だったため
    // widthScale / heightScale がスプライトの確保サイズにしか効かず、
    // **実際には拡大縮小されない**という欠陥があった
    // （メソッド名と挙動が一致していなかった）。
    // 拡大縮小は pushRotateZoom() を使う回転付き実装側で正しく行われているので、
    // そちらに一本化して重複も解消する。
    drawScaledCharacterWithRotation(unicode_char, x, y, widthScale, heightScale, 0.0f);
}

// ========================================
// 横書きテキスト描画
// ========================================
size_t TypoWrite::drawHorizontalTextEnhanced(const std::vector<uint16_t> &unicode_chars,
                                             size_t start,
                                             const std::vector<RubyRun> &rubyRuns)
{
    // ルビ帯は本文の上に確保する（横組みのルビは本文の上）。
    // ルビの有無にかかわらず全行に同じ帯を取ることで行間が一定に保たれる。
    const int rubyStrip = getRubyStripSize();

    // 行の高さは全体で不変なのでループ外で1回だけ求める
    const int lineHeight = getLineHeight() + rubyStrip;

    _currentY = 0;
    size_t i = start;

    while (i < unicode_chars.size())
    {
        // 改行のみの行を消費する
        if (unicode_chars[i] == '\n')
        {
            ++i;
            _currentY += lineHeight + _lineSpacing;
            continue;
        }

        // --- 1パス目: この視覚行に入る範囲と幅を測る ---
        // TextAlignment を反映するには行の総幅が先に必要なため、
        // 折り返しを適用した「視覚行」単位で measure してから描画する。
        // 末尾の字間は幅に含めない（含めると中央/右揃えがずれる）。
        size_t lineEnd = i;
        int lineWidth = 0;
        while (lineEnd < unicode_chars.size() && unicode_chars[lineEnd] != '\n')
        {
            // ルビ付き範囲は「ひとかたまり」として扱い、途中で折り返さない。
            // 理由は縦書き側と同じ（ページ境界が範囲内に落ちると記法を読み直せない）。
            size_t groupEnd = lineEnd + 1;
            const size_t runIdx = findRubyRunStartingAt(rubyRuns, lineEnd);
            if (runIdx < rubyRuns.size())
            {
                groupEnd = rubyRuns[runIdx].baseEnd;
            }

            int groupWidth = lineWidth;
            for (size_t m = lineEnd; m < groupEnd; ++m)
            {
                const uint16_t ch = unicode_chars[m];
                const CharMetrics metrics = getCharMetrics(ch);
                const CharTypeAdjustment adjustment = getCharAdjustment(ch);

                // 送りはフォントが定める送り幅(setWidth)を使う。
                // グリフのビットマップ幅(width)ではプロポーショナル字形で詰まりすぎるうえ、
                // calculateTextSize() が setWidth で計算しているため値が食い違っていた。
                const int advance = static_cast<int>(metrics.setWidth * adjustment.widthScale);
                const int spacing = _charSpacing + adjustment.spacingOffset;

                groupWidth = (m == i) ? advance : groupWidth + spacing + advance;
            }

            if (_wrap && lineEnd > i && groupWidth > _width)
            {
                break;  // ここで折り返す
            }
            lineWidth = groupWidth;
            lineEnd = groupEnd;
        }

        // 描画範囲チェック（行単位）
        // ここで抜けたとき i はこの行の先頭を指したままなので、
        // そのまま次ページの開始位置として返せる。
        if (_currentY + lineHeight > _height)
        {
            break;
        }

        // --- 揃えに応じた行頭オフセット ---
        int lineOffsetX = 0;
        switch (_alignment)
        {
        case TextAlignment::CENTER:
            lineOffsetX = (_width - lineWidth) / 2;
            break;
        case TextAlignment::RIGHT:
            lineOffsetX = _width - lineWidth;
            break;
        case TextAlignment::LEFT:
        default:
            lineOffsetX = 0;
            break;
        }
        if (lineOffsetX < 0) { lineOffsetX = 0; }

        // --- 2パス目: 実際に描画する ---
        _currentX = lineOffsetX;

        // ルビは本文を描き終えてから、その範囲の中央に揃えて描く。
        size_t activeRun = rubyRuns.size();
        int rubyXStart = 0;

        for (size_t k = i; k < lineEnd; ++k)
        {
            if (activeRun == rubyRuns.size())
            {
                const size_t idx = findRubyRunStartingAt(rubyRuns, k);
                if (idx < rubyRuns.size())
                {
                    activeRun = idx;
                    rubyXStart = _currentX;
                }
            }

            const uint16_t ch = unicode_chars[k];
            const CharMetrics metrics = getCharMetrics(ch);
            const CharTypeAdjustment adjustment = getCharAdjustment(ch);

            // 送りはフォントが定める送り幅(setWidth)を使う。
            // グリフのビットマップ幅(width)ではプロポーショナル字形で詰まりすぎるうえ、
            // calculateTextSize() が setWidth で計算しているため値が食い違っていた。
            const int advance = static_cast<int>(metrics.setWidth * adjustment.widthScale);
            const int spacing = _charSpacing + adjustment.spacingOffset;

            // 本文はルビ帯のぶん下げて描く（帯は行の上側に確保してある）
            drawEnhancedCharacter(ch,
                                  _currentX + adjustment.horizontalOffset,
                                  _currentY + rubyStrip + adjustment.verticalOffset,
                                  adjustment.widthScale, adjustment.heightScale);

            _currentX += advance + spacing;

            // ルビ範囲の最後の文字を描き終えた。本文の上にルビを描く。
            if (activeRun < rubyRuns.size() && (k + 1) == rubyRuns[activeRun].baseEnd)
            {
                const RubyRun &run = rubyRuns[activeRun];
                const int runLeft = rubyXStart;
                const int runRight = _currentX - spacing;

                int rubyTotal = 0;
                for (size_t n = 0; n < run.ruby.size(); ++n)
                {
                    rubyTotal += getRubyAdvance(run.ruby[n]);
                }

                int rubyX = runLeft + ((runRight - runLeft) - rubyTotal) / 2;
                if (rubyX < 0) { rubyX = 0; }

                for (size_t n = 0; n < run.ruby.size(); ++n)
                {
                    drawEnhancedCharacter(run.ruby[n], rubyX, _currentY,
                                          _rubyScale, _rubyScale);
                    rubyX += getRubyAdvance(run.ruby[n]);
                }

                activeRun = rubyRuns.size();
            }
        }

        // 次の行へ
        i = lineEnd;
        if (i < unicode_chars.size() && unicode_chars[i] == '\n')
        {
            ++i;  // 改行文字を消費
        }
        _currentY += lineHeight + _lineSpacing;
    }

    return i;
}


// ========================================
// 縦書きテキスト描画（簡略化版）
// ========================================
size_t TypoWrite::drawVerticalTextEnhanced(const std::vector<uint16_t> &unicode_chars,
                                           size_t start,
                                           const std::vector<RubyRun> &rubyRuns)
{
    // 列幅と列送りはテキスト全体で不変なのでループ外で1回だけ求める。
    // 小文字の変位計算に使う em ボックス寸法（全文字共通）
    int emW = 0;
    int emH = 0;
    getEmBoxSize(emW, emH);

    const int baseColumnWidth = getMaxCharWidth();

    // ルビ帯は本文の右側に確保する（縦組みのルビは本文の右）。
    // ルビの有無にかかわらず全列に同じ帯を取ることで列の間隔が一定に保たれる。
    // 列ごとに幅が変わると、ルビのある列だけ隣がずれて読みにくくなる。
    const int rubyStrip = getRubyStripSize();
    const int columnWidth = baseColumnWidth + rubyStrip;

    // 縦組みでは「行」＝「列」なので、列の間隔は _lineSpacing で表す。
    // 以前は _columnSpacing と _lineSpacing の両方を加算しており、
    // 同じ意味の設定が二重に効く状態だった（_columnSpacing は廃止）。
    const int columnStep = columnWidth + _lineSpacing;

    // 1文字の解決結果。
    // TextAlignment のために列の高さを先に測る必要があり、
    // 測定と描画で同じ値を使わないと揃えがずれるため一箇所にまとめる。
    struct ResolvedChar
    {
        uint16_t displayChar;        // 実際に描画する文字（小文字変換・縦書きグリフ適用後）
        CharMetrics metrics;         // displayChar のメトリクス
        CharTypeAdjustment adjust;   // 元の文字種別による調整
        float widthScale;            // 最終的な横倍率
        float heightScale;           // 最終的な縦倍率
        float rotation;              // 0 または 90 度
        float smallOffsetX;          // 小文字の横オフセット比率
        float smallOffsetY;          // 小文字の縦オフセット比率
        int advance;                 // 列方向の送り量（em固定）
        int spacing;                 // 調整済みの字間
    };

    auto resolve = [this](uint16_t unicode_char) -> ResolvedChar
    {
        ResolvedChar r{};
        uint16_t workChar = unicode_char;
        float smallScale = 1.0f;
        r.smallOffsetX = 0.0f;
        r.smallOffsetY = 0.0f;

        // 小文字（捨て仮名）の縦組み処理。
        //
        // フォントの小文字グリフは横書き用に設計されており、
        // 大文字と同じベースラインに揃う「下寄せ」になっている。
        //     あ U+3042 : インク y+ 6..y+21
        //     ぁ U+3041 : インク y+ 8..y+21  ← 下端が大文字と同じ
        // 縦組みでは em の右上へ寄せるのが正しいため、変位を与える。
        //
        // この変位は「専用グリフをそのまま使う場合」と
        // 「大文字を縮小して代用する場合」の両方に適用する。
        if (_enableSmallCharHandling && isSmallChar(unicode_char))
        {
            r.smallOffsetX = _smallCharSettings.offsetX;
            r.smallOffsetY = _smallCharSettings.offsetY;

            // フォントに専用グリフが無い場合のみ、大文字を縮小して代用する
            if (needsSmallCharSubstitution(unicode_char))
            {
                workChar = getCorrespondingLargeChar(unicode_char);
                smallScale = _smallCharSettings.scale;
            }
        }

        r.displayChar = convertToVerticalGlyph(workChar);
        r.metrics = getCharMetrics(r.displayChar);
        // 調整は「元の文字」の種別で判定する
        r.adjust = getCharAdjustment(unicode_char);
        // 回転要否も「元の文字」で判定する
        r.rotation = shouldRotateInVertical(unicode_char) ? 90.0f : 0.0f;
        r.widthScale = r.adjust.widthScale * smallScale;
        r.heightScale = r.adjust.heightScale * smallScale;

        // 列方向の送りは em 固定（setWidth）。
        // グリフのビットマップ高を使うと あ=15 / ー=7 / 、=4 と変動し、
        // _charSpacing が負のとき送りが負になって文字が逆行していた。
        // 小文字も組版上は1emを占めるため、縮小率は送りに掛けない。
        r.advance = r.metrics.setWidth;
        r.spacing = _charSpacing + r.adjust.spacingOffset;
        return r;
    };

    // 縦書きは右上の列から始める
    _currentX = _width - columnWidth;
    size_t i = start;

    while (i < unicode_chars.size())
    {
        // 改行のみの列を消費する
        if (unicode_chars[i] == '\n')
        {
            ++i;
            _currentX -= columnStep;
            continue;
        }

        // --- 1パス目: この列に入る範囲と高さを測る ---
        // 末尾の字間は高さに含めない（含めると中央/下揃えがずれる）。
        size_t colEnd = i;
        int colHeight = 0;
        while (colEnd < unicode_chars.size() && unicode_chars[colEnd] != '\n')
        {
            // ルビ付き範囲は「ひとかたまり」として扱い、途中で折り返さない。
            //
            // 範囲の途中で列が切れると、そこがページ境界になったときに
            // 記法を読み直せなくなる（baseByteOffsets が ｜ の位置を指すのは
            // 範囲の先頭文字だけのため）。ルビが本文から外れて表示される。
            size_t groupEnd = colEnd + 1;
            const size_t runIdx = findRubyRunStartingAt(rubyRuns, colEnd);
            if (runIdx < rubyRuns.size())
            {
                groupEnd = rubyRuns[runIdx].baseEnd;
            }

            int groupHeight = colHeight;
            for (size_t m = colEnd; m < groupEnd; ++m)
            {
                const ResolvedChar r = resolve(unicode_chars[m]);
                groupHeight = (m == i) ? r.advance
                                       : groupHeight + r.spacing + r.advance;
            }

            if (_wrap && colEnd > i && groupHeight > _height)
            {
                break;  // ここで次の列へ折り返す
            }
            colHeight = groupHeight;
            colEnd = groupEnd;
        }

        // 描画範囲チェック（列単位）
        // ここで抜けたとき i はこの列の先頭を指したままなので、
        // そのまま次ページの開始位置として返せる。
        if (_currentX < 0)
        {
            break;
        }

        // --- 揃えに応じた列頭オフセット（縦書きでは LEFT=上揃え / RIGHT=下揃え）---
        int colOffsetY = 0;
        switch (_alignment)
        {
        case TextAlignment::CENTER:
            colOffsetY = (_height - colHeight) / 2;
            break;
        case TextAlignment::RIGHT:
            colOffsetY = _height - colHeight;
            break;
        case TextAlignment::LEFT:
        default:
            colOffsetY = 0;
            break;
        }
        if (colOffsetY < 0) { colOffsetY = 0; }

        // --- 2パス目: 実際に描画する ---
        _currentY = colOffsetY;

        // ルビは本文を描き終えてから、その範囲の中央に揃えて描く。
        // 開始位置を覚えておき、範囲の最後の文字を描いた時点で確定させる。
        size_t activeRun = rubyRuns.size();
        int rubyYStart = 0;

        for (size_t k = i; k < colEnd; ++k)
        {
            if (activeRun == rubyRuns.size())
            {
                const size_t idx = findRubyRunStartingAt(rubyRuns, k);
                if (idx < rubyRuns.size())
                {
                    activeRun = idx;
                    rubyYStart = _currentY;
                }
            }

            const ResolvedChar r = resolve(unicode_chars[k]);

            // 列の左端は _currentX と一致させる。
            // 以前はここで getMaxCharWidth() をもう一度引いており、
            // 開始位置と合わせて列幅が二重に減算されていた。
            // 小文字の変位は em ボックス基準で計算する。
            // グリフのビットマップ寸法を基準にすると
            // ぁ(h=13) と っ(h=7) で変位量が変わってしまい、
            // 同じ列の中で寄せ具合が揃わない。
            const int draw_x = _currentX + r.adjust.horizontalOffset +
                               static_cast<int>(emW * r.smallOffsetX);
            const int draw_y = _currentY + r.adjust.verticalOffset +
                               static_cast<int>(emH * r.smallOffsetY);

            drawEnhancedCharacterWithRotation(r.displayChar, draw_x, draw_y,
                                              r.widthScale, r.heightScale, r.rotation);

            _currentY += r.advance + r.spacing;

            // ルビ範囲の最後の文字を描き終えた。本文の右側にルビを描く。
            // 末尾の字間は範囲の高さに含めない（中央揃えがずれるため）。
            if (activeRun < rubyRuns.size() && (k + 1) == rubyRuns[activeRun].baseEnd)
            {
                const RubyRun &run = rubyRuns[activeRun];
                const int runTop = rubyYStart;
                const int runBottom = _currentY - r.spacing;

                int rubyTotal = 0;
                for (size_t n = 0; n < run.ruby.size(); ++n)
                {
                    rubyTotal += getRubyAdvance(run.ruby[n]);
                }

                int rubyY = runTop + ((runBottom - runTop) - rubyTotal) / 2;
                if (rubyY < 0) { rubyY = 0; }

                const int rubyX = _currentX + baseColumnWidth;
                for (size_t n = 0; n < run.ruby.size(); ++n)
                {
                    // ルビも縦組みなので、句読点などは縦書き用グリフに置き換える
                    drawEnhancedCharacterWithRotation(convertToVerticalGlyph(run.ruby[n]),
                                                      rubyX, rubyY,
                                                      _rubyScale, _rubyScale, 0.0f);
                    rubyY += getRubyAdvance(run.ruby[n]);
                }

                activeRun = rubyRuns.size();
            }
        }

        // 次の列へ
        i = colEnd;
        if (i < unicode_chars.size() && unicode_chars[i] == '\n')
        {
            ++i;  // 改行文字を消費
        }
        _currentX -= columnStep;
    }

    return i;
}

// ========================================
// その他の既存メソッド（必要最小限のみ実装）
// ========================================

// 文字変換ヘルパー
std::vector<uint16_t> TypoWrite::utf8ToUnicode(const std::string &utf8_string)
{
    std::vector<size_t> ignored;
    return utf8ToUnicode(utf8_string, ignored);
}

std::vector<uint16_t> TypoWrite::utf8ToUnicode(const std::string &utf8_string,
                                               std::vector<size_t> &byteOffsets)
{
    std::vector<uint16_t> unicode_chars;
    byteOffsets.clear();

    const uint8_t *str = (const uint8_t *)utf8_string.c_str();
    size_t len = utf8_string.length();
    size_t i = 0;

    while (i < len)
    {
        uint16_t unicode_char = 0;
        const size_t charStart = i;

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
        byteOffsets.push_back(charStart);
    }

    // 末尾に文字列全体の長さを入れておく。
    // 「全部描き切った」ときの nextOffset がこれになる。
    byteOffsets.push_back(len);

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
    if (it == _verticalGlyphMap.end())
    {
        return unicode_char; // 変換なし
    }

    const uint16_t vertical_char = it->second;

    // 差し替え先（U+FE10〜FE4F の縦書き用字形）がフォントに無い場合は
    // 元の文字に戻す。
    //
    // 無いまま描くと VLWFontParser がフォールバック値を返し、
    // 豆腐や空白として描かれてしまう。元の文字なら向きは正しくないが
    // 少なくとも読める。
    //
    // 現在使用中の shippori_16 は登録28件すべての縦書き字形を収録しているため
    // このフォールバックは発動しない（実測で確認済み）。
    // 他フォントに差し替えたときの保険として入れてある。
    if (_useVLWParser && _vlwParser && _vlwParser->isInitialized() &&
        !_vlwParser->hasChar(vertical_char))
    {
        ESP_LOGD(TAG, "Vertical glyph U+%04X not in font, falling back to U+%04X",
                 vertical_char, unicode_char);
        return unicode_char;
    }

    return vertical_char;
}

bool TypoWrite::isSmallChar(uint16_t unicode_char) const
{
    return _smallToLargeMap.find(unicode_char) != _smallToLargeMap.end();
}

bool TypoWrite::needsSmallCharSubstitution(uint16_t unicode_char) const
{
    if (!_enableSmallCharHandling) { return false; }
    if (!isSmallChar(unicode_char)) { return false; }

    // フォントが小文字の専用グリフを持っているなら代用しない。
    //
    // 専用グリフはサイズも em ボックス内の位置も適切に設計されている。
    // 例（shippori_16, ascent=19）:
    //     あ U+3042 : h=15 top=13 -> インク y+ 6..y+21
    //     ぁ U+3041 : h=13 top=11 -> インク y+ 8..y+21   ← 下端が大文字と揃う
    // これをそのまま描けば次の文字との間隔は通常文字と同じになる。
    //
    // 一方「大文字を0.75倍に縮小」で代用すると、em ボックスの左上基準で
    // 縮むためインク下端が上がり、次の文字との空きが 2px -> 8px 程度に広がる。
    if (_useVLWParser && _vlwParser && _vlwParser->isInitialized() &&
        _vlwParser->hasChar(unicode_char))
    {
        return false;
    }

    return true;
}

void TypoWrite::setSmallCharHandling(bool enable)
{
    _enableSmallCharHandling = enable;
    ESP_LOGD(TAG, "Small char substitution: %s", enable ? "enabled" : "disabled");
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

    // TFT_TRANSPARENT(0x0120) は「透明」を表す特別なフラグではなく実在の色値なので、
    // 単に _bgColor に入れるだけでは 0x0120 という色で塗りつぶされてしまう。
    // 呼び出し側の意図（透過したい）を汲んで背景透明モードを切り替える。
    // 従来は _transparentBg を立てるAPIが存在せず、常に false のままだった。
    _transparentBg = (bgColor == static_cast<uint16_t>(TFT_TRANSPARENT));

    ESP_LOGD(TAG, "Background color set to 0x%04X (transparent: %s)",
             bgColor, _transparentBg ? "yes" : "no");
}

// 背景透明モードの明示設定
void TypoWrite::setTransparentBackground(bool transparent)
{
    _transparentBg = transparent;
    ESP_LOGD(TAG, "Transparent background: %s", transparent ? "enabled" : "disabled");
}

// ========================================
// ルビ（ふりがな）
// ========================================

void TypoWrite::setRubyEnabled(bool enabled)
{
    _rubyEnabled = enabled;
    ESP_LOGD(TAG, "Ruby %s", enabled ? "enabled" : "disabled");
}

void TypoWrite::setRubyScale(float scale)
{
    if (scale <= 0.0f || scale >= 1.0f)
    {
        ESP_LOGW(TAG, "Ruby scale %.2f out of range (0, 1). Ignored", scale);
        return;
    }
    _rubyScale = scale;
    ESP_LOGD(TAG, "Ruby scale set to %.2f", scale);
}

// ルビ記法の区切り文字。
//
// 青空文庫式の正式な記法は全角（｜漢字《かんじ》）だが、
// ｜ も 《》 も日本語入力から出しにくい。
// シナリオは SD 上のテキストを手で書くことになるため、
// 半角（|漢字<かんじ>）でも同じように書けるようにしてある。
// 既存の全角記法もそのまま使える。
namespace {
inline bool isRubyMark(uint16_t c)
{
    return c == 0xFF5C     // ｜ 全角縦棒
        || c == 0x007C;    // |  半角縦棒
}
inline bool isRubyOpen(uint16_t c)
{
    return c == 0x300A     // 《
        || c == 0x003C     // <
        || c == 0xFF1C;    // ＜ 全角不等号
}
inline bool isRubyClose(uint16_t c)
{
    return c == 0x300B     // 》
        || c == 0x003E     // >
        || c == 0xFF1E;    // ＞ 全角不等号
}
}  // namespace

void TypoWrite::parseRubyMarkup(const std::vector<uint16_t> &raw,
                                const std::vector<size_t> &rawByteOffsets,
                                size_t rawTextLength,
                                std::vector<uint16_t> &baseChars,
                                std::vector<size_t> &baseByteOffsets,
                                std::vector<RubyRun> &runs)
{
    baseChars.clear();
    baseByteOffsets.clear();
    runs.clear();

    size_t i = 0;
    while (i < raw.size())
    {
        if (!isRubyMark(raw[i]))
        {
            baseChars.push_back(raw[i]);
            baseByteOffsets.push_back(rawByteOffsets[i]);
            ++i;
            continue;
        }

        // 縦棒を見つけた。対応する開き括弧と閉じ括弧を探す。
        size_t open = i + 1;
        while (open < raw.size() && !isRubyOpen(raw[open]) && raw[open] != '\n')
        {
            ++open;
        }
        size_t close = (open < raw.size()) ? open + 1 : raw.size();
        while (close < raw.size() && !isRubyClose(raw[close]) && raw[close] != '\n')
        {
            ++close;
        }

        const bool wellFormed = (open < raw.size() && isRubyOpen(raw[open])) &&
                                (close < raw.size() && isRubyClose(raw[close])) &&
                                (open > i + 1);   // ベースが空でないこと

        if (!wellFormed)
        {
            // 記法が壊れている。縦棒を普通の文字として扱い、本文を失わないようにする。
            // 本文中にたまたま | が出てきた場合もここに来る。
            ESP_LOGW(TAG, "Malformed ruby markup at char %u (treating as literal)",
                     static_cast<unsigned>(i));
            baseChars.push_back(raw[i]);
            baseByteOffsets.push_back(rawByteOffsets[i]);
            ++i;
            continue;
        }

        RubyRun run;
        run.baseStart = baseChars.size();

        for (size_t m = i + 1; m < open; ++m)
        {
            baseChars.push_back(raw[m]);
            // 範囲の先頭文字には ｜ の位置を入れる。
            // ページ送りでこの位置から再開したとき、記法ごと読み直せるようにするため。
            baseByteOffsets.push_back((m == i + 1) ? rawByteOffsets[i] : rawByteOffsets[m]);
        }

        run.baseEnd = baseChars.size();
        for (size_t m = open + 1; m < close; ++m)
        {
            run.ruby.push_back(raw[m]);
        }

        if (!run.ruby.empty())
        {
            runs.push_back(run);
        }

        i = close + 1;
    }

    // 末尾に文字列全体の長さを入れる（全部描き切ったときの nextOffset）
    baseByteOffsets.push_back(rawTextLength);
}

size_t TypoWrite::findRubyRunStartingAt(const std::vector<RubyRun> &runs, size_t index) const
{
    for (size_t n = 0; n < runs.size(); ++n)
    {
        if (runs[n].baseStart == index)
        {
            return n;
        }
    }
    return runs.size();
}

int TypoWrite::getRubyAdvance(uint16_t unicode_char)
{
    const CharMetrics metrics = getCharMetrics(unicode_char);
    return static_cast<int>(metrics.setWidth * _rubyScale);
}

int TypoWrite::getRubyStripSize()
{
    if (!_rubyEnabled)
    {
        return 0;
    }
    int emW = 0;
    int emH = 0;
    getEmBoxSize(emW, emH);
    // 縦書きは幅、横書きは高さを使う。em ボックスは正方形に近いので
    // どちらでも大差ないが、方向に合わせておく。
    const int base = (_direction == TextDirection::VERTICAL) ? emW : emH;
    return static_cast<int>(base * _rubyScale);
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
        // 読み込んだフォントを現在のフォントとして設定。
        //
        // 描画時は _isCustomFont / _vlwFont が優先されるため _font は使われないが、
        // ここで _font = nullptr にはしない。
        // メトリクス取得のフォールバック（getCharMetrics()）が _font を使うため、
        // nullptr にすると setVLWParser() を呼び忘れた場合に
        // nullptr 参照でクラッシュしていた（getCharMetrics() 側にもガードを入れたが、
        // 使える IFont を捨てないほうがフォールバックとして妥当）。
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

// 補足: setColumnSpacing() / _columnSpacing は廃止した。
//       縦組みでは「行」＝「列」であり、列の間隔は _lineSpacing が表す。
//       両方を加算していたため設定が二重に効いていた。

// ========== メインテキスト描画メソッド ==========

// テキスト描画
void TypoWrite::drawText(const std::string &text)
{
    // 先頭から描くだけ。収まらない分は捨てられる（従来どおりの挙動）。
    drawTextPaged(text, 0);
}

TypoWrite::DrawResult TypoWrite::drawTextPaged(const std::string &text, size_t startOffset)
{
    std::vector<size_t> rawByteOffsets;
    const std::vector<uint16_t> raw = utf8ToUnicode(text, rawByteOffsets);

    // ルビ記法を本文とルビ範囲に分解する。
    // 無効時は記法を解釈しないので raw がそのまま本文になる
    // （｜ や 《》 は普通の文字として表示される）。
    std::vector<uint16_t> unicode_chars;
    std::vector<size_t> byteOffsets;
    std::vector<RubyRun> rubyRuns;

    if (_rubyEnabled)
    {
        parseRubyMarkup(raw, rawByteOffsets, text.size(),
                        unicode_chars, byteOffsets, rubyRuns);
    }
    else
    {
        unicode_chars = raw;
        byteOffsets = rawByteOffsets;
    }

    // startOffset（バイト）を文字の添字へ直す。
    // UTF-8 の途中を指していた場合は直前の文字境界へ切り下げる。
    size_t startIndex = 0;
    while (startIndex < unicode_chars.size() && byteOffsets[startIndex] < startOffset)
    {
        ++startIndex;
    }

    // 既に末尾に達しているなら何も描かない。
    // 背景クリアもしないのは、呼び出し側が終端を検出する前に
    // 画面を消してしまわないようにするため。
    if (startIndex >= unicode_chars.size())
    {
        return DrawResult{text.size(), false};
    }

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

    // フォント・サイズ・色は描画開始前に1回だけ適用する。
    // 1文字ごとに loadFont() を呼ぶとVLWヘッダの解析が繰り返されるため。
    applyTextStyle(target);

    // テキスト描画
    size_t endIndex;
    if (_direction == TextDirection::HORIZONTAL)
    {
        endIndex = drawHorizontalTextEnhanced(unicode_chars, startIndex, rubyRuns);
    }
    else
    {
        endIndex = drawVerticalTextEnhanced(unicode_chars, startIndex, rubyRuns);
    }

    releaseTextStyle(target);

    // クリッピング領域を解除
    target->clearClipRect();

    // 1文字も進まなかった場合は無限ループになるため強制的に打ち切る。
    // 描画領域が1文字分の高さすら無いときに起こりうる。
    if (endIndex == startIndex)
    {
        ESP_LOGW(TAG, "drawTextPaged: no progress at offset %u (area %dx%d too small?)",
                 static_cast<unsigned>(startOffset), _width, _height);
        return DrawResult{text.size(), false};
    }

    return DrawResult{byteOffsets[endIndex], endIndex < unicode_chars.size()};
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
    // 以前は 5 をローカルで再定義しており、
    // TypoWriteConstants::Border::MARK_SIZE（同じく5）が未使用のままだった。
    const int markSize = TypoWriteConstants::Border::MARK_SIZE;
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
    const std::vector<uint16_t> unicode_chars = utf8ToUnicode(text);

    width = 0;
    height = 0;

    // 送りの求め方は描画側（drawHorizontalTextEnhanced /
    // drawVerticalTextEnhanced）と一致させること。
    // 食い違うと getTextWidth()/getTextHeight() と実際の描画結果がずれ、
    // drawTextCentered() の中央位置も外れる。
    //
    // 末尾の字間は寸法に含めない（描画側も最後の1文字の後ろには何も置かない）。
    //
    // 注意: 折り返し(_wrap)は考慮していない。ここが返すのは
    //       「改行だけで区切った場合の自然な寸法」である。

    if (_direction == TextDirection::HORIZONTAL)
    {
        int current_line_width = 0;
        int line_count = 1;
        bool first_in_line = true;

        for (uint16_t ch : unicode_chars)
        {
            if (ch == '\n')
            {
                width = std::max(width, current_line_width);
                current_line_width = 0;
                first_in_line = true;
                line_count++;
                continue;
            }

            const CharMetrics metrics = getCharMetrics(ch);
            const CharTypeAdjustment adjustment = getCharAdjustment(ch);

            // 描画側と同じ「setWidth × 横倍率」
            const int advance = static_cast<int>(metrics.setWidth * adjustment.widthScale);
            const int spacing = _charSpacing + adjustment.spacingOffset;

            current_line_width += first_in_line ? advance : (spacing + advance);
            first_in_line = false;
        }

        width = std::max(width, current_line_width);
        height = line_count * (getLineHeight() + _lineSpacing) - _lineSpacing;
    }
    else
    { // VERTICAL
        int current_column_height = 0;
        int column_count = 1;
        bool first_in_column = true;

        for (uint16_t ch : unicode_chars)
        {
            if (ch == '\n')
            {
                height = std::max(height, current_column_height);
                current_column_height = 0;
                first_in_column = true;
                column_count++;
                continue;
            }

            // 描画側と同じ手順で表示文字を決めてから送りを取る。
            // 小文字は大文字に変換され、さらに縦書き用グリフへ差し替えられる。
            uint16_t work_char = ch;
            if (needsSmallCharSubstitution(ch))
            {
                work_char = getCorrespondingLargeChar(ch);
            }
            const uint16_t display_char = convertToVerticalGlyph(work_char);

            const CharMetrics metrics = getCharMetrics(display_char);
            const CharTypeAdjustment adjustment = getCharAdjustment(ch);

            // 縦書きは em 固定送り（縮小率は掛けない）。描画側と同一。
            // 従来はここだけ metrics.height（ビットマップ高）を使っており、
            // 描画側を em 固定に変更した後も追随していなかった。
            const int advance = metrics.setWidth;
            const int spacing = _charSpacing + adjustment.spacingOffset;

            current_column_height += first_in_column ? advance : (spacing + advance);
            first_in_column = false;
        }

        height = std::max(height, current_column_height);

        // 列幅は描画側の columnWidth（= getMaxCharWidth()）に合わせる。
        // 従来は走査中の最大文字幅を使っており、描画側と基準が異なっていた。
        width = column_count * (getMaxCharWidth() + _lineSpacing) - _lineSpacing;
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
    // U+3008〜U+3011 は 〈〉《》「」『』【】 の10文字すべてが括弧なので範囲判定でよい。
    // 以前はこの後に (0x300C〜0x300F) の判定が続いていたが、
    // 上の範囲に完全に含まれるため到達しない冗長な条件だった（削除）。
    // また 〔U+3014〕U+3015 は _verticalGlyphMap には登録されているのに
    // ここでの分類から漏れていたため追加した。
    if ((unicode_char >= 0x3008 && unicode_char <= 0x3011) ||
        unicode_char == 0x3014 || unicode_char == 0x3015 ||
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

    ESP_LOGI(TAG, "Total: %u mappings", static_cast<unsigned>(_smallToLargeMap.size()));
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

    // CharCategory の宣言順と一致させること
    static const char *const categoryNames[] = {
        "NORMAL", "BRACKET", "HORIZONTAL_BAR",
        "PUNCTUATION", "SMALL_CHAR", "OTHER_SPECIAL"};
    constexpr size_t categoryNameCount = sizeof(categoryNames) / sizeof(categoryNames[0]);

    for (const auto &pair : _charAdjustments)
    {
        const CharCategory cat = pair.first;
        const CharTypeAdjustment &adj = pair.second;

        // 範囲チェックを追加。従来は categoryNames[static_cast<int>(cat)] を
        // 無検査で参照しており、CharCategory に要素が追加されると
        // 配列外アクセスになる状態だった。
        const size_t index = static_cast<size_t>(cat);
        const char *name = (index < categoryNameCount) ? categoryNames[index] : "UNKNOWN";

        ESP_LOGI(TAG, "%s: w=%.2f, h=%.2f, s=%d, v=%d, h=%d",
                 name,
                 adj.widthScale, adj.heightScale,
                 adj.spacingOffset, adj.verticalOffset, adj.horizontalOffset);
    }
    ESP_LOGI(TAG, "==================================");
}

// 補足: debugShowFixedAdjustments() をここに実装していたが、
//       同じ内容を debugShowCharAdjustments() が
//       （定数直読みではなく実際の _charAdjustments テーブルから）出力するため削除した。
//       テーブルは定数から構築されるので出力は一致し、
//       かつテーブル側を読むほうが実態を反映する。
