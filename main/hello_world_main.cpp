// main/hello_world_main.cpp - SimpleTransition Integration

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
#include "SimpleTransition.hpp" // 新しいシンプルトランジション

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
SimpleTransition *simpleTransition = nullptr; // 新しいシンプルトランジション

// アプリケーション用メインキャンバス
M5Canvas *mainCanvas = nullptr;

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
SimpleTransitionType transitionTypes[] = {
    SimpleTransitionType::FADE_IN,
    SimpleTransitionType::SLIDE_DOWN,
    SimpleTransitionType::SLIDE_UP,
    SimpleTransitionType::SLIDE_LEFT,
    SimpleTransitionType::SLIDE_RIGHT,
    SimpleTransitionType::WIPE_DOWN,
    SimpleTransitionType::WIPE_UP,
    SimpleTransitionType::WIPE_LEFT,
    SimpleTransitionType::WIPE_RIGHT,
    SimpleTransitionType::CENTER_OUT,
    SimpleTransitionType::RANDOM_BLOCKS,
    SimpleTransitionType::LINE_SCAN,
    SimpleTransitionType::TYPEWRITER
};

const int transitionTypeCount = sizeof(transitionTypes) / sizeof(transitionTypes[0]);

// トランジションタイプの名前
const char *getTransitionTypeName(SimpleTransitionType type)
{
  switch (type)
  {
  case SimpleTransitionType::FADE_IN:
    return "Fade In";
  case SimpleTransitionType::SLIDE_DOWN:
    return "Slide Down";
  case SimpleTransitionType::SLIDE_UP:
    return "Slide Up";
  case SimpleTransitionType::SLIDE_LEFT:
    return "Slide Left";
  case SimpleTransitionType::SLIDE_RIGHT:
    return "Slide Right";
  case SimpleTransitionType::WIPE_DOWN:
    return "Wipe Down";
  case SimpleTransitionType::WIPE_UP:
    return "Wipe Up";
  case SimpleTransitionType::WIPE_LEFT:
    return "Wipe Left";
  case SimpleTransitionType::WIPE_RIGHT:
    return "Wipe Right";
  case SimpleTransitionType::CENTER_OUT:
    return "Center Out";
  case SimpleTransitionType::RANDOM_BLOCKS:
    return "Random Blocks";
  case SimpleTransitionType::LINE_SCAN:
    return "Line Scan";
  case SimpleTransitionType::TYPEWRITER:
    return "Typewriter";
  default:
    return "Unknown";
  }
}

// デモ画面描画関数（メインキャンバスに描画）
void drawDemoScreen1()
{
  if (!mainCanvas) return;

  mainCanvas->fillSprite(TFT_BLUE);
  mainCanvas->setTextColor(TFT_WHITE);
  mainCanvas->setTextSize(3);
  mainCanvas->setTextDatum(middle_center);
  mainCanvas->drawString("SCREEN 1", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 - 100);

  // 装飾的な図形を追加
  mainCanvas->fillCircle(100, 200, 50, TFT_YELLOW);
  mainCanvas->fillRect(300, 150, 100, 100, TFT_RED);
  mainCanvas->drawTriangle(200, 400, 150, 500, 250, 500, TFT_GREEN);

  // テキスト情報
  mainCanvas->setTextSize(2);
  mainCanvas->drawString("Simple Transition Demo", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 + 50);
  mainCanvas->setTextSize(1);
  mainCanvas->drawString("New System Test", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 + 100);
}

void drawDemoScreen2()
{
  if (!mainCanvas) return;

  mainCanvas->fillSprite(TFT_RED);
  mainCanvas->setTextColor(TFT_WHITE);
  mainCanvas->setTextSize(3);
  mainCanvas->setTextDatum(middle_center);
  mainCanvas->drawString("SCREEN 2", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 - 100);

  // 異なる装飾
  mainCanvas->fillEllipse(TRANSITION_WIDTH / 2, 300, 80, 40, TFT_CYAN);
  mainCanvas->drawRoundRect(150, 450, 200, 80, 20, TFT_MAGENTA);

  // パターン描画
  for (int i = 0; i < 10; i++)
  {
    mainCanvas->drawLine(i * 54, 0, i * 54, TRANSITION_HEIGHT, TFT_DARKGRAY);
  }

  mainCanvas->setTextSize(2);
  mainCanvas->drawString("Next Scene", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 + 50);
  mainCanvas->setTextSize(1);
  mainCanvas->drawString("M5Canvas Powered", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 + 100);
}

void drawDemoScreen3()
{
  if (!mainCanvas) return;

  mainCanvas->fillSprite(TFT_GREEN);
  mainCanvas->setTextColor(TFT_BLACK);
  mainCanvas->setTextSize(3);
  mainCanvas->setTextDatum(middle_center);
  mainCanvas->drawString("SCREEN 3", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 - 100);

  // 複雑なパターン
  for (int y = 0; y < TRANSITION_HEIGHT; y += 40)
  {
    for (int x = 0; x < TRANSITION_WIDTH; x += 40)
    {
      if ((x / 40 + y / 40) % 2 == 0)
      {
        mainCanvas->fillRect(x, y, 40, 40, TFT_DARKGREEN);
      }
    }
  }

  mainCanvas->setTextSize(2);
  mainCanvas->drawString("Final Screen", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 + 50);
  mainCanvas->setTextSize(1);
  mainCanvas->drawString("Transition Complete!", TRANSITION_WIDTH / 2, TRANSITION_HEIGHT / 2 + 100);
}

// 縦書きと横書きテキスト表示のデモ（メインキャンバスに描画）
void drawTextOnMainCanvas()
{
  if (!mainCanvas) return;
  
  ESP_LOGI(TAG, "Drawing text on main canvas...");

  // VLWフォントデータの初期化（例：shipporiフォント）
  if (vlwParser.init(shippori, sizeof(shippori)))
  {
    ESP_LOGI(TAG, "VLW font initialized successfully");

    // TypoWriteでVLWパーサーを使用（メインキャンバスに描画）
    TypoWrite verticalWriter(&display);
    verticalWriter.setVLWParser(&vlwParser); // VLWパーサーを設定
    verticalWriter.setDrawTarget(mainCanvas); // メインキャンバスに描画

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
    verticalWriter.drawText("新しいシンプル\nトランジション\nシステムで\n美しい切り替えが\n可能です。");
  }
  else
  {
    ESP_LOGE(TAG, "Failed to initialize VLW font");

    // エラー表示をメインキャンバスに
    mainCanvas->setTextColor(TFT_RED);
    mainCanvas->setTextSize(1);
    mainCanvas->setCursor(10, 100);
    mainCanvas->println("VLW Font Load Failed");
  }
}

// ファイルフォルダ一覧表示（メインキャンバスに描画）
void drawFileListOnMainCanvas()
{
  if (!mainCanvas) return;
  
  // SDカードのルートディレクトリを読み込み
  DirInfo *rootDir = SD.listDir("/");
  if (rootDir)
  {
    mainCanvas->setTextColor(TFT_WHITE);
    mainCanvas->setTextSize(1);
    mainCanvas->setCursor(10, 10);
    mainCanvas->println("SD Card Files:");

    int y = 30;
    for (size_t i = 0; i < rootDir->count; i++)
    {
      FileInfo *file = &rootDir->files[i];

      // ディレクトリには[DIR]マークを付ける
      if (file->isDirectory)
      {
        mainCanvas->printf("[DIR] %s\n", file->name);
      }
      else
      {
        // ファイルサイズを表示（KB単位）
        float size_kb = file->size / 1024.0f;
        mainCanvas->printf("%s (%.1f KB)\n", file->name, size_kb);
      }

      y += 20;
      if (y > mainCanvas->height() - 20)
      {
        // 画面の下部に達したら表示を止める
        mainCanvas->println("... and more files");
        break;
      }
    }

    // メモリ解放を忘れずに
    SD.freeDirInfo(rootDir);
  }
  else
  {
    mainCanvas->fillSprite(TFT_BLACK);
    mainCanvas->setTextColor(TFT_RED);
    mainCanvas->setTextSize(1);
    mainCanvas->setCursor(10, 10);
    mainCanvas->println("Failed to read SD card directory");
  }
}

// メインキャンバスに通常画面を描画
void drawMainScreen()
{
  if (!mainCanvas) return;
  
  // 背景をクリア
  mainCanvas->fillSprite(TFT_BLACK);
  
  // ファイル一覧を描画
  drawFileListOnMainCanvas();
  
  // テキストを描画
  drawTextOnMainCanvas();
}

// トランジションデモの次の画面を準備
void prepareNextDemoScreen()
{
  static int screenIndex = 0;
  screenIndex = (screenIndex + 1) % 3;

  switch (screenIndex)
  {
  case 0:
    drawDemoScreen1();
    break;
  case 1:
    drawDemoScreen2();
    break;
  case 2:
    drawDemoScreen3();
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

  // タッチ開始時の処理をメインキャンバスに描画
  mainCanvas->setTextColor(TFT_GREEN, TFT_BLACK);
  mainCanvas->setTextSize(1);
  mainCanvas->setCursor(10, mainCanvas->height() - 100);
  mainCanvas->printf("Touch started at (%d, %d)   ", point.x, point.y);
  
  // すぐに表示
  mainCanvas->pushSprite(0, 0);
  
  // ✅ ボタンを再描画
  if (buttonManager) {
    buttonManager->drawButtons();
  }
}

void onTouchEnd(const ExtendedTouchPoint &point)
{
  ESP_LOGI(TAG, "Touch ended at (%d, %d)", point.x, point.y);

  // テスト実行中は通常のタッチ処理をスキップ
  if (currentTestMode != TestMode::NORMAL)
  {
    return;
  }

  // タッチ終了時の処理をメインキャンバスに描画
  mainCanvas->setTextColor(TFT_RED, TFT_BLACK);
  mainCanvas->setTextSize(1);
  mainCanvas->setCursor(10, mainCanvas->height() - 120);
  mainCanvas->printf("Touch ended at (%d, %d)   ", point.x, point.y);
  
  // すぐに表示
  mainCanvas->pushSprite(0, 0);
  
  // ✅ ボタンを再描画
  if (buttonManager) {
    buttonManager->drawButtons();
  }
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

  // スワイプ情報をメインキャンバスに描画
  mainCanvas->setTextColor(TFT_YELLOW, TFT_BLACK);
  mainCanvas->setTextSize(1);
  mainCanvas->setCursor(10, mainCanvas->height() - 140);
  mainCanvas->printf("Swipe: %s   ", dirStr);
  
  // すぐに表示
  mainCanvas->pushSprite(0, 0);
  
  // ✅ ボタンを再描画
  if (buttonManager) {
    buttonManager->drawButtons();
  }
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

  // テキスト表示例をメインキャンバスに
  mainCanvas->setTextColor(TFT_YELLOW, TFT_BLACK);
  mainCanvas->setTextSize(1);
  mainCanvas->setCursor(10, mainCanvas->height() - 80);
  mainCanvas->println("テストボタンが押されました");
  
  // すぐに表示
  mainCanvas->pushSprite(0, 0);
  
  // ✅ ボタンを再描画
  if (buttonManager) {
    buttonManager->drawButtons();
  }
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
      drawMainScreen();
      mainCanvas->pushSprite(0, 0);
      
      // ✅ ボタンを再描画
      if (buttonManager) {
        buttonManager->drawButtons();
      }
    }
  }
  else
  {
    // USB MSCを有効化
    if (SD.enableUSBMSC())
    {
      ESP_LOGI(TAG, "USB MSC enabled");
      btn->setLabel("Disable USB MSC");

      // 情報表示をメインキャンバスに
      mainCanvas->fillSprite(TFT_BLACK);
      mainCanvas->setTextColor(TFT_WHITE, TFT_BLACK);
      mainCanvas->setTextSize(1.5);
      mainCanvas->setCursor(10, 100);
      mainCanvas->println("USB MSC Enabled");
      mainCanvas->println("Connect to PC to access SD card");
      mainCanvas->pushSprite(0, 0);
      
      // ✅ ボタンを再描画
      if (buttonManager) {
        buttonManager->drawButtons();
      }
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
  drawMainScreen();
  mainCanvas->pushSprite(0, 0);
  buttonManager->drawButtons(); // ✅ ボタン再描画追加

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

  if (!simpleTransition)
  {
    ESP_LOGE(TAG, "Simple transition not initialized");
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

  ESP_LOGI(TAG, "Starting simple transition demo...");
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
    simpleTransition->stop();

    currentTestMode = TestMode::NORMAL;
    btnTransitionTest->setLabel("Transition Test");
    btnTransitionTest->setEnabled(true);
    btn->setVisible(false);
    btn->setLabel("Stop Test");

    // 元の画面に戻る
    drawMainScreen();
    mainCanvas->pushSprite(0, 0);
    buttonManager->drawButtons(); // ✅ ボタン再描画追加 // ✅ ボタン再描画追加

    ESP_LOGI(TAG, "Simple transition demo stopped by user");
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
    drawMainScreen();
    mainCanvas->pushSprite(0, 0);
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

  mainCanvas->setTextColor(TFT_CYAN, TFT_BLACK);
  mainCanvas->setTextSize(1);
  mainCanvas->setCursor(10, mainCanvas->height() - 160);
  mainCanvas->printf("Button swiped up: %s   ", btn->getLabel());
  mainCanvas->pushSprite(0, 0);
  
  // ✅ ボタンを再描画
  if (buttonManager) {
    buttonManager->drawButtons();
  }
}

void onButtonSwipeDown(Button *btn, SwipeDirection dir)
{
  ESP_LOGI(TAG, "Button swiped down: %s", btn->getLabel());

  // テスト実行中は処理しない
  if (currentTestMode != TestMode::NORMAL)
  {
    return;
  }

  mainCanvas->setTextColor(TFT_MAGENTA, TFT_BLACK);
  mainCanvas->setTextSize(1);
  mainCanvas->setCursor(10, mainCanvas->height() - 160);
  mainCanvas->printf("Button swiped down: %s   ", btn->getLabel());
  mainCanvas->pushSprite(0, 0);
  
  // ✅ ボタンを再描画
  if (buttonManager) {
    buttonManager->drawButtons();
  }
}

void onButtonSwipeLeft(Button *btn, SwipeDirection dir)
{
  ESP_LOGI(TAG, "Button swiped left: %s", btn->getLabel());

  // テスト実行中は処理しない
  if (currentTestMode != TestMode::NORMAL)
  {
    return;
  }

  mainCanvas->setTextColor(TFT_ORANGE, TFT_BLACK);
  mainCanvas->setTextSize(1);
  mainCanvas->setCursor(10, mainCanvas->height() - 160);
  mainCanvas->printf("Button swiped left: %s   ", btn->getLabel());
  mainCanvas->pushSprite(0, 0);
  
  // ✅ ボタンを再描画
  if (buttonManager) {
    buttonManager->drawButtons();
  }
}

void onButtonSwipeRight(Button *btn, SwipeDirection dir)
{
  ESP_LOGI(TAG, "Button swiped right: %s", btn->getLabel());

  // テスト実行中は処理しない
  if (currentTestMode != TestMode::NORMAL)
  {
    return;
  }

  mainCanvas->setTextColor(TFT_PINK, TFT_BLACK);
  mainCanvas->setTextSize(1);
  mainCanvas->setCursor(10, mainCanvas->height() - 160);
  mainCanvas->printf("Button swiped right: %s   ", btn->getLabel());
  mainCanvas->pushSprite(0, 0);
  
  // ✅ ボタンを再描画
  if (buttonManager) {
    buttonManager->drawButtons();
  }
}

void setup()
{
  ESP_LOGI(TAG, "Initializing M5Paper S3...");
  display.begin();
  display.setEpdMode(lgfx::v1::epd_mode::epd_mode_t::epd_fastest);
  display.setColorDepth(1);
  display.fillScreen(TFT_BLACK);

  // メインキャンバスの初期化
  ESP_LOGI(TAG, "Creating main canvas...");
  mainCanvas = new M5Canvas(&display);
  if (mainCanvas) {
    mainCanvas->setPsram(true); // PSRAM使用
    if (mainCanvas->createSprite(TRANSITION_WIDTH, TRANSITION_HEIGHT)) {
      ESP_LOGI(TAG, "Main canvas created successfully");
    } else {
      ESP_LOGE(TAG, "Failed to create main canvas sprite");
      delete mainCanvas;
      mainCanvas = nullptr;
    }
  } else {
    ESP_LOGE(TAG, "Failed to allocate main canvas");
  }

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

  // シンプルトランジションオブジェクトの初期化
  ESP_LOGI(TAG, "Initializing Simple Transition...");
  simpleTransition = new SimpleTransition(&display);
  if (simpleTransition)
  {
    ESP_LOGI(TAG, "Simple transition initialized successfully");
  }
  else
  {
    ESP_LOGE(TAG, "Simple transition initialization failed");
  }

  // タッチハンドラの初期化を追加
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
    btnTransitionTest = new Button(&display, 340, 350, 100, 40, "Simple Trans");
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

  // メイン画面を初期描画
  if (mainCanvas) {
    drawMainScreen();
    mainCanvas->pushSprite(0, 0);
    
    // ✅ ボタンを再描画（pushSpriteで消されるため）
    if (buttonManager) {
      buttonManager->drawButtons();
    }
  }
}

void loop(void)
{
  // トランジションデモの処理
  if (currentTestMode == TestMode::TRANSITION_DEMO && transitionDemoRunning && simpleTransition)
  {
    // トランジションが実行中でなければ次のトランジションを開始
    if (!simpleTransition->isRunning())
    {
      if (currentTransitionIndex < transitionTypeCount)
      {
        SimpleTransitionType currentType = transitionTypes[currentTransitionIndex];

        ESP_LOGI(TAG, "Starting transition %d: %s",
                 currentTransitionIndex, getTransitionTypeName(currentType));

        // 次の画面を準備
        prepareNextDemoScreen();

        // トランジション設定
        SimpleTransitionConfig config = SimpleTransitionConfig::defaultConfig();
        config.type = currentType;
        config.total_steps = 20; // 20ステップ
        config.clear_before_step = true;

        // トランジション開始
        simpleTransition->start(mainCanvas, config);

        currentTransitionIndex++;
      }
      else
      {
        // 全てのトランジション完了
        transitionDemoRunning = false;
        currentTestMode = TestMode::NORMAL;

        btnTransitionTest->setLabel("Simple Trans");
        btnTransitionTest->setEnabled(true);
        btnCanvasStop->setVisible(false);

        // 元の画面に戻る
        drawMainScreen();
        mainCanvas->pushSprite(0, 0);
        buttonManager->drawButtons(); // ✅ ボタン再描画追加

        ESP_LOGI(TAG, "All simple transitions completed");
      }
    }
    else
    {
      // トランジションを1ステップ実行
      simpleTransition->step();
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

      // 接続状態をメインキャンバスに表示
      if (mainCanvas) {
        mainCanvas->setTextColor(TFT_WHITE, TFT_BLACK);
        mainCanvas->setTextSize(1);
        mainCanvas->setCursor(10, mainCanvas->height() - 20);
        mainCanvas->printf("USB Status: %s    ", connected ? "Connected" : "Disconnected");
        mainCanvas->pushSprite(0, 0);
        
        // ✅ ボタンを再描画
        if (buttonManager) {
          buttonManager->drawButtons();
        }
      }
    }
  }

  // ボタン更新処理（テスト実行中でも有効）
  if (buttonManager)
  {
    buttonManager->update();
  }

  // 通常のタッチ処理（テスト実行中でない場合のみ）
  // ✅ ButtonManagerの更新は完了しているが、汎用タッチイベントも処理する
  if (currentTestMode == TestMode::NORMAL && touchHandler.isTouched())
  {
    const ExtendedTouchPoint &point = touchHandler.getLastPoint();

    // タッチされた位置に円を描画
    touchHandler.drawCircleAtTouch(10, TFT_RED);

    // タッチ情報をログに出力
    ESP_LOGI(TAG, "Touch at (%d, %d)", point.x, point.y);

    // タッチ座標をメインキャンバスに表示
    if (mainCanvas) {
      mainCanvas->setTextColor(TFT_GREEN, TFT_BLACK);
      mainCanvas->setTextSize(1);
      mainCanvas->setCursor(10, mainCanvas->height() - 40);
      mainCanvas->printf("Touch: (%d, %d)     ", point.x, point.y);
      mainCanvas->pushSprite(0, 0);
      
      // ✅ ボタンを再描画
      if (buttonManager) {
        buttonManager->drawButtons();
      }
    }
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

    ESP_LOGI(TAG, "Application starting...");
    initializeTask();
  }
}