// main/hello_world_main.cpp - Complete External Canvas System

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
#include "ScreenTransition.hpp" // 完全外部キャンバス専用版

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
Button *btnTransitionTest = nullptr; // トランジションテスト用ボタン
Button *btnCanvasStop = nullptr;

VLWFontParser vlwParser;
CanvasTest *canvasTest = nullptr;
ScreenTransition *screenTransition = nullptr; // 完全外部キャンバス専用版トランジションオブジェクト

// メインキャンバス（ゲームシステム用 - ScreenTransitionと共有）
M5Canvas *mainCanvas = nullptr;
M5Canvas *subCanvas = nullptr;

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
    TransitionType::CIRCLE_SHRINK,
    TransitionType::PIXELATE,
    TransitionType::VENETIAN_BLIND,
    TransitionType::CHECKERBOARD,
    TransitionType::SPIRAL,
    TransitionType::RIPPLE,
    TransitionType::MOSAIC,
    TransitionType::PAGE_TURN
};

const int transitionTypeCount = sizeof(transitionTypes) / sizeof(transitionTypes[0]);

// トランジションタイプの名前を取得
const char *getTransitionTypeName(TransitionType type)
{
  switch (type)
  {
  case TransitionType::FADE_BLACK: return "Fade Black";
  case TransitionType::FADE_WHITE: return "Fade White";
  case TransitionType::SLIDE_LEFT: return "Slide Left";
  case TransitionType::SLIDE_RIGHT: return "Slide Right";
  case TransitionType::SLIDE_UP: return "Slide Up";
  case TransitionType::SLIDE_DOWN: return "Slide Down";
  case TransitionType::WIPE_LEFT: return "Wipe Left";
  case TransitionType::WIPE_RIGHT: return "Wipe Right";
  case TransitionType::CIRCLE_EXPAND: return "Circle Expand";
  case TransitionType::CIRCLE_SHRINK: return "Circle Shrink";
  case TransitionType::PIXELATE: return "Pixelate";
  case TransitionType::VENETIAN_BLIND: return "Venetian Blind";
  default: return "Unknown";
  }
}

// デモ画面描画関数群
void drawDemoScreen1(M5Canvas *canvas)
{
  if (!canvas) return;

  canvas->fillSprite(TFT_BLUE);
  canvas->setTextColor(TFT_WHITE);
  canvas->setTextSize(3);
  canvas->setTextDatum(middle_center);
  canvas->drawString("SCREEN 1", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 - 100);

  // 装飾的な図形を追加
  canvas->fillCircle(100, 200, 50, TFT_YELLOW);
  canvas->fillRect(300, 150, 100, 100, TFT_RED);
  canvas->drawTriangle(200, 400, 150, 500, 250, 500, TFT_GREEN);

  // テキスト情報
  canvas->setTextSize(2);
  canvas->drawString("Adventure Game Demo", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 + 50);
  canvas->setTextSize(1);
  canvas->drawString("Zero Memory Transition!", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 + 100);
  
  ESP_LOGI(TAG, "Demo screen 1 drawn to canvas");
}

void drawDemoScreen2(M5Canvas *canvas)
{
  if (!canvas) return;

  canvas->fillSprite(TFT_RED);
  canvas->setTextColor(TFT_WHITE);
  canvas->setTextSize(3);
  canvas->setTextDatum(middle_center);
  canvas->drawString("SCREEN 2", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 - 100);

  // 異なる装飾
  canvas->fillEllipse(TRANSITION_WIDTH / 2, 300, 80, 40, TFT_CYAN);
  canvas->drawRoundRect(150, 450, 200, 80, 20, TFT_MAGENTA);

  // パターン描画
  for (int i = 0; i < 10; i++)
  {
    canvas->drawLine(i * 54, 0, i * 54, TRANSITION_HEIGHT, TFT_DARKGRAY);
  }

  canvas->setTextSize(2);
  canvas->drawString("Next Scene", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 + 50);
  canvas->setTextSize(1);
  canvas->drawString("Maximum Memory Saved!", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 + 100);
  
  ESP_LOGI(TAG, "Demo screen 2 drawn to canvas");
}

void drawDemoScreen3(M5Canvas *canvas)
{
  if (!canvas) return;

  canvas->fillSprite(TFT_GREEN);
  canvas->setTextColor(TFT_BLACK);
  canvas->setTextSize(3);
  canvas->setTextDatum(middle_center);
  canvas->drawString("SCREEN 3", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 - 100);

  // 複雑なパターン
  for (int y = 0; y < TRANSITION_HEIGHT; y += 40)
  {
    for (int x = 0; x < TRANSITION_WIDTH; x += 40)
    {
      if ((x / 40 + y / 40) % 2 == 0)
      {
        canvas->fillRect(x, y, 40, 40, TFT_DARKGREEN);
      }
    }
  }

  canvas->setTextSize(2);
  canvas->drawString("Final Screen", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 + 50);
  canvas->setTextSize(1);
  canvas->drawString("External Canvas Only!", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 + 100);
  
  ESP_LOGI(TAG, "Demo screen 3 drawn to canvas");
}

// メインキャンバスの作成
bool createMainCanvases()
{
  ESP_LOGI(TAG, "Creating main canvases for external-only transition system...");
  
  // PSRAMの使用可能容量をチェック
  size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  size_t canvas_size = TRANSITION_WIDTH * TRANSITION_HEIGHT * 2; // RGB565 = 2 bytes per pixel
  size_t total_required = canvas_size * 2; // 2つのキャンバス（ScreenTransitionは内部キャンバスなし）
  
  ESP_LOGI(TAG, "Required memory: %zu bytes, Available PSRAM: %zu bytes", total_required, psram_free);
  ESP_LOGI(TAG, "ScreenTransition internal memory: 0 bytes (External Canvas Only)");

  if (psram_free < total_required) {
    ESP_LOGE(TAG, "Insufficient PSRAM memory for main canvases");
    return false;
  }

  // メインキャンバス作成
  mainCanvas = new M5Canvas(&display);
  if (!mainCanvas) {
    ESP_LOGE(TAG, "Failed to allocate main canvas");
    return false;
  }
  
  mainCanvas->setPsram(true);
  if (!mainCanvas->createSprite(TRANSITION_WIDTH, TRANSITION_HEIGHT)) {
    ESP_LOGE(TAG, "Failed to create main canvas sprite");
    delete mainCanvas;
    mainCanvas = nullptr;
    return false;
  }

  // サブキャンバス作成
  subCanvas = new M5Canvas(&display);
  if (!subCanvas) {
    ESP_LOGE(TAG, "Failed to allocate sub canvas");
    delete mainCanvas;
    mainCanvas = nullptr;
    return false;
  }
  
  subCanvas->setPsram(true);
  if (!subCanvas->createSprite(TRANSITION_WIDTH, TRANSITION_HEIGHT)) {
    ESP_LOGE(TAG, "Failed to create sub canvas sprite");
    delete mainCanvas;
    delete subCanvas;
    mainCanvas = nullptr;
    subCanvas = nullptr;
    return false;
  }

  ESP_LOGI(TAG, "Main canvases created successfully");
  ESP_LOGI(TAG, "Total memory usage: %zu bytes (External Canvas Only System)", total_required);
  
  // 初期化
  mainCanvas->fillSprite(TFT_BLACK);
  subCanvas->fillSprite(TFT_BLACK);
  
  return true;
}

// メインキャンバスのクリーンアップ
void cleanupMainCanvases()
{
  ESP_LOGI(TAG, "Cleaning up main canvases...");
  
  if (mainCanvas) {
    mainCanvas->deleteSprite();
    delete mainCanvas;
    mainCanvas = nullptr;
  }
  
  if (subCanvas) {
    subCanvas->deleteSprite();
    delete subCanvas;
    subCanvas = nullptr;
  }
  
  ESP_LOGI(TAG, "Main canvases cleanup completed");
}

// 縦書きと横書きテキスト表示のデモ
void textDisplayDemo()
{
  ESP_LOGI(TAG, "Starting VLW font demo...");

  // VLWフォントデータの初期化（例：shipporiフォント）
  if (vlwParser.init(shippori, sizeof(shippori)))
  {
    ESP_LOGI(TAG, "VLW font initialized successfully");

    // フォント情報をデバッグ出力
    vlwParser.debugPrintFontInfo();

    // TypoWriteでVLWパーサーを使用
    TypoWrite verticalWriter(&display);
    verticalWriter.setVLWParser(&vlwParser); // VLWパーサーを設定

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

    // テキスト描画
    verticalWriter.drawText("完全外部\nキャンバス\n専用システム\nでメモリを\n最大限節約\nしましたにゃ！");
  }
  else
  {
    ESP_LOGE(TAG, "Failed to initialize VLW font");

    // エラー表示
    display.setTextColor(TFT_RED);
    display.setTextSize(1);
    display.setCursor(10, 100);
    display.println("VLW Font Load Failed");
  }
}

// ファイルフォルダ一覧表示
void listAndDisplayFiles()
{
  // SDカードのルートディレクトリを読み込み
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

      // ディレクトリには[DIR]マークを付ける
      if (file->isDirectory)
      {
        display.printf("[DIR] %s\n", file->name);
      }
      else
      {
        // ファイルサイズを表示（KB単位）
        float size_kb = file->size / 1024.0f;
        display.printf("%s (%.1f KB)\n", file->name, size_kb);
      }

      y += 20;
      if (y > display.height() - 20)
      {
        // 画面の下部に達したら表示を止める
        display.println("... and more files");
        break;
      }
    }

    // メモリ解放を忘れずに
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

  // テスト実行中は通常のタッチ処理をスキップ
  if (currentTestMode != TestMode::NORMAL)
  {
    return;
  }

  // タッチ開始時の処理
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.setTextSize(1);
  display.setCursor(10, display.height() - 100);
  display.printf("Touch started at (%d, %d)   ", point.x, point.y);
}

void onTouchEnd(const ExtendedTouchPoint &point)
{
  ESP_LOGI(TAG, "Touch ended at (%d, %d)", point.x, point.y);

  // テスト実行中は通常のタッチ処理をスキップ
  if (currentTestMode != TestMode::NORMAL)
  {
    return;
  }

  // タッチ終了時の処理
  display.setTextColor(TFT_RED, TFT_BLACK);
  display.setTextSize(1);
  display.setCursor(10, display.height() - 120);
  display.printf("Touch ended at (%d, %d)   ", point.x, point.y);
}

void onSwipe(SwipeDirection direction, const ExtendedTouchPoint &start, const ExtendedTouchPoint &end)
{
  // スワイプ方向を文字列に変換
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

  // テスト実行中は通常のスワイプ処理をスキップ
  if (currentTestMode != TestMode::NORMAL)
  {
    return;
  }

  // スワイプ情報を表示
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

  // テスト実行中は処理しない
  if (currentTestMode != TestMode::NORMAL)
  {
    return;
  }

  // テキスト表示例
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

  // テスト実行中は処理しない
  if (currentTestMode != TestMode::NORMAL)
  {
    return;
  }

  // USB MSCの切り替え
  if (SD.isUSBMSCEnabled())
  {
    // USB MSCを無効化
    if (SD.disableUSBMSC())
    {
      ESP_LOGI(TAG, "USB MSC disabled");
      btn->setLabel("Enable USB MSC");

      // 再度ファイル一覧を表示
      listAndDisplayFiles();
    }
  }
  else
  {
    // USB MSCを有効化
    if (SD.enableUSBMSC())
    {
      ESP_LOGI(TAG, "USB MSC enabled");
      btn->setLabel("Disable USB MSC");

      // 情報表示
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

  // テスト実行中は処理しない
  if (currentTestMode != TestMode::NORMAL)
  {
    return;
  }

  if (!canvasTest)
  {
    ESP_LOGE(TAG, "Canvas test not initialized");
    return;
  }

  // キャンバステストの実行
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_CYAN);
  display.setTextSize(2);
  display.setCursor(10, 50);
  display.println("Starting Canvas Tests...");
  display.println("Please wait...");

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

  // 元の画面に戻る
  display.fillScreen(TFT_BLACK);
  listAndDisplayFiles();
  textDisplayDemo();
  buttonManager->drawButtons();

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
  if (currentTestMode != TestMode::NORMAL)
  {
    return;
  }

  if (!screenTransition || !mainCanvas || !subCanvas)
  {
    ESP_LOGE(TAG, "Screen transition or canvases not initialized");
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

  ESP_LOGI(TAG, "Starting external canvas only transition demo...");

  // 最初の画面をメインキャンバスに準備
  drawDemoScreen1(mainCanvas);

  ESP_LOGI(TAG, "External canvas only transition demo started");
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
    // トランジションデモ停止
    transitionDemoRunning = false;
    screenTransition->stopTransition();

    currentTestMode = TestMode::NORMAL;
    btnTransitionTest->setLabel("Transition Test");
    btnTransitionTest->setEnabled(true);
    btn->setVisible(false);
    btn->setLabel("Stop Test");

    // 元の画面に戻る
    display.fillScreen(TFT_BLACK);
    listAndDisplayFiles();
    textDisplayDemo();
    buttonManager->drawButtons();

    ESP_LOGI(TAG, "External canvas only transition demo stopped by user");
  }
  else if (canvasTest && canvasTest->isTestRunning())
  {
    // キャンバステスト停止
    canvasTest->stopTest();

    currentTestMode = TestMode::NORMAL;
    btnCanvasTest->setLabel("Canvas Test");
    btnCanvasTest->setEnabled(true);
    btn->setVisible(false);

    // 元の画面に戻る
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

  // テスト実行中は処理しない
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

  // テスト実行中は処理しない
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

  // テスト実行中は処理しない
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

  // テスト実行中は処理しない
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
  ESP_LOGI(TAG, "Initializing M5Paper S3 with External Canvas Only System...");
  display.begin();
  display.setEpdMode(lgfx::v1::epd_mode::epd_mode_t::epd_fastest);
  display.setColorDepth(8);
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

      // 3. 画像を読み込んで表示
      display.drawPngFile(&SD, IMAGE_FILE, 0, 0);
      ESP_LOGI(TAG, "Image displayed successfully");
    }
    else
    {
      ESP_LOGE(TAG, "Image file not found: %s", IMAGE_FILE);

      // エラーメッセージを表示
      display.setTextColor(TFT_RED);
      display.setTextSize(2);
      display.setCursor(10, 10);
      display.printf("File not found: %s", IMAGE_FILE);
    }
    display.fillScreen(TFT_BLACK);
    listAndDisplayFiles();

    // 4. ファイルアクセスが完了したので、ファイルをクローズ
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

  // 5. メインキャンバスの作成
  ESP_LOGI(TAG, "Creating main canvases for external-only transition system...");
  if (!createMainCanvases())
  {
    ESP_LOGE(TAG, "Failed to create main canvases");
    // 続行するが機能は制限される
  }

  // 6. キャンバステストオブジェクトの初期化
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

  // 7. スクリーントランジションオブジェクトの初期化（完全外部キャンバス専用版）
  ESP_LOGI(TAG, "Initializing External Canvas Only Screen Transition...");
  screenTransition = new ScreenTransition(&display);
  
  if (screenTransition && mainCanvas && subCanvas)
  {
    // 外部キャンバス必須初期化
    if (screenTransition->init(mainCanvas, subCanvas))
    {
      ESP_LOGI(TAG, "External canvas only screen transition initialized successfully");
      ESP_LOGI(TAG, "ScreenTransition internal memory usage: 0 bytes");
    }
    else
    {
      ESP_LOGE(TAG, "External canvas only screen transition initialization failed");
      delete screenTransition;
      screenTransition = nullptr;
    }
  }
  else
  {
    ESP_LOGE(TAG, "Cannot initialize screen transition - missing canvases or object");
    if (screenTransition)
    {
      delete screenTransition;
      screenTransition = nullptr;
    }
  }

  // 8. タッチハンドラの初期化を追加
  ESP_LOGI(TAG, "Initializing touch handler...");
  if (touchHandler.init(&display))
  {
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

    // ボタンを描画
    buttonManager->drawButtons();
  }
  else
  {
    ESP_LOGE(TAG, "Touch handler initialization failed");
  }

  textDisplayDemo();
  
  ESP_LOGI(TAG, "Setup completed with External Canvas Only system!");
}

void loop(void)
{
  // トランジションデモの処理（完全外部キャンバス版）
  if (currentTestMode == TestMode::TRANSITION_DEMO && transitionDemoRunning && screenTransition)
  {
    static int64_t lastTransitionTime = 0;
    int64_t currentTime = esp_timer_get_time() / 1000;

    if (!screenTransition->isRunning())
    {
      // 5秒間隔で次のトランジションを開始
      if (currentTime - lastTransitionTime > 5000)
      {
        if (currentTransitionIndex < transitionTypeCount)
        {
          TransitionType currentType = transitionTypes[currentTransitionIndex];

          ESP_LOGI(TAG, "Starting external canvas only transition %d: %s",
                   currentTransitionIndex, getTransitionTypeName(currentType));

          // トランジション設定
          TransitionConfig config = TransitionConfig::defaultConfig();
          config.type = currentType;
          config.step_delay_ms = 50;

          // 完全外部キャンバス専用版トランジションを実行
          // mainCanvas(source) → subCanvas(target)にキャンバスを切り替え
          screenTransition->setCanvases(mainCanvas, subCanvas);
          screenTransition->executeTransition(prepareNextDemoScreen, config);

          currentTransitionIndex++;
          lastTransitionTime = currentTime;

          // 次回のためにキャンバスを交換
          M5Canvas* temp = mainCanvas;
          mainCanvas = subCanvas;
          subCanvas = temp;
        }
        else
        {
          // 全てのトランジション完了
          transitionDemoRunning = false;
          currentTestMode = TestMode::NORMAL;

          btnTransitionTest->setLabel("Transition Test");
          btnTransitionTest->setEnabled(true);
          btnCanvasStop->setVisible(false);

          // 元の画面に戻る
          display.fillScreen(TFT_BLACK);
          listAndDisplayFiles();
          textDisplayDemo();
          buttonManager->drawButtons();

          ESP_LOGI(TAG, "All external canvas only transitions completed");
        }
      }
    }
    else
    {
      // トランジションを更新
      screenTransition->updateTransition();
    }
  }

  // メインループ処理
  // USB接続状態を定期的にチェックできます
  static int64_t last_check = 0;
  int64_t now = esp_timer_get_time() / 1000; // マイクロ秒からミリ秒に変換

  if (now - last_check > 5000)
  { // 5秒ごとにチェック
    last_check = now;

    // テスト実行中でない場合のみUSBステータスチェック
    if (currentTestMode == TestMode::NORMAL && SD.isUSBMSCEnabled())
    {
      bool connected = SD.isUSBMSCConnected();
      ESP_LOGI(TAG, "USB MSC connection status: %s", connected ? "Connected" : "Disconnected");

      // 接続状態をディスプレイに表示
      display.setTextColor(TFT_WHITE, TFT_BLACK);
      display.setTextSize(1);
      display.setCursor(10, display.height() - 20);
      display.printf("USB Status: %s    ", connected ? "Connected" : "Disconnected");
    }
  }

  // ボタン更新処理（テスト実行中でも有効）
  if (buttonManager)
  {
    buttonManager->update();
  }

  // 通常のタッチ処理（テスト実行中でない場合のみ）
  if (currentTestMode == TestMode::NORMAL && touchHandler.update() &&
      touchHandler.isTouched() && !buttonManager)
  {
    // ボタンマネージャーがない場合のみ実行

    const ExtendedTouchPoint &point = touchHandler.getLastPoint();

    // タッチされた位置に円を描画
    touchHandler.drawCircleAtTouch(10, TFT_RED);

    // タッチ情報をログに出力
    ESP_LOGI(TAG, "Touch at (%d, %d)", point.x, point.y);

    // タッチ座標を画面に表示
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

    ESP_LOGI(TAG, "Application starting with External Canvas Only System...");
    initializeTask();
  }
}