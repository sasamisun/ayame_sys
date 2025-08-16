// main/hello_world_main.cpp - SimpleTransition対応版（完全書き換え）
// 新しい超シンプル設計でアドベンチャーゲームシステムを実現するにゃ！

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
#include "SimpleTransition.hpp"    // 新しいシンプルトランジション！

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
Button *btnTransitionTest = nullptr;     // シンプルトランジションテスト用
Button *btnCanvasStop = nullptr;

VLWFontParser vlwParser;
CanvasTest *canvasTest = nullptr;
SimpleTransition *simpleTransition = nullptr;    // 新しい！超シンプルトランジション

// 超シンプル！テスト実行状態管理
enum class TestMode
{
  NORMAL,               // 通常モード
  CANVAS_MEMORY,        // キャンバスメモリテスト
  CANVAS_DOUBLE,        // ダブルバッファテスト
  CANVAS_PERFORMANCE,   // パフォーマンステスト
  SIMPLE_TRANSITION     // 新しい！シンプルトランジションデモ
};

TestMode currentTestMode = TestMode::NORMAL;

// シンプル！シーン管理
int currentSceneId = 1;         // 現在のシーンID（1, 2, 3を循環）
const int MAX_SCENES = 3;       // 最大シーン数

/**
 * @brief シーン1の描画（青い画面）
 * @param canvas 描画先キャンバス
 */
void drawScene1(M5Canvas *canvas)
{
    if (!canvas) return;

    ESP_LOGI(TAG, "Drawing scene 1 - Blue Adventure");
    
    canvas->fillSprite(TFT_BLUE);
    canvas->setTextColor(TFT_WHITE);
    canvas->setTextSize(3);
    canvas->setTextDatum(middle_center);
    
    // タイトル
    canvas->drawString("ADVENTURE GAME", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 - 200);
    canvas->drawString("SCENE 1", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 - 150);
    canvas->setTextSize(2);
    canvas->drawString("青い世界の始まり", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 - 100);

    // 装飾（軽量化版）
    canvas->fillRect(100, 300, 120, 80, TFT_YELLOW);
    canvas->fillCircle(350, 340, 60, TFT_RED);
    canvas->fillTriangle(200, 450, 300, 400, 280, 500, TFT_GREEN);

    // シナリオテキスト
    canvas->setTextSize(1.5);
    canvas->drawString("ここは不思議な青い世界...", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 + 100);
    canvas->drawString("冒険が今、始まろうとしている", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 + 130);
}

/**
 * @brief シーン2の描画（緑の森）
 * @param canvas 描画先キャンバス
 */
void drawScene2(M5Canvas *canvas)
{
    if (!canvas) return;

    ESP_LOGI(TAG, "Drawing scene 2 - Green Forest");
    
    canvas->fillSprite(TFT_DARKGREEN);
    canvas->setTextColor(TFT_WHITE);
    canvas->setTextSize(3);
    canvas->setTextDatum(middle_center);
    
    // タイトル
    canvas->drawString("MYSTERIOUS FOREST", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 - 200);
    canvas->drawString("SCENE 2", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 - 150);
    canvas->setTextSize(2);
    canvas->drawString("深い森の奥へ", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 - 100);

    // 森のイメージ（軽量化版）
    canvas->fillEllipse(SIMPLE_TRANSITION_WIDTH / 2, 350, 150, 75, TFT_GREEN);
    canvas->fillRect(80, 400, 40, 120, 0x4208);   // 茶色の木
    canvas->fillRect(300, 380, 35, 140, 0x4208);  // 茶色の木
    canvas->fillRect(420, 420, 45, 100, 0x4208);  // 茶色の木
    
    // 葉っぱ
    canvas->fillCircle(100, 390, 35, TFT_GREEN);
    canvas->fillCircle(317, 370, 30, TFT_GREEN);
    canvas->fillCircle(442, 410, 40, TFT_GREEN);

    // シナリオテキスト
    canvas->setTextSize(1.5);
    canvas->drawString("深い森に足を踏み入れた", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 + 150);
    canvas->drawString("何かが潜んでいる気配が...", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 + 180);
}

/**
 * @brief シーン3の描画（紫の神秘的な空間）
 * @param canvas 描画先キャンバス
 */
void drawScene3(M5Canvas *canvas)
{
    if (!canvas) return;

    ESP_LOGI(TAG, "Drawing scene 3 - Mystic Space");
    
    canvas->fillSprite(TFT_PURPLE);
    canvas->setTextColor(TFT_WHITE);
    canvas->setTextSize(3);
    canvas->setTextDatum(middle_center);
    
    // タイトル
    canvas->drawString("MYSTIC DIMENSION", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 - 200);
    canvas->drawString("SCENE 3", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 - 150);
    canvas->setTextSize(2);
    canvas->drawString("神秘の次元", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 - 100);

    // 神秘的なパターン（軽量化版）
    const int circle_count = 5;
    for (int i = 0; i < circle_count; i++) {
        int radius = 20 + i * 15;
        uint16_t color = TFT_CYAN + (i * 0x0820);  // グラデーション風
        canvas->drawCircle(SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2, radius, color);
    }
    
    // 魔法陣風
    canvas->fillCircle(150, 400, 25, TFT_MAGENTA);
    canvas->fillCircle(390, 400, 25, TFT_CYAN);
    canvas->fillRect(SIMPLE_TRANSITION_WIDTH / 2 - 2, 300, 4, 200, TFT_YELLOW);
    canvas->fillRect(200, SIMPLE_TRANSITION_HEIGHT / 2 - 2, 140, 4, TFT_YELLOW);

    // シナリオテキスト
    canvas->setTextSize(1.5);
    canvas->drawString("ついに辿り着いた神秘の空間", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 + 150);
    canvas->drawString("ここに秘密が隠されている...", SIMPLE_TRANSITION_WIDTH / 2, SIMPLE_TRANSITION_HEIGHT / 2 + 180);
}

/**
 * @brief 指定シーンをメインキャンバスに描画
 * @param sceneId シーンID（1-3）
 */
void drawSceneToMainCanvas(int sceneId)
{
    if (!simpleTransition) {
        ESP_LOGE(TAG, "Simple transition not available");
        return;
    }

    M5Canvas* mainCanvas = simpleTransition->getMainCanvas();
    if (!mainCanvas) {
        ESP_LOGE(TAG, "Main canvas not available");
        return;
    }

    // シーンIDに応じて描画
    switch (sceneId) {
        case 1:
            drawScene1(mainCanvas);
            break;
        case 2:
            drawScene2(mainCanvas);
            break;
        case 3:
            drawScene3(mainCanvas);
            break;
        default:
            ESP_LOGW(TAG, "Unknown scene ID: %d", sceneId);
            drawScene1(mainCanvas);  // デフォルトはシーン1
            break;
    }
    
    ESP_LOGI(TAG, "Scene %d drawn to main canvas", sceneId);
}

/**
 * @brief 次のシーンに進む（超シンプル！）
 * @param transitionType トランジション種類
 */
void advanceToNextScene(SimpleTransitionType transitionType = SimpleTransitionType::FADE_IN)
{
    if (!simpleTransition) {
        ESP_LOGE(TAG, "Simple transition not available");
        return;
    }

    // 次のシーンIDを計算
    currentSceneId++;
    if (currentSceneId > MAX_SCENES) {
        currentSceneId = 1;  // 最初に戻る
    }

    ESP_LOGI(TAG, "Advancing to scene %d with transition type %d", 
             currentSceneId, static_cast<int>(transitionType));

    // 1. メインキャンバスに次のシーンを描画
    drawSceneToMainCanvas(currentSceneId);

    // 2. トランジション開始（超簡単！）
    simpleTransition->startTransition(transitionType, 16);

    // あとはloop()で自動進行するにゃ！
}

/**
 * @brief 縦書きと横書きテキスト表示のデモ
 */
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
        verticalWriter.setDirection(TextDirection::VERTICAL);
        verticalWriter.setFontSize(1.0);
        verticalWriter.setLineSpacing(6);
        verticalWriter.setCharSpacing(-8);

        verticalWriter.drawText("ジャン・フィリップ・トゥーサン\nおはよう。いんたぁねっと\nフォンふぉんfon");
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

/**
 * @brief ファイルフォルダ一覧表示
 */
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

// === タッチイベントコールバック（変更なし） ===
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

// === ボタンコールバック関数 ===
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

// USB MSCボタンコールバック（変更なし）
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

// キャンバステストボタンコールバック（変更なし）
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

// === 新しい！超シンプルトランジションテストボタン ===
void onTransitionTestButtonPressed(Button *btn)
{
    ESP_LOGI(TAG, "Simple transition test button pressed");
}

void onTransitionTestButtonReleased(Button *btn)
{
    ESP_LOGI(TAG, "Simple transition test button released");

    if (currentTestMode != TestMode::NORMAL)
    {
        return;
    }

    if (!simpleTransition)
    {
        ESP_LOGE(TAG, "Simple transition not initialized");
        return;
    }

    // 超シンプル！トランジションデモ開始
    currentTestMode = TestMode::SIMPLE_TRANSITION;

    btn->setLabel("進行中...");
    btn->setEnabled(false);
    btnCanvasStop->setVisible(true);
    btnCanvasStop->setLabel("Stop Transition");
    buttonManager->drawButtons();

    ESP_LOGI(TAG, "Starting simple transition demo...");

    // 様々なトランジション効果を順番に実行するにゃ！
    // まずは最初のシーンを表示
    currentSceneId = 1;
    drawSceneToMainCanvas(currentSceneId);
    simpleTransition->showImmediate();  // 即座に表示

    ESP_LOGI(TAG, "Simple transition demo started! Use touch to advance scenes.");
}

// キャンバステスト停止ボタンコールバック（修正版）
void onCanvasStopButtonPressed(Button *btn)
{
    ESP_LOGI(TAG, "Canvas/Transition stop button pressed");
}

void onCanvasStopButtonReleased(Button *btn)
{
    ESP_LOGI(TAG, "Canvas/Transition stop button released");

    if (currentTestMode == TestMode::SIMPLE_TRANSITION)
    {
        // シンプルトランジションデモ停止
        if (simpleTransition) {
            simpleTransition->stop();
        }

        currentTestMode = TestMode::NORMAL;
        btnTransitionTest->setLabel("Simple Transition");
        btnTransitionTest->setEnabled(true);
        btn->setVisible(false);
        btn->setLabel("Stop Test");

        display.fillScreen(TFT_BLACK);
        listAndDisplayFiles();
        textDisplayDemo();
        buttonManager->drawButtons();

        ESP_LOGI(TAG, "Simple transition demo stopped by user");
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

// スワイプイベントのコールバック関数（変更なし）
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

void setup()
{
    ESP_LOGI(TAG, "Initializing M5Paper S3 with Simple Transition...");
    display.begin();
    display.setEpdMode(lgfx::v1::epd_mode::epd_mode_t::epd_quality);
    display.setColorDepth(1);
    display.fillScreen(TFT_BLACK);

    // 1. SDカードの初期化（変更なし）
    ESP_LOGI(TAG, "Initializing SD card via SPI...");
    if (SD.init())
    {
        ESP_LOGI(TAG, "SD card initialized successfully");

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

    // 2. キャンバステストオブジェクトの初期化（変更なし）
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

    // 3. 新しい！超シンプルトランジションオブジェクトの初期化
    ESP_LOGI(TAG, "Initializing Simple Transition...");
    simpleTransition = new SimpleTransition(&display);
    if (simpleTransition && simpleTransition->init(true))  // PSRAM使用
    {
        ESP_LOGI(TAG, "Simple transition initialized successfully! 🎉");
        
        // 完了コールバックを設定
        simpleTransition->setOnComplete([]() {
            ESP_LOGI(TAG, "✨ Transition completed callback fired! ✨");
        });
        
        // ステップコールバックを設定（オプション）
        simpleTransition->setOnStep([](int current, int total) {
            ESP_LOGD(TAG, "Transition step: %d/%d (%.1f%%)", 
                     current, total, (float)current * 100.0f / total);
        });
        
    }
    else
    {
        ESP_LOGE(TAG, "Simple transition initialization failed");
        if (simpleTransition)
        {
            delete simpleTransition;
            simpleTransition = nullptr;
        }
    }

    // 4. タッチハンドラの初期化（変更なし）
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

        // ボタン作成（変更なし、ラベルのみ更新）
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

        btnUSBMSC = new Button(&display, 120, 350, 100, 40, "USB MSC");
        btnUSBMSC->setOnPressed(onUSBMSCButtonPressed);
        btnUSBMSC->setOnReleased(onUSBMSCButtonReleased);

        btnCanvasTest = new Button(&display, 230, 350, 100, 40, "Canvas Test");
        btnCanvasTest->setOnPressed(onCanvasTestButtonPressed);
        btnCanvasTest->setOnReleased(onCanvasTestButtonReleased);

        // 新しい！シンプルトランジションテストボタン
        btnTransitionTest = new Button(&display, 340, 350, 100, 40, "Simple Trans");
        btnTransitionTest->setOnPressed(onTransitionTestButtonPressed);
        btnTransitionTest->setOnReleased(onTransitionTestButtonReleased);

        btnCanvasStop = new Button(&display, 450, 350, 80, 40, "Stop Test");
        btnCanvasStop->setOnPressed(onCanvasStopButtonPressed);
        btnCanvasStop->setOnReleased(onCanvasStopButtonReleased);
        btnCanvasStop->setVisible(false);

        // スタイル設定
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
    // 1. 超重要！シンプルトランジション更新（最優先）
    if (simpleTransition && simpleTransition->isActive()) {
        simpleTransition->update();  // 1ステップ実行
        return;  // トランジション中は他の処理をスキップ
    }

    // 2. シンプルトランジションデモ中のタッチ処理
    if (currentTestMode == TestMode::SIMPLE_TRANSITION) {
        if (touchHandler.update() && touchHandler.isTouchEvent()) {
            const ExtendedTouchPoint& point = touchHandler.getLastPoint();
            
            // タッチされたら次のシーンに進む（様々なトランジション効果で）
            static int transitionTypeIndex = 0;
            SimpleTransitionType transitions[] = {
                SimpleTransitionType::FADE_IN,
                SimpleTransitionType::SLIDE_LEFT,
                SimpleTransitionType::SLIDE_RIGHT,
                SimpleTransitionType::SLIDE_UP,
                SimpleTransitionType::SLIDE_DOWN,
                SimpleTransitionType::WIPE_HORIZONTAL,
                SimpleTransitionType::WIPE_VERTICAL,
                SimpleTransitionType::REVEAL_CENTER,
                SimpleTransitionType::REVEAL_CORNER
            };
            
            const int transitionCount = sizeof(transitions) / sizeof(transitions[0]);
            SimpleTransitionType currentTransition = transitions[transitionTypeIndex % transitionCount];
            transitionTypeIndex++;
            
            ESP_LOGI(TAG, "Touch detected! Advancing to next scene with transition type %d", 
                     static_cast<int>(currentTransition));
            
            // 次のシーンに進む（超シンプル！）
            advanceToNextScene(currentTransition);
        }
        return;  // デモ中は他の処理をスキップ
    }

    // 3. 通常の処理（トランジション完了時のみ実行）
    static int64_t last_check = 0;
    int64_t now = esp_timer_get_time() / 1000;

    if (now - last_check > 5000) {
        last_check = now;

        if (currentTestMode == TestMode::NORMAL && SD.isUSBMSCEnabled()) {
            bool connected = SD.isUSBMSCConnected();
            ESP_LOGI(TAG, "USB MSC connection status: %s", connected ? "Connected" : "Disconnected");

            display.setTextColor(TFT_WHITE, TFT_BLACK);
            display.setTextSize(1);
            display.setCursor(10, display.height() - 20);
            display.printf("USB Status: %s    ", connected ? "Connected" : "Disconnected");
        }
    }

    // 4. ボタン更新処理
    if (buttonManager) {
        buttonManager->update();
    }

    // 5. 通常のタッチ処理
    if (currentTestMode == TestMode::NORMAL && touchHandler.update() &&
        touchHandler.isTouched() && !buttonManager) {
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
        ESP_LOGI(TAG, "Application starting with Simple Transition...");
        initializeTask();
    }
}