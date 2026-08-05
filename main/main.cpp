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

/**
 * @brief メニューと、向きを指定しないシナリオで使う画面の向き
 *
 * パネルの物理的な向きは 960x540（横長）で、M5GFX のパネル定義が
 * `offset_rotation = 3` を持つため、`setRotation()` の値はこれに加算される。
 *
 * | 値 | 画面 |
 * |---|---|
 * | 0 | 540x960（縦長） |
 * | 1 | 960x540（横長） |
 * | 2 | 540x960（縦長・0 の180度反転） |
 * | 3 | 960x540（横長・1 の180度反転） |
 *
 * シナリオは `meta.rotation` で自分の向きを指定できる。
 * 指定が無ければこの値のまま再生する。
 */
constexpr int MENU_ROTATION = 0;

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

/**
 * @brief 画面の向きを変え、向きに依存するものを追従させる
 *
 * `setRotation()` だけでは足りない。回転すると 540x960 と 960x540 が
 * 入れ替わるので、次の2つも作り直す必要がある。
 *
 *   ・トランジションのキャンバス（縦横が食い違うと中間フレームが崩れる）
 *   ・既定のテキストボックス（縦長前提の座標は横長では画面外へ出る）
 *
 * シナリオが `textboxes` で定義したボックスはここでは触らない。
 * それらの座標は作者が向きを決めたうえで書いているため。
 *
 * @param rotation 0〜3
 */
void applyRotation(int rotation)
{
    if (display.getRotation() == rotation)
    {
        return;
    }

    ESP_LOGI(TAG, "Rotation %d -> %d", display.getRotation(), rotation);
    display.setRotation(rotation);

    if (simpleTransition && !simpleTransition->resizeToDisplay())
    {
        // キャンバスを作り直せなかった。演出は使えないが本文は読める。
        ESP_LOGE(TAG, "Transitions are unavailable at this rotation");
    }
    textSystem.layoutDefaultBoxes();

    // 回転すると全画素が別の内容になる。
    // 部分更新のままだと前の向きの残像が残るので、一度振り切っておく。
    SimpleTransition::clearGhosting(&display);
}

void enterMenu()
{
    ESP_LOGI(TAG, "Screen -> Menu");

    // メニューの配置は縦長 540x960 の決め打ち。
    // 横向きのシナリオから戻ってきたときのために必ず戻す。
    applyRotation(MENU_ROTATION);

    currentScreen = AppScreen::Menu;
    systemMenu.enter();
}

void enterPlaying(const std::string &scenarioId, bool resume = false)
{
    ESP_LOGI(TAG, "Loading scenario '%s'%s",
             scenarioId.c_str(), resume ? " (resume)" : "");

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

    // 画面の向き。
    //
    // **テキストボックスを組む前に済ませること。**
    // ボックスの座標は向きが決まってからでないと意味を持たない。
    const int rotation = scenarioLoader.rotation();
    applyRotation(rotation >= 0 ? rotation : MENU_ROTATION);

    // シナリオ独自のフォント。
    //
    // **テキストボックスを組む前に済ませること。**
    // ボックスは作られた時点のフォントでメトリクスを持つので、
    // 後から差し替えると寸法の計算が合わなくなる。
    //
    // 読めなくても内蔵フォントで再生できるので、失敗しても続行する。
    const std::string fontPath = scenarioLoader.fontPath();
    if (!fontPath.empty())
    {
        if (!textSystem.loadScenarioFont(fontPath))
        {
            ESP_LOGW(TAG, "Falling back to the built-in font");
        }
    }

    // シナリオ本文はルビ記法を使う前提
    textSystem.setRubyEnabled(true);

    scenarioPlayer.begin(&display, &scenarioLoader,
                         textSystem.vertical(), textSystem.horizontal(),
                         simpleTransition);

    if (resume)
    {
        // 栞は一度だけ使う。
        // 残したままだと、いつの状態から始まるのか分からなくなる。
        // 再開が失敗しても消すのは、壊れた栞が居座らないようにするため。
        const int slot = settings.resumeSlot();
        settings.clearResume();
        settings.save();

        if (scenarioPlayer.resumeFrom(slot))
        {
            return;
        }
        // 失敗しても resumeFrom() が冒頭から再生を始めている
        ESP_LOGW(TAG, "Resume failed. Playing from the beginning");
        return;
    }

    scenarioPlayer.start();
}

void leavePlaying()
{
    ESP_LOGI(TAG, "Scenario finished (ending='%s')",
             scenarioPlayer.endingId().c_str());

    clearChoiceButtons();
    textSystem.setRubyEnabled(false);

    // シナリオが定義したテキストボックスを捨てる。
    // 残すと次のシナリオに前作のボックスが混ざる。
    textSystem.clearBoxes();

    // シナリオ独自のフォントを解放して内蔵へ戻す。
    // 忘れると PSRAM を 1MB 前後抱えたままになり、
    // 次のシナリオの本文が載らなくなる。
    textSystem.useBuiltinFont();

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

    // 画面の向き。描画より前に設定すること。
    display.setRotation(MENU_ROTATION);

    display.setEpdMode(lgfx::v1::epd_mode::epd_mode_t::epd_quality);

    // 色深度は設定しない。
    //
    // Panel_EPD::setColorDepth() は**引数を無視して常に grayscale_8bit にする**。
    //     _write_depth = color_depth_t::grayscale_8bit;
    //     _read_depth  = color_depth_t::grayscale_8bit;
    // パネルのバッファも (幅 x 高さ) / 2 の 4bpp で、**常に16階調**。
    //
    // 以前ここに setColorDepth(1) と書いていたが、効果が無いうえに
    // 「この機種は白黒2値」という誤解を生むので削除した。
    //
    // 階調が実際に出るかは EPD モードで決まる。
    //   epd_quality / epd_text … 16階調そのまま
    //   epd_fast / epd_fastest … ベイヤーディザで2階調

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
            const bool resume = systemMenu.selectionIsResume();
            systemMenu.clearSelection();
            enterPlaying(id, resume);
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
            // 文字送りの途中なら、onTap() が全文表示へ飛ばす
            scenarioPlayer.onTap();
        }
        else if (scenarioPlayer.isWaiting())
        {
            // wait の経過を見る。skippable なら上のタップ判定で飛ばされる。
            scenarioPlayer.tickWait();
        }
        else if (scenarioPlayer.isTyping())
        {
            // 文字送りを1文字進める。間隔が来ていなければ何もしない。
            // タップの判定より後に置くのは、送っている最中の
            // タップを取りこぼさないため。
            scenarioPlayer.tickTyping();
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
