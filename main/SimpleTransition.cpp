// main/SimpleTransition.cpp - E-Paper最適化版（激速化！）
// 電子ペーパーの特性に最適化した超高速トランジションシステムにゃ！

#include "SimpleTransition.hpp"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <algorithm>
#include <cmath>

// ログタグ
static const char* TAG = "EPAPER_TRANSITION";

/**
 * @brief E-Paper最適化の定数
 */
static const int EPAPER_BLOCK_SIZE = 128;           // 大きなブロック単位で処理
static const int EPAPER_OPTIMAL_STEPS = 6;         // E-Paperに最適なステップ数
static const int EPAPER_FAST_STEPS = 4;            // 高速モード用ステップ数
static const int EPAPER_SLOW_STEPS = 8;            // 演出用ステップ数

/**
 * @brief コンストラクタ
 */
SimpleTransition::SimpleTransition(M5GFX* display)
    : _display(display), _mainCanvas(nullptr), _initialized(false), _use_psram(true),
      _isActive(false), _type(SimpleTransitionType::NONE), _currentStep(0), _totalSteps(0),
      _onComplete(nullptr), _onStep(nullptr)
{
    ESP_LOGI(TAG, "E-Paper最適化SimpleTransition constructor");
}

/**
 * @brief デストラクタ
 */
SimpleTransition::~SimpleTransition()
{
    if (_mainCanvas) {
        _mainCanvas->deleteSprite();
        delete _mainCanvas;
        _mainCanvas = nullptr;
        ESP_LOGI(TAG, "Main canvas deleted");
    }
    ESP_LOGI(TAG, "E-Paper最適化SimpleTransition destructor completed");
}

/**
 * @brief 初期化処理
 */
bool SimpleTransition::init(bool use_psram)
{
    if (_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }
    
    if (!_display) {
        ESP_LOGE(TAG, "Display not available");
        return false;
    }
    
    _use_psram = use_psram;
    ESP_LOGI(TAG, "Initializing E-Paper最適化SimpleTransition with PSRAM: %s", 
             _use_psram ? "enabled" : "disabled");
    
    // PSRAMメモリチェック
    if (_use_psram) {
        size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t canvas_size = SIMPLE_TRANSITION_WIDTH * SIMPLE_TRANSITION_HEIGHT * 2;
        
        ESP_LOGI(TAG, "PSRAM check - Free: %zu KB, Required: %zu KB", 
                 psram_free / 1024, canvas_size / 1024);
        
        if (psram_free < canvas_size) {
            ESP_LOGE(TAG, "Insufficient PSRAM memory. Required: %zu, Available: %zu",
                     canvas_size, psram_free);
            return false;
        }
    }
    
    // メインキャンバス作成
    ESP_LOGI(TAG, "Creating main canvas...");
    _mainCanvas = new M5Canvas(_display);
    if (!_mainCanvas) {
        ESP_LOGE(TAG, "Failed to allocate main canvas");
        return false;
    }
    
    if (_use_psram) {
        _mainCanvas->setPsram(true);
    }
    
    if (!_mainCanvas->createSprite(SIMPLE_TRANSITION_WIDTH, SIMPLE_TRANSITION_HEIGHT)) {
        ESP_LOGE(TAG, "Failed to create main sprite");
        delete _mainCanvas;
        _mainCanvas = nullptr;
        return false;
    }
    
    _initialized = true;
    _isActive = false;
    
    ESP_LOGI(TAG, "E-Paper最適化SimpleTransition initialized successfully! ⚡");
    return true;
}

/**
 * @brief トランジション開始
 */
bool SimpleTransition::startTransition(SimpleTransitionType type, int steps)
{
    if (!_initialized || !_mainCanvas) {
        ESP_LOGE(TAG, "Not initialized");
        return false;
    }
    
    if (_isActive) {
        ESP_LOGW(TAG, "Transition already active, stopping current transition");
        stop();
    }
    
    // E-Paper最適化：ステップ数を調整
    _type = type;
    _totalSteps = std::max(1, std::min(steps, EPAPER_SLOW_STEPS));  // 最大8ステップに制限
    _currentStep = 0;
    _isActive = true;
    
    ESP_LOGI(TAG, "Starting E-Paper最適化transition: type=%d, steps=%d", 
             static_cast<int>(type), _totalSteps);
    
    // 即座に表示する場合
    if (type == SimpleTransitionType::NONE) {
        showImmediate();
        _isActive = false;
        if (_onComplete) {
            _onComplete();
        }
        return true;
    }
    
    return true;
}

/**
 * @brief トランジション更新
 */
bool SimpleTransition::update()
{
    if (!_isActive || !_mainCanvas) {
        return false;
    }
    
    // ステップ進行コールバック実行
    if (_onStep) {
        _onStep(_currentStep, _totalSteps);
    }
    
    // E-Paper最適化：効果に応じた描画処理
    switch (_type) {
        case SimpleTransitionType::FADE_IN:
            drawFadeInStepOptimized();
            break;
            
        case SimpleTransitionType::SLIDE_LEFT:
        case SimpleTransitionType::SLIDE_RIGHT:
        case SimpleTransitionType::SLIDE_UP:
        case SimpleTransitionType::SLIDE_DOWN:
            drawSlideStepOptimized();
            break;
            
        case SimpleTransitionType::WIPE_HORIZONTAL:
        case SimpleTransitionType::WIPE_VERTICAL:
            drawWipeStepOptimized();
            break;
            
        case SimpleTransitionType::REVEAL_CENTER:
            drawRevealCenterStepOptimized();
            break;
            
        case SimpleTransitionType::REVEAL_CORNER:
            drawRevealCornerStepOptimized();
            break;
            
        default:
            showImmediate();
            _isActive = false;
            if (_onComplete) {
                _onComplete();
            }
            return false;
    }
    
    // 次のステップに進む
    _currentStep++;
    
    // 完了チェック
    if (_currentStep >= _totalSteps) {
        _isActive = false;
        showImmediate();  // 最終画面を確実に表示
        
        if (_onComplete) {
            _onComplete();
        }
        
        ESP_LOGI(TAG, "E-Paper最適化transition completed! ⚡");
        return false;
    }
    
    return true;
}

/**
 * @brief E-Paper最適化フェードイン描画
 * 段階的な矩形表示で高速化
 */
void SimpleTransition::drawFadeInStepOptimized()
{
    float progress = static_cast<float>(_currentStep) / static_cast<float>(_totalSteps - 1);
    progress = std::min(1.0f, progress);
    
    ESP_LOGD(TAG, "E-Paper optimized fade step %d/%d (%.1f%%)", 
             _currentStep, _totalSteps, progress * 100.0f);
    
    if (progress < 0.3f) {
        // 前期：黒い画面（1回のfillScreen）
        _display->fillScreen(TFT_BLACK);
    } else if (progress < 0.7f) {
        // 中期：上部から段階的に表示（大きなブロック単位）
        int reveal_height = static_cast<int>(SIMPLE_TRANSITION_HEIGHT * ((progress - 0.3f) / 0.4f));
        
        // 高速化：大きなブロック単位でのコピー
        reveal_height = (reveal_height / EPAPER_BLOCK_SIZE) * EPAPER_BLOCK_SIZE;  // ブロック境界に調整
        
        _display->fillScreen(TFT_BLACK);
        if (reveal_height > 0) {
            drawOptimizedRegion(0, 0, SIMPLE_TRANSITION_WIDTH, reveal_height);
        }
    } else {
        // 後期：完全表示（1回のpushSprite）
        _mainCanvas->pushSprite(0, 0);
    }
}

/**
 * @brief E-Paper最適化スライド描画
 * 作業キャンバスに合成してから一括表示
 */
void SimpleTransition::drawSlideStepOptimized()
{
    float progress = static_cast<float>(_currentStep) / static_cast<float>(_totalSteps - 1);
    progress = std::min(1.0f, progress);
    
    ESP_LOGD(TAG, "E-Paper optimized slide step %d/%d (%.1f%%)", 
             _currentStep, _totalSteps, progress * 100.0f);
    
    // 背景を黒でクリア
    _display->fillScreen(TFT_BLACK);
    
    int reveal_width = SIMPLE_TRANSITION_WIDTH;
    int reveal_height = SIMPLE_TRANSITION_HEIGHT;
    int offset_x = 0, offset_y = 0;
    
    switch (_type) {
        case SimpleTransitionType::SLIDE_LEFT:
            // 左からスライドイン（ブロック境界に調整）
            reveal_width = static_cast<int>(SIMPLE_TRANSITION_WIDTH * progress);
            reveal_width = (reveal_width / EPAPER_BLOCK_SIZE) * EPAPER_BLOCK_SIZE;
            offset_x = SIMPLE_TRANSITION_WIDTH - reveal_width;
            break;
            
        case SimpleTransitionType::SLIDE_RIGHT:
            // 右からスライドイン
            reveal_width = static_cast<int>(SIMPLE_TRANSITION_WIDTH * progress);
            reveal_width = (reveal_width / EPAPER_BLOCK_SIZE) * EPAPER_BLOCK_SIZE;
            break;
            
        case SimpleTransitionType::SLIDE_UP:
            // 上からスライドイン
            reveal_height = static_cast<int>(SIMPLE_TRANSITION_HEIGHT * progress);
            reveal_height = (reveal_height / EPAPER_BLOCK_SIZE) * EPAPER_BLOCK_SIZE;
            offset_y = SIMPLE_TRANSITION_HEIGHT - reveal_height;
            break;
            
        case SimpleTransitionType::SLIDE_DOWN:
            // 下からスライドイン
            reveal_height = static_cast<int>(SIMPLE_TRANSITION_HEIGHT * progress);
            reveal_height = (reveal_height / EPAPER_BLOCK_SIZE) * EPAPER_BLOCK_SIZE;
            break;
            
        default:
            break;
    }
    
    // E-Paper最適化：直接描画（作業キャンバス不要）
    if (reveal_width > 0 && reveal_height > 0) {
        drawOptimizedRegion(offset_x, offset_y, reveal_width, reveal_height);
    }
}

/**
 * @brief E-Paper最適化ワイプ描画
 */
void SimpleTransition::drawWipeStepOptimized()
{
    float progress = static_cast<float>(_currentStep) / static_cast<float>(_totalSteps - 1);
    progress = std::min(1.0f, progress);
    
    ESP_LOGD(TAG, "E-Paper optimized wipe step %d/%d (%.1f%%)", 
             _currentStep, _totalSteps, progress * 100.0f);
    
    // 背景を黒でクリア
    _display->fillScreen(TFT_BLACK);
    
    if (_type == SimpleTransitionType::WIPE_HORIZONTAL) {
        // 水平ワイプ（ブロック境界に調整）
        int reveal_width = static_cast<int>(SIMPLE_TRANSITION_WIDTH * progress);
        reveal_width = (reveal_width / EPAPER_BLOCK_SIZE) * EPAPER_BLOCK_SIZE;
        
        if (reveal_width > 0) {
            drawOptimizedRegion(0, 0, reveal_width, SIMPLE_TRANSITION_HEIGHT);
        }
    } else {
        // 垂直ワイプ
        int reveal_height = static_cast<int>(SIMPLE_TRANSITION_HEIGHT * progress);
        reveal_height = (reveal_height / EPAPER_BLOCK_SIZE) * EPAPER_BLOCK_SIZE;
        
        if (reveal_height > 0) {
            drawOptimizedRegion(0, 0, SIMPLE_TRANSITION_WIDTH, reveal_height);
        }
    }
}

/**
 * @brief E-Paper最適化中央展開描画
 */
void SimpleTransition::drawRevealCenterStepOptimized()
{
    float progress = static_cast<float>(_currentStep) / static_cast<float>(_totalSteps - 1);
    progress = std::min(1.0f, progress);
    
    ESP_LOGD(TAG, "E-Paper optimized reveal center step %d/%d (%.1f%%)", 
             _currentStep, _totalSteps, progress * 100.0f);
    
    // 背景を黒でクリア
    _display->fillScreen(TFT_BLACK);
    
    // 中央から展開する矩形サイズを計算（ブロック境界に調整）
    int reveal_width = static_cast<int>(SIMPLE_TRANSITION_WIDTH * progress);
    int reveal_height = static_cast<int>(SIMPLE_TRANSITION_HEIGHT * progress);
    
    reveal_width = (reveal_width / EPAPER_BLOCK_SIZE) * EPAPER_BLOCK_SIZE;
    reveal_height = (reveal_height / EPAPER_BLOCK_SIZE) * EPAPER_BLOCK_SIZE;
    
    if (reveal_width > 0 && reveal_height > 0) {
        int x = (SIMPLE_TRANSITION_WIDTH - reveal_width) / 2;
        int y = (SIMPLE_TRANSITION_HEIGHT - reveal_height) / 2;
        
        drawOptimizedRegion(x, y, reveal_width, reveal_height);
    }
}

/**
 * @brief E-Paper最適化角展開描画
 */
void SimpleTransition::drawRevealCornerStepOptimized()
{
    float progress = static_cast<float>(_currentStep) / static_cast<float>(_totalSteps - 1);
    progress = std::min(1.0f, progress);
    
    ESP_LOGD(TAG, "E-Paper optimized reveal corner step %d/%d (%.1f%%)", 
             _currentStep, _totalSteps, progress * 100.0f);
    
    // 背景を黒でクリア
    _display->fillScreen(TFT_BLACK);
    
    // 左上角から展開（ブロック境界に調整）
    int reveal_width = static_cast<int>(SIMPLE_TRANSITION_WIDTH * progress);
    int reveal_height = static_cast<int>(SIMPLE_TRANSITION_HEIGHT * progress);
    
    reveal_width = (reveal_width / EPAPER_BLOCK_SIZE) * EPAPER_BLOCK_SIZE;
    reveal_height = (reveal_height / EPAPER_BLOCK_SIZE) * EPAPER_BLOCK_SIZE;
    
    if (reveal_width > 0 && reveal_height > 0) {
        drawOptimizedRegion(0, 0, reveal_width, reveal_height);
    }
}

/**
 * @brief E-Paper最適化領域描画
 * 大きなブロック単位での効率的な描画
 */
void SimpleTransition::drawOptimizedRegion(int x, int y, int w, int h)
{
    if (!_mainCanvas) return;
    
    // 境界チェック
    x = std::max(0, std::min(x, SIMPLE_TRANSITION_WIDTH));
    y = std::max(0, std::min(y, SIMPLE_TRANSITION_HEIGHT));
    w = std::max(0, std::min(w, SIMPLE_TRANSITION_WIDTH - x));
    h = std::max(0, std::min(h, SIMPLE_TRANSITION_HEIGHT - y));
    
    if (w <= 0 || h <= 0) return;
    
    // E-Paper最適化：readRectとpushImageを使った効率的な部分描画
    const int buffer_size = w * h;
    uint16_t* pixel_buffer = new uint16_t[buffer_size];
    
    if (pixel_buffer) {
        // メインキャンバスから指定領域を読み取り
        _mainCanvas->readRect(x, y, w, h, pixel_buffer);
        
        // ディスプレイに一括描画
        _display->pushImage(x, y, w, h, pixel_buffer);
        
        delete[] pixel_buffer;
    } else {
        // メモリ不足時のフォールバック：行単位で処理
        uint16_t* line_buffer = new uint16_t[w];
        if (line_buffer) {
            for (int row = 0; row < h; row++) {
                _mainCanvas->readRect(x, y + row, w, 1, line_buffer);
                _display->pushImage(x, y + row, w, 1, line_buffer);
            }
            delete[] line_buffer;
        }
    }
}

/**
 * @brief E-Paper最適化キャンバス間コピー
 * 大きなブロック単位での効率的なコピー
 */
void SimpleTransition::copyCanvasRegionOptimized(M5Canvas* src, M5Canvas* dst, 
                                                int sx, int sy, int sw, int sh, int dx, int dy)
{
    if (!src || !dst) return;
    
    // 境界チェック
    if (sx < 0 || sy < 0 || dx < 0 || dy < 0) return;
    if (sx + sw > SIMPLE_TRANSITION_WIDTH || sy + sh > SIMPLE_TRANSITION_HEIGHT) return;
    if (dx + sw > SIMPLE_TRANSITION_WIDTH || dy + sh > SIMPLE_TRANSITION_HEIGHT) return;
    
    // E-Paper最適化：readRectとpushImageを使った効率的なコピー
    const int buffer_size = sw * sh;
    uint16_t* pixel_buffer = new uint16_t[buffer_size];
    
    if (pixel_buffer) {
        // ソースキャンバスから指定領域を読み取り
        src->readRect(sx, sy, sw, sh, pixel_buffer);
        
        // デスティネーションキャンバスに描画
        dst->pushImage(dx, dy, sw, sh, pixel_buffer);
        
        delete[] pixel_buffer;
    } else {
        // メモリ不足時のフォールバック：行単位で処理
        uint16_t* line_buffer = new uint16_t[sw];
        if (line_buffer) {
            for (int row = 0; row < sh; row++) {
                src->readRect(sx, sy + row, sw, 1, line_buffer);
                dst->pushImage(dx, dy + row, sw, 1, line_buffer);
            }
            delete[] line_buffer;
        }
    }
}

/**
 * @brief トランジション停止
 */
void SimpleTransition::stop()
{
    if (_isActive) {
        _isActive = false;
        ESP_LOGI(TAG, "E-Paper最適化transition stopped");
    }
}

/**
 * @brief 即座に表示
 */
void SimpleTransition::showImmediate()
{
    if (!_mainCanvas) {
        ESP_LOGE(TAG, "Main canvas not available");
        return;
    }
    
    // E-Paper最適化：1回のpushSpriteで全体表示
    _mainCanvas->pushSprite(0, 0);
    ESP_LOGD(TAG, "E-Paper optimized immediate display complete");
}