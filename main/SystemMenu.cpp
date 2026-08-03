// main/SystemMenu.cpp - シナリオ選択と本体機能のメニュー

#include "SystemMenu.hpp"

#include "Buzzer.hpp"
#include "Power.hpp"
#include "Settings.hpp"
#include "SDcard.hpp"
#include "icons/images.h"
#include "icons/menu_icons.h"
#include "ScenarioLoader.hpp"
#include "SimpleTransition.hpp"
#include "TextSystem.hpp"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
// 下段の高さはアイコン画像の寸法（MENU_ICON_HEIGHT）で決まるため、
// ここで別に持たない。
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

int SystemMenu::pageCount() const
{
    if (_scenarioIds.empty()) {
        return 1;
    }
    return (static_cast<int>(_scenarioIds.size()) + MAX_LIST_ITEMS - 1) / MAX_LIST_ITEMS;
}

bool SystemMenu::changePage(int delta)
{
    const int total = pageCount();
    const int next = _page + delta;

    // 端で止める。巡回させると「今どこか」が分からなくなるため。
    if (next < 0 || next >= total) {
        return false;
    }

    _page = next;
    ESP_LOGI(TAG, "Scenario list page %d/%d", _page + 1, total);

    // 電子ペーパーなので部分更新はせず画面ごと作り直す
    draw();
    return true;
}

void SystemMenu::scanScenarios()
{
    _scenarioIds.clear();
    _scenarioTitles.clear();

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

    // 表示名を集める。
    // 各 scenario.json の先頭だけを読むので、全文解析より桁違いに軽い。
    // タイトルが取れなければフォルダ名がそのまま入る。
    _scenarioTitles.reserve(_scenarioIds.size());
    for (const std::string& id : _scenarioIds) {
        _scenarioTitles.push_back(ScenarioLoader::peekTitle(id.c_str()));
    }

    // 件数が減ってページが範囲外に残ることがある
    if (_page >= pageCount()) {
        _page = pageCount() - 1;
    }

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

    delete _powerOffButton;
    _powerOffButton = nullptr;

    delete _aboutButton;
    _aboutButton = nullptr;

    delete _soundButton;
    _soundButton = nullptr;
}

void SystemMenu::cancelPowerOffConfirm()
{
    if (!_powerOffArmed) {
        return;
    }
    _powerOffArmed = false;
    ESP_LOGI(TAG, "Power-off confirmation cancelled");
    draw();
}

void SystemMenu::shutdown()
{
    ESP_LOGI(TAG, "Shutting down");

    // 電子ペーパーは電源を切っても最後の像が残る。
    // メニューのまま落とすと、次に入れるまでそれが見えたままになるので、
    // 「切った」と分かる画面に置き換えてから落とす。
    //
    // 描く前に白黒反転で粒子を振り切っておく。
    //
    // 電源を切った後に像が薄くなるのは、粒子が端まで動ききっていないため。
    // refreshScreen() は「今の値へ再駆動」するだけなので、
    // 直前の状態から少ししか動かない画素は中途半端な位置で止まる。
    // 給電が続いていれば見た目は保たれるが、電源を落とすと崩れる。
    //
    // clearGhosting() の白→黒→白は全画素を両端まで振るので、
    // その後に描いた像は端から端への移動になり、しっかり定着する。
    SimpleTransition::clearGhosting(_display);

    _display->fillScreen(TFT_WHITE);

    // ロゴを中央に置く。
    // 画像はファームウェアに埋め込んである（SD だと未挿入や USB MSC 中に出せない）。
    // 1bpp なので拡大縮小はせず原寸で描く。
    const int logoX = (_display->width() - image_ayamelogo_width) / 2;
    const int logoY = (_display->height() - image_ayamelogo_height) / 2;

    if (!_display->drawPng(image_ayamelogo, image_ayamelogo_len, logoX, logoY)) {
        // 画像が壊れていても「電源が切れた」ことは伝わるようにする
        ESP_LOGE(TAG, "Failed to draw the shutdown logo");
        _display->unloadFont();
        _display->setTextColor(TFT_BLACK, TFT_WHITE);
        _display->setTextSize(3);
        _display->setTextDatum(middle_center);
        _display->drawString("AYAME", _display->width() / 2, _display->height() / 2);
        _display->setTextDatum(top_left);
    }

    // 表示を確実に定着させてから電源を落とす。
    // 描いただけでは走査が終わっておらず、途中で電源が切れると像が崩れる。
    SimpleTransition::refreshScreen(_display);
    _display->waitDisplay();

    // 粒子が落ち着くまで少し置いてから電源を切る。
    //
    // ここで display->sleep() を呼んで EPD の電源レールを先に落とす案を試したが、
    // **画面が真っ黒になって悪化した**ため採用していない。
    // Bus_EPD::powerControl(false) は pwr → oe → spv の順に下げるだけで、
    // sph / ckv / cl / le は元の状態のまま残る。
    // その状態で放置するとパネルが暗転する。触らないほうがよい。
    vTaskDelay(pdMS_TO_TICKS(300));

    power.powerOff();

    // ここへ戻るのは電源が落ちなかったとき（USB 給電中など）。
    // 画面を消してしまったままだと操作不能に見えるのでメニューへ戻す。
    ESP_LOGW(TAG, "Power off did not take effect. Returning to the menu");
    _powerOffArmed = false;
    draw();
}

void SystemMenu::enter()
{
    // USB MSC で PC からシナリオが追加されている可能性があるので毎回読み直す
    scanScenarios();

    if (_view == View::About) {
        drawAbout();
    } else {
        draw();
    }
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

    // 直前に誰が何のフォントを載せたか分からない状態で描き始めない。
    //
    // setTextSize() は「今読み込まれているフォント」に対する倍率なので、
    // VLW(16pt) が残っていると 3 倍で 48px、既定フォント(8px)なら 24px と
    // 同じコードが別の大きさで描かれる。
    // この画面の文字は ASCII だけなので、既定フォントに戻してから描く。
    _display->unloadFont();

    // --- 見出し ---
    _display->setTextColor(TFT_WHITE, TFT_BLACK);
    _display->setTextSize(3);
    _display->setCursor(ITEM_LEFT, TITLE_Y);
    _display->print("AYAME");
    _display->setTextSize(1);
    _display->setCursor(ITEM_LEFT, TITLE_Y + 40);
    _display->printf("%u scenario(s) on SD",
                     static_cast<unsigned>(_scenarioIds.size()));

    // --- 電池残量（右上） ---
    //
    // ボタンにはせず常時表示にする。押す用事が無く、
    // 画面を作り直すたびに更新されれば十分なため。
    {
        const int percent = power.batteryPercent();
        const int mv = power.batteryMilliVolts();

        const int gaugeW = 60;
        const int gaugeH = 22;
        const int gaugeX = _display->width() - ITEM_LEFT - gaugeW;
        const int gaugeY = TITLE_Y;

        _display->drawRect(gaugeX, gaugeY, gaugeW, gaugeH, TFT_WHITE);
        // 電池の突起
        _display->fillRect(gaugeX + gaugeW, gaugeY + gaugeH / 2 - 4, 3, 8, TFT_WHITE);

        if (percent >= 0) {
            const int fillW = (gaugeW - 4) * percent / 100;
            if (fillW > 0) {
                _display->fillRect(gaugeX + 2, gaugeY + 2, fillW, gaugeH - 4, TFT_WHITE);
            }
        }

        _display->setTextSize(1);
        _display->setCursor(gaugeX - 4, gaugeY + gaugeH + 6);
        if (percent >= 0) {
            _display->printf("%3d%%  %d.%02dV", percent, mv / 1000, (mv % 1000) / 10);
        } else {
            // ADC が使えないとき（初期化失敗など）
            _display->print(" --%   --.--V");
        }
    }

    // --- シナリオ一覧（現在ページぶん） ---
    const size_t first = static_cast<size_t>(_page) * MAX_LIST_ITEMS;
    const size_t last = std::min<size_t>(first + MAX_LIST_ITEMS, _scenarioIds.size());

    for (size_t i = first; i < last; ++i) {
        const int slot = static_cast<int>(i - first);
        const int y = LIST_TOP + slot * (ITEM_HEIGHT + ITEM_GAP);

        // サムネイル。無ければ枠だけ描く。
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

        // 表示名のボタン。サムネイルの右側だけを当たり判定にする。
        const int btnX = thumbX + THUMB_SIZE + 12;
        const int btnW = _display->width() - btnX - ITEM_LEFT;

        Button* btn = new Button(_display, btnX, y, btnW, ITEM_HEIGHT,
                                 _scenarioTitles[i].c_str());
        btn->setOnReleased(&SystemMenu::onScenarioTapped);
        // タイトルは日本語なので VLW が要る
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

    // --- ページ表示 ---
    //
    // スワイプは見えない操作なので、送れることが分かる手がかりを必ず出す。
    // 1ページに収まっているときは出さない（意味が無く紛らわしいため）。
    if (pageCount() > 1) {
        _display->setTextColor(TFT_ORANGE, TFT_BLACK);
        _display->setTextSize(2);
        const int y = LIST_TOP + MAX_LIST_ITEMS * (ITEM_HEIGHT + ITEM_GAP) + 8;

        _display->setCursor(ITEM_LEFT, y);
        _display->printf("%d / %d", _page + 1, pageCount());

        _display->setTextSize(1);
        _display->setCursor(ITEM_LEFT + 90, y + 8);
        if (_page == 0) {
            _display->print("swipe up for more");
        } else if (_page == pageCount() - 1) {
            _display->print("swipe down to go back");
        } else {
            _display->print("swipe up / down");
        }

        _display->setTextColor(TFT_WHITE, TFT_BLACK);
    }

    buildFooterButtons();

    _buttons->drawButtons();

    // 電子ペーパーは描いていない領域が薄くなるので、
    // 画面を作り直したここで一度だけ全画素を駆動しておく。
    SimpleTransition::refreshScreen(_display);
}

void SystemMenu::buildFooterButtons()
{
    // アイコンは画像そのものが見た目になるので、隙間なく並べる。
    // 画面幅 540 に対し 108 x 5 = 540 でちょうど収まる。
    const int cellW = MENU_ICON_WIDTH;
    const int footerY = _display->height() - MENU_ICON_HEIGHT;
    const int startX = (_display->width() - cellW * FOOTER_BUTTON_COUNT) / 2;

    auto makeButton = [&](int slot, const uint8_t* icon, size_t iconLen,
                          const char* fallbackLabel,
                          void (*handler)(Button*)) -> Button* {
        const int x = startX + slot * cellW;
        Button* btn = new Button(_display, x, footerY, cellW, MENU_ICON_HEIGHT,
                                 fallbackLabel);
        btn->setOnReleased(handler);

        // アイコンPNGはファームウェアに埋め込んである。
        // SD に置くと USB MSC 中に読めなくなり、
        // 「USB を切る」ボタンが描けず操作不能になる。
        btn->setIcon(icon, iconLen);

        // 画像が壊れていた場合に文字で出せるよう、日本語フォントも渡しておく
        btn->setVlwFont(textSystem.fontData());

        _buttons->addButton(btn);
        return btn;
    };

    const bool usbOn = SD.isUSBMSCEnabled();
    _usbMscButton = makeButton(0,
                               usbOn ? icon_usb_off : icon_usb_on,
                               usbOn ? icon_usb_off_len : icon_usb_on_len,
                               usbOn ? "USB 切" : "USB 入",
                               &SystemMenu::onUsbMscTapped);

    _refreshButton = makeButton(1, icon_reload, icon_reload_len,
                                "再読込", &SystemMenu::onRefreshTapped);

    const bool soundOn = !buzzer.isMuted();
    _soundButton = makeButton(2,
                              soundOn ? icon_sound_on : icon_sound_off,
                              soundOn ? icon_sound_on_len : icon_sound_off_len,
                              soundOn ? "音 入" : "音 切",
                              &SystemMenu::onSoundTapped);

    const bool inAbout = (_view == View::About);
    _aboutButton = makeButton(3,
                              inAbout ? icon_back : icon_info,
                              inAbout ? icon_back_len : icon_info_len,
                              inAbout ? "戻る" : "情報",
                              &SystemMenu::onAboutTapped);

    // 確認状態では絵を変えて、次のタップで切れることを示す
    _powerOffButton = makeButton(4,
                                 _powerOffArmed ? icon_power_confirm : icon_power,
                                 _powerOffArmed ? icon_power_confirm_len : icon_power_len,
                                 _powerOffArmed ? "もう一度" : "電源",
                                 &SystemMenu::onPowerOffTapped);
}

void SystemMenu::drawAbout()
{
    destroyButtons();

    _display->fillScreen(TFT_BLACK);

    // draw() と同じ理由でフォント状態を既定へ戻してから描く
    _display->unloadFont();

    _display->setTextColor(TFT_WHITE, TFT_BLACK);
    _display->setTextSize(3);
    _display->setCursor(ITEM_LEFT, TITLE_Y);
    _display->print("INFO");

    int y = LIST_TOP;
    const int lineHeight = 26;
    _display->setTextSize(1);

    auto line = [&](const char* fmt, ...) {
        char buf[128];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        _display->setCursor(ITEM_LEFT, y);
        _display->print(buf);
        y += lineHeight;
    };

    const esp_app_desc_t* app = esp_app_get_description();
    if (app) {
        line("Firmware : %s", app->version);
        line("Built    : %s %s", app->date, app->time);
    }
    line("ESP-IDF  : %s", esp_get_idf_version());
    y += 10;

    const int percent = power.batteryPercent();
    const int mv = power.batteryMilliVolts();
    if (percent >= 0) {
        line("Battery  : %d%%  (%d mV)", percent, mv);
    } else {
        line("Battery  : unavailable");
    }
    y += 10;

    line("PSRAM    : %u KB free",
         static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    line("Internal : %u KB free",
         static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
    y += 10;

    sdmmc_card_t* card = SD.getCard();
    if (card) {
        const uint64_t capacityMb =
            (static_cast<uint64_t>(card->csd.capacity) * card->csd.sector_size) / (1024 * 1024);
        line("SD card  : %s", card->cid.name);
        line("Capacity : %llu MB", capacityMb);
    } else {
        line("SD card  : not mounted");
    }
    line("Scenarios: %u", static_cast<unsigned>(_scenarioIds.size()));

    buildFooterButtons();
    _buttons->drawButtons();

    SimpleTransition::refreshScreen(_display);
}

void SystemMenu::update(bool hasTouchEvent)
{
    // 一覧のページ送り。
    //
    // 下段はアイコン5つで埋まっていて前/次ボタンを置く余地が無いため、
    // スワイプで送る。上へ払うと次（下の項目が上がってくる）向き。
    //
    // ボタンより先に見るのは、スワイプの終点がボタンの上でも
    // ページ送りとして扱いたいため。changePage() は draw() を呼ぶので、
    // 移った後にボタンを触らせない。
    if (hasTouchEvent && _view == View::List && _touch && _touch->isSwipeEvent()) {
        if (_touch->isSwipeUp() && changePage(+1)) {
            return;
        }
        if (_touch->isSwipeDown() && changePage(-1)) {
            return;
        }
    }

    // 電源OFFの確認状態を放置したら解除する。
    // 押したまま忘れて、あとで別の用事でタップして切れる事故を防ぐ。
    if (_powerOffArmed) {
        const int64_t nowMs = esp_timer_get_time() / 1000;
        if (nowMs - _powerOffArmedAtMs > POWER_OFF_CONFIRM_TIMEOUT_MS) {
            cancelPowerOffConfirm();
        }
    }

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
        if (s_instance->_scenarioButtons[i] != btn) {
            continue;
        }

        // ボタンは現在ページぶんしか作られていないので、
        // ページの先頭を足して本来の位置に直す。
        // これを忘れると2ページ目以降で別のシナリオが起動する。
        const size_t index =
            static_cast<size_t>(s_instance->_page) * MAX_LIST_ITEMS + i;

        if (index >= s_instance->_scenarioIds.size()) {
            ESP_LOGE(TAG, "Scenario index %u is out of range",
                     static_cast<unsigned>(index));
            return;
        }

        s_instance->_selectedId = s_instance->_scenarioIds[index];
        ESP_LOGI(TAG, "Scenario selected: %s (page %d, slot %u)",
                 s_instance->_selectedId.c_str(),
                 s_instance->_page + 1, static_cast<unsigned>(i));
        return;
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
        s_instance->_view = View::List;
        s_instance->enter();
    }
}

void SystemMenu::onSoundTapped(Button* btn)
{
    (void)btn;
    if (!s_instance) {
        return;
    }

    // 別の操作をしたので、電源OFFの構えは解いておく
    s_instance->_powerOffArmed = false;

    const bool nextMuted = !buzzer.isMuted();
    buzzer.setMuted(nextMuted);

    // 設定は変更のたびに書く。
    // ここは押されたときしか通らないので書き込み回数は限られるし、
    // 「切ったのに次の起動で鳴る」のが分かりにくいため即座に残す。
    settings.setSoundEnabled(!nextMuted);
    if (!settings.save()) {
        // USB MSC 中や SD 無しでは保存できない。
        // 今回の切り替え自体は効いているので、動作は続ける。
        ESP_LOGW(TAG, "Sound setting changed but could not be saved");
    }

    if (!nextMuted) {
        // 音を入れたことが分かるよう、その場で短く鳴らす
        buzzer.tone(880, 80);
    }

    if (s_instance->_view == View::About) {
        s_instance->drawAbout();
    } else {
        s_instance->draw();
    }
}

void SystemMenu::onAboutTapped(Button* btn)
{
    (void)btn;
    if (!s_instance) {
        return;
    }

    // 別の操作をしたので、電源OFFの構えは解いておく
    s_instance->_powerOffArmed = false;

    if (s_instance->_view == View::About) {
        s_instance->_view = View::List;
        s_instance->draw();
    } else {
        s_instance->_view = View::About;
        s_instance->drawAbout();
    }
}

void SystemMenu::onPowerOffTapped(Button* btn)
{
    (void)btn;
    if (!s_instance) {
        return;
    }

    if (s_instance->_powerOffArmed) {
        s_instance->shutdown();
        return;
    }

    // 1回目。押し間違いで電源が落ちると作業が飛ぶので、確認を挟む。
    s_instance->_powerOffArmed = true;
    s_instance->_powerOffArmedAtMs = esp_timer_get_time() / 1000;
    ESP_LOGI(TAG, "Power off armed. Tap again within %d ms",
             static_cast<int>(POWER_OFF_CONFIRM_TIMEOUT_MS));

    if (s_instance->_view == View::About) {
        s_instance->drawAbout();
    } else {
        s_instance->draw();
    }
}
