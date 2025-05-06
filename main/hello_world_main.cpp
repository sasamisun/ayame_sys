// main/hello_world_main.cpp の修正版（関連部分のみ）

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

// 縦書きと横書きテキスト表示のデモ
void textDisplayDemo()
{
  ESP_LOGI(TAG, "Running text display demo...");

  // 背景をクリア
  // display.fillScreen(TFT_BLACK);

  // 横書きテキスト表示
  TypoWrite horizontalWriter(&display);
  horizontalWriter.setPosition(10, 100);
  horizontalWriter.setArea(400, 200);
  horizontalWriter.setColor(TFT_WHITE);
  horizontalWriter.setBackgroundColor(TFT_BLACK);
  horizontalWriter.setDirection(TextDirection::HORIZONTAL);
  horizontalWriter.setFont(&fonts::lgfxJapanGothic_24);
  horizontalWriter.setFontSize(1.0);

  // 横書きテキスト描画
  horizontalWriter.drawText("これは横書きテキストのデモです。\nM5Paper S3でアドベンチャーゲームを作ります。");

  // 縦書きテキスト表示
  TypoWrite verticalWriter(&display);
  verticalWriter.setPosition(400, 100);
  verticalWriter.setArea(140, 700);
  verticalWriter.setDirection(TextDirection::VERTICAL);
  verticalWriter.setFont(&fonts::lgfxJapanGothic_24);
  verticalWriter.setFontSize(1.0);

  // 縦書きテキスト描画
  verticalWriter.drawText("縦書きの例だよ。いつか、私の夢を叶える。\n特殊記号\n()「」{}[]【】『』（）-=~!?<>_―――");

  ESP_LOGI(TAG, "Text display demo completed");
}

// ファイルフォルダ一覧表示
void listAndDisplayFiles()
{
  // SDカードのルートディレクトリを読み込み
  DirInfo *rootDir = SD.listDir("/");
  if (rootDir)
  {
    // display.fillScreen(TFT_BLACK);
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

// タッチイベントのコールバック関数
void onTouchStart(const ExtendedTouchPoint &point)
{
  ESP_LOGI(TAG, "Touch started at (%d, %d)", point.x, point.y);

  // タッチ開始時の処理
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.setTextSize(1);
  display.setCursor(10, display.height() - 100);
  display.printf("Touch started at (%d, %d)   ", point.x, point.y);
}

void onTouchEnd(const ExtendedTouchPoint &point)
{
  ESP_LOGI(TAG, "Touch ended at (%d, %d)", point.x, point.y);

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

  // テキスト表示例
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.setTextSize(1);
  display.setCursor(10, display.height() - 80);
  display.println("テストボタンが押されました");
}

void onTestButtonReleased(Button *btn)
{
  ESP_LOGI(TAG, "Test button released");
}

// USB MSCボタンコールバック
void onUSBMSCButtonPressed(Button *btn)
{
  ESP_LOGI(TAG, "USB MSC button pressed");
}

void onUSBMSCButtonReleased(Button *btn)
{
  ESP_LOGI(TAG, "USB MSC button released");

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

// スワイプイベントのコールバック関数を定義
void onButtonSwipeUp(Button *btn, SwipeDirection dir)
{
  ESP_LOGI(TAG, "Button swiped up: %s", btn->getLabel());

  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setTextSize(1);
  display.setCursor(10, display.height() - 160);
  display.printf("Button swiped up: %s   ", btn->getLabel());
}

void onButtonSwipeDown(Button *btn, SwipeDirection dir)
{
  ESP_LOGI(TAG, "Button swiped down: %s", btn->getLabel());

  display.setTextColor(TFT_MAGENTA, TFT_BLACK);
  display.setTextSize(1);
  display.setCursor(10, display.height() - 160);
  display.printf("Button swiped down: %s   ", btn->getLabel());
}

void onButtonSwipeLeft(Button *btn, SwipeDirection dir)
{
  ESP_LOGI(TAG, "Button swiped left: %s", btn->getLabel());

  display.setTextColor(TFT_ORANGE, TFT_BLACK);
  display.setTextSize(1);
  display.setCursor(10, display.height() - 160);
  display.printf("Button swiped left: %s   ", btn->getLabel());
}

void onButtonSwipeRight(Button *btn, SwipeDirection dir)
{
  ESP_LOGI(TAG, "Button swiped right: %s", btn->getLabel());

  display.setTextColor(TFT_PINK, TFT_BLACK);
  display.setTextSize(1);
  display.setCursor(10, display.height() - 160);
  display.printf("Button swiped right: %s   ", btn->getLabel());
}

void setup()
{
  ESP_LOGI(TAG, "Initializing M5Paper S3...");
  display.begin();
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

    // タッチキャリブレーションを実行（必要な場合）
    // ESP_LOGI(TAG, "Running touch calibration...");
    // touchHandler.calibrate(TFT_BLACK, TFT_WHITE);

    // ButtonManagerの初期化
    buttonManager = new ButtonManager(&display, &touchHandler);

    // テストボタンの作成
    btnTest = new Button(&display, 10, 350, 150, 50, "テストボタン");
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
    btnUSBMSC = new Button(&display, 170, 350, 150, 50, "Enable USB MSC");
    btnUSBMSC->setOnPressed(onUSBMSCButtonPressed);
    btnUSBMSC->setOnReleased(onUSBMSCButtonReleased);

    // ボタンマネージャーに追加
    buttonManager->addButton(btnTest);
    buttonManager->addButton(btnUSBMSC);

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
  // メインループ処理
  // USB接続状態を定期的にチェックできます
  static int64_t last_check = 0;
  int64_t now = esp_timer_get_time() / 1000; // マイクロ秒からミリ秒に変換

  if (now - last_check > 5000)
  { // 5秒ごとにチェック
    last_check = now;

    if (SD.isUSBMSCEnabled())
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

  // ボタン更新処理
  if (buttonManager)
  {
    buttonManager->update();
  }

  // 既存のタッチ処理部分を修正（拡張機能を使用）
  // ※以下の部分はbuttonManager->update()が行うので、重複する場合は削除または条件分岐で処理
  if (touchHandler.update() && touchHandler.isTouched() &&
      !buttonManager)
  { // ボタンマネージャーがない場合のみ実行

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

    ESP_LOGI(TAG, "Application starting...");
    initializeTask();
  }
}