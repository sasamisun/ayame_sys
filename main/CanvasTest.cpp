// main/CanvasTest.cpp
// M5Canvasを使用してPSRAMでダブルバッファリングのテストを行うプログラム

#include "CanvasTest.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_random.h"  // ESP32用乱数生成関数を追加

// ログタグの定義
static const char* TAG = "CANVAS_TEST";

// コンストラクタ
CanvasTest::CanvasTest(M5GFX* display)
    : _display(display), _canvas1(nullptr), _canvas2(nullptr), _currentCanvas(0), _initialized(false), _testRunning(false)
{
    ESP_LOGI(TAG, "CanvasTest constructor called");
}

// デストラクタ
CanvasTest::~CanvasTest()
{
    cleanup();
    ESP_LOGI(TAG, "CanvasTest destructor called");
}

// 初期化処理
bool CanvasTest::init()
{
    if (_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }

    if (!_display) {
        ESP_LOGE(TAG, "Display not available");
        return false;
    }

    ESP_LOGI(TAG, "Starting M5Canvas initialization test...");

    // PSRAMの使用可能容量をチェック
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "PSRAM Status - Total: %zu bytes, Free: %zu bytes", psram_total, psram_free);

    // 必要なメモリサイズを計算（540×960×2バイト×2キャンバス）
    size_t canvas_size = CANVAS_WIDTH * CANVAS_HEIGHT * 2; // RGB565 = 2 bytes per pixel
    size_t total_required = canvas_size * 2; // 2つのキャンバス
    
    ESP_LOGI(TAG, "Required memory per canvas: %zu bytes", canvas_size);
    ESP_LOGI(TAG, "Total required memory: %zu bytes", total_required);

    if (psram_free < total_required) {
        ESP_LOGE(TAG, "Insufficient PSRAM memory. Required: %zu, Available: %zu", 
                 total_required, psram_free);
        return false;
    }

    // 最初のキャンバスを作成
    ESP_LOGI(TAG, "Creating first canvas (540x960)...");
    _canvas1 = new M5Canvas(_display);
    if (!_canvas1) {
        ESP_LOGE(TAG, "Failed to allocate Canvas1");
        return false;
    }

    // PSRAMを強制使用に設定
    _canvas1->setPsram(true);
    
    // キャンバス1を作成
    if (!_canvas1->createSprite(CANVAS_WIDTH, CANVAS_HEIGHT)) {
        ESP_LOGE(TAG, "Failed to create Canvas1 sprite");
        delete _canvas1;
        _canvas1 = nullptr;
        return false;
    }
    ESP_LOGI(TAG, "Canvas1 created successfully");

    // 2番目のキャンバスを作成
    ESP_LOGI(TAG, "Creating second canvas (540x960)...");
    _canvas2 = new M5Canvas(_display);
    if (!_canvas2) {
        ESP_LOGE(TAG, "Failed to allocate Canvas2");
        cleanup();
        return false;
    }

    // PSRAMを強制使用に設定
    _canvas2->setPsram(true);
    
    // キャンバス2を作成
    if (!_canvas2->createSprite(CANVAS_WIDTH, CANVAS_HEIGHT)) {
        ESP_LOGE(TAG, "Failed to create Canvas2 sprite");
        cleanup();
        return false;
    }
    ESP_LOGI(TAG, "Canvas2 created successfully");

    // メモリ使用量を再確認
    size_t psram_free_after = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "PSRAM after allocation - Free: %zu bytes (Used: %zu bytes)", 
             psram_free_after, psram_free - psram_free_after);

    // キャンバスの初期設定
    setupCanvases();

    _initialized = true;
    ESP_LOGI(TAG, "M5Canvas test initialization completed successfully!");
    
    return true;
}

// キャンバスの初期設定
void CanvasTest::setupCanvases()
{
    if (!_canvas1 || !_canvas2) return;

    // Canvas1の初期設定（青い背景）
    _canvas1->fillSprite(TFT_BLUE);
    _canvas1->setTextColor(TFT_WHITE);
    _canvas1->setTextSize(2);
    _canvas1->setTextDatum(middle_center);
    _canvas1->drawString("Canvas 1 - Blue Background", CANVAS_WIDTH/2, CANVAS_HEIGHT/2 - 50);
    _canvas1->drawString("PSRAM Test - Buffer A", CANVAS_WIDTH/2, CANVAS_HEIGHT/2);
    
    // 四角形を描画
    _canvas1->fillRect(50, 50, 100, 100, TFT_YELLOW);
    _canvas1->drawRect(200, 50, 100, 100, TFT_WHITE);

    // Canvas2の初期設定（赤い背景）
    _canvas2->fillSprite(TFT_RED);
    _canvas2->setTextColor(TFT_WHITE);
    _canvas2->setTextSize(2);
    _canvas2->setTextDatum(middle_center);
    _canvas2->drawString("Canvas 2 - Red Background", CANVAS_WIDTH/2, CANVAS_HEIGHT/2 - 50);
    _canvas2->drawString("PSRAM Test - Buffer B", CANVAS_WIDTH/2, CANVAS_HEIGHT/2);
    
    // 円を描画
    _canvas2->fillCircle(100, 150, 50, TFT_GREEN);
    _canvas2->drawCircle(250, 150, 50, TFT_WHITE);

    ESP_LOGI(TAG, "Canvas setup completed");
}

// 現在のキャンバスを取得
M5Canvas* CanvasTest::getCurrentCanvas()
{
    if (!_initialized) return nullptr;
    
    return (_currentCanvas == 0) ? _canvas1 : _canvas2;
}

// 非アクティブなキャンバスを取得
M5Canvas* CanvasTest::getBackCanvas()
{
    if (!_initialized) return nullptr;
    
    return (_currentCanvas == 0) ? _canvas2 : _canvas1;
}

// キャンバスを切り替える
void CanvasTest::swapCanvases()
{
    if (!_initialized) return;
    
    _currentCanvas = (_currentCanvas == 0) ? 1 : 0;
    ESP_LOGI(TAG, "Swapped to canvas %d", _currentCanvas);
}

// 現在のキャンバスを画面に表示
void CanvasTest::pushCurrentCanvas()
{
    if (!_initialized || !_display) return;
    
    M5Canvas* current = getCurrentCanvas();
    if (current) {
        // キャンバス全体を画面に転送
        current->pushSprite(0, 0);
        ESP_LOGI(TAG, "Pushed canvas %d to display", _currentCanvas);
    }
}

// 指定したキャンバスを画面に表示
void CanvasTest::pushCanvas(int canvasIndex)
{
    if (!_initialized || !_display) return;
    
    M5Canvas* canvas = (canvasIndex == 0) ? _canvas1 : _canvas2;
    if (canvas) {
        canvas->pushSprite(0, 0);
        ESP_LOGI(TAG, "Pushed canvas %d to display", canvasIndex);
    }
}

// ダブルバッファリングテストを実行
void CanvasTest::runDoubleBufferTest()
{
    if (!_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return;
    }

    ESP_LOGI(TAG, "Starting double buffer test...");
    _testRunning = true;

    // テスト用の描画データ
    uint32_t frame_count = 0;
    int64_t last_time = esp_timer_get_time();
    
    for (int i = 0; i < TEST_FRAME_COUNT && _testRunning; i++) {
        // バックバッファに描画
        M5Canvas* backBuffer = getBackCanvas();
        if (backBuffer) {
            // 背景色を交互に変更（M5GFXで利用可能な色を使用）
            uint32_t bg_color = (i % 2 == 0) ? 0x0010 : 0x8800; // 濃い青と濃い赤
            backBuffer->fillSprite(bg_color);
            
            // フレームカウンタを表示
            backBuffer->setTextColor(TFT_WHITE);
            backBuffer->setTextSize(3);
            backBuffer->setTextDatum(middle_center);
            backBuffer->drawString("Double Buffer Test", CANVAS_WIDTH/2, 100);
            
            char frame_str[64];
            snprintf(frame_str, sizeof(frame_str), "Frame: %lu", frame_count);
            backBuffer->drawString(frame_str, CANVAS_WIDTH/2, 200);
            
            // 動く円を描画
            int circle_x = (CANVAS_WIDTH/2) + (int)(150 * cos(frame_count * 0.1));
            int circle_y = (CANVAS_HEIGHT/2) + (int)(100 * sin(frame_count * 0.1));
            backBuffer->fillCircle(circle_x, circle_y, 30, TFT_YELLOW);
            
            // 時間情報を表示
            int64_t current_time = esp_timer_get_time();
            float fps = 1000000.0f / (current_time - last_time);
            last_time = current_time;
            
            char fps_str[64];
            snprintf(fps_str, sizeof(fps_str), "FPS: %.1f", fps);
            backBuffer->setTextSize(2);
            backBuffer->drawString(fps_str, CANVAS_WIDTH/2, 300);
        }
        
        // フロントバッファとバックバッファを交換
        swapCanvases();
        
        // 新しいフロントバッファを画面に表示
        pushCurrentCanvas();
        
        frame_count++;
        
        // フレームレート調整（約30FPS）
        vTaskDelay(pdMS_TO_TICKS(33));
    }
    
    _testRunning = false;
    ESP_LOGI(TAG, "Double buffer test completed. Total frames: %lu", frame_count);
}

// メモリ使用量テスト
void CanvasTest::testMemoryUsage()
{
    if (!_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return;
    }

    ESP_LOGI(TAG, "=== Memory Usage Test ===");
    
    // PSRAM使用量
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t psram_used = psram_total - psram_free;
    
    ESP_LOGI(TAG, "PSRAM - Total: %zu KB, Used: %zu KB, Free: %zu KB", 
             psram_total/1024, psram_used/1024, psram_free/1024);
    
    // 内部RAM使用量
    size_t heap_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t heap_total = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    size_t heap_used = heap_total - heap_free;
    
    ESP_LOGI(TAG, "Internal RAM - Total: %zu KB, Used: %zu KB, Free: %zu KB", 
             heap_total/1024, heap_used/1024, heap_free/1024);
             
    // キャンバスのメモリ効率を表示
    size_t expected_canvas_size = CANVAS_WIDTH * CANVAS_HEIGHT * 2; // RGB565
    ESP_LOGI(TAG, "Expected canvas size: %zu KB each", expected_canvas_size/1024);
    ESP_LOGI(TAG, "Total expected for 2 canvases: %zu KB", (expected_canvas_size*2)/1024);
    
    // 画面にメモリ情報を表示
    M5Canvas* canvas = getCurrentCanvas();
    if (canvas) {
        canvas->fillSprite(TFT_BLACK);
        canvas->setTextColor(TFT_GREEN);
        canvas->setTextSize(2);
        canvas->setTextDatum(top_left);
        
        int y = 50;
        canvas->drawString("=== Memory Usage Test ===", 20, y); y += 40;
        
        char info[128];
        snprintf(info, sizeof(info), "PSRAM Total: %zu KB", psram_total/1024);
        canvas->drawString(info, 20, y); y += 30;
        
        snprintf(info, sizeof(info), "PSRAM Used:  %zu KB", psram_used/1024);
        canvas->drawString(info, 20, y); y += 30;
        
        snprintf(info, sizeof(info), "PSRAM Free:  %zu KB", psram_free/1024);
        canvas->drawString(info, 20, y); y += 50;
        
        snprintf(info, sizeof(info), "Internal RAM Total: %zu KB", heap_total/1024);
        canvas->drawString(info, 20, y); y += 30;
        
        snprintf(info, sizeof(info), "Internal RAM Used:  %zu KB", heap_used/1024);
        canvas->drawString(info, 20, y); y += 30;
        
        snprintf(info, sizeof(info), "Internal RAM Free:  %zu KB", heap_free/1024);
        canvas->drawString(info, 20, y); y += 50;
        
        snprintf(info, sizeof(info), "Canvas Size: %dx%d", CANVAS_WIDTH, CANVAS_HEIGHT);
        canvas->drawString(info, 20, y); y += 30;
        
        snprintf(info, sizeof(info), "Expected Memory/Canvas: %zu KB", expected_canvas_size/1024);
        canvas->drawString(info, 20, y); y += 30;
        
        // 結果を画面に表示
        pushCurrentCanvas();
    }
}

// 描画パフォーマンステスト
void CanvasTest::testDrawingPerformance()
{
    if (!_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return;
    }

    ESP_LOGI(TAG, "=== Drawing Performance Test ===");
    
    M5Canvas* canvas = getCurrentCanvas();
    if (!canvas) return;
    
    // 描画速度テスト用の関数ポインタ配列
    struct {
        const char* name;
        std::function<void(M5Canvas*)> test_func;
    } performance_tests[] = {
        {"Fill Screen", [](M5Canvas* c) { 
            c->fillSprite(TFT_BLUE); 
        }},
        {"Draw 1000 Lines", [](M5Canvas* c) {
            c->fillSprite(TFT_BLACK);
            for(int i = 0; i < 1000; i++) {
                uint32_t rand1 = esp_random();
                uint32_t rand2 = esp_random();
                c->drawLine(rand1 % CANVAS_WIDTH, (rand1 >> 16) % CANVAS_HEIGHT,
                           rand2 % CANVAS_WIDTH, (rand2 >> 16) % CANVAS_HEIGHT, TFT_WHITE);
            }
        }},
        {"Draw 100 Circles", [](M5Canvas* c) {
            c->fillSprite(TFT_BLACK);
            for(int i = 0; i < 100; i++) {
                uint32_t rand_val = esp_random();
                c->drawCircle(rand_val % CANVAS_WIDTH, (rand_val >> 16) % CANVAS_HEIGHT,
                             (rand_val >> 8) % 50 + 10, TFT_RED);
            }
        }},
        {"Fill 100 Rectangles", [](M5Canvas* c) {
            c->fillSprite(TFT_BLACK);
            for(int i = 0; i < 100; i++) {
                uint32_t rand_val = esp_random();
                c->fillRect(rand_val % (CANVAS_WIDTH-100), (rand_val >> 16) % (CANVAS_HEIGHT-100),
                           50, 50, TFT_GREEN);
            }
        }}
    };
    
    // 各テストを実行
    for (auto& test : performance_tests) {
        int64_t start_time = esp_timer_get_time();
        
        // テスト実行
        test.test_func(canvas);
        
        int64_t end_time = esp_timer_get_time();
        float duration_ms = (end_time - start_time) / 1000.0f;
        
        ESP_LOGI(TAG, "%s: %.2f ms", test.name, duration_ms);
        
        // 結果を画面に表示
        canvas->setTextColor(TFT_YELLOW);
        canvas->setTextSize(1.5);
        canvas->setTextDatum(bottom_center);
        
        char result[64];
        snprintf(result, sizeof(result), "%s: %.2f ms", test.name, duration_ms);
        canvas->drawString(result, CANVAS_WIDTH/2, CANVAS_HEIGHT - 50);
        
        pushCurrentCanvas();
        vTaskDelay(pdMS_TO_TICKS(2000)); // 2秒間表示
    }
}

// テスト停止
void CanvasTest::stopTest()
{
    _testRunning = false;
    ESP_LOGI(TAG, "Test stop requested");
}

// クリーンアップ処理
void CanvasTest::cleanup()
{
    ESP_LOGI(TAG, "Cleaning up canvases...");
    
    if (_canvas1) {
        _canvas1->deleteSprite();
        delete _canvas1;
        _canvas1 = nullptr;
        ESP_LOGI(TAG, "Canvas1 deleted");
    }
    
    if (_canvas2) {
        _canvas2->deleteSprite();
        delete _canvas2;
        _canvas2 = nullptr;
        ESP_LOGI(TAG, "Canvas2 deleted");
    }
    
    _initialized = false;
    _currentCanvas = 0;
    _testRunning = false;
    
    // メモリ使用量を確認
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "PSRAM after cleanup: %zu bytes free", psram_free);
}

// 初期化状態の確認
bool CanvasTest::isInitialized() const
{
    return _initialized;
}

// テスト実行状態の確認
bool CanvasTest::isTestRunning() const
{
    return _testRunning;
}