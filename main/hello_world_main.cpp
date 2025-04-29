// main/hello_world_main.cpp
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <M5GFX.h>
#include "SDcard.hpp"

// ログタグの定義
static const char *TAG = "APP_MAIN";

M5GFX display;
TaskHandle_t g_handle = nullptr;

// 画像ファイルの定数定義
const char *IMAGE_FILE = "tes.png";

// ファイルフォルダ一覧表示
void listAndDisplayFiles()
{
    // SDカードのルートディレクトリを読み込み
    DirInfo* rootDir = SD.listDir("/");
    if (rootDir) {
        //display.fillScreen(TFT_BLACK);
        display.setTextColor(TFT_WHITE);
        display.setTextSize(1);
        display.setCursor(10, 10);
        display.println("SD Card Files:");
        
        int y = 30;
        for (size_t i = 0; i < rootDir->count; i++) {
            FileInfo* file = &rootDir->files[i];
            
            // ディレクトリには[DIR]マークを付ける
            if (file->isDirectory) {
                display.printf("[DIR] %s\n", file->name);
            } else {
                // ファイルサイズを表示（KB単位）
                float size_kb = file->size / 1024.0f;
                display.printf("%s (%.1f KB)\n", file->name, size_kb);
            }
            
            y += 20;
            if (y > display.height() - 20) {
                // 画面の下部に達したら表示を止める
                display.println("... and more files");
                break;
            }
        }
        
        // メモリ解放を忘れずに
        SD.freeDirInfo(rootDir);
    } else {
        display.fillScreen(TFT_BLACK);
        display.setTextColor(TFT_RED);
        display.setTextSize(1);
        display.setCursor(10, 10);
        display.println("Failed to read SD card directory");
    }
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
//      display.drawBmpFile(&SD, IMAGE_FILE, 0, 0);
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

    listAndDisplayFiles();

    // 4. ファイルアクセスが完了したので、ファイルをクローズ
    SD.close();

    // 5. USB MSCを有効化
    ESP_LOGI(TAG, "Enabling USB MSC...");
    if (SD.enableUSBMSC())
    {
      ESP_LOGI(TAG, "USB MSC enabled successfully");

      // 情報をディスプレイに表示
      display.setTextColor(TFT_WHITE, TFT_BLACK);
      display.setTextSize(2);
      display.setCursor(10, display.height() - 60);
      display.println("USB MSC Enabled");
      display.println("Connect to PC to access SD card");
    }
    else
    {
      ESP_LOGE(TAG, "Failed to enable USB MSC");

      // エラーメッセージを表示
      display.setTextColor(TFT_RED, TFT_BLACK);
      display.setTextSize(2);
      display.setCursor(10, display.height() - 60);
      display.println("USB MSC Failed");
    }
  }
  else
  {
    ESP_LOGE(TAG, "SD card initialization failed");

    // main/hello_world_main.cpp (続き)
    display.setTextColor(TFT_RED);
    display.setTextSize(2);
    display.setCursor(10, 10);
    display.println("SD Card Init Failed");
  }
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