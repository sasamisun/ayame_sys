// main/main.cpp - アプリケーションの入口と画面の統括
//
// M5PaperS3 向けアドベンチャーゲーム基盤。
// 起動するとシステムメニューが出て、SD の scenarios/ から選んで再生する。
//
// 旧 hello_world_main.cpp は各機能の動作確認を1ファイルに詰めたもので、
// 現在はビルド対象から外してある（main/CMakeLists.txt を参照）。

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"   // xTaskCreatePinnedToCore の宣言元
#include "esp_log.h"
#include <M5GFX.h>

#include "Buzzer.hpp"
#include "Power.hpp"
#include "SDcard.hpp"
#include "ScenarioLoader.hpp"
#include "ScenarioPlayer.hpp"
#include "Settings.hpp"
#include "SimpleTransition.hpp"
#include "SystemMenu.hpp"
#include "TextSystem.hpp"
#include "TouchHandler.hpp"

#include <string>
#include <vector>

static const char *TAG = "APP";

M5GFX display;
TaskHandle_t g_taskHandle = nullptr;

TouchHandler touchHandler;
SimpleTransition *simpleTransition = nullptr;

SystemMenu systemMenu;
ScenarioLoader scenarioLoader;
ScenarioPlayer scenarioPlayer;

/**
 * @brief 今どの画面を出しているか
 *
 * 画面ごとにボタンの集合が別になっている。
 * 切り替えるときは前の画面の `leave()` で片付けてから次を `enter()` する。
 */
enum class AppScreen
{
    Menu,      //!< シナリオ選択
    Playing,   //!< シナリオ再生中
};

AppScreen currentScreen = AppScreen::Menu;

// ========================================
// 選択肢の UI
// ========================================
//
// ScenarioPlayer は文言と可否を公開するだけで、描画も入力も持たない。
// ここで作って捨てる。

ButtonManager *choiceButtonManager = nullptr;
std::vector<Button *> choiceButtons;

void clearChoiceButtons()
{
    if (choiceButtonManager)
    {
        choiceButtonManager->clearButtons();
    }
    for (Button *btn : choiceButtons)
    {
        delete btn;
    }
    choiceButtons.clear();
}

void onChoiceButtonReleased(Button *btn)
{
    for (size_t i = 0; i < choiceButtons.size(); ++i)
    {
        if (choiceButtons[i] == btn)
        {
            ESP_LOGI(TAG, "Choice %u tapped", static_cast<unsigned>(i));
            clearChoiceButtons();
            display.fillScreen(TFT_BLACK);
            scenarioPlayer.selectChoice(i);
            return;
        }
    }
}

void showChoiceButtons()
{
    clearChoiceButtons();

    const std::vector<std::string> &labels = scenarioPlayer.choiceLabels();
    const std::vector<bool> &enabled = scenarioPlayer.choiceEnabled();

    const int width = 320;
    const int height = 56;
    const int gap = 8;
    const int startY = display.height() - 40 - static_cast<int>(labels.size()) * (height + gap);

    for (size_t i = 0; i < labels.size(); ++i)
    {
        const int y = startY + static_cast<int>(i) * (height + gap);
        Button *btn = new Button(&display, 20, y, width, height, labels[i].c_str());
        btn->setOnReleased(onChoiceButtonReleased);

        // 選択肢は日本語なので VLW を指定する。既定フォントでは豆腐になる。
        btn->setVlwFont(textSystem.fontData());

        ButtonStyle style = ButtonStyle::defaultStyle();
        style.bgColor = enabled[i] ? TFT_DARKGREY : TFT_BLACK;
        style.textColor = enabled[i] ? TFT_WHITE : TFT_DARKGREY;
        btn->setStyle(style);

        choiceButtonManager->addButton(btn);
        choiceButtons.push_back(btn);
    }

    choiceButtonManager->drawButtons();
    SimpleTransition::refreshScreen(&display);

    ESP_LOGI(TAG, "Showing %u choice button(s)", static_cast<unsigned>(labels.size()));
}

// ========================================
// 画面の切り替え
// ========================================

void enterMenu()
{
    ESP_LOGI(TAG, "Screen -> Menu");
    currentScreen = AppScreen::Menu;
    systemMenu.enter();
}

void enterPlaying(const std::string &scenarioId)
{
    ESP_LOGI(TAG, "Loading scenario '%s'", scenarioId.c_str());

    systemMenu.leave();
    display.fillScreen(TFT_BLACK);

    if (!scenarioLoader.load(scenarioId.c_str()))
    {
        // 読めなかった理由は SCENARIO タグのログに出ている。
        // SDを抜いたのかJSONが壊れているのか、画面でも気づけるようにする。
        ESP_LOGE(TAG, "Failed to load scenario");

        // setTextSize() は「今読み込まれているフォント」への倍率なので、
        // VLW が載ったままだと想定外の大きさで描かれる。既定へ戻してから描く。
        display.unloadFont();
        display.setTextColor(TFT_RED, TFT_BLACK);
        display.setTextSize(2);
        display.setCursor(20, 100);
        display.print("Scenario load failed:");
        display.setCursor(20, 130);
        display.printf("scenarios/%s/scenario.json", scenarioId.c_str());
        display.setTextColor(TFT_WHITE, TFT_BLACK);
        SimpleTransition::refreshScreen(&display);

        vTaskDelay(pdMS_TO_TICKS(2500));
        enterMenu();
        return;
    }

    currentScreen = AppScreen::Playing;

    // シナリオ本文はルビ記法を使う前提
    textSystem.setRubyEnabled(true);

    scenarioPlayer.begin(&display, &scenarioLoader,
                         textSystem.vertical(), textSystem.horizontal(),
                         simpleTransition);
    scenarioPlayer.start();
}

void leavePlaying()
{
    ESP_LOGI(TAG, "Scenario finished (ending='%s')",
             scenarioPlayer.endingId().c_str());

    clearChoiceButtons();
    textSystem.setRubyEnabled(false);
    scenarioLoader.unload();   // PSRAM を返す
    enterMenu();
}

// ========================================
// 起動
// ========================================

void setup()
{
    ESP_LOGI(TAG, "Initializing M5PaperS3...");

    display.begin();

    // 画面の向き。
    //
    // パネルの物理的な向きは 960x540（横長）で、M5GFX のパネル定義が
    // offset_rotation = 3 を持つため、setRotation() の値はこれに加算される。
    //   r=0 -> 540x960（縦長・既定）
    //   r=2 -> 540x960（縦長・180度反転）  ← これを使う
    // 描画より前に設定すること。
    display.setRotation(2);

    display.setEpdMode(lgfx::v1::epd_mode::epd_mode_t::epd_quality);
    display.setColorDepth(1);

    // 電子ペーパーはリセットしても直前の像を保持しているが、
    // Panel_EPD は初期化時に「全白」と仮定して内部バッファを埋める。
    // この不一致があると前の像が残るので、白→黒→白で状態を揃える。
    SimpleTransition::clearGhosting(&display);

    if (!buzzer.begin())
    {
        ESP_LOGE(TAG, "Buzzer initialization failed (sound disabled)");
    }

    // 電源制御と電池監視。失敗しても電源OFF自体は使えるので起動は続ける
    if (!power.begin())
    {
        ESP_LOGE(TAG, "Power initialization failed (battery level unavailable)");
    }

    // cJSON の確保先を PSRAM に向ける。
    // 解析を始める前に、プロセス全体で1回だけ呼ぶ必要がある。
    ScenarioLoader::initAllocator();

    display.fillScreen(TFT_BLACK);

    if (!textSystem.begin(&display))
    {
        ESP_LOGE(TAG, "Text system initialization failed");
    }

    if (!SD.init())
    {
        ESP_LOGE(TAG, "SD card initialization failed");
    }

    // 本体設定。SD の初期化後、かつ設定を使う前に読む。
    // 読めなくても既定値で動くので、失敗しても起動は止めない。
    settings.load();
    buzzer.setMuted(!settings.soundEnabled());

    simpleTransition = new SimpleTransition(&display);
    if (!simpleTransition->init())
    {
        ESP_LOGE(TAG, "Simple transition initialization failed");
        delete simpleTransition;
        simpleTransition = nullptr;
    }

    if (!touchHandler.init(&display))
    {
        ESP_LOGE(TAG, "Touch handler initialization failed");
    }

    choiceButtonManager = new ButtonManager(&display, &touchHandler);

    systemMenu.begin(&display, &touchHandler);
    enterMenu();

    ESP_LOGI(TAG, "Ready");
}

// ========================================
// メインループ
// ========================================

void loop()
{
    // トランジション実行中は描画を進めることに専念する
    if (simpleTransition && simpleTransition->isActive())
    {
        simpleTransition->update();
        return;
    }

    // タッチの取得はここで1回だけ。
    // TouchHandler::update() はイベントを1回しか返さない破壊的メソッドなので、
    // 複数箇所で呼ぶと取りこぼす。
    const bool hasTouchEvent = touchHandler.update();

    switch (currentScreen)
    {
    case AppScreen::Menu:
        systemMenu.update(hasTouchEvent);
        if (systemMenu.hasSelection())
        {
            const std::string id = systemMenu.selectedScenarioId();
            systemMenu.clearSelection();
            enterPlaying(id);
        }
        break;

    case AppScreen::Playing:
        if (scenarioPlayer.isWaitingTransition())
        {
            // ここへ来た時点で遷移は終わっている（上の分岐を抜けたため）
            scenarioPlayer.onTransitionFinished();
        }
        else if (scenarioPlayer.isWaitingChoice())
        {
            // まだ並べていなければ選択肢を出す。
            // 入力は choiceButtonManager が拾い、
            // onChoiceButtonReleased() から selectChoice() が呼ばれる。
            if (choiceButtons.empty())
            {
                showChoiceButtons();
            }
            choiceButtonManager->update();
        }
        else if (hasTouchEvent && touchHandler.isTouchEvent())
        {
            scenarioPlayer.onTap();
        }

        if (scenarioPlayer.isFinished())
        {
            leavePlaying();
        }
        break;
    }
}

// ========================================
// タスク
// ========================================

void runMainLoop(void *args)
{
    (void)args;
    setup();

    // このループは抜けない（タスクは常駐）
    for (;;)
    {
        loop();
        vTaskDelay(1);
    }
}

extern "C" void app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "ayame_sys starting");

    xTaskCreatePinnedToCore(&runMainLoop, "task1-main", 8192, nullptr, 1,
                            &g_taskHandle, 1);
    configASSERT(g_taskHandle);
}
