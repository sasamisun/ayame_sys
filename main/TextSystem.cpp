// main/TextSystem.cpp - テキスト描画系の初期化と保持

#include "TextSystem.hpp"

#include "esp_log.h"
#include "fonts/shippori_16.h"

static const char* TAG = "TEXTSYS";

// グローバル実体
TextSystem textSystem;

TextSystem::~TextSystem()
{
    clearBoxes();
    delete _vertical;
    delete _horizontal;
}

TypoWrite* TextSystem::createWriter()
{
    TypoWrite* w = new TypoWrite(_display);
    w->setVLWParser(&_parser);
    w->loadFontFromArray(shippori);
    w->setColor(TFT_WHITE);
    w->setBackgroundColor(TFT_TRANSPARENT);
    w->setRubyEnabled(_rubyEnabled);
    return w;
}

bool TextSystem::defineBox(const std::string& name, int x, int y, int w, int h,
                           bool vertical, float fontSize,
                           int lineSpacing, int charSpacing, TextAlignment align)
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
    writer->setDirection(vertical ? TextDirection::VERTICAL : TextDirection::HORIZONTAL);
    writer->setFontSize(fontSize);
    writer->setLineSpacing(lineSpacing);
    writer->setCharSpacing(charSpacing);
    writer->setAlignment(align);

    ESP_LOGI(TAG, "Text box '%s': (%d,%d) %dx%d %s x%.2f",
             name.c_str(), x, y, w, h, vertical ? "vertical" : "horizontal", fontSize);
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
    return shippori;
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

    if (!_parser.init(shippori, sizeof(shippori))) {
        ESP_LOGE(TAG, "Failed to initialize VLW font");
        return false;
    }

    ESP_LOGI(TAG, "VLW font initialized successfully");
    _parser.debugPrintFontInfo();

    // --- 縦書き（画面右側の帯） ---
    _vertical = new TypoWrite(display);
    _vertical->setVLWParser(&_parser);
    _vertical->loadFontFromArray(shippori);
    _vertical->setPosition(400, 0);
    _vertical->setArea(130, 700);
    _vertical->setColor(TFT_WHITE);
    _vertical->setBackgroundColor(TFT_TRANSPARENT);
    _vertical->setDirection(TextDirection::VERTICAL);
    _vertical->setFontSize(1.0);
    _vertical->setLineSpacing(6);

    // 縦書きの字間。
    //
    // 送りは em 固定（setWidth = 17px）なので、0 で「ベタ組み」になる。
    // shippori_16 の実測では、あ(h=15/topExtent=13) を並べたとき
    //   charSpacing=0  -> 送り17px, インク間隔 2px（適正）
    //   charSpacing=-4 -> 送り13px, 2px 重なる
    //   charSpacing=-8 -> 送り 9px, 6px 重なる
    // 詰めたい場合でも -4 程度までにとどめること。
    _vertical->setCharSpacing(0);

    // --- 横書き（画面下側の帯） ---
    //
    // 送り幅は setWidth ベース（プロポーショナル）なので、
    // 全角は一定間隔、半角英数は詰まって描かれるのが正しい。
    // 縦書きと違い小文字の変位・回転を通らないため、切り分けにも使える。
    _horizontal = new TypoWrite(display);
    _horizontal->setVLWParser(&_parser);
    _horizontal->loadFontFromArray(shippori);
    _horizontal->setPosition(10, 420);
    _horizontal->setArea(380, 180);
    _horizontal->setColor(TFT_WHITE);
    _horizontal->setBackgroundColor(TFT_TRANSPARENT);
    _horizontal->setDirection(TextDirection::HORIZONTAL);
    _horizontal->setFontSize(1.0);
    _horizontal->setLineSpacing(6);
    _horizontal->setCharSpacing(0);
    _horizontal->setAlignment(TextAlignment::LEFT);

    _ready = true;
    ESP_LOGI(TAG, "Text renderers ready (vertical: 400,0 130x700 / horizontal: 10,420 380x180)");
    return true;
}

void TextSystem::setRubyEnabled(bool enabled)
{
    _rubyEnabled = enabled;

    if (_vertical) {
        _vertical->setRubyEnabled(enabled);
    }
    if (_horizontal) {
        _horizontal->setRubyEnabled(enabled);
    }

    // シナリオが定義したボックスにも同じ設定を配る。
    // ここを忘れると、名前付きボックスだけルビが出ない。
    for (auto& kv : _boxes) {
        kv.second->setRubyEnabled(enabled);
    }
}
