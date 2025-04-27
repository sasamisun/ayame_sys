// hello_world_main.cpp

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "esp_task_wdt.h"
#include <M5GFX.h>
#include "SDcard.hpp"

M5GFX display;
TaskHandle_t g_handle = nullptr;

//bg2.bmp card.png testimg16.png testimgful.png

void setup() {
  display.begin();
  
  // SDカードの初期化
  if (SD.init()) {
    // 画像を開く
    if (SD.open("card.png")) {  // /sdcard/ プレフィックスは自動的に追加される
      // 画像を描画
      display.drawPngFile(&SD, "card.png", 0, 0);
      SD.close();
    }
  }
}

void loop(void)
{
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
    //
    initializeTask();
  }
}