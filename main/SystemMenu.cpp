// main/SystemMenu.cpp - シナリオ選択と本体機能のメニュー

#include "SystemMenu.hpp"

#include "SDcard.hpp"
#include "ScenarioLoader.hpp"
#include "SimpleTransition.hpp"
#include "TextSystem.hpp"
#include "esp_log.h"

#include <algorithm>

static const char* TAG = "MENU";

SystemMenu* SystemMenu::s_instance = nullptr;

// 画面レイアウト
namespace {
constexpr int TITLE_Y        = 24;
constexpr int LIST_TOP       = 90;
constexpr int ITEM_HEIGHT    = 64;
constexpr int ITEM_GAP       = 10;
constexpr int ITEM_LEFT      = 20;
constexpr int THUMB_SIZE     = 56;
constexpr int FOOTER_HEIGHT  = 60;
constexpr int MAX_LIST_ITEMS = 8;
}

SystemMenu::~SystemMenu()
{
    destroyButtons();
    delete _buttons;
}

void SystemMenu::begin(M5GFX* display, TouchHandler* touch)
{
    _display = display;
    _touch = touch;
    s_instance = this;

    if (!_buttons) {
        _buttons = new ButtonManager(display, touch);
    }
}

void SystemMenu::scanScenarios()
{
    _scenarioIds.clear();

    // USB MSC 中は全ファイル操作が失敗する。
    // ここで弾いておかないと「シナリオが0件」と紛らわしい表示になる。
    if (SD.isUSBMSCEnabled()) {
        ESP_LOGW(TAG, "USB MSC is active. Scenario list is unavailable");
        return;
    }

    DirInfo* dir = SD.listDir(ScenarioLoader::SCENARIOS_ROOT);
    if (!dir) {
        ESP_LOGW(TAG, "Cannot list '%s'. Is the folder present on the SD card?",
                 ScenarioLoader::SCENARIOS_ROOT);
        return;
    }

    for (size_t i = 0; i < dir->count; ++i) {
        const FileInfo& entry = dir->files[i];
        if (!entry.isDirectory) {
            continue;
        }
        if (entry.name[0] == '.') {
            continue;   // "." ".." と隠しフォルダ
        }
        _scenarioIds.push_back(entry.name);
    }

    SD.freeDirInfo(dir);

    std::sort(_scenarioIds.begin(), _scenarioIds.end());

    ESP_LOGI(TAG, "Found %u scenario(s)", static_cast<unsigned>(_scenarioIds.size()));
}

void SystemMenu::destroyButtons()
{
    if (_buttons) {
        _buttons->clearButtons();
    }

    for (Button* btn : _scenarioButtons) {
        delete btn;
    }
    _scenarioButtons.clear();

    delete _usbMscButton;
    _usbMscButton = nullptr;

    delete _refreshButton;
    _refreshButton = nullptr;
}

void SystemMenu::enter()
{
    // USB MSC で PC からシナリオが追加されている可能性があるので毎回読み直す
    scanScenarios();
    draw();
}

void SystemMenu::leave()
{
    destroyButtons();
}

void SystemMenu::draw()
{
    if (!_display || !_buttons) {
        return;
    }

    destroyButtons();

    _display->fillScreen(TFT_BLACK);

    // --- 見出し ---
    _display->setTextColor(TFT_WHITE, TFT_BLACK);
    _display->setTextSize(3);
    _display->setCursor(ITEM_LEFT, TITLE_Y);
    _display->print("AYAME");
    _display->setTextSize(1);
    _display->setCursor(ITEM_LEFT, TITLE_Y + 40);
    _display->printf("%u scenario(s) on SD",
                     static_cast<unsigned>(_scenarioIds.size()));

    // --- シナリオ一覧 ---
    const size_t shown = std::min<size_t>(_scenarioIds.size(), MAX_LIST_ITEMS);

    for (size_t i = 0; i < shown; ++i) {
        const int y = LIST_TOP + static_cast<int>(i) * (ITEM_HEIGHT + ITEM_GAP);

        // サムネイル。無ければ枠だけ描く。
        // ここでは scenario.json を読まないので、見た目の手がかりはこれとフォルダ名だけ。
        const std::string thumbPath =
            std::string(ScenarioLoader::SCENARIOS_ROOT) + "/" +
            _scenarioIds[i] + "/thumbnail.png";

        const int thumbX = ITEM_LEFT + 4;
        const int thumbY = y + (ITEM_HEIGHT - THUMB_SIZE) / 2;

        if (!SD.exists(thumbPath.c_str()) ||
            !_display->drawPngFile(&SD, thumbPath.c_str(), thumbX, thumbY,
                                   THUMB_SIZE, THUMB_SIZE)) {
            _display->drawRect(thumbX, thumbY, THUMB_SIZE, THUMB_SIZE, TFT_DARKGREY);
        }

        // フォルダ名のボタン。サムネイルの右側だけを当たり判定にする。
        const int btnX = thumbX + THUMB_SIZE + 12;
        const int btnW = _display->width() - btnX - ITEM_LEFT;

        Button* btn = new Button(_display, btnX, y, btnW, ITEM_HEIGHT,
                                 _scenarioIds[i].c_str());
        btn->setOnReleased(&SystemMenu::onScenarioTapped);
        btn->setVlwFont(textSystem.fontData());

        ButtonStyle style = ButtonStyle::defaultStyle();
        style.bgColor = TFT_DARKGREY;
        style.textColor = TFT_WHITE;
        btn->setStyle(style);

        _buttons->addButton(btn);
        _scenarioButtons.push_back(btn);
    }

    if (_scenarioIds.empty()) {
        _display->setTextColor(TFT_ORANGE, TFT_BLACK);
        _display->setTextSize(2);
        _display->setCursor(ITEM_LEFT, LIST_TOP + 20);
        if (SD.isUSBMSCEnabled()) {
            _display->print("USB MSC is active.");
            _display->setCursor(ITEM_LEFT, LIST_TOP + 50);
            _display->print("Disable it to see scenarios.");
        } else {
            _display->print("No scenarios found.");
            _display->setCursor(ITEM_LEFT, LIST_TOP + 50);
            _display->print("Put them in /scenarios/");
        }
        _display->setTextColor(TFT_WHITE, TFT_BLACK);
    }

    if (_scenarioIds.size() > MAX_LIST_ITEMS) {
        _display->setTextColor(TFT_ORANGE, TFT_BLACK);
        _display->setTextSize(1);
        _display->setCursor(ITEM_LEFT,
                            LIST_TOP + MAX_LIST_ITEMS * (ITEM_HEIGHT + ITEM_GAP) + 6);
        _display->printf("(showing first %d of %u)",
                         MAX_LIST_ITEMS, static_cast<unsigned>(_scenarioIds.size()));
        _display->setTextColor(TFT_WHITE, TFT_BLACK);
    }

    // --- 下段の本体機能 ---
    const int footerY = _display->height() - FOOTER_HEIGHT - 20;
    const int halfW = (_display->width() - ITEM_LEFT * 2 - 12) / 2;

    _usbMscButton = new Button(_display, ITEM_LEFT, footerY, halfW, FOOTER_HEIGHT,
                               SD.isUSBMSCEnabled() ? "USB: ON" : "USB: OFF");
    _usbMscButton->setOnReleased(&SystemMenu::onUsbMscTapped);
    ButtonStyle usbStyle = ButtonStyle::defaultStyle();
    usbStyle.bgColor = SD.isUSBMSCEnabled() ? TFT_MAROON : TFT_NAVY;
    usbStyle.textColor = TFT_WHITE;
    _usbMscButton->setStyle(usbStyle);
    _buttons->addButton(_usbMscButton);

    _refreshButton = new Button(_display, ITEM_LEFT + halfW + 12, footerY,
                                halfW, FOOTER_HEIGHT, "Reload");
    _refreshButton->setOnReleased(&SystemMenu::onRefreshTapped);
    ButtonStyle refreshStyle = ButtonStyle::defaultStyle();
    refreshStyle.bgColor = TFT_DARKGREEN;
    refreshStyle.textColor = TFT_WHITE;
    _refreshButton->setStyle(refreshStyle);
    _buttons->addButton(_refreshButton);

    _buttons->drawButtons();

    // 電子ペーパーは描いていない領域が薄くなるので、
    // 画面を作り直したここで一度だけ全画素を駆動しておく。
    SimpleTransition::refreshScreen(_display);
}

void SystemMenu::update(bool hasTouchEvent)
{
    (void)hasTouchEvent;   // ButtonManager が TouchHandler を直接見る
    if (_buttons) {
        _buttons->update();
    }
}

void SystemMenu::onScenarioTapped(Button* btn)
{
    if (!s_instance || !btn) {
        return;
    }

    for (size_t i = 0; i < s_instance->_scenarioButtons.size(); ++i) {
        if (s_instance->_scenarioButtons[i] == btn) {
            s_instance->_selectedId = s_instance->_scenarioIds[i];
            ESP_LOGI(TAG, "Scenario selected: %s", s_instance->_selectedId.c_str());
            return;
        }
    }
}

void SystemMenu::onUsbMscTapped(Button* btn)
{
    (void)btn;
    if (!s_instance) {
        return;
    }

    if (SD.isUSBMSCEnabled()) {
        ESP_LOGI(TAG, "Disabling USB MSC");
        SD.disableUSBMSC();
    } else {
        ESP_LOGI(TAG, "Enabling USB MSC");
        SD.enableUSBMSC();
    }

    // 有効化でシナリオが引けなくなり、無効化で PC 側の追加分が見えるようになる。
    // どちらの向きでも一覧を作り直す必要がある。
    s_instance->enter();
}

void SystemMenu::onRefreshTapped(Button* btn)
{
    (void)btn;
    if (s_instance) {
        ESP_LOGI(TAG, "Reloading scenario list");
        s_instance->enter();
    }
}
