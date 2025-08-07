// main/hello_world_main.cpp - Canvas Based Drawing System
// メインCanvas描画システム対応版

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
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

// メインCanvas描画システム
M5Canvas *mainCanvas = nullptr;     // メイン描画用Canvas
bool canvasInitialized = false;     // Canvas初期化フラグ
bool needsRedraw = true;            // 再描画フラグ

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

// トランジションタイプの配列（デモ用）
TransitionType transitionTypes[] = {
    TransitionType::FADE_BLACK,
    TransitionType::FADE_WHITE,
    TransitionType::SLIDE_LEFT,
    TransitionType::SLIDE_RIGHT,
    TransitionType::SLIDE_UP,
    TransitionType::SLIDE_DOWN,
    TransitionType::WIPE_LEFT,
    TransitionType::WIPE_RIGHT,
    TransitionType::CIRCLE_EXPAND,
    TransitionType::CIRCLE_SHRINK
};

const int transitionTypeCount = 1;

// トランジションタイプの名前
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
    case TransitionType::CIRCLE_EXPAND:
        return "Circle Expand";
    case TransitionType::CIRCLE_SHRINK:
        return "Circle Shrink";
    default:
        return "Unknown";
    }
}

/**
 * @brief メインCanvasの初期化
 * @return 成功時true
 */
bool initMainCanvas()
{
    ESP_LOGI(TAG, "Initializing main canvas...");
    
    // PSRAMの使用可能容量をチェック
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t canvas_size = display.width() * display.height() * 2; // RGB565 = 2 bytes per pixel
    
    ESP_LOGI(TAG, "Required canvas memory: %zu bytes", canvas_size);
    ESP_LOGI(TAG, "Available PSRAM: %zu bytes", psram_free);
    
    if (psram_free < canvas_size) {
        ESP_LOGE(TAG, "Insufficient PSRAM memory for main canvas");
        return false;
    }
    
    // メインCanvasを作成
    mainCanvas = new M5Canvas(&display);
    if (!mainCanvas) {
        ESP_LOGE(TAG, "Failed to allocate main canvas");
        return false;
    }
    
    // PSRAMを使用するように設定
    mainCanvas->setPsram(true);
    
    // Canvasスプライトを作成
    if (!mainCanvas->createSprite(display.width(), display.height())) {
        ESP_LOGE(TAG, "Failed to create main canvas sprite");
        delete mainCanvas;
        mainCanvas = nullptr;
        return false;
    }
    
    // 初期背景色を設定
    mainCanvas->fillSprite(TFT_BLACK);
    
    canvasInitialized = true;
    ESP_LOGI(TAG, "Main canvas initialized successfully (%ldx%ld)", 
             display.width(), display.height());
    
    return true;
}

/**
 * @brief メインCanvasのクリーンアップ
 */
void cleanupMainCanvas()
{
    if (mainCanvas) {
        ESP_LOGI(TAG, "Cleaning up main canvas...");
        mainCanvas->deleteSprite();
        delete mainCanvas;
        mainCanvas = nullptr;
        canvasInitialized = false;
    }
}

/**
 * @brief メインCanvasを画面に表示
 */
void updateDisplay()
{
    if (canvasInitialized && mainCanvas) {
        mainCanvas->pushSprite(0, 0);
        needsRedraw = false;
        ESP_LOGD(TAG, "Display updated from main canvas");
    }
}

/**
 * @brief 再描画フラグを設定
 */
void requestRedraw()
{
    needsRedraw = true;
}

// デモ画面描画関数（Canvas対応版）
void drawDemoScreen1(M5Canvas *canvas)
{
    if (!canvas) return;
    
    canvas->fillSprite(TFT_BLUE);
    canvas->setTextColor(TFT_WHITE);
    canvas->setTextSize(3);
    canvas->setTextDatum(middle_center);
    canvas->drawString("SCREEN 1", canvas->width() / 2, canvas->height() / 2 - 100);
    
    // 装飾的な図形を追加
    canvas->fillCircle(100, 200, 50, TFT_YELLOW);
    canvas->fillRect(300, 150, 100, 100, TFT_RED);
    canvas->drawTriangle(200, 400, 150, 500, 250, 500, TFT_GREEN);
    
    // テキスト情報
    canvas->setTextSize(2);
    canvas->drawString("Adventure Game Demo", canvas->width() / 2, canvas->height() / 2 + 50);
    canvas->setTextSize(1);
    canvas->drawString("Canvas System Test", canvas->width() / 2, canvas->height() / 2 + 100);
}

void drawDemoScreen2(M5Canvas *canvas)
{
    if (!canvas) return;
    
    canvas->fillSprite(TFT_RED);
    canvas->setTextColor(TFT_WHITE);
    canvas->setTextSize(3);
    canvas->setTextDatum(middle_center);
    canvas->drawString("SCREEN 2", canvas->width() / 2, canvas->height() / 2 - 100);
    
    // 異なる装飾
    canvas->fillEllipse(canvas->width() / 2, 300, 80, 40, TFT_CYAN);
    canvas->drawRoundRect(150, 450, 200, 80, 20, TFT_MAGENTA);
    
    // パターン描画
    for (int i = 0; i < 10; i++) {
        canvas->drawLine(i * (canvas->width()/10), 0, i * (canvas->width()/10), canvas->height(), TFT_DARKGRAY);
    }
    
    canvas->setTextSize(2);
    canvas->drawString("Next Scene", canvas->width() / 2, canvas->height() / 2 + 50);
    canvas->setTextSize(1);
    canvas->drawString("Canvas Powered", canvas->width() / 2, canvas->height() / 2 + 100);
}

/**
 * @brief 縦書きと横書きテキスト表示のデモ（Canvas対応版）
 */
void textDisplayDemo()
{
    if (!canvasInitialized || !mainCanvas) return;
    
    ESP_LOGI(TAG, "Starting VLW font demo with canvas...");
    
    // VLWフォントデータの初期化（例：shipporiフォント）
    if (vlwParser.init(shippori, sizeof(shippori))) {
        ESP_LOGI(TAG, "VLW font initialized successfully");
        
        // フォント情報をデバッグ出力
        vlwParser.debugPrintFontInfo();
        
        // TypoWriteでVLWパーサーを使用
        TypoWrite verticalWriter(&display);
        verticalWriter.setDrawTarget(mainCanvas); // ★ Canvas描画先を設定
        verticalWriter.setVLWParser(&vlwParser);  // VLWパーサーを設定
        
        // フォント設定
        verticalWriter.loadFontFromArray(shippori); // M5GFXにもフォントを設定
        verticalWriter.setPosition(400, 0);
        verticalWriter.setArea(130, 700);
        verticalWriter.setColor(TFT_WHITE);
        verticalWriter.setBackgroundColor(TFT_TRANSPARENT);
        verticalWriter.setTransparentBg(true);
        verticalWriter.setDirection(TextDirection::VERTICAL);
        verticalWriter.setFontSize(1.0);
        verticalWriter.setLineSpacing(6);
        
        // テキスト描画（Canvasに描画される）
        verticalWriter.drawText("画面遷移システムを\n実装しました。\n様々なエフェクトで\n美しい切り替えが\n可能です。");
        
        requestRedraw(); // 再描画要求
    }
    else {
        ESP_LOGE(TAG, "Failed to initialize VLW font");
        
        // エラー表示（Canvas経由）
        mainCanvas->setTextColor(TFT_RED);
        mainCanvas->setTextSize(1);
        mainCanvas->drawString("VLW Font Load Failed", 10, 100);
        requestRedraw();
    }
}

/**
 * @brief ファイル情報を安全にフォーマットする
 * @param file ファイル情報
 * @param buffer 出力バッファ
 * @param bufferSize バッファサイズ
 * @return フォーマット成功時はtrue
 */
bool formatFileInfo(const FileInfo* file, char* buffer, size_t bufferSize)
{
    if (!file || !buffer || bufferSize == 0) {
        return false;
    }
    
    // 安全な文字列長チェック
    size_t nameLen = strnlen(file->name, sizeof(file->name));
    const size_t maxDisplayLen = 180;  // 表示用最大長
    
    // バッファサイズチェック（最低限必要なサイズ）
    const size_t minRequiredSize = maxDisplayLen + 50;  // 余裕を持ったサイズ
    if (bufferSize < minRequiredSize) {
        ESP_LOGE("FILE_FORMAT", "Buffer too small: %zu < %zu", bufferSize, minRequiredSize);
        return false;
    }
    
    int result;
    if (file->isDirectory) {
        if (nameLen > maxDisplayLen) {
            // 長いディレクトリ名は切り詰め
            result = snprintf(buffer, bufferSize, "[DIR] %.*s...", 
                            static_cast<int>(maxDisplayLen), file->name);
        } else {
            // 通常のディレクトリ名
            result = snprintf(buffer, bufferSize, "[DIR] %.*s", 
                            static_cast<int>(nameLen), file->name);
        }
    } else {
        float size_kb = static_cast<float>(file->size) / 1024.0f;
        
        if (nameLen > maxDisplayLen) {
            // 長いファイル名は切り詰め
            result = snprintf(buffer, bufferSize, "%.*s... (%.1f KB)", 
                            static_cast<int>(maxDisplayLen), file->name, size_kb);
        } else {
            // 通常のファイル名
            result = snprintf(buffer, bufferSize, "%.*s (%.1f KB)", 
                            static_cast<int>(nameLen), file->name, size_kb);
        }
    }
    
    // snprintfの戻り値チェック
    if (result < 0 || static_cast<size_t>(result) >= bufferSize) {
        ESP_LOGE("FILE_FORMAT", "snprintf failed or truncated: result=%d, bufferSize=%zu", 
                 result, bufferSize);
        return false;
    }
    
    return true;
}

/**
 * @brief ファイルフォルダ一覧表示（完全安全版）
 */
void listAndDisplayFiles()
{
    if (!canvasInitialized || !mainCanvas) return;
    
    ESP_LOGI(TAG, "Listing files with canvas (safe version)...");
    
    // SDカードのルートディレクトリを読み込み
    DirInfo *rootDir = SD.listDir("/");
    if (rootDir) {
        // Canvas経由でファイル一覧を描画
        mainCanvas->setTextColor(TFT_WHITE);
        mainCanvas->setTextSize(1);
        
        int x = 10;
        int y = 10;
        mainCanvas->drawString("SD Card Files:", x, y);
        
        y += 20;
        for (size_t i = 0; i < rootDir->count; i++) {
            FileInfo *file = &rootDir->files[i];
            
            // 十分に大きな固定サイズバッファを確保
            char fileInfo[300];  // 計算済みの安全サイズ
            
            // 安全なフォーマット関数を使用
            if (formatFileInfo(file, fileInfo, sizeof(fileInfo))) {
                mainCanvas->drawString(fileInfo, x, y);
                y += 20;
                
                if (y > mainCanvas->height() - 120) {
                    // 画面の下部に達したら表示を止める
                    mainCanvas->drawString("... and more files", x, y);
                    break;
                }
            } else {
                // フォーマット失敗時のフォールバック
                ESP_LOGW(TAG, "Failed to format file info for: %s", file->name);
                mainCanvas->drawString("[FORMAT ERROR]", x, y);
                y += 20;
            }
        }
        
        // メモリ解放を忘れずに
        SD.freeDirInfo(rootDir);
        requestRedraw(); // 再描画要求
    }
    else {
        // エラー表示（Canvas経由）
        mainCanvas->setTextColor(TFT_RED);
        mainCanvas->setTextSize(1);
        mainCanvas->drawString("Failed to read SD card directory", 10, 10);
        requestRedraw();
    }
}

/**
 * @brief ステータス情報を Canvas に描画
 */
void drawStatusInfo(const char* message, uint32_t color = TFT_GREEN)
{
    if (!canvasInitialized || !mainCanvas) return;
    
    // ステータス表示エリア（画面下部）をクリア
    int status_y = mainCanvas->height() - 100;
    mainCanvas->fillRect(0, status_y, mainCanvas->width(), 100, TFT_BLACK);
    
    // ステータステキストを描画
    mainCanvas->setTextColor(color, TFT_BLACK);
    mainCanvas->setTextSize(1);
    mainCanvas->drawString(message, 10, status_y + 10);
    
    requestRedraw();
}

/**
 * @brief タッチ座標をCanvas に描画
 */
void drawTouchInfo(int x, int y)
{
    if (!canvasInitialized || !mainCanvas) return;
    
    // タッチ情報表示エリアをクリア
    int touch_y = mainCanvas->height() - 40;
    mainCanvas->fillRect(0, touch_y, mainCanvas->width(), 40, TFT_BLACK);
    
    // タッチ座標を描画
    char touchInfo[64];
    snprintf(touchInfo, sizeof(touchInfo), "Touch: (%d, %d)", x, y);
    mainCanvas->setTextColor(TFT_GREEN, TFT_BLACK);
    mainCanvas->setTextSize(1);
    mainCanvas->drawString(touchInfo, 10, touch_y + 10);
    
    // タッチされた位置に円を描画
    mainCanvas->fillCircle(x, y, 10, TFT_RED);
    
    requestRedraw();
}

/**
 * @brief メイン画面の再描画
 * 画面に表示するすべての要素をCanvasに描画
 */
void redrawMainScreen()
{
    if (!canvasInitialized || !mainCanvas) return;
    
    ESP_LOGI(TAG, "Redrawing main screen to canvas...");
    
    // Canvas背景をクリア
    mainCanvas->fillSprite(TFT_BLACK);
    
    // テスト実行中でない場合のみ通常画面を描画
    if (currentTestMode == TestMode::NORMAL) {
        // ファイル一覧を描画
        listAndDisplayFiles();
        
        // 縦書きテキストデモを描画
        textDisplayDemo();
        
        // ボタンをCanvasに描画
        if (buttonManager) {
            buttonManager->drawButtons();
            ESP_LOGI(TAG, "Buttons drawn to canvas");
        }
    }
    
    requestRedraw();
}

// トランジションデモの次の画面を準備
void prepareNextDemoScreen(M5Canvas *canvas)
{
    static int screenIndex = 0;
    screenIndex = (screenIndex + 1) % 2;
    
    switch (screenIndex) {
        case 0:
            drawDemoScreen1(canvas);
            break;
        case 1:
            drawDemoScreen2(canvas);
            break;
    }
}

// タッチイベントのコールバック関数
void onTouchStart(const ExtendedTouchPoint &point)
{
    ESP_LOGI(TAG, "Touch started at (%d, %d)", point.x, point.y);
    
    // テスト実行中は通常のタッチ処理をスキップ
    if (currentTestMode != TestMode::NORMAL) {
        return;
    }
    
    // Canvas経由でタッチ開始情報を描画
    drawStatusInfo("Touch started", TFT_GREEN);
}

void onTouchEnd(const ExtendedTouchPoint &point)
{
    ESP_LOGI(TAG, "Touch ended at (%d, %d)", point.x, point.y);
    
    // テスト実行中は通常のタッチ処理をスキップ
    if (currentTestMode != TestMode::NORMAL) {
        return;
    }
    
    // Canvas経由でタッチ終了情報を描画
    drawStatusInfo("Touch ended", TFT_RED);
}

void onSwipe(SwipeDirection direction, const ExtendedTouchPoint &start, const ExtendedTouchPoint &end)
{
    // スワイプ方向を文字列に変換
    const char *dirStr = "Unknown";
    switch (direction) {
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
    
    // テスト実行中は通常のスワイプ処理をスキップ
    if (currentTestMode != TestMode::NORMAL) {
        return;
    }
    
    // Canvas経由でスワイプ情報を描画
    char swipeInfo[64];
    snprintf(swipeInfo, sizeof(swipeInfo), "Swipe: %s", dirStr);
    drawStatusInfo(swipeInfo, TFT_YELLOW);
}

// ボタンコールバック関数
void onTestButtonPressed(Button *btn)
{
    ESP_LOGI(TAG, "Test button pressed");
}

void onTestButtonReleased(Button *btn)
{
    ESP_LOGI(TAG, "Test button released");
    
    // テスト実行中は処理しない
    if (currentTestMode != TestMode::NORMAL) {
        return;
    }
    
    // Canvas経由でメッセージを表示
    drawStatusInfo("テストボタンが押されました", TFT_YELLOW);
}

// USB MSCボタンコールバック
void onUSBMSCButtonPressed(Button *btn)
{
    ESP_LOGI(TAG, "USB MSC button pressed");
}

void onUSBMSCButtonReleased(Button *btn)
{
    ESP_LOGI(TAG, "USB MSC button released");
    
    // テスト実行中は処理しない
    if (currentTestMode != TestMode::NORMAL) {
        return;
    }
    
    // USB MSCの切り替え
    if (SD.isUSBMSCEnabled()) {
        // USB MSCを無効化
        if (SD.disableUSBMSC()) {
            ESP_LOGI(TAG, "USB MSC disabled");
            btn->setLabel("Enable USB MSC");
            
            // メイン画面を再描画（ボタンも含む）
            redrawMainScreen();
            updateDisplay();
        }
    }
    else {
        // USB MSCを有効化
        if (SD.enableUSBMSC()) {
            ESP_LOGI(TAG, "USB MSC enabled");
            btn->setLabel("Disable USB MSC");
            
            // Canvas経由で情報表示
            mainCanvas->fillSprite(TFT_BLACK);
            mainCanvas->setTextColor(TFT_WHITE);
            mainCanvas->setTextSize(1.5);
            mainCanvas->drawString("USB MSC Enabled", 10, 100);
            mainCanvas->drawString("Connect to PC to access SD card", 10, 130);
            requestRedraw();
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
    
    // テスト実行中は処理しない
    if (currentTestMode != TestMode::NORMAL) {
        return;
    }
    
    if (!canvasTest) {
        ESP_LOGE(TAG, "Canvas test not initialized");
        return;
    }
    
    // キャンバステストの実行（Canvas経由で準備メッセージ）
    mainCanvas->fillSprite(TFT_BLACK);
    mainCanvas->setTextColor(TFT_CYAN);
    mainCanvas->setTextSize(2);
    mainCanvas->drawString("Starting Canvas Tests...", 10, 50);
    mainCanvas->drawString("Please wait...", 10, 90);
    updateDisplay(); // 即座に表示
    
    // ボタンの状態を更新
    btn->setLabel("Testing...");
    btn->setEnabled(false);
    btnCanvasStop->setVisible(true);
    buttonManager->drawButtons();
    
    // 段階的にテストを実行
    ESP_LOGI(TAG, "Running Canvas Memory Test...");
    currentTestMode = TestMode::CANVAS_MEMORY;
    canvasTest->testMemoryUsage();
    vTaskDelay(pdMS_TO_TICKS(3000)); // 3秒間表示
    
    ESP_LOGI(TAG, "Running Canvas Performance Test...");
    currentTestMode = TestMode::CANVAS_PERFORMANCE;
    canvasTest->testDrawingPerformance();
    
    ESP_LOGI(TAG, "Running Canvas Double Buffer Test...");
    currentTestMode = TestMode::CANVAS_DOUBLE;
    canvasTest->runDoubleBufferTest();
    
    // テスト終了
    currentTestMode = TestMode::NORMAL;
    btn->setLabel("Canvas Test");
    btn->setEnabled(true);
    btnCanvasStop->setVisible(false);
    
    // メイン画面を再描画（ボタンも含む）
    redrawMainScreen();
    updateDisplay();
    
    ESP_LOGI(TAG, "Canvas tests completed");
}

// トランジションテストボタンコールバック
void onTransitionTestButtonPressed(Button *btn)
{
    ESP_LOGI(TAG, "Transition test button pressed");
}

void onTransitionTestButtonReleased(Button *btn)
{
    ESP_LOGI(TAG, "Transition test button released");
    
    // テスト実行中は処理しない
    if (currentTestMode != TestMode::NORMAL) {
        return;
    }
    
    if (!screenTransition) {
        ESP_LOGE(TAG, "Screen transition not initialized");
        return;
    }
    
    // トランジションデモ開始
    currentTestMode = TestMode::TRANSITION_DEMO;
    transitionDemoRunning = true;
    currentTransitionIndex = 0;
    
    // ボタンの状態を更新
    btn->setLabel("Running...");
    btn->setEnabled(false);
    btnCanvasStop->setVisible(true);
    btnCanvasStop->setLabel("Stop Transition");
    buttonManager->drawButtons();
    
    ESP_LOGI(TAG, "Starting transition demo...");
    
    // 最初の画面をキャプチャ
    screenTransition->captureSource();
    
    ESP_LOGI(TAG, "Transition demo started");
}

// キャンバステスト停止ボタンコールバック
void onCanvasStopButtonPressed(Button *btn)
{
    ESP_LOGI(TAG, "Canvas/Transition stop button pressed");
}

void onCanvasStopButtonReleased(Button *btn)
{
    ESP_LOGI(TAG, "Canvas/Transition stop button released");
    
    if (currentTestMode == TestMode::TRANSITION_DEMO) {
        // トランジションデモ停止
        transitionDemoRunning = false;
        screenTransition->stopTransition();
        
        currentTestMode = TestMode::NORMAL;
        btnTransitionTest->setLabel("Transition Test");
        btnTransitionTest->setEnabled(true);
        btn->setVisible(false);
        btn->setLabel("Stop Test");
        
        // メイン画面を再描画（ボタンも含む）
        redrawMainScreen();
        updateDisplay();
        
        ESP_LOGI(TAG, "Transition demo stopped by user");
    }
    else if (canvasTest && canvasTest->isTestRunning()) {
        // キャンバステスト停止
        canvasTest->stopTest();
        
        currentTestMode = TestMode::NORMAL;
        btnCanvasTest->setLabel("Canvas Test");
        btnCanvasTest->setEnabled(true);
        btn->setVisible(false);
        
        // メイン画面を再描画
        redrawMainScreen();
        buttonManager->drawButtons();
        updateDisplay();
        
        ESP_LOGI(TAG, "Canvas test stopped by user");
    }
}

// スワイプイベントのコールバック関数を定義
void onButtonSwipeUp(Button *btn, SwipeDirection dir)
{
    ESP_LOGI(TAG, "Button swiped up: %s", btn->getLabel());
    
    // テスト実行中は処理しない
    if (currentTestMode != TestMode::NORMAL) {
        return;
    }
    
    // 十分なバッファサイズを確保（プレフィックス + ラベル + 余裕）
    char swipeInfo[128];  // 64 + 余裕分
    snprintf(swipeInfo, sizeof(swipeInfo), "Button swiped up: %.60s", btn->getLabel());
    drawStatusInfo(swipeInfo, TFT_CYAN);
}

void onButtonSwipeDown(Button *btn, SwipeDirection dir)
{
    ESP_LOGI(TAG, "Button swiped down: %s", btn->getLabel());
    
    // テスト実行中は処理しない
    if (currentTestMode != TestMode::NORMAL) {
        return;
    }
    
    char swipeInfo[128];
    snprintf(swipeInfo, sizeof(swipeInfo), "Button swiped down: %.60s", btn->getLabel());
    drawStatusInfo(swipeInfo, TFT_MAGENTA);
}

void onButtonSwipeLeft(Button *btn, SwipeDirection dir)
{
    ESP_LOGI(TAG, "Button swiped left: %s", btn->getLabel());
    
    // テスト実行中は処理しない
    if (currentTestMode != TestMode::NORMAL) {
        return;
    }
    
    char swipeInfo[128];
    snprintf(swipeInfo, sizeof(swipeInfo), "Button swiped left: %.60s", btn->getLabel());
    drawStatusInfo(swipeInfo, TFT_ORANGE);
}

void onButtonSwipeRight(Button *btn, SwipeDirection dir)
{
    ESP_LOGI(TAG, "Button swiped right: %s", btn->getLabel());
    
    // テスト実行中は処理しない
    if (currentTestMode != TestMode::NORMAL) {
        return;
    }
    
    char swipeInfo[128];
    snprintf(swipeInfo, sizeof(swipeInfo), "Button swiped right: %.60s", btn->getLabel());
    drawStatusInfo(swipeInfo, TFT_PINK);
}


void setup()
{
    ESP_LOGI(TAG, "Initializing M5Paper S3 with Canvas system...");
    display.begin();
    display.setEpdMode(lgfx::v1::epd_mode::epd_mode_t::epd_fastest);
    display.setColorDepth(1);
    display.fillScreen(TFT_BLACK);
    
    // メインCanvasの初期化
    if (!initMainCanvas()) {
        ESP_LOGE(TAG, "Failed to initialize main canvas, using direct drawing");
    }
    
    // 1. SDカードの初期化（SPI接続）
    ESP_LOGI(TAG, "Initializing SD card via SPI...");
    if (SD.init()) {
        ESP_LOGI(TAG, "SD card initialized successfully");
        
        // 2. 画像の存在確認と読み込み
        if (SD.exists(IMAGE_FILE)) {
            ESP_LOGI(TAG, "Loading image: %s", IMAGE_FILE);
            
            // Canvas経由で画像を読み込んで表示
            if (canvasInitialized) {
                mainCanvas->drawPngFile(&SD, IMAGE_FILE, 0, 0);
                updateDisplay();
                vTaskDelay(pdMS_TO_TICKS(2000)); // 2秒間表示
            } else {
                display.drawPngFile(&SD, IMAGE_FILE, 0, 0); // フォールバック
            }
            ESP_LOGI(TAG, "Image displayed successfully");
        }
        else {
            ESP_LOGE(TAG, "Image file not found: %s", IMAGE_FILE);
            
            // エラーメッセージをCanvas経由で表示
            if (canvasInitialized) {
                mainCanvas->fillSprite(TFT_BLACK);
                mainCanvas->setTextColor(TFT_RED);
                mainCanvas->setTextSize(2);
                char errorMsg[64];
                snprintf(errorMsg, sizeof(errorMsg), "File not found: %s", IMAGE_FILE);
                mainCanvas->drawString(errorMsg, 10, 10);
                updateDisplay();
            }
        }
        
        // 4. ファイルアクセスが完了したので、ファイルをクローズ
        SD.close();
    }
    else {
        ESP_LOGE(TAG, "SD card initialization failed");
        
        // エラーメッセージをCanvas経由で表示
        if (canvasInitialized) {
            mainCanvas->fillSprite(TFT_BLACK);
            mainCanvas->setTextColor(TFT_RED);
            mainCanvas->setTextSize(2);
            mainCanvas->drawString("SD Card Init Failed", 10, 10);
            updateDisplay();
        }
    }
    
    // キャンバステストオブジェクトの初期化
    ESP_LOGI(TAG, "Initializing Canvas Test...");
    canvasTest = new CanvasTest(&display);
    if (canvasTest && canvasTest->init()) {
        ESP_LOGI(TAG, "Canvas test initialized successfully");
    }
    else {
        ESP_LOGE(TAG, "Canvas test initialization failed");
        if (canvasTest) {
            delete canvasTest;
            canvasTest = nullptr;
        }
    }
    
    // スクリーントランジションオブジェクトの初期化
    ESP_LOGI(TAG, "Initializing Screen Transition...");
    screenTransition = new ScreenTransition(&display);
    if (screenTransition && screenTransition->init(true)) {
        ESP_LOGI(TAG, "Screen transition initialized successfully");
    }
    else {
        ESP_LOGE(TAG, "Screen transition initialization failed");
        if (screenTransition) {
            delete screenTransition;
            screenTransition = nullptr;
        }
    }
    
    // タッチハンドラの初期化を追加
    ESP_LOGI(TAG, "Initializing touch handler...");
    if (touchHandler.init(&display)) {
        ESP_LOGI(TAG, "Touch handler initialized successfully");
        
        // タッチイベントのコールバックを設定
        touchHandler.setOnTouchStart(onTouchStart);
        touchHandler.setOnTouchEnd(onTouchEnd);
        touchHandler.setOnSwipe(onSwipe);
        
        // スワイプの最小距離を設定（ピクセル単位）
        touchHandler.setMinSwipeDistance(50);
        
        // ButtonManagerの初期化
        buttonManager = new ButtonManager(&display, &touchHandler);
        
        // テストボタンの作成（サイズを調整）
        btnTest = new Button(&display, 10, 350, 100, 40, "テストボタン");
        btnTest->setOnPressed(onTestButtonPressed);
        btnTest->setOnReleased(onTestButtonReleased);
        btnTest->setOnSwipeUp(onButtonSwipeUp);
        btnTest->setOnSwipeDown(onButtonSwipeDown);
        btnTest->setOnSwipeLeft(onButtonSwipeLeft);
        btnTest->setOnSwipeRight(onButtonSwipeRight);
        
        // カスタムスタイルの設定
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
        
        // 停止ボタンの作成（初期は非表示）
        btnCanvasStop = new Button(&display, 450, 350, 80, 40, "Stop Test");
        btnCanvasStop->setOnPressed(onCanvasStopButtonPressed);
        btnCanvasStop->setOnReleased(onCanvasStopButtonReleased);
        btnCanvasStop->setVisible(false); // 初期は非表示
        
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
    }
    else {
        ESP_LOGE(TAG, "Touch handler initialization failed");
    }
    
    // ButtonManagerをCanvas対応に設定
    if (buttonManager) {
        buttonManager->setDrawTarget(mainCanvas); // ★ Canvas描画先を設定
        ESP_LOGI(TAG, "ButtonManager configured for canvas drawing");
    }
    
    // メイン画面の初期描画
    redrawMainScreen();
    
    // ボタンをCanvasに描画
    if (buttonManager) {
        buttonManager->drawButtons();
    }
    
    // 最初の画面更新
    updateDisplay();
}

void loop(void)
{
    // トランジションデモの処理
    if (currentTestMode == TestMode::TRANSITION_DEMO && transitionDemoRunning && screenTransition) {
        static int64_t lastTransitionTime = 0;
        int64_t currentTime = esp_timer_get_time() / 1000;
        
        if (!screenTransition->isRunning()) {
            // 1秒間隔で次のトランジションを開始
            if (currentTime - lastTransitionTime > 1000) {
                if (currentTransitionIndex < transitionTypeCount) {
                    TransitionType currentType = transitionTypes[currentTransitionIndex];
                    
                    ESP_LOGI(TAG, "Starting transition %d: %s",
                             currentTransitionIndex, getTransitionTypeName(currentType));
                    
                    // トランジション設定
                    TransitionConfig config = TransitionConfig::defaultConfig();
                    config.type = currentType;
                    
                    // 次の画面を準備してトランジション開始
                    screenTransition->transition(prepareNextDemoScreen, config);
                    
                    currentTransitionIndex++;
                    lastTransitionTime = currentTime;
                }
                else {
                    // 全てのトランジション完了
                    transitionDemoRunning = false;
                    currentTestMode = TestMode::NORMAL;
                    
                    btnTransitionTest->setLabel("Transition Test");
                    btnTransitionTest->setEnabled(true);
                    btnCanvasStop->setVisible(false);
                    
                    // メイン画面を再描画
                    redrawMainScreen();
                    buttonManager->drawButtons();
                    updateDisplay();
                    
                    ESP_LOGI(TAG, "All transitions completed");
                }
            }
        }
        else {
            // トランジションを更新
            screenTransition->updateTransition();
        }
    }
    
    // メインループ処理
    // USB接続状態を定期的にチェック
    static int64_t last_check = 0;
    int64_t now = esp_timer_get_time() / 1000; // マイクロ秒からミリ秒に変換
    
    if (now - last_check > 5000) { // 5秒ごとにチェック
        last_check = now;
        
        // テスト実行中でない場合のみUSBステータスチェック
        if (currentTestMode == TestMode::NORMAL && SD.isUSBMSCEnabled()) {
            bool connected = SD.isUSBMSCConnected();
            ESP_LOGI(TAG, "USB MSC connection status: %s", connected ? "Connected" : "Disconnected");
            
            // 接続状態をCanvas経由で表示
            char usbStatus[64];
            snprintf(usbStatus, sizeof(usbStatus), "USB Status: %s", 
                     connected ? "Connected" : "Disconnected");
            drawStatusInfo(usbStatus, TFT_WHITE);
        }
    }
    
    // ボタン更新処理（テスト実行中でも有効）
    if (buttonManager) {
        buttonManager->update();
    }
    
    // 通常のタッチ処理（テスト実行中でない場合のみ）
    if (currentTestMode == TestMode::NORMAL && touchHandler.update() &&
        touchHandler.isTouched() && buttonManager) {
        
        const ExtendedTouchPoint &point = touchHandler.getLastPoint();
        
        // Canvas経由でタッチ情報を描画
        drawTouchInfo(point.x, point.y);
        
        // タッチ情報をログに出力
        ESP_LOGI(TAG, "Touch at (%d, %d)", point.x, point.y);
    }
    
    // 再描画が必要な場合に画面を更新
    if (needsRedraw) {
        updateDisplay();
    }
}

void runMainLoop(void *args)
{
    setup();
    for (;;) {
        loop();
        // avoid `The following tasks did not reset the watchdog in time`
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
        // ログ初期化
        esp_log_level_set(TAG, ESP_LOG_INFO);
        
        ESP_LOGI(TAG, "Application starting with Canvas system...");
        initializeTask();
    }
}