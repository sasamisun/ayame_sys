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

#include <algorithm>
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
    // **解放は ButtonManager が行う。**
    // choiceButtons は「何番目が押されたか」を引くための参照用。
    if (choiceButtonManager)
    {
        choiceButtonManager->clearButtons();
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

            // 画面は塗らない。ボタンと問いかけを消して元へ戻すのは
            // selectChoice() の仕事（下に何があったかを知っているのは
            // プレイヤー側だけなので）。ここで黒く塗ると背景まで消える。
            scenarioPlayer.selectChoice(i);
            return;
        }
    }
}

/**
 * @brief 選択肢の問いかけ（`choice` の `prompt`）をボタンの上に出す
 *
 * ボタンと同じ幅の帯にして、**下端が `bottom` に来るように**置く。
 * 何行になるかは文言と画面の向きで変わるので、
 * 先に高さを測ってから上端を決める。
 *
 * 帯は不透明で塗る。電子ペーパーは前の描画が消えずに残るので、
 * 透過のまま重ねると下の本文と混ざって読めなくなる。
 *
 * @param x      帯の左端（ボタンと揃える）
 * @param bottom 帯の下端。ここから上へ伸ばす
 * @param width  帯の幅（ボタンと揃える）
 */
void showChoicePrompt(int x, int bottom, int width)
{
    const std::string &prompt = scenarioPlayer.choicePrompt();
    if (prompt.empty())
    {
        return;
    }

    TypoWrite *writer = textSystem.choicePrompt();
    if (!writer)
    {
        return;
    }

    constexpr int PAD = 8;

    // **測る前に幅を入れる。** 折り返しの位置は幅で決まるので、
    // 幅を入れないまま測ると行数が変わって高さが合わない。
    // 高さは仮に画面いっぱいにしておく（折り返しには効かない）。
    writer->setPosition(x, 0);
    writer->setArea(width, static_cast<int>(display.height()));
    writer->setPadding(PAD, PAD, PAD, PAD);

    int h = writer->getTextHeight(prompt) + PAD * 2;
    int y = bottom - h;
    if (y < 0)
    {
        // 長すぎて画面に収まらない。上で切る（描画側が行単位で打ち切る）
        y = 0;
        h = bottom;
    }
    if (h <= PAD * 2)
    {
        return;
    }

    writer->setPosition(x, y);
    writer->setArea(width, h);
    display.fillRect(x, y, width, h, TFT_BLACK);
    writer->drawText(prompt);
}

void showChoiceButtons()
{
    clearChoiceButtons();

    const std::vector<std::string> &labels = scenarioPlayer.choiceLabels();
    const std::vector<bool> &enabled = scenarioPlayer.choiceEnabled();

    const int height = 56;
    const int gap = 8;

    // 横向き（960x540）だと左寄せでは立ち絵に重なる。
    // 画面幅の中央へ寄せる。縦長でも収まりが良いので向きでは分けない。
    const int screenW = static_cast<int>(display.width());
    const int width = std::min(320, screenW - 40);
    const int x = (screenW - width) / 2;
    const int startY = static_cast<int>(display.height()) - 40
                       - static_cast<int>(labels.size()) * (height + gap);

    // **問いかけとボタンをまとめて1回で出す。**
    // 電子ペーパーは1回の書き換えに 100ms 以上かかるので、
    // 別々に出すと問いかけ→ボタンが1つずつ現れる様子が見えてしまう。
    display.startWrite();

    // 問いかけを先に描く。ボタンの下敷きにならないよう、
    // ボタンの並びの上端（startY）から間隔ぶん空けた位置を下端にする。
    showChoicePrompt(x, startY - gap, width);

    for (size_t i = 0; i < labels.size(); ++i)
    {
        const int y = startY + static_cast<int>(i) * (height + gap);
        Button *btn = new Button(&display, x, y, width, height, labels[i].c_str());
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
    display.endWrite();

    ESP_LOGI(TAG, "Showing %u choice button(s)", static_cast<unsigned>(labels.size()));
}

// ========================================
// 電池切れの手当て
// ========================================

/// これを下回ったら保存する（%）
constexpr int LOW_BATTERY_PERCENT = 2;

/// 電池を見る間隔（ミリ秒）。ADC を毎周期読む必要はない
constexpr int64_t BATTERY_CHECK_MS = 10000;

int64_t lastBatteryCheckMs = 0;
bool lowBatterySaved = false;

/**
 * @brief 電池が残り少なければ、今の位置を残して知らせる
 *
 * 電子ペーパーは電源が落ちても画面が残るので、
 * 何も出さずに切れると「止まったのか電池切れなのか」が分からない。
 */
void checkBattery()
{
    const int64_t now = esp_timer_get_time() / 1000;
    if (now - lastBatteryCheckMs < BATTERY_CHECK_MS)
    {
        return;
    }
    lastBatteryCheckMs = now;

    const int percent = power.batteryPercent();
    if (percent < 0 || percent > LOW_BATTERY_PERCENT)
    {
        return;
    }

    // **一度だけ。** 何度も書くと、残り少ない電力を書き込みで使い切る。
    if (lowBatterySaved)
    {
        return;
    }
    lowBatterySaved = true;

    ESP_LOGW(TAG, "Battery at %d%%", percent);

    if (currentScreen == AppScreen::Playing)
    {
        scenarioPlayer.emergencySave();
    }

    display.unloadFont();
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextSize(2);
    display.fillRect(0, 0, display.width(), 44, TFT_BLACK);
    display.setCursor(16, 12);
    display.print("Battery low - please charge");
    SimpleTransition::refreshScreen(&display);
}

// ========================================
// バックログ（本文の履歴）
// ========================================
//
// 再生中に上スワイプで開き、タップで戻る。
// **保存はしない。** 電源を切れば消える。
// 「さっき何を読んだか」を見返すためだけのもので、
// 既読スキップのような周回をまたぐ記録とは別物。

bool backlogOpen = false;

void showBacklog()
{
    TypoWrite *writer = textSystem.backlog();
    if (!writer)
    {
        return;
    }

    const std::vector<std::string> &history = scenarioPlayer.history();

    // 新しいものを上に出す。開いた直後に直前の本文が見えるようにするため。
    std::string text;
    for (auto it = history.rbegin(); it != history.rend(); ++it)
    {
        if (!text.empty())
        {
            text += "\n\n";
        }
        text += *it;
    }
    if (text.empty())
    {
        text = "まだ何も読んでいません。";
    }

    display.fillScreen(TFT_BLACK);
    writer->drawTextPaged(text, 0);
    SimpleTransition::refreshScreen(&display);

    backlogOpen = true;
    ESP_LOGI(TAG, "Backlog opened (%u entries)",
             static_cast<unsigned>(history.size()));
}

void hideBacklog()
{
    backlogOpen = false;

    // 舞台だけ描き直すと本文が消えたままになるので、
    // 直近の本文まで戻す。
    scenarioPlayer.redrawCurrentScreen();
    ESP_LOGI(TAG, "Backlog closed");
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

    // 起動した時点で残りが少なければ、一覧を出す前に知らせる。
    // 選んで読み始めた直後に落ちるより、先に充電してもらうほうがよい。
    const int battery = power.batteryPercent();
    if (battery >= 0 && battery <= LOW_BATTERY_PERCENT)
    {
        ESP_LOGW(TAG, "Battery at %d%% on boot", battery);

        display.unloadFont();
        display.fillScreen(TFT_BLACK);
        display.setTextColor(TFT_WHITE, TFT_BLACK);
        display.setTextSize(3);
        display.setCursor(40, 400);
        display.print("Battery low");
        display.setTextSize(2);
        display.setCursor(40, 460);
        display.print("Please charge before reading");
        SimpleTransition::refreshScreen(&display);

        // ここで止める。電源ボタンで切るか、充電して入れ直してもらう。
        for (;;)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

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

    // 電池の残りを見る。10秒に1回なので毎周期の負担はほぼ無い。
    checkBattery();

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
        // バックログを開いている間は、閉じる操作だけ拾う。
        // 本文を進めてしまうと、読み返している最中に話が動く。
        if (backlogOpen)
        {
            if (hasTouchEvent && touchHandler.isReleaseEvent())
            {
                hideBacklog();
            }
            break;
        }

        // 上スワイプで履歴を開く。
        // **タップ判定より前に見ること。** スワイプは「方向を伴うタッチ終了」
        // なので、後ろに置くと本文が1つ進んでから開くことになる。
        if (hasTouchEvent && touchHandler.isSwipeEvent()
            && touchHandler.getLastSwipe() == SwipeDirection::Up
            && !scenarioPlayer.isWaitingChoice())
        {
            showBacklog();
            break;
        }

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
