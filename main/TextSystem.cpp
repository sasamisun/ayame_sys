// main/TextSystem.cpp - テキスト描画系の初期化と保持

#include "TextSystem.hpp"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "fonts/active_font.h"

#include "SDcard.hpp"

static const char* TAG = "TEXTSYS";

// グローバル実体
TextSystem textSystem;

TextSystem::~TextSystem()
{
    clearBoxes();
    delete _vertical;
    delete _horizontal;
    delete _backlog;
    delete _prompt;

    if (_scenarioFont) {
        heap_caps_free(_scenarioFont);
        _scenarioFont = nullptr;
    }
}

TypoWrite* TextSystem::createWriter()
{
    TypoWrite* w = new TypoWrite(_display);
    w->setVLWParser(&_parser);
    w->loadFontFromArray(_activeFont);
    w->setColor(TFT_WHITE);
    w->setBackgroundColor(TFT_TRANSPARENT);
    w->setRubyEnabled(_rubyEnabled);
    return w;
}

std::vector<TypoWrite*> TextSystem::systemWriters()
{
    std::vector<TypoWrite*> out;
    for (TypoWrite* w : {_vertical, _horizontal, _backlog, _prompt}) {
        if (w) { out.push_back(w); }
    }
    return out;
}

bool TextSystem::defineBox(const std::string& name, int x, int y, int w, int h,
                           bool vertical, float fontSize,
                           int lineSpacing, int charSpacing, TextAlignment align,
                           const TextBoxPadding& padding, uint16_t textColor,
                           bool kinsoku)
{
    if (!_ready) {
        ESP_LOGE(TAG, "Not initialized");
        return false;
    }
    if (name.empty()) {
        ESP_LOGE(TAG, "Text box needs a name");
        return false;
    }
    if (w <= 0 || h <= 0) {
        ESP_LOGE(TAG, "Text box '%s' has a bad size (%dx%d)", name.c_str(), w, h);
        return false;
    }

    auto it = _boxes.find(name);
    TypoWrite* writer = (it != _boxes.end()) ? it->second : nullptr;

    if (!writer) {
        // TypoWrite の生成はマッピングテーブルの構築とスプライト確保を伴い重い。
        // 同じ名前が来たら作り直さず設定だけ入れ替える。
        writer = createWriter();
        _boxes[name] = writer;
    }

    writer->setPosition(x, y);
    writer->setArea(w, h);
    writer->setPadding(padding.top, padding.right, padding.bottom, padding.left);
    writer->setDirection(vertical ? TextDirection::VERTICAL : TextDirection::HORIZONTAL);
    writer->setFontSize(fontSize);
    writer->setLineSpacing(lineSpacing);
    writer->setCharSpacing(charSpacing);
    writer->setAlignment(align);
    writer->setColor(textColor);
    writer->setKinsoku(kinsoku);

    ESP_LOGI(TAG, "Text box '%s': (%d,%d) %dx%d %s x%.2f padding(%d,%d,%d,%d) -> text %dx%d",
             name.c_str(), x, y, w, h, vertical ? "vertical" : "horizontal", fontSize,
             padding.top, padding.right, padding.bottom, padding.left,
             writer->textWidth(), writer->textHeight());
    return true;
}

TypoWrite* TextSystem::box(const std::string& name)
{
    auto it = _boxes.find(name);
    return (it != _boxes.end()) ? it->second : nullptr;
}

void TextSystem::clearBoxes()
{
    for (auto& kv : _boxes) {
        delete kv.second;
    }
    _boxes.clear();
}

const uint8_t* TextSystem::fontData() const
{
    // 内蔵とは限らない。シナリオが差し替えていればそちらを返す。
    // ボタンのラベルもシナリオのフォントで描かれるほうが揃う。
    return _activeFont ? _activeFont : AYAME_FONT_DATA;
}

bool TextSystem::begin(M5GFX* display)
{
    if (_ready) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }
    if (!display) {
        ESP_LOGE(TAG, "display is null");
        return false;
    }

    _display = display;

    // 内蔵フォントで始める。描画器はまだ無いので applyFont() は使わない。
    _activeFont = AYAME_FONT_DATA;
    _activeFontSize = sizeof(AYAME_FONT_DATA);
    _fontName = AYAME_FONT_LABEL;

    if (!_parser.init(_activeFont, _activeFontSize)) {
        ESP_LOGE(TAG, "Failed to initialize VLW font");
        return false;
    }

    // どのフォントでビルドされたかをログに残す。
    // 見え方の違いを試している最中、書き込んだものが分からなくなるため。
    ESP_LOGI(TAG, "Font: %s (%u bytes)", _fontName.c_str(),
             static_cast<unsigned>(_activeFontSize));
    _parser.debugPrintFontInfo();

    // --- 縦書き（画面右側の帯） ---
    _vertical = new TypoWrite(display);
    _vertical->setVLWParser(&_parser);
    _vertical->loadFontFromArray(_activeFont);
    _vertical->setColor(TFT_WHITE);
    _vertical->setBackgroundColor(TFT_TRANSPARENT);
    _vertical->setDirection(TextDirection::VERTICAL);
    _vertical->setFontSize(1.0);
    _vertical->setLineSpacing(6);

    // 縦書きの字間。
    //
    // 送りは全角なら em 固定（16px フォントなら 16px）で、
    // ここに setCharSpacing() の値が足される。**負値は文字を重ねる方向。**
    //
    // 0 ではなく 2 にしてあるのは、送りが 1em ちょうどだと
    // 字面どうしの隙間が 1px しか空かず、縦書きが詰まって見えるため。
    //
    // 実測（shippori_16 / あ は h=15, topExtent=13）:
    //   charSpacing=0 -> 送り16px, インク間隔 1px（詰まって見える）
    //   charSpacing=1 -> 送り17px, インク間隔 2px
    //   charSpacing=2 -> 送り18px, インク間隔 3px（読みやすい）
    //   charSpacing=4 -> 送り20px, インク間隔 5px（間延びし始める）
    //
    // 旧フォント（`AYAME_FONT 9`）は送りが 17px あったので、
    // 0 でもインク間隔 2px が取れていた。16px のフォントに替えたぶん、
    // ここで足して補う形にしてある。
    //
    // ボックスごとに変えたい場合は `textboxes` の `char_spacing`。
    _vertical->setCharSpacing(DEFAULT_VERTICAL_CHAR_SPACING);

    // --- 横書き（画面下側の帯） ---
    //
    // 送り幅は setWidth ベース（プロポーショナル）なので、
    // 全角は一定間隔、半角英数は詰まって描かれるのが正しい。
    // 縦書きと違い小文字の変位・回転を通らないため、切り分けにも使える。
    _horizontal = new TypoWrite(display);
    _horizontal->setVLWParser(&_parser);
    _horizontal->loadFontFromArray(_activeFont);
    _horizontal->setColor(TFT_WHITE);
    _horizontal->setBackgroundColor(TFT_TRANSPARENT);
    _horizontal->setDirection(TextDirection::HORIZONTAL);
    _horizontal->setFontSize(1.0);
    _horizontal->setLineSpacing(6);
    _horizontal->setCharSpacing(0);
    _horizontal->setAlignment(TextAlignment::LEFT);

    // 位置と大きさは画面の向きで変わるので、まとめて別に置いてある
    layoutDefaultBoxes();

    _ready = true;
    return true;
}

TypoWrite* TextSystem::backlog()
{
    if (!_ready) {
        return nullptr;
    }

    if (!_backlog) {
        _backlog = createWriter();
        _backlog->setDirection(TextDirection::HORIZONTAL);
        _backlog->setFontSize(1.0);
        _backlog->setLineSpacing(10);
        _backlog->setCharSpacing(0);
        _backlog->setColor(TFT_WHITE);
    }

    // 画面いっぱい。向きが変わっても毎回合わせ直す。
    _backlog->setPosition(0, 0);
    _backlog->setArea(_display->width(), _display->height());
    _backlog->setPadding(24, 20, 24, 20);
    return _backlog;
}

TypoWrite* TextSystem::choicePrompt()
{
    if (!_ready) {
        return nullptr;
    }

    if (!_prompt) {
        _prompt = createWriter();
        _prompt->setDirection(TextDirection::HORIZONTAL);
        _prompt->setFontSize(1.0);
        _prompt->setLineSpacing(6);
        _prompt->setCharSpacing(0);
        _prompt->setAlignment(TextAlignment::CENTER);
        _prompt->setColor(TFT_WHITE);
    }

    // 位置と大きさは入れない。ボタンの個数で決まるので呼び出し側の仕事。
    return _prompt;
}

void TextSystem::layoutDefaultBoxes()
{
    if (!_display || !_vertical || !_horizontal) {
        return;
    }

    const int sw = _display->width();
    const int sh = _display->height();

    // 縦長か横長かで置き場所を変える。
    //
    // 縦長のときの値（右端の帯 (400,0) 130x700、下寄りの帯 (10,420) 380x180）は
    // 画面が 540x960 である前提で決め打ちしてあった。
    // 横向き（960x540）にすると縦書きの帯が高さ 700 で画面からはみ出すため、
    // 向きごとに持つ必要がある。
    if (sh >= sw) {
        // 縦長（540x960）。従来の値をそのまま使う。
        _vertical->setPosition(400, 0);
        _vertical->setArea(130, 700);
        _horizontal->setPosition(10, 420);
        _horizontal->setArea(380, 180);
    } else {
        // 横長（960x540）。同じ考え方で、縦書きは右端の帯、横書きは下側の帯。
        _vertical->setPosition(sw - 140, 20);
        _vertical->setArea(130, sh - 40);
        _horizontal->setPosition(20, sh - 180);
        _horizontal->setArea(sw - 180, 160);
    }

    ESP_LOGI(TAG, "Default boxes for %dx%d (vertical: %d,%d %dx%d / horizontal: %d,%d %dx%d)",
             sw, sh,
             _vertical->areaX(), _vertical->areaY(),
             _vertical->areaWidth(), _vertical->areaHeight(),
             _horizontal->areaX(), _horizontal->areaY(),
             _horizontal->areaWidth(), _horizontal->areaHeight());
}

bool TextSystem::applyFont(const uint8_t* data, size_t size, const std::string& name)
{
    if (!_parser.init(data, size)) {
        ESP_LOGE(TAG, "Failed to parse font '%s'", name.c_str());
        return false;
    }

    _activeFont = data;
    _activeFontSize = size;
    _fontName = name;

    // **生成済みの描画器すべてに配る。**
    // 配り忘れると、その描画器だけ前のフォントのメトリクスで組まれる。
    // さらに悪いことに、シナリオ独自フォントを解放したあとは
    // 解放済みのメモリを指したままになる。
    for (TypoWrite* w : systemWriters()) {
        w->loadFontFromArray(data);
    }
    for (auto& kv : _boxes) {
        kv.second->loadFontFromArray(data);
    }

    ESP_LOGI(TAG, "Font: %s (%u bytes)", name.c_str(), static_cast<unsigned>(size));
    _parser.debugPrintFontInfo();
    return true;
}

bool TextSystem::loadScenarioFont(const std::string& path)
{
    if (!_ready) {
        ESP_LOGE(TAG, "Not initialized");
        return false;
    }

    // 読む前に今のものを捨てる。
    // 1MB 級を2つ同時に抱えると PSRAM が苦しく、
    // シナリオ本文の展開に回すぶんが足りなくなる。
    useBuiltinFont();

    size_t len = 0;
    char* raw = SD.readFileToBuffer(path.c_str(), &len);
    if (!raw) {
        // SD 無し・USB MSC 中・パス違いなど。理由は SD 側のログに出ている。
        ESP_LOGW(TAG, "Could not read the scenario font: %s", path.c_str());
        return false;
    }

    uint8_t* data = reinterpret_cast<uint8_t*>(raw);

    // VLW かどうかを先に見る。
    // 別形式を渡されたまま解析へ進むと、でたらめなグリフ数で
    // 巨大な確保を試みることになる。
    if (len < 24) {
        ESP_LOGE(TAG, "Font file is too small to be a VLW: %s", path.c_str());
        heap_caps_free(data);
        return false;
    }
    const uint32_t version = (static_cast<uint32_t>(data[4]) << 24)
                           | (static_cast<uint32_t>(data[5]) << 16)
                           | (static_cast<uint32_t>(data[6]) << 8)
                           | static_cast<uint32_t>(data[7]);
    if (version != 11) {
        ESP_LOGE(TAG, "Not a VLW font (version %lu, expected 11): %s",
                 static_cast<unsigned long>(version), path.c_str());
        heap_caps_free(data);
        return false;
    }

    if (!applyFont(data, len, path)) {
        // 解析に失敗した。内蔵へ戻して再生は続けられるようにする。
        heap_caps_free(data);
        applyFont(AYAME_FONT_DATA, sizeof(AYAME_FONT_DATA), AYAME_FONT_LABEL);
        return false;
    }

    // ここで初めて所有権を持つ。applyFont が成功した後に代入するのは、
    // 失敗経路で二重解放しないため。
    _scenarioFont = data;

    ESP_LOGI(TAG, "Scenario font in use (%u KB, PSRAM free %u KB)",
             static_cast<unsigned>(len / 1024),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    return true;
}

void TextSystem::useBuiltinFont()
{
    if (!_scenarioFont) {
        return;   // 既に内蔵
    }

    // 解放する前に描画器の参照を内蔵へ戻す。
    // 順番を逆にすると、解放済みの領域を指したまま1回でも描画が走ると落ちる。
    applyFont(AYAME_FONT_DATA, sizeof(AYAME_FONT_DATA), AYAME_FONT_LABEL);

    heap_caps_free(_scenarioFont);
    _scenarioFont = nullptr;

    ESP_LOGI(TAG, "Back to the built-in font (PSRAM free %u KB)",
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
}

void TextSystem::setRubyEnabled(bool enabled)
{
    _rubyEnabled = enabled;

    // 生成済みの描画器すべてと、シナリオが定義したボックスに配る。
    // ここを忘れると、その描画器だけルビが出ない。
    for (TypoWrite* w : systemWriters()) {
        w->setRubyEnabled(enabled);
    }
    for (auto& kv : _boxes) {
        kv.second->setRubyEnabled(enabled);
    }
}
