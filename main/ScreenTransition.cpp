// main/ScreenTransition.cpp
// 完全外部キャンバス専用版画面遷移システムの実装（最大メモリ節約版）

#include "ScreenTransition.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include <cmath>
#include <algorithm>

// ログタグ
static const char *TAG = "EXTERNAL_TRANSITION";

// コンストラクタ
ScreenTransition::ScreenTransition(M5GFX *display)
    : _display(display), _initialized(false),
      _sourceCanvas(nullptr), _targetCanvas(nullptr), _workCanvas(nullptr),
      _owns_work_canvas(false),
      _state(TransitionState::IDLE), _currentStep(0), _totalSteps(0), _lastStepTime(0),
      _onTransitionStart(nullptr), _onTransitionStep(nullptr), _onTransitionComplete(nullptr)
{
    ESP_LOGI(TAG, "External Canvas Only ScreenTransition constructor called");
}

// デストラクタ
ScreenTransition::~ScreenTransition()
{
    cleanup();
    ESP_LOGI(TAG, "External Canvas Only ScreenTransition destructor called");
}

// 初期化処理（外部キャンバス必須）
bool ScreenTransition::init(M5Canvas *sourceCanvas, M5Canvas *targetCanvas)
{
    if (_initialized)
    {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }

    if (!_display)
    {
        ESP_LOGE(TAG, "Display not available");
        return false;
    }

    if (!sourceCanvas || !targetCanvas)
    {
        ESP_LOGE(TAG, "Both source and target canvases are required for initialization");
        return false;
    }

    // キャンバスサイズの確認
    if (sourceCanvas->width() != TRANSITION_WIDTH || sourceCanvas->height() != TRANSITION_HEIGHT ||
        targetCanvas->width() != TRANSITION_WIDTH || targetCanvas->height() != TRANSITION_HEIGHT)
    {
        ESP_LOGE(TAG, "Canvas size mismatch. Required %dx%d", TRANSITION_WIDTH, TRANSITION_HEIGHT);
        ESP_LOGE(TAG, "Source: %ldx%ld, Target: %ldx%ld",
                 sourceCanvas->width(), sourceCanvas->height(),
                 targetCanvas->width(), targetCanvas->height());
        return false;
    }

    ESP_LOGI(TAG, "Initializing External Canvas Only ScreenTransition (Zero internal memory)");

    // 外部キャンバスを設定
    _sourceCanvas = sourceCanvas;
    _targetCanvas = targetCanvas;

    _initialized = true;
    _state = TransitionState::IDLE;

    ESP_LOGI(TAG, "External Canvas Only ScreenTransition initialized successfully");
    ESP_LOGI(TAG, "Memory usage: Zero internal canvas memory!");
    ESP_LOGI(TAG, "Source canvas: %ldx%ld, Target canvas: %ldx%ld",
             _sourceCanvas->width(), _sourceCanvas->height(),
             _targetCanvas->width(), _targetCanvas->height());

    return true;
}

// 外部キャンバスを変更（実行時変更用）
void ScreenTransition::setCanvases(M5Canvas *sourceCanvas, M5Canvas *targetCanvas)
{
    if (!sourceCanvas || !targetCanvas)
    {
        ESP_LOGE(TAG, "Invalid canvas pointers provided");
        return;
    }

    // キャンバスサイズの確認
    if (sourceCanvas->width() != TRANSITION_WIDTH || sourceCanvas->height() != TRANSITION_HEIGHT ||
        targetCanvas->width() != TRANSITION_WIDTH || targetCanvas->height() != TRANSITION_HEIGHT)
    {
        ESP_LOGW(TAG, "Canvas size mismatch. Expected %dx%d", TRANSITION_WIDTH, TRANSITION_HEIGHT);
        ESP_LOGW(TAG, "Source: %ldx%ld, Target: %ldx%ld",
                 sourceCanvas->width(), sourceCanvas->height(),
                 targetCanvas->width(), targetCanvas->height());
    }

    _sourceCanvas = sourceCanvas;
    _targetCanvas = targetCanvas;

    ESP_LOGI(TAG, "External canvases updated successfully");
}

// 必要時に作業用キャンバス作成（最小限の一時使用）
bool ScreenTransition::ensureWorkCanvas()
{
    if (_workCanvas)
    {
        return true; // 既に存在
    }

    ESP_LOGI(TAG, "Creating temporary work canvas for complex transition...");

    // PSRAMの空き容量をチェック
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t canvas_size = TRANSITION_WIDTH * TRANSITION_HEIGHT * 2;

    if (psram_free < canvas_size * 1.2f)
    { // 20%のマージンを確保
        ESP_LOGW(TAG, "Low PSRAM memory. Free: %zu, Required: %zu", psram_free, canvas_size);
        // 続行するが警告
    }

    _workCanvas = new M5Canvas(_display);
    if (!_workCanvas)
    {
        ESP_LOGE(TAG, "Failed to allocate temporary work canvas");
        return false;
    }

    _workCanvas->setPsram(true);
    if (!_workCanvas->createSprite(TRANSITION_WIDTH, TRANSITION_HEIGHT))
    {
        ESP_LOGE(TAG, "Failed to create temporary work sprite");
        delete _workCanvas;
        _workCanvas = nullptr;
        return false;
    }

    _owns_work_canvas = true;
    ESP_LOGI(TAG, "Temporary work canvas created successfully");

    return true;
}

// 作業用キャンバス解放
void ScreenTransition::releaseWorkCanvas()
{
    if (_workCanvas && _owns_work_canvas)
    {
        ESP_LOGI(TAG, "Releasing temporary work canvas...");
        _workCanvas->deleteSprite();
        delete _workCanvas;
        _workCanvas = nullptr;
        _owns_work_canvas = false;
        ESP_LOGI(TAG, "Temporary work canvas released, memory recovered");
    }
}

// トランジション開始
bool ScreenTransition::startTransition(const TransitionConfig &config)
{
    if (!_initialized)
    {
        ESP_LOGE(TAG, "Not initialized. Call init() with external canvases first");
        return false;
    }

    if (!_sourceCanvas || !_targetCanvas)
    {
        ESP_LOGE(TAG, "External canvases not set. This should not happen after proper init()");
        return false;
    }

    if (_state == TransitionState::RUNNING)
    {
        ESP_LOGW(TAG, "Transition already running");
        return false;
    }

    _config = config;
    _currentStep = 0;
    _totalSteps = static_cast<int>(config.speed);
    _lastStepTime = 0;
    _state = TransitionState::RUNNING;

    ESP_LOGI(TAG, "Starting external canvas transition: type=%d, steps=%d, delay=%lums",
             static_cast<int>(config.type), _totalSteps, config.step_delay_ms);

    // 開始コールバックを実行
    if (_onTransitionStart)
    {
        _onTransitionStart();
    }

    return true;
}

// 現在ステップの進行度取得
float ScreenTransition::getStepProgress() const
{
    if (_totalSteps <= 0)
        return 0.0f;
    return static_cast<float>(_currentStep) / static_cast<float>(_totalSteps);
}

// 次ステップ実行可能かチェック
bool ScreenTransition::isStepReady()
{
    int64_t current_time = esp_timer_get_time() / 1000; // ミリ秒に変換

    if (_lastStepTime == 0)
    {
        // 初回実行
        _lastStepTime = current_time;
        return true;
    }

    // 指定された間隔が経過したかチェック
    return (current_time - _lastStepTime) >= _config.step_delay_ms;
}

// 1ステップ実行
void ScreenTransition::executeStep()
{
    ESP_LOGD(TAG, "Executing step %d/%d (%.1f%%)",
             _currentStep, _totalSteps, getStepProgress() * 100.0f);

    // トランジション効果を描画
    switch (_config.type)
    {
    case TransitionType::NONE:
        _currentStep = _totalSteps;
        break;

    case TransitionType::FADE_BLACK:
    case TransitionType::FADE_WHITE:
        renderFadeStep(_currentStep, _totalSteps);
        break;

    case TransitionType::SLIDE_LEFT:
    case TransitionType::SLIDE_RIGHT:
    case TransitionType::SLIDE_UP:
    case TransitionType::SLIDE_DOWN:
        renderSlideStep(_currentStep, _totalSteps);
        break;

    case TransitionType::WIPE_LEFT:
    case TransitionType::WIPE_RIGHT:
        renderWipeStep(_currentStep, _totalSteps);
        break;

    case TransitionType::CIRCLE_EXPAND:
    case TransitionType::CIRCLE_SHRINK:
        renderCircleStep(_currentStep, _totalSteps);
        break;

    case TransitionType::PIXELATE:
        renderPixelateStep(_currentStep, _totalSteps);
        break;

    case TransitionType::VENETIAN_BLIND:
        renderVenetianBlindStep(_currentStep, _totalSteps);
        break;
    case TransitionType::CHECKERBOARD:
        renderCheckerboardStep(_currentStep, _totalSteps);
        break;
    case TransitionType::SPIRAL:
        renderSpiralStep(_currentStep, _totalSteps);
        break;
    case TransitionType::RIPPLE:
        renderRippleStep(_currentStep, _totalSteps);
        break;
    case TransitionType::MOSAIC:
        renderMosaicStep(_currentStep, _totalSteps);
        break;
    case TransitionType::PAGE_TURN:
        renderPageTurnStep(_currentStep, _totalSteps);
        break;
    default:
        ESP_LOGW(TAG, "Unknown transition type: %d", static_cast<int>(_config.type));
        renderFadeStep(_currentStep, _totalSteps); // フォールバック
        break;
    }

    // ステップコールバックを実行
    if (_onTransitionStep)
    {
        _onTransitionStep(_currentStep, _totalSteps);
    }

    // 次ステップに進む
    _currentStep++;
    _lastStepTime = esp_timer_get_time() / 1000;
}

// トランジション更新（メインループから呼び出し）
bool ScreenTransition::updateTransition()
{
    if (_state != TransitionState::RUNNING)
    {
        return false;
    }

    // ステップ実行タイミングをチェック
    if (!isStepReady())
    {
        return true; // まだ次のステップではない
    }

    // ステップを実行
    executeStep();

    // 完了チェック
    if (_currentStep >= _totalSteps)
    {
        _state = TransitionState::FINISHED;

        // 最終画面を表示
        if (_targetCanvas)
        {
            _targetCanvas->pushSprite(0, 0);
        }

        // 作業用キャンバスを解放してメモリ回収
        releaseWorkCanvas();

        // 完了コールバックを実行
        if (_onTransitionComplete)
        {
            _onTransitionComplete();
        }

        ESP_LOGI(TAG, "External canvas transition completed, memory cleaned up");
        return false;
    }

    return true;
}

// FADE_BLACK/FADE_WHITE効果（外部キャンバス使用版）
void ScreenTransition::renderFadeStep(int step, int totalSteps)
{
    if (!_sourceCanvas || !_targetCanvas)
    {
        ESP_LOGE(TAG, "External canvases not available for fade effect");
        return;
    }

    ESP_LOGD(TAG, "Fade step %d/%d (External Canvas Only)", step, totalSteps);

    if (totalSteps <= 2)
    {
        // 2ステップの場合：即座切り替え
        if (step == 0)
        {
            _sourceCanvas->pushSprite(0, 0);
        }
        else
        {
            _targetCanvas->pushSprite(0, 0);
        }
    }
    else if (totalSteps <= 4)
    {
        // 4ステップの場合：ソース→フェード色→ターゲット
        if (step < totalSteps / 2)
        {
            _sourceCanvas->pushSprite(0, 0);
        }
        else if (step == totalSteps / 2)
        {
            // フェード色を直接画面に描画（キャンバス不使用でメモリ節約）
            _display->fillScreen(_config.fade_color);
        }
        else
        {
            _targetCanvas->pushSprite(0, 0);
        }
    }
    else
    {
        // 8ステップ以上：段階的フェード
        float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);

        if (progress < 0.3f)
        {
            // 前期：ソース画面
            _sourceCanvas->pushSprite(0, 0);
        }
        else if (progress < 0.7f)
        {
            // 中期：フェード処理
            float fade_progress = (progress - 0.3f) / 0.4f; // 0.0-1.0に正規化

            uint16_t fade_color;
            if (_config.type == TransitionType::FADE_WHITE)
            {
                fade_color = getGrayscaleColor(fade_progress);
            }
            else
            {
                fade_color = getGrayscaleColor(1.0f - fade_progress);
            }

            // 直接画面に描画してメモリ節約
            _display->fillScreen(fade_color);
        }
        else
        {
            // 後期：ターゲット画面
            _targetCanvas->pushSprite(0, 0);
        }
    }
}

// スライド効果（外部キャンバス使用版 - 必要時のみ作業キャンバス）
void ScreenTransition::renderSlideStep(int step, int totalSteps)
{
    // スライド効果は作業用キャンバスが必要
    if (!ensureWorkCanvas())
    {
        ESP_LOGE(TAG, "Failed to create temporary work canvas for slide effect");
        // フォールバック：フェード効果を使用
        renderFadeStep(step, totalSteps);
        return;
    }

    if (!_sourceCanvas || !_targetCanvas)
    {
        ESP_LOGE(TAG, "External canvases not available for slide effect");
        return;
    }

    ESP_LOGD(TAG, "Slide step %d/%d (using temporary work canvas)", step, totalSteps);

    float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);
    int offset_x = 0, offset_y = 0;

    switch (_config.type)
    {
    case TransitionType::SLIDE_LEFT:
        offset_x = static_cast<int>(-TRANSITION_WIDTH * progress);
        break;
    case TransitionType::SLIDE_RIGHT:
        offset_x = static_cast<int>(TRANSITION_WIDTH * progress);
        break;
    case TransitionType::SLIDE_UP:
        offset_y = static_cast<int>(-TRANSITION_HEIGHT * progress);
        break;
    case TransitionType::SLIDE_DOWN:
        offset_y = static_cast<int>(TRANSITION_HEIGHT * progress);
        break;
    default:
        break;
    }

    // 作業キャンバスをクリア
    _workCanvas->fillSprite(TFT_BLACK);

    // ソース画面をオフセット位置に描画
    copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, offset_x, offset_y);

    // ターゲット画面を反対側に描画
    int target_x = offset_x > 0 ? offset_x - TRANSITION_WIDTH : offset_x + TRANSITION_WIDTH;
    int target_y = offset_y > 0 ? offset_y - TRANSITION_HEIGHT : offset_y + TRANSITION_HEIGHT;
    copyCanvasRegion(_targetCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, target_x, target_y);

    // 結果を画面に表示
    _workCanvas->pushSprite(0, 0);
}

// ワイプ効果（外部キャンバス使用版 - 必要時のみ作業キャンバス）
void ScreenTransition::renderWipeStep(int step, int totalSteps)
{
    // ワイプ効果は作業用キャンバスが必要
    if (!ensureWorkCanvas())
    {
        ESP_LOGE(TAG, "Failed to create temporary work canvas for wipe effect");
        renderFadeStep(step, totalSteps);
        return;
    }

    if (!_sourceCanvas || !_targetCanvas)
    {
        ESP_LOGE(TAG, "External canvases not available for wipe effect");
        return;
    }

    ESP_LOGD(TAG, "Wipe step %d/%d (using temporary work canvas)", step, totalSteps);

    float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);

    // ソース画面をベースとしてコピー
    copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);

    int wipe_pos = 0;
    if (_config.type == TransitionType::WIPE_LEFT)
    {
        wipe_pos = static_cast<int>(TRANSITION_WIDTH * progress);
        // 左からワイプ
        if (wipe_pos > 0)
        {
            copyCanvasRegion(_targetCanvas, _workCanvas, 0, 0, wipe_pos, TRANSITION_HEIGHT, 0, 0);
        }
    }
    else
    { // WIPE_RIGHT
        wipe_pos = static_cast<int>(TRANSITION_WIDTH * (1.0f - progress));
        // 右からワイプ
        if (wipe_pos < TRANSITION_WIDTH)
        {
            copyCanvasRegion(_targetCanvas, _workCanvas, wipe_pos, 0, TRANSITION_WIDTH - wipe_pos, TRANSITION_HEIGHT, wipe_pos, 0);
        }
    }

    // 結果を画面に表示
    _workCanvas->pushSprite(0, 0);
}

// 円形効果（外部キャンバス使用版 - 簡略版、必要時のみ作業キャンバス）
void ScreenTransition::renderCircleStep(int step, int totalSteps)
{
    if (!ensureWorkCanvas())
    {
        ESP_LOGE(TAG, "Failed to create temporary work canvas for circle effect");
        renderFadeStep(step, totalSteps);
        return;
    }

    if (!_sourceCanvas || !_targetCanvas)
    {
        ESP_LOGE(TAG, "External canvases not available for circle effect");
        return;
    }

    ESP_LOGD(TAG, "Circle step %d/%d (using temporary work canvas)", step, totalSteps);

    float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);

    // 簡略版：中央部分だけ段階的に切り替え
    int center_x = TRANSITION_WIDTH / 2;
    int center_y = TRANSITION_HEIGHT / 2;
    int max_radius = std::min(center_x, center_y);
    int current_radius = static_cast<int>(max_radius * progress);

    if (_config.type == TransitionType::CIRCLE_EXPAND)
    {
        // ソース画面をベースとしてworkCanvasにコピー
        copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);

        // 中央の矩形部分をターゲット画面で置き換え
        int rect_size = current_radius * 2;
        int rect_x = center_x - current_radius;
        int rect_y = center_y - current_radius;

        if (rect_size > 0 && rect_x >= 0 && rect_y >= 0)
        {
            copyCanvasRegion(_targetCanvas, _workCanvas, rect_x, rect_y, rect_size, rect_size, rect_x, rect_y);
        }
    }
    else
    { // CIRCLE_SHRINK
        // ターゲット画面をベースとしてworkCanvasにコピー
        copyCanvasRegion(_targetCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);

        // 中央の矩形部分をソース画面で置き換え
        int rect_size = (max_radius - current_radius) * 2;
        int rect_x = center_x - (max_radius - current_radius);
        int rect_y = center_y - (max_radius - current_radius);

        if (rect_size > 0 && rect_x >= 0 && rect_y >= 0)
        {
            copyCanvasRegion(_sourceCanvas, _workCanvas, rect_x, rect_y, rect_size, rect_size, rect_x, rect_y);
        }
    }

    // Canvas経由で表示
    _workCanvas->pushSprite(0, 0);
}

// ピクセル化効果（ステップベース完全実装）
void ScreenTransition::renderPixelateStep(int step, int totalSteps)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    ESP_LOGD(TAG, "Pixelate step %d/%d", step, totalSteps);

    if (totalSteps <= 2) {
        // 2ステップの場合：即座切り替え
        if (step == 0) {
            _sourceCanvas->pushSprite(0, 0);
        } else {
            _targetCanvas->pushSprite(0, 0);
        }
    } else if (totalSteps <= 4) {
        // 4ステップの場合：ソース→低解像度→高解像度→ターゲット
        float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);
        
        if (progress < 0.33f) {
            // 前期：ソース画面をそのまま表示
            _sourceCanvas->pushSprite(0, 0);
        } else if (progress < 0.66f) {
            // 中期：低解像度でピクセル化表現
            _workCanvas->fillSprite(TFT_BLACK);
            
            // 大きなブロックでピクセル化効果を作成
            const int block_size = 32;  // 大きなピクセルサイズ
            for (int y = 0; y < TRANSITION_HEIGHT; y += block_size) {
                for (int x = 0; x < TRANSITION_WIDTH; x += block_size) {
                    // ソース画面の代表色を取得（ブロックの中央の色）
                    uint16_t pixel_color = _sourceCanvas->readPixel(
                        std::min(x + block_size/2, TRANSITION_WIDTH-1),
                        std::min(y + block_size/2, TRANSITION_HEIGHT-1)
                    );
                    
                    // ブロック全体を同じ色で塗りつぶし
                    _workCanvas->fillRect(x, y, 
                        std::min(block_size, TRANSITION_WIDTH - x),
                        std::min(block_size, TRANSITION_HEIGHT - y), 
                        pixel_color);
                }
            }
            _workCanvas->pushSprite(0, 0);
        } else {
            // 後期：ターゲット画面を表示
            _targetCanvas->pushSprite(0, 0);
        }
    } else {
        // 8ステップ以上：段階的ピクセル化
        float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);
        
        if (progress < 0.25f) {
            // 第1期：ソース画面
            _sourceCanvas->pushSprite(0, 0);
        } else if (progress < 0.5f) {
            // 第2期：粗いピクセル化（ソース→低解像度）
            float pixelate_progress = (progress - 0.25f) / 0.25f;
            int block_size = static_cast<int>(2 + pixelate_progress * 30);  // 2-32ピクセル
            
            _workCanvas->fillSprite(TFT_BLACK);
            for (int y = 0; y < TRANSITION_HEIGHT; y += block_size) {
                for (int x = 0; x < TRANSITION_WIDTH; x += block_size) {
                    uint16_t pixel_color = _sourceCanvas->readPixel(
                        std::min(x + block_size/2, TRANSITION_WIDTH-1),
                        std::min(y + block_size/2, TRANSITION_HEIGHT-1)
                    );
                    _workCanvas->fillRect(x, y, 
                        std::min(block_size, TRANSITION_WIDTH - x),
                        std::min(block_size, TRANSITION_HEIGHT - y), 
                        pixel_color);
                }
            }
            _workCanvas->pushSprite(0, 0);
        } else if (progress < 0.75f) {
            // 第3期：細かいピクセル化（ターゲット画面）
            float pixelate_progress = (progress - 0.5f) / 0.25f;
            int block_size = static_cast<int>(32 - pixelate_progress * 30);  // 32-2ピクセル
            
            _workCanvas->fillSprite(TFT_BLACK);
            for (int y = 0; y < TRANSITION_HEIGHT; y += block_size) {
                for (int x = 0; x < TRANSITION_WIDTH; x += block_size) {
                    uint16_t pixel_color = _targetCanvas->readPixel(
                        std::min(x + block_size/2, TRANSITION_WIDTH-1),
                        std::min(y + block_size/2, TRANSITION_HEIGHT-1)
                    );
                    _workCanvas->fillRect(x, y, 
                        std::min(block_size, TRANSITION_WIDTH - x),
                        std::min(block_size, TRANSITION_HEIGHT - y), 
                        pixel_color);
                }
                // WDTタイムアウト防止
                if (y % 128 == 0) vTaskDelay(1);
            }
            _workCanvas->pushSprite(0, 0);
        } else {
            // 第4期：ターゲット画面をそのまま表示
            _targetCanvas->pushSprite(0, 0);
        }
    }
}

void ScreenTransition::renderVenetianBlindStep(int step, int totalSteps)
{
    renderWipeStep(step, totalSteps);
}

// チェッカーボード効果（ステップベース完全実装）
void ScreenTransition::renderCheckerboardStep(int step, int totalSteps)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    ESP_LOGD(TAG, "Checkerboard step %d/%d", step, totalSteps);

    if (totalSteps <= 2) {
        // 2ステップの場合：即座切り替え
        if (step == 0) {
            _sourceCanvas->pushSprite(0, 0);
        } else {
            _targetCanvas->pushSprite(0, 0);
        }
    } else if (totalSteps <= 4) {
        // 4ステップの場合：チェッカー模様で段階的切り替え
        const int checker_size = 64;  // チェッカーのサイズ
        int revealed_pattern = step;   // 表示するパターン数
        
        // ソース画面をベースとしてコピー
        copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);
        
        // チェッカーパターンでターゲット画面を重ね合わせ
        for (int y = 0; y < TRANSITION_HEIGHT; y += checker_size) {
            for (int x = 0; x < TRANSITION_WIDTH; x += checker_size) {
                // チェッカーパターンの判定
                bool is_checker = ((x / checker_size) + (y / checker_size)) % 2 == 0;
                
                // ステップに応じて表示
                if ((is_checker && revealed_pattern >= 1) || 
                    (!is_checker && revealed_pattern >= 3)) {
                    int copy_w = std::min(checker_size, TRANSITION_WIDTH - x);
                    int copy_h = std::min(checker_size, TRANSITION_HEIGHT - y);
                    copyCanvasRegion(_targetCanvas, _workCanvas, x, y, copy_w, copy_h, x, y);
                }
            }
        }
        _workCanvas->pushSprite(0, 0);
    } else {
        // 8ステップ以上：細かいチェッカーパターンで段階的切り替え
        float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);
        const int checker_size = 32;  // より細かいチェッカー
        
        // ソース画面をベース
        copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);
        
        // 進行度に応じてチェッカーパターンを展開
        int total_checkers = (TRANSITION_WIDTH / checker_size) * (TRANSITION_HEIGHT / checker_size);
        int revealed_checkers = static_cast<int>(total_checkers * progress);
        int current_checker = 0;
        
        for (int y = 0; y < TRANSITION_HEIGHT; y += checker_size) {
            for (int x = 0; x < TRANSITION_WIDTH; x += checker_size) {
                current_checker++;
                
                // ランダム風の順序で表示（実際は決定的パターン）
                int checker_index = ((x / checker_size) * 7 + (y / checker_size) * 3) % total_checkers;
                
                if (checker_index < revealed_checkers) {
                    int copy_w = std::min(checker_size, TRANSITION_WIDTH - x);
                    int copy_h = std::min(checker_size, TRANSITION_HEIGHT - y);
                    copyCanvasRegion(_targetCanvas, _workCanvas, x, y, copy_w, copy_h, x, y);
                }
            }
            // WDTタイムアウト防止
            if (y % 128 == 0) vTaskDelay(1);
        }
        _workCanvas->pushSprite(0, 0);
    }
}

// 螺旋効果（ステップベース完全実装）
void ScreenTransition::renderSpiralStep(int step, int totalSteps)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    ESP_LOGD(TAG, "Spiral step %d/%d", step, totalSteps);

    if (totalSteps <= 2) {
        // 2ステップの場合：即座切り替え
        if (step == 0) {
            _sourceCanvas->pushSprite(0, 0);
        } else {
            _targetCanvas->pushSprite(0, 0);
        }
    } else if (totalSteps <= 4) {
        // 4ステップの場合：中央から同心円状に展開
        float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);
        int center_x = TRANSITION_WIDTH / 2;
        int center_y = TRANSITION_HEIGHT / 2;
        int max_radius = std::max(center_x, center_y);
        int current_radius = static_cast<int>(max_radius * progress);
        
        // ソース画面をベース
        copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);
        
        // 中央の円形領域をターゲット画面で置き換え
        if (current_radius > 0) {
            int rect_size = current_radius;
            int rect_x = std::max(0, center_x - rect_size);
            int rect_y = std::max(0, center_y - rect_size);
            int rect_w = std::min(rect_size * 2, TRANSITION_WIDTH - rect_x);
            int rect_h = std::min(rect_size * 2, TRANSITION_HEIGHT - rect_y);
            
            copyCanvasRegion(_targetCanvas, _workCanvas, rect_x, rect_y, rect_w, rect_h, rect_x, rect_y);
        }
        _workCanvas->pushSprite(0, 0);
    } else {
        // 8ステップ以上：螺旋パターンで展開（簡易版）
        float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);
        
        if (progress < 0.3f) {
            // 前期：ソース画面
            _sourceCanvas->pushSprite(0, 0);
        } else if (progress < 0.7f) {
            // 中期：螺旋展開（同心円近似）
            float spiral_progress = (progress - 0.3f) / 0.4f;
            int center_x = TRANSITION_WIDTH / 2;
            int center_y = TRANSITION_HEIGHT / 2;
            int max_radius = std::max(center_x, center_y);
            int current_radius = static_cast<int>(max_radius * spiral_progress);
            
            // ソース画面をベース
            copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);
            
            // 螺旋状に展開（矩形近似）
            const int ring_count = 5;
            for (int ring = 0; ring < ring_count; ring++) {
                int ring_radius = (current_radius * (ring + 1)) / ring_count;
                int ring_x = std::max(0, center_x - ring_radius);
                int ring_y = std::max(0, center_y - ring_radius);
                int ring_w = std::min(ring_radius * 2, TRANSITION_WIDTH - ring_x);
                int ring_h = std::min(ring_radius * 2, TRANSITION_HEIGHT - ring_y);
                
                if (ring_w > 0 && ring_h > 0) {
                    copyCanvasRegion(_targetCanvas, _workCanvas, ring_x, ring_y, ring_w, ring_h, ring_x, ring_y);
                }
            }
            _workCanvas->pushSprite(0, 0);
        } else {
            // 後期：ターゲット画面
            _targetCanvas->pushSprite(0, 0);
        }
    }
}


// 波紋効果（ステップベース完全実装）
void ScreenTransition::renderRippleStep(int step, int totalSteps)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    ESP_LOGD(TAG, "Ripple step %d/%d", step, totalSteps);

    if (totalSteps <= 2) {
        // 2ステップの場合：即座切り替え
        if (step == 0) {
            _sourceCanvas->pushSprite(0, 0);
        } else {
            _targetCanvas->pushSprite(0, 0);
        }
    } else if (totalSteps <= 4) {
        // 4ステップの場合：同心円状の波紋効果
        float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);
        
        // 複数の波紋中心を設定
        const int ripple_count = 3;
        int centers_x[] = {TRANSITION_WIDTH/4, TRANSITION_WIDTH*3/4, TRANSITION_WIDTH/2};
        int centers_y[] = {TRANSITION_HEIGHT/3, TRANSITION_HEIGHT/3, TRANSITION_HEIGHT*2/3};
        
        // ソース画面をベース
        copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);
        
        // 各波紋中心から円形に展開
        for (int r = 0; r < ripple_count; r++) {
            if (step > r) {  // 時間差で波紋を開始
                int radius = static_cast<int>(80 * progress);  // 波紋の半径
                int rect_size = radius;
                int rect_x = std::max(0, centers_x[r] - rect_size);
                int rect_y = std::max(0, centers_y[r] - rect_size);
                int rect_w = std::min(rect_size * 2, TRANSITION_WIDTH - rect_x);
                int rect_h = std::min(rect_size * 2, TRANSITION_HEIGHT - rect_y);
                
                if (rect_w > 0 && rect_h > 0) {
                    copyCanvasRegion(_targetCanvas, _workCanvas, rect_x, rect_y, rect_w, rect_h, rect_x, rect_y);
                }
            }
        }
        _workCanvas->pushSprite(0, 0);
    } else {
        // 8ステップ以上：多重波紋効果
        float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);
        
        if (progress < 0.2f) {
            // 前期：ソース画面
            _sourceCanvas->pushSprite(0, 0);
        } else if (progress < 0.8f) {
            // 中期：波紋展開
            float ripple_progress = (progress - 0.2f) / 0.6f;
            
            // ソース画面をベース
            copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);
            
            // 複数の波紋を時間差で展開
            const int wave_count = 5;
            for (int w = 0; w < wave_count; w++) {
                float wave_delay = static_cast<float>(w) / static_cast<float>(wave_count);
                if (ripple_progress > wave_delay) {
                    float wave_progress = (ripple_progress - wave_delay) * wave_count;
                    wave_progress = std::min(1.0f, wave_progress);
                    
                    // 波紋の中心位置（擬似ランダム）
                    int center_x = (TRANSITION_WIDTH * (w * 3 + 1)) / (wave_count * 2);
                    int center_y = (TRANSITION_HEIGHT * (w * 2 + 1)) / (wave_count + 1);
                    int radius = static_cast<int>(120 * wave_progress);
                    
                    int rect_x = std::max(0, center_x - radius);
                    int rect_y = std::max(0, center_y - radius);
                    int rect_w = std::min(radius * 2, TRANSITION_WIDTH - rect_x);
                    int rect_h = std::min(radius * 2, TRANSITION_HEIGHT - rect_y);
                    
                    if (rect_w > 0 && rect_h > 0) {
                        copyCanvasRegion(_targetCanvas, _workCanvas, rect_x, rect_y, rect_w, rect_h, rect_x, rect_y);
                    }
                }
            }
            _workCanvas->pushSprite(0, 0);
        } else {
            // 後期：ターゲット画面
            _targetCanvas->pushSprite(0, 0);
        }
    }
}

// モザイク効果（ステップベース完全実装）
void ScreenTransition::renderMosaicStep(int step, int totalSteps)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    ESP_LOGD(TAG, "Mosaic step %d/%d", step, totalSteps);

    if (totalSteps <= 2) {
        // 2ステップの場合：即座切り替え
        if (step == 0) {
            _sourceCanvas->pushSprite(0, 0);
        } else {
            _targetCanvas->pushSprite(0, 0);
        }
    } else if (totalSteps <= 4) {
        // 4ステップの場合：タイル状に順次切り替え
        const int tile_size = 80;  // モザイクタイルのサイズ
        int tiles_x = (TRANSITION_WIDTH + tile_size - 1) / tile_size;
        int tiles_y = (TRANSITION_HEIGHT + tile_size - 1) / tile_size;
        int total_tiles = tiles_x * tiles_y;
        int revealed_tiles = (total_tiles * step) / totalSteps;
        
        // ソース画面をベース
        copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);
        
        // タイルを順次切り替え
        int current_tile = 0;
        for (int ty = 0; ty < tiles_y; ty++) {
            for (int tx = 0; tx < tiles_x; tx++) {
                if (current_tile < revealed_tiles) {
                    int tile_x = tx * tile_size;
                    int tile_y = ty * tile_size;
                    int tile_w = std::min(tile_size, TRANSITION_WIDTH - tile_x);
                    int tile_h = std::min(tile_size, TRANSITION_HEIGHT - tile_y);
                    
                    copyCanvasRegion(_targetCanvas, _workCanvas, tile_x, tile_y, tile_w, tile_h, tile_x, tile_y);
                }
                current_tile++;
            }
        }
        _workCanvas->pushSprite(0, 0);
    } else {
        // 8ステップ以上：ランダム風モザイク切り替え
        float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);
        const int tile_size = 40;  // より細かいタイル
        
        // ソース画面をベース
        copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);
        
        int tiles_x = (TRANSITION_WIDTH + tile_size - 1) / tile_size;
        int tiles_y = (TRANSITION_HEIGHT + tile_size - 1) / tile_size;
        int total_tiles = tiles_x * tiles_y;
        int revealed_tiles = static_cast<int>(total_tiles * progress);
        
        // 擬似ランダム順序でタイルを切り替え
        for (int ty = 0; ty < tiles_y; ty++) {
            for (int tx = 0; tx < tiles_x; tx++) {
                // 擬似ランダムなタイル順序を生成
                int tile_index = ((tx * 7 + ty * 11) * 13) % total_tiles;
                
                if (tile_index < revealed_tiles) {
                    int tile_x = tx * tile_size;
                    int tile_y = ty * tile_size;
                    int tile_w = std::min(tile_size, TRANSITION_WIDTH - tile_x);
                    int tile_h = std::min(tile_size, TRANSITION_HEIGHT - tile_y);
                    
                    copyCanvasRegion(_targetCanvas, _workCanvas, tile_x, tile_y, tile_w, tile_h, tile_x, tile_y);
                }
            }
            // WDTタイムアウト防止
            if (ty % 8 == 0) vTaskDelay(1);
        }
        _workCanvas->pushSprite(0, 0);
    }
}

// ページめくり効果（ステップベース完全実装）
void ScreenTransition::renderPageTurnStep(int step, int totalSteps)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    ESP_LOGD(TAG, "Page turn step %d/%d", step, totalSteps);

    if (totalSteps <= 2) {
        // 2ステップの場合：即座切り替え
        if (step == 0) {
            _sourceCanvas->pushSprite(0, 0);
        } else {
            _targetCanvas->pushSprite(0, 0);
        }
    } else if (totalSteps <= 4) {
        // 4ステップの場合：左から右へのページめくり
        float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);
        int page_pos = static_cast<int>(TRANSITION_WIDTH * progress);
        
        // ソース画面をベース
        copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);
        
        // ページがめくられた部分をターゲット画面で置き換え
        if (page_pos > 0) {
            copyCanvasRegion(_targetCanvas, _workCanvas, 0, 0, page_pos, TRANSITION_HEIGHT, 0, 0);
            
            // ページめくりの境界線を描画（影効果）
            if (page_pos < TRANSITION_WIDTH - 2) {
                for (int y = 0; y < TRANSITION_HEIGHT; y += 4) {
                    _workCanvas->drawPixel(page_pos, y, getGrayscaleColor(0.7f));
                    if (page_pos + 1 < TRANSITION_WIDTH) {
                        _workCanvas->drawPixel(page_pos + 1, y, getGrayscaleColor(0.8f));
                    }
                }
            }
        }
        _workCanvas->pushSprite(0, 0);
    } else {
        // 8ステップ以上：滑らかなページめくり（カール効果付き）
        float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);
        
        if (progress < 0.1f) {
            // 開始期：ソース画面
            _sourceCanvas->pushSprite(0, 0);
        } else if (progress < 0.9f) {
            // 中期：ページめくり効果
            float page_progress = (progress - 0.1f) / 0.8f;
            int base_pos = static_cast<int>(TRANSITION_WIDTH * page_progress);
            
            // ソース画面をベース
            copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);
            
            // カール効果を作成（波形でページの曲がりを表現）
            for (int y = 0; y < TRANSITION_HEIGHT; y += 2) {
                // 波形でページの曲がりを計算
                float wave = sinf(static_cast<float>(y) / 50.0f) * 15.0f * (1.0f - page_progress);
                int page_pos = base_pos + static_cast<int>(wave);
                page_pos = std::max(0, std::min(TRANSITION_WIDTH, page_pos));
                
                // ターゲット画面からコピー
                if (page_pos > 0) {
                    copyCanvasRegion(_targetCanvas, _workCanvas, 0, y, page_pos, 2, 0, y);
                }
                
                // 境界線の描画（影効果）
                if (page_pos < TRANSITION_WIDTH - 3) {
                    _workCanvas->drawPixel(page_pos, y, getGrayscaleColor(0.6f));
                    _workCanvas->drawPixel(page_pos + 1, y, getGrayscaleColor(0.7f));
                    _workCanvas->drawPixel(page_pos + 2, y, getGrayscaleColor(0.8f));
                }
            }
            _workCanvas->pushSprite(0, 0);
        } else {
            // 終了期：ターゲット画面
            _targetCanvas->pushSprite(0, 0);
        }
    }
}

// グレースケール色取得（E-Paper用）
uint16_t ScreenTransition::getGrayscaleColor(float intensity)
{
    intensity = std::max(0.0f, std::min(1.0f, intensity));
    int gray_level = static_cast<int>(intensity * 15.0f);

    switch (gray_level)
    {
    case 0:
        return 0x0000; // 黒
    case 1:
        return 0x1082; // 非常に濃いグレー
    case 2:
        return 0x2104; // 濃いグレー
    case 3:
        return 0x3186; // やや濃いグレー
    case 4:
        return 0x4208; // 中濃いグレー
    case 5:
        return 0x528A; // グレー
    case 6:
        return 0x630C; // やや薄いグレー
    case 7:
        return 0x738E; // 薄いグレー
    case 8:
        return 0x8410; // 中薄いグレー
    case 9:
        return 0x9492; // 明るいグレー
    case 10:
        return 0xA514; // やや明るいグレー
    case 11:
        return 0xB596; // 明るいグレー
    case 12:
        return 0xC618; // 非常に明るいグレー
    case 13:
        return 0xD69A; // ほぼ白に近いグレー
    case 14:
        return 0xE71C; // 白に近いグレー
    default:
        return 0xFFFF; // 白
    }
}

// キャンバス領域コピー（WDT対策版）
void ScreenTransition::copyCanvasRegion(M5Canvas *src, M5Canvas *dst, int sx, int sy, int sw, int sh, int dx, int dy)
{
    if (!src || !dst)
        return;

    // 大きなブロック単位でコピー（WDT対策）
    const int block_size = 64;

    for (int y = 0; y < sh; y += block_size)
    {
        for (int x = 0; x < sw; x += block_size)
        {
            int copy_width = std::min(block_size, sw - x);
            int copy_height = std::min(block_size, sh - y);

            for (int by = 0; by < copy_height; by++)
            {
                for (int bx = 0; bx < copy_width; bx++)
                {
                    int src_x = sx + x + bx;
                    int src_y = sy + y + by;
                    int dst_x = dx + x + bx;
                    int dst_y = dy + y + by;

                    // 境界チェック
                    if (src_x >= 0 && src_x < src->width() && src_y >= 0 && src_y < src->height() &&
                        dst_x >= 0 && dst_x < dst->width() && dst_y >= 0 && dst_y < dst->height())
                    {
                        uint16_t pixel = src->readPixel(src_x, src_y);
                        dst->writePixel(dst_x, dst_y, pixel);
                    }
                }
            }
        }

        // WDTタイムアウト防止
        if (y % (block_size * 2) == 0)
        {
            vTaskDelay(1);
        }
    }
}

// トランジション停止
void ScreenTransition::stopTransition()
{
    if (_state == TransitionState::RUNNING)
    {
        _state = TransitionState::CANCELLED;
        releaseWorkCanvas(); // メモリ解放
        ESP_LOGI(TAG, "External canvas transition cancelled, memory cleaned");
    }
}

// 即座に画面切り替え
void ScreenTransition::switchImmediate()
{
    if (_targetCanvas)
    {
        _targetCanvas->pushSprite(0, 0);
        ESP_LOGI(TAG, "Immediate screen switch (external canvas)");
    }
}

// ワンステップトランジション実行
void ScreenTransition::executeTransition(std::function<void(M5Canvas *)> prepare_func, const TransitionConfig &config)
{
    if (!_initialized)
    {
        ESP_LOGE(TAG, "Not initialized. Call init() with external canvases first");
        return;
    }

    // 次画面を準備
    if (prepare_func)
    {
        prepare_func(_targetCanvas);
    }

    // トランジション開始
    startTransition(config);
}

// 元画面のキャプチャ（簡略版）
void ScreenTransition::captureSource(bool captureFromDisplay)
{
    if (!_initialized || !_sourceCanvas)
    {
        ESP_LOGE(TAG, "Not initialized or source canvas not available");
        return;
    }

    ESP_LOGI(TAG, "Capturing source screen (external canvas mode)...");

    if (captureFromDisplay)
    {
        // 画面からの直接キャプチャは制限があるため、通常は使用しない
        ESP_LOGW(TAG, "Direct screen capture not recommended. Use pre-prepared canvas.");
        _sourceCanvas->fillSprite(TFT_BLACK);
    }
    // else: 外部キャンバスは既に準備済みと仮定

    ESP_LOGI(TAG, "Source screen capture completed");
}

// 次画面の準備（外部キャンバス使用版）
void ScreenTransition::prepareTarget(std::function<void(M5Canvas *)> prepare_func)
{
    if (!_initialized || !_targetCanvas)
    {
        ESP_LOGE(TAG, "Not initialized or target canvas not available");
        return;
    }

    ESP_LOGI(TAG, "Preparing target screen (external canvas)...");

    // ターゲットキャンバスをクリア
    _targetCanvas->fillSprite(TFT_BLACK);

    // 準備関数を実行してターゲット画面を描画
    if (prepare_func)
    {
        prepare_func(_targetCanvas);
    }

    ESP_LOGI(TAG, "Target screen prepared");
}

// プリセット設定関数
TransitionConfig ScreenTransition::getFadeConfig(TransitionSpeed speed, uint32_t color)
{
    TransitionConfig config = TransitionConfig::defaultConfig();
    config.type = (color == TFT_BLACK) ? TransitionType::FADE_BLACK : TransitionType::FADE_WHITE;
    config.speed = speed;
    config.fade_color = color;
    return config;
}

TransitionConfig ScreenTransition::getSlideConfig(TransitionType slide_type, TransitionSpeed speed)
{
    TransitionConfig config = TransitionConfig::defaultConfig();
    config.type = slide_type;
    config.speed = speed;
    config.step_delay_ms = 100;
    return config;
}

TransitionConfig ScreenTransition::getCircleConfig(TransitionType circle_type, TransitionSpeed speed)
{
    TransitionConfig config = TransitionConfig::defaultConfig();
    config.type = circle_type;
    config.speed = speed;
    config.step_delay_ms = 200;
    return config;
}

TransitionConfig ScreenTransition::getEffectConfig(TransitionType effect_type, TransitionSpeed speed)
{
    TransitionConfig config = TransitionConfig::defaultConfig();
    config.type = effect_type;
    config.speed = speed;
    config.step_delay_ms = 150;
    return config;
}

TransitionConfig ScreenTransition::createCustomConfig(TransitionType type, int steps, uint32_t step_delay_ms)
{
    TransitionConfig config = TransitionConfig::defaultConfig();
    config.type = type;
    config.speed = static_cast<TransitionSpeed>(steps);
    config.step_delay_ms = step_delay_ms;
    return config;
}

// リソース解放（作業用キャンバスのみ）
void ScreenTransition::cleanup()
{
    ESP_LOGI(TAG, "Cleaning up external canvas transition resources...");

    // 作業用キャンバスのみ解放（外部キャンバスは解放しない）
    releaseWorkCanvas();

    // 外部キャンバスへの参照をクリア
    _sourceCanvas = nullptr;
    _targetCanvas = nullptr;

    _initialized = false;
    _state = TransitionState::IDLE;

    ESP_LOGI(TAG, "Cleanup completed. Zero internal memory usage.");
}