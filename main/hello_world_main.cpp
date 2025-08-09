// main/hello_world_main.cpp - エラー修正版

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <M5GFX.h>
#include "SDcard.hpp"
#include "TouchHandler.hpp"
#include "Button.hpp"
#include "TypoWrite.hpp"
#include "VLWFontParser.hpp"
#include "CanvasTest.hpp"
#include "ScreenTransition.hpp"

#include "fonts/shippori_16.h"

// ログタグの定義
static const char *TAG = "APP_MAIN";

M5GFX display;
TaskHandle_t g_handle = nullptr;

// 画像ファイルの定数定義
const char *IMAGE_FILE = "tes.png";

TouchHandler touchHandler;
ButtonManager *buttonManager = nullptr;
Button *btnTest = nullptr;
Button *btnUSBMSC = nullptr;
Button *btnCanvasTest = nullptr;
Button *btnTransitionTest = nullptr;
Button *btnCanvasStop = nullptr;

VLWFontParser vlwParser;
CanvasTest *canvasTest = nullptr;
ScreenTransition *screenTransition = nullptr;

// テスト実行状態管理
enum class TestMode
{
  NORMAL,             // 通常モード
  CANVAS_MEMORY,      // キャンバスメモリテスト
  CANVAS_DOUBLE,      // ダブルバッファテスト
  CANVAS_PERFORMANCE, // パフォーマンステスト
  TRANSITION_DEMO     // トランジションデモ
};

TestMode currentTestMode = TestMode::NORMAL;

// トランジションデモ用の状態管理
int currentTransitionIndex = 0;
bool transitionDemoRunning = false;

// 軽量版トランジションタイプの配列（デモ用）
TransitionType transitionTypes[] = {
    TransitionType::FADE_BLACK,      // 基本フェード
    TransitionType::FADE_WHITE,      // 回想フェード
    TransitionType::SLIDE_LEFT,      // 左スライド
    TransitionType::SLIDE_RIGHT,     // 右スライド
    TransitionType::SLIDE_UP,        // 上スライド
    TransitionType::SLIDE_DOWN,      // 下スライド
    TransitionType::WIPE_LEFT,       // 左ワイプ
    TransitionType::WIPE_RIGHT,      // 右ワイプ
    TransitionType::VENETIAN_BLIND,  // ブラインド効果
    TransitionType::CUT_IN,          // カットイン効果
    TransitionType::PUSH_LEFT,       // 左プッシュ
    TransitionType::PUSH_RIGHT       // 右プッシュ
};

const int transitionTypeCount = sizeof(transitionTypes) / sizeof(transitionTypes[0]);

// 軽量版トランジションタイプの名前取得
const char *getTransitionTypeName(TransitionType type)
{
    switch (type)
    {
    case TransitionType::FADE_BLACK:
        return "Fade Black";
    case TransitionType::FADE_WHITE:
        return "Fade White";
    case TransitionType::SLIDE_LEFT:
        return "Slide Left";
    case TransitionType::SLIDE_RIGHT:
        return "Slide Right";
    case TransitionType::SLIDE_UP:
        return "Slide Up";
    case TransitionType::SLIDE_DOWN:
        return "Slide Down";
    case TransitionType::WIPE_LEFT:
        return "Wipe Left";
    case TransitionType::WIPE_RIGHT:
        return "Wipe Right";
    case TransitionType::VENETIAN_BLIND:
        return "Venetian Blind";
    case TransitionType::CUT_IN:
        return "Cut In";
    case TransitionType::PUSH_LEFT:
        return "Push Left";
    case TransitionType::PUSH_RIGHT:
        return "Push Right";
    default:
        return "Unknown";
    }
}

// 軽量版デモ画面描画関数（高速化版）
void drawDemoScreen1(M5Canvas *canvas)
{
    if (!canvas) return;

    canvas->fillSprite(TFT_BLUE);
    canvas->setTextColor(TFT_WHITE);
    canvas->setTextSize(3);
    canvas->setTextDatum(middle_center);
    canvas->drawString("ADVENTURE GAME", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 - 150);
    canvas->drawString("SCREEN 1", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 - 100);

    // 軽量な装飾（矩形と円のみ）
    canvas->fillRect(100, 200, 80, 80, TFT_YELLOW);
    canvas->fillCircle(300, 240, 40, TFT_RED);
    canvas->fillRect(180, 350, 180, 60, TFT_GREEN);

    // テキスト情報
    canvas->setTextSize(2);
    canvas->drawString("軽量版トランジション", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 + 50);
    canvas->setTextSize(1.5);
    canvas->drawString("高速・軽量・E-Paper最適化", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 + 100);
    canvas->drawString("アドベンチャーゲーム用", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 + 130);
}

void drawDemoScreen2(M5Canvas *canvas)
{
    if (!canvas) return;

    canvas->fillSprite(TFT_DARKGREEN);
    canvas->setTextColor(TFT_WHITE);
    canvas->setTextSize(3);
    canvas->setTextDatum(middle_center);
    canvas->drawString("NEXT SCENE", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 - 150);
    canvas->drawString("SCREEN 2", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 - 100);

    // 軽量な装飾
    canvas->fillEllipse(TRANSITION_WIDTH / 2, 300, 100, 50, TFT_CYAN);
    canvas->fillRect(120, 450, 300, 80, TFT_MAGENTA);

    // 軽量なパターン（垂直線）
    for (int i = 0; i < 10; i++)
    {
        int x = i * (TRANSITION_WIDTH / 10);
        canvas->drawFastVLine(x, 0, TRANSITION_HEIGHT, TFT_DARKGRAY);
    }

    canvas->setTextSize(2);
    canvas->drawString("場面転換完了", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 + 50);
    canvas->setTextSize(1.5);
    canvas->drawString("M5Canvas + PSRAM", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 + 100);
}

void drawDemoScreen3(M5Canvas *canvas)
{
    if (!canvas) return;

    canvas->fillSprite(TFT_PURPLE);
    canvas->setTextColor(TFT_WHITE);
    canvas->setTextSize(3);
    canvas->setTextDatum(middle_center);
    canvas->drawString("FINAL SCENE", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 - 150);
    canvas->drawString("SCREEN 3", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 - 100);

    // 軽量なチェッカーパターン（修正：TFT_MAGENTAを使用）
    const int checker_size = 40;
    for (int y = 0; y < TRANSITION_HEIGHT; y += checker_size)
    {
        for (int x = 0; x < TRANSITION_WIDTH; x += checker_size)
        {
            if ((x / checker_size + y / checker_size) % 2 == 0)
            {
                // 修正：TFT_DARKMAGENTA → TFT_MAGENTA
                canvas->fillRect(x, y, checker_size, checker_size, TFT_MAGENTA);
            }
        }
    }

    canvas->setTextSize(2);
    canvas->drawString("トランジション完了", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 + 50);
    canvas->setTextSize(1.5);
    canvas->drawString("軽量・高速・美しい", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 + 100);
}

// 縦書きと横書きテキスト表示のデモ
void textDisplayDemo()
{
    ESP_LOGI(TAG, "Starting VLW font demo...");

    if (vlwParser.init(shippori, sizeof(shippori)))
    {
        ESP_LOGI(TAG, "VLW font initialized successfully");
        vlwParser.debugPrintFontInfo();

        TypoWrite verticalWriter(&display);
        verticalWriter.setVLWParser(&vlwParser);
        verticalWriter.loadFontFromArray(shippori);
        verticalWriter.setPosition(400, 0);
        verticalWriter.setArea(130, 700);
        verticalWriter.setColor(TFT_WHITE);
        verticalWriter.setBackgroundColor(TFT_TRANSPARENT);
        verticalWriter.setTransparentBg(true);
        verticalWriter.setDirection(TextDirection::VERTICAL);
        verticalWriter.setFontSize(1.0);
        verticalWriter.setLineSpacing(6);

        verticalWriter.drawText("画面遷移システムを\n実装しました。\n様々なエフェクトで\n美しい切り替えが\n可能です。");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize VLW font");
        display.setTextColor(TFT_RED);
        display.setTextSize(1);
        display.setCursor(10, 100);
        display.println("VLW Font Load Failed");
    }
}

// ファイルフォルダ一覧表示
void listAndDisplayFiles()
{
    DirInfo *rootDir = SD.listDir("/");
    if (rootDir)
    {
        display.setTextColor(TFT_WHITE);
        display.setTextSize(1);
        display.setCursor(10, 10);
        display.println("SD Card Files:");

        int y = 30;
        for (size_t i = 0; i < rootDir->count; i++)
        {
            FileInfo *file = &rootDir->files[i];

            if (file->isDirectory)
            {
                display.printf("[DIR] %s\n", file->name);
            }
            else
            {
                float size_kb = file->size / 1024.0f;
                display.printf("%s (%.1f KB)\n", file->name, size_kb);
            }

            y += 20;
            if (y > display.height() - 20)
            {
                display.println("... and more files");
                break;
            }
        }

        SD.freeDirInfo(rootDir);
    }
    else
    {
        display.fillScreen(TFT_BLACK);
        display.setTextColor(TFT_RED);
        display.setTextSize(1);
        display.setCursor(10, 10);
        display.println("Failed to read SD card directory");
    }
}

// トランジションデモの次の画面を準備
void prepareNextDemoScreen(M5Canvas *canvas)
{
    static int screenIndex = 0;
    screenIndex = (screenIndex + 1) % 3;

    switch (screenIndex)
    {
    case 0:
        drawDemoScreen1(canvas);
        break;
    case 1:
        drawDemoScreen2(canvas);
        break;
    case 2:
        drawDemoScreen3(canvas);
        break;
    }
}

// タッチイベントのコールバック関数
void onTouchStart(const ExtendedTouchPoint &point)
{
    ESP_LOGI(TAG, "Touch started at (%d, %d)", point.x, point.y);

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    display.setTextColor(TFT_GREEN, TFT_BLACK);
    display.setTextSize(1);
    display.setCursor(10, display.height() - 100);
    display.printf("Touch started at (%d, %d)   ", point.x, point.y);
}

void onTouchEnd(const ExtendedTouchPoint &point)
{
    ESP_LOGI(TAG, "Touch ended at (%d, %d)", point.x, point.y);

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    display.setTextColor(TFT_RED, TFT_BLACK);
    display.setTextSize(1);
    display.setCursor(10, display.height() - 120);
    display.printf("Touch ended at (%d, %d)   ", point.x, point.y);
}

void onSwipe(SwipeDirection direction, const ExtendedTouchPoint &start, const ExtendedTouchPoint &end)
{
    const char *dirStr = "Unknown";
    switch (direction)
    {
    case SwipeDirection::Up:
        dirStr = "Up";
        break;
    case SwipeDirection::Down:
        dirStr = "Down";
        break;
    case SwipeDirection::Left:
        dirStr = "Left";
        break;
    case SwipeDirection::Right:
        dirStr = "Right";
        break;
    default:
        break;
    }

    ESP_LOGI(TAG, "Swipe detected: %s", dirStr);

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.setTextSize(1);
    display.setCursor(10, display.height() - 140);
    display.printf("Swipe: %s   ", dirStr);
}

// ボタンコールバック関数
void onTestButtonPressed(Button *btn)
{
    ESP_LOGI(TAG, "Test button pressed");
}

void onTestButtonReleased(Button *btn)
{
    ESP_LOGI(TAG, "Test button released");

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.setTextSize(1);
    display.setCursor(10, display.height() - 80);
    display.println("テストボタンが押されました");
}

// USB MSCボタンコールバック
void onUSBMSCButtonPressed(Button *btn)
{
    ESP_LOGI(TAG, "USB MSC button pressed");
}

void onUSBMSCButtonReleased(Button *btn)
{
    ESP_LOGI(TAG, "USB MSC button released");

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    if (SD.isUSBMSCEnabled())
    {
        if (SD.disableUSBMSC())
        {
            ESP_LOGI(TAG, "USB MSC disabled");
            btn->setLabel("Enable USB MSC");
            listAndDisplayFiles();
        }
    }
    else
    {
        if (SD.enableUSBMSC())
        {
            ESP_LOGI(TAG, "USB MSC enabled");
            btn->setLabel("Disable USB MSC");

            display.setTextColor(TFT_WHITE, TFT_BLACK);
            display.setTextSize(1.5);
            display.setCursor(10, 100);
            display.println("USB MSC Enabled");
            display.println("Connect to PC to access SD card");
        }
    }
}

// キャンバステストボタンコールバック
void onCanvasTestButtonPressed(Button *btn)
{
    ESP_LOGI(TAG, "Canvas test button pressed");
}

void onCanvasTestButtonReleased(Button *btn)
{
    ESP_LOGI(TAG, "Canvas test button released");

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    if (!canvasTest)
    {
        ESP_LOGE(TAG, "Canvas test not initialized");
        return;
    }

    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_CYAN);
    display.setTextSize(2);
    display.setCursor(10, 50);
    display.println("Starting Canvas Tests...");
    display.println("Please wait...");

    btn->setLabel("Testing...");
    btn->setEnabled(false);
    btnCanvasStop->setVisible(true);
    buttonManager->drawButtons();

    ESP_LOGI(TAG, "Running Canvas Memory Test...");
    currentTestMode = TestMode::CANVAS_MEMORY;
    canvasTest->testMemoryUsage();
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Running Canvas Performance Test...");
    currentTestMode = TestMode::CANVAS_PERFORMANCE;
    canvasTest->testDrawingPerformance();

    ESP_LOGI(TAG, "Running Canvas Double Buffer Test...");
    currentTestMode = TestMode::CANVAS_DOUBLE;
    canvasTest->runDoubleBufferTest();

    currentTestMode = TestMode::NORMAL;
    btn->setLabel("Canvas Test");
    btn->setEnabled(true);
    btnCanvasStop->setVisible(false);

    display.fillScreen(TFT_BLACK);
    listAndDisplayFiles();
    textDisplayDemo();
    buttonManager->drawButtons();

    ESP_LOGI(TAG, "Canvas tests completed");
}

// トランジションテストボタンコールバック（修正版）
void onTransitionTestButtonPressed(Button *btn)
{
    ESP_LOGI(TAG, "Transition test button pressed");
}

void onTransitionTestButtonReleased(Button *btn)
{
    ESP_LOGI(TAG, "軽量版Transition test button released");

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    if (!screenTransition)
    {
        ESP_LOGE(TAG, "Screen transition not initialized");
        return;
    }

    // 軽量版トランジションデモ開始
    currentTestMode = TestMode::TRANSITION_DEMO;
    transitionDemoRunning = true;
    currentTransitionIndex = 0;

    btn->setLabel("軽量版実行中...");
    btn->setEnabled(false);
    btnCanvasStop->setVisible(true);
    btnCanvasStop->setLabel("Stop Light Transition");
    buttonManager->drawButtons();

    ESP_LOGI(TAG, "Starting 軽量版transition demo...");

    // 最初の画面をキャプチャ
    screenTransition->captureSource();

    ESP_LOGI(TAG, "軽量版Transition demo started");
}

// キャンバステスト停止ボタンコールバック
void onCanvasStopButtonPressed(Button *btn)
{
    ESP_LOGI(TAG, "Canvas/Transition stop button pressed");
}

void onCanvasStopButtonReleased(Button *btn)
{
    ESP_LOGI(TAG, "Canvas/Transition stop button released");

    if (currentTestMode == TestMode::TRANSITION_DEMO)
    {
        transitionDemoRunning = false;
        screenTransition->stopTransition();

        currentTestMode = TestMode::NORMAL;
        btnTransitionTest->setLabel("Transition Test");
        btnTransitionTest->setEnabled(true);
        btn->setVisible(false);
        btn->setLabel("Stop Test");

        display.fillScreen(TFT_BLACK);
        listAndDisplayFiles();
        textDisplayDemo();
        buttonManager->drawButtons();

        ESP_LOGI(TAG, "Transition demo stopped by user");
    }
    else if (canvasTest && canvasTest->isTestRunning())
    {
        canvasTest->stopTest();

        currentTestMode = TestMode::NORMAL;
        btnCanvasTest->setLabel("Canvas Test");
        btnCanvasTest->setEnabled(true);
        btn->setVisible(false);

        display.fillScreen(TFT_BLACK);
        listAndDisplayFiles();
        textDisplayDemo();
        buttonManager->drawButtons();

        ESP_LOGI(TAG, "Canvas test stopped by user");
    }
}

// スワイプイベントのコールバック関数を定義
void onButtonSwipeUp(Button *btn, SwipeDirection dir)
{
    ESP_LOGI(TAG, "Button swiped up: %s", btn->getLabel());

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    display.setTextColor(TFT_CYAN, TFT_BLACK);
    display.setTextSize(1);
    display.setCursor(10, display.height() - 160);
    display.printf("Button swiped up: %s   ", btn->getLabel());
}

void onButtonSwipeDown(Button *btn, SwipeDirection dir)
{
    ESP_LOGI(TAG, "Button swiped down: %s", btn->getLabel());

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    display.setTextColor(TFT_MAGENTA, TFT_BLACK);
    display.setTextSize(1);
    display.setCursor(10, display.height() - 160);
    display.printf("Button swiped down: %s   ", btn->getLabel());
}

void onButtonSwipeLeft(Button *btn, SwipeDirection dir)
{
    ESP_LOGI(TAG, "Button swiped left: %s", btn->getLabel());

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.setTextSize(1);
    display.setCursor(10, display.height() - 160);
    display.printf("Button swiped left: %s   ", btn->getLabel());
}

void onButtonSwipeRight(Button *btn, SwipeDirection dir)
{
    ESP_LOGI(TAG, "Button swiped right: %s", btn->getLabel());

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    display.setTextColor(TFT_PINK, TFT_BLACK);
    display.setTextSize(1);
    display.setCursor(10, display.height() - 160);
    display.printf("Button swiped right: %s   ", btn->getLabel());
}

// メインループのトランジションデモ処理（修正版）
void updateTransitionDemo()
{
    if (currentTestMode != TestMode::TRANSITION_DEMO || !transitionDemoRunning || !screenTransition)
    {
        return;
    }

    static int64_t lastTransitionTime = 0;
    int64_t currentTime = esp_timer_get_time() / 1000;

    if (!screenTransition->isRunning())
    {
        // 1.5秒間隔で次のトランジションを開始
        if (currentTime - lastTransitionTime > 1500)
        {
            if (currentTransitionIndex < transitionTypeCount)
            {
                TransitionType currentType = transitionTypes[currentTransitionIndex];

                ESP_LOGI(TAG, "Starting 軽量版transition %d: %s",
                         currentTransitionIndex, getTransitionTypeName(currentType));

                // 軽量版トランジション設定
                TransitionConfig config;
                
                // 各効果に応じた最適設定
                switch (currentType) {
                    case TransitionType::FADE_BLACK:
                        config = ScreenTransition::sceneChange();
                        break;
                    case TransitionType::FADE_WHITE:
                        config = ScreenTransition::flashback();
                        break;
                    case TransitionType::SLIDE_LEFT:
                        config = ScreenTransition::moveLeft();
                        break;
                    case TransitionType::SLIDE_RIGHT:
                        config = ScreenTransition::moveRight();
                        break;
                    case TransitionType::WIPE_LEFT:
                        config = ScreenTransition::pageNext();
                        break;
                    case TransitionType::WIPE_RIGHT:
                        config = ScreenTransition::pagePrev();
                        break;
                    case TransitionType::VENETIAN_BLIND:
                        config = ScreenTransition::suspense();
                        break;
                    default:
                        config = TransitionConfig::defaultConfig();
                        config.type = currentType;
                        config.speed = TransitionSpeed::FAST;
                        config.step_delay_ms = 100;
                        break;
                }

                // 次の画面を準備してトランジション開始
                screenTransition->transition(prepareNextDemoScreen, config);

                currentTransitionIndex++;
                lastTransitionTime = currentTime;
            }
            else
            {
                // 全ての軽量版トランジション完了
                transitionDemoRunning = false;
                currentTestMode = TestMode::NORMAL;

                btnTransitionTest->setLabel("軽量版Transition");
                btnTransitionTest->setEnabled(true);
                btnCanvasStop->setVisible(false);

                // 元の画面に戻る
                display.fillScreen(TFT_BLACK);
                listAndDisplayFiles();
                textDisplayDemo();
                buttonManager->drawButtons();

                ESP_LOGI(TAG, "All 軽量版transitions completed");
            }
        }
    }
    else
    {
        // トランジションを更新
        screenTransition->updateTransition();
    }
}

void setup()
{
    ESP_LOGI(TAG, "Initializing M5Paper S3...");
    display.begin();
    display.setEpdMode(lgfx::v1::epd_mode::epd_mode_t::epd_fastest);
    display.setColorDepth(1);
    display.fillScreen(TFT_BLACK);

    // 1. SDカードの初期化（SPI接続）
    ESP_LOGI(TAG, "Initializing SD card via SPI...");
    if (SD.init())
    {
        ESP_LOGI(TAG, "SD card initialized successfully");

        // 2. 画像の存在確認
        if (SD.exists(IMAGE_FILE))
        {
            ESP_LOGI(TAG, "Loading image: %s", IMAGE_FILE);
            display.drawPngFile(&SD, IMAGE_FILE, 0, 0);
            ESP_LOGI(TAG, "Image displayed successfully");
        }
        else
        {
            ESP_LOGE(TAG, "Image file not found: %s", IMAGE_FILE);
            display.setTextColor(TFT_RED);
            display.setTextSize(2);
            display.setCursor(10, 10);
            display.printf("File not found: %s", IMAGE_FILE);
        }
        display.fillScreen(TFT_BLACK);
        listAndDisplayFiles();

        SD.close();
    }
    else
    {
        ESP_LOGE(TAG, "SD card initialization failed");
        display.setTextColor(TFT_RED);
        display.setTextSize(2);
        display.setCursor(10, 10);
        display.println("SD Card Init Failed");
    }

    // キャンバステストオブジェクトの初期化
    ESP_LOGI(TAG, "Initializing Canvas Test...");
    canvasTest = new CanvasTest(&display);
    if (canvasTest && canvasTest->init())
    {
        ESP_LOGI(TAG, "Canvas test initialized successfully");
    }
    else
    {
        ESP_LOGE(TAG, "Canvas test initialization failed");
        if (canvasTest)
        {
            delete canvasTest;
            canvasTest = nullptr;
        }
    }

    // スクリーントランジションオブジェクトの初期化（修正版）
    ESP_LOGI(TAG, "Initializing Screen Transition...");
    screenTransition = new ScreenTransition(&display);
    // 修正：引数を正しく変更
    if (screenTransition && screenTransition->init(true))  // PSRAMを使用
    {
        ESP_LOGI(TAG, "Screen transition initialized successfully");
    }
    else
    {
        ESP_LOGE(TAG, "Screen transition initialization failed");
        if (screenTransition)
        {
            delete screenTransition;
            screenTransition = nullptr;
        }
    }

    // タッチハンドラの初期化
    ESP_LOGI(TAG, "Initializing touch handler...");
    if (touchHandler.init(&display))
    {
        ESP_LOGI(TAG, "Touch handler initialized successfully");

        touchHandler.setOnTouchStart(onTouchStart);
        touchHandler.setOnTouchEnd(onTouchEnd);
        touchHandler.setOnSwipe(onSwipe);
        touchHandler.setMinSwipeDistance(50);

        // ButtonManagerの初期化
        buttonManager = new ButtonManager(&display, &touchHandler);

        // テストボタンの作成
        btnTest = new Button(&display, 10, 350, 100, 40, "テストボタン");
        btnTest->setOnPressed(onTestButtonPressed);
        btnTest->setOnReleased(onTestButtonReleased);
        btnTest->setOnSwipeUp(onButtonSwipeUp);
        btnTest->setOnSwipeDown(onButtonSwipeDown);
        btnTest->setOnSwipeLeft(onButtonSwipeLeft);
        btnTest->setOnSwipeRight(onButtonSwipeRight);

        ButtonStyle testStyle = ButtonStyle::defaultStyle();
        testStyle.bgColor = TFT_BLUE;
        testStyle.textColor = TFT_WHITE;
        btnTest->setStyle(testStyle);

        // USB MSCボタンの作成
        btnUSBMSC = new Button(&display, 120, 350, 100, 40, "USB MSC");
        btnUSBMSC->setOnPressed(onUSBMSCButtonPressed);
        btnUSBMSC->setOnReleased(onUSBMSCButtonReleased);

        // キャンバステストボタンの作成
        btnCanvasTest = new Button(&display, 230, 350, 100, 40, "Canvas Test");
        btnCanvasTest->setOnPressed(onCanvasTestButtonPressed);
        btnCanvasTest->setOnReleased(onCanvasTestButtonReleased);

        // トランジションテストボタンの作成
        btnTransitionTest = new Button(&display, 340, 350, 100, 40, "Transition");
        btnTransitionTest->setOnPressed(onTransitionTestButtonPressed);
        btnTransitionTest->setOnReleased(onTransitionTestButtonReleased);

        // 停止ボタンの作成
        btnCanvasStop = new Button(&display, 450, 350, 80, 40, "Stop Test");
        btnCanvasStop->setOnPressed(onCanvasStopButtonPressed);
        btnCanvasStop->setOnReleased(onCanvasStopButtonReleased);
        btnCanvasStop->setVisible(false);

        // カスタムスタイルの設定
        ButtonStyle canvasStyle = ButtonStyle::defaultStyle();
        canvasStyle.bgColor = TFT_PURPLE;
        canvasStyle.textColor = TFT_WHITE;
        btnCanvasTest->setStyle(canvasStyle);

        ButtonStyle transitionStyle = ButtonStyle::defaultStyle();
        transitionStyle.bgColor = TFT_DARKGREEN;
        transitionStyle.textColor = TFT_WHITE;
        btnTransitionTest->setStyle(transitionStyle);

        ButtonStyle stopStyle = ButtonStyle::defaultStyle();
        stopStyle.bgColor = TFT_RED;
        stopStyle.textColor = TFT_WHITE;
        btnCanvasStop->setStyle(stopStyle);

        // ボタンマネージャーに追加
        buttonManager->addButton(btnTest);
        buttonManager->addButton(btnUSBMSC);
        buttonManager->addButton(btnCanvasTest);
        buttonManager->addButton(btnTransitionTest);
        buttonManager->addButton(btnCanvasStop);

        // ボタンを描画
        buttonManager->drawButtons();
    }
    else
    {
        ESP_LOGE(TAG, "Touch handler initialization failed");
    }

    textDisplayDemo();
}

void loop(void)
{
    // 軽量版トランジションデモの処理
    updateTransitionDemo();

    // USB接続状態チェック
    static int64_t last_check = 0;
    int64_t now = esp_timer_get_time() / 1000;

    if (now - last_check > 5000)
    {
        last_check = now;

        if (currentTestMode == TestMode::NORMAL && SD.isUSBMSCEnabled())
        {
            bool connected = SD.isUSBMSCConnected();
            ESP_LOGI(TAG, "USB MSC connection status: %s", connected ? "Connected" : "Disconnected");

            display.setTextColor(TFT_WHITE, TFT_BLACK);
            display.setTextSize(1);
            display.setCursor(10, display.height() - 20);
            display.printf("USB Status: %s    ", connected ? "Connected" : "Disconnected");
        }
    }

    // ボタン更新処理
    if (buttonManager)
    {
        buttonManager->update();
    }

    // 通常のタッチ処理
    if (currentTestMode == TestMode::NORMAL && touchHandler.update() &&
        touchHandler.isTouched() && !buttonManager)
    {
        const ExtendedTouchPoint &point = touchHandler.getLastPoint();
        touchHandler.drawCircleAtTouch(10, TFT_RED);
        ESP_LOGI(TAG, "Touch at (%d, %d)", point.x, point.y);

        display.setTextColor(TFT_GREEN, TFT_BLACK);
        display.setTextSize(1);
        display.setCursor(10, display.height() - 40);
        display.printf("Touch: (%d, %d)     ", point.x, point.y);
    }
}

void runMainLoop(void *args)
{
    setup();
    for (;;)
    {
        loop();
        vTaskDelay(1);
    }
    vTaskDelete(g_handle);
}

void initializeTask()
{
    xTaskCreatePinnedToCore(&runMainLoop, "task1-main", 8192, nullptr, 1,
                          &g_handle, 1);
    configASSERT(g_handle);
}

extern "C"
{
    void app_main(void)
    {
        esp_log_level_set(TAG, ESP_LOG_INFO);
        ESP_LOGI(TAG, "Application starting...");
        initializeTask();
    }
}