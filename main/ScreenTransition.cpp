// main/ScreenTransition.cpp
// 軽量版ステップベース画面遷移システムの実装（E-Paper最適化版）

#include "ScreenTransition.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include <cmath>
#include <algorithm>

// ログタグ
static const char *TAG = "LIGHT_TRANSITION";

// コンストラクタ
ScreenTransition::ScreenTransition(M5GFX *display)
    : _display(display), _initialized(false), _use_psram(true),
      _sourceCanvas(nullptr), _targetCanvas(nullptr), _workCanvas(nullptr),
      _state(TransitionState::IDLE), _currentStep(0), _totalSteps(0), _lastStepTime(0),
      _onTransitionStart(nullptr), _onTransitionStep(nullptr), _onTransitionComplete(nullptr)
{
    ESP_LOGI(TAG, "軽量版ScreenTransition constructor called");
}

// デストラクタ
ScreenTransition::~ScreenTransition()
{
    cleanup();
    ESP_LOGI(TAG, "軽量版ScreenTransition destructor called");
}

// 初期化処理
bool ScreenTransition::init(bool use_psram)
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

    _use_psram = use_psram;
    ESP_LOGI(TAG, "Initializing 軽量版ScreenTransition with PSRAM: %s",
             _use_psram ? "enabled" : "disabled");

    // PSRAMメモリチェック
    if (_use_psram)
    {
        size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t canvas_size = TRANSITION_WIDTH * TRANSITION_HEIGHT * 2; // RGB565
        size_t total_required = canvas_size * 3; // 3つのキャンバス

        ESP_LOGI(TAG, "PSRAM check - Free: %zu KB, Required: %zu KB", 
                 psram_free / 1024, total_required / 1024);

        if (psram_free < total_required)
        {
            ESP_LOGE(TAG, "Insufficient PSRAM memory. Required: %zu, Available: %zu",
                     total_required, psram_free);
            return false;
        }
    }

    // キャンバス作成
    if (!createCanvases())
    {
        ESP_LOGE(TAG, "Failed to create canvases");
        cleanup();
        return false;
    }

    _initialized = true;
    _state = TransitionState::IDLE;

    ESP_LOGI(TAG, "軽量版ScreenTransition initialized successfully");
    return true;
}

// キャンバス作成
bool ScreenTransition::createCanvases()
{
    ESP_LOGI(TAG, "Creating transition canvases...");

    // ソースキャンバス作成（元画面）
    _sourceCanvas = new M5Canvas(_display);
    if (!_sourceCanvas)
    {
        ESP_LOGE(TAG, "Failed to allocate source canvas");
        return false;
    }
    if (_use_psram) _sourceCanvas->setPsram(true);
    if (!_sourceCanvas->createSprite(TRANSITION_WIDTH, TRANSITION_HEIGHT))
    {
        ESP_LOGE(TAG, "Failed to create source sprite");
        return false;
    }

    // ターゲットキャンバス作成（次画面）
    _targetCanvas = new M5Canvas(_display);
    if (!_targetCanvas)
    {
        ESP_LOGE(TAG, "Failed to allocate target canvas");
        return false;
    }
    if (_use_psram) _targetCanvas->setPsram(true);
    if (!_targetCanvas->createSprite(TRANSITION_WIDTH, TRANSITION_HEIGHT))
    {
        ESP_LOGE(TAG, "Failed to create target sprite");
        return false;
    }

    // 作業用キャンバス作成（トランジション処理用）
    _workCanvas = new M5Canvas(_display);
    if (!_workCanvas)
    {
        ESP_LOGE(TAG, "Failed to allocate work canvas");
        return false;
    }
    if (_use_psram) _workCanvas->setPsram(true);
    if (!_workCanvas->createSprite(TRANSITION_WIDTH, TRANSITION_HEIGHT))
    {
        ESP_LOGE(TAG, "Failed to create work sprite");
        return false;
    }

    ESP_LOGI(TAG, "All canvases created successfully");
    return true;
}

// トランジション開始
bool ScreenTransition::startTransition(const TransitionConfig &config)
{
    if (!_initialized)
    {
        ESP_LOGE(TAG, "Not initialized");
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

    ESP_LOGI(TAG, "Starting 軽量版transition: type=%d, steps=%d, delay=%lums",
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
    if (_totalSteps <= 0) return 0.0f;
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

// 1ステップ実行（軽量版）
void ScreenTransition::executeStep()
{
    ESP_LOGD(TAG, "Executing step %d/%d (%.1f%%)", 
             _currentStep, _totalSteps, getStepProgress() * 100.0f);

    // トランジション効果を描画（軽量版のみ）
    switch (_config.type) {
        case TransitionType::NONE:
            // 即座に切り替え
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
            
        case TransitionType::VENETIAN_BLIND:
            renderVenetianBlindStep(_currentStep, _totalSteps);
            break;
            
        case TransitionType::CUT_IN:
            renderCutInStep(_currentStep, _totalSteps);
            break;
            
        case TransitionType::PUSH_LEFT:
        case TransitionType::PUSH_RIGHT:
            renderPushStep(_currentStep, _totalSteps);
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown transition type: %d", static_cast<int>(_config.type));
            renderFadeStep(_currentStep, _totalSteps); // フォールバック
            break;
    }
    
    // ステップコールバックを実行
    if (_onTransitionStep) {
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

        // 完了コールバックを実行
        if (_onTransitionComplete)
        {
            _onTransitionComplete();
        }

        ESP_LOGI(TAG, "軽量版transition completed");
        return false;
    }

    return true;
}

// FADE効果（超軽量版）
void ScreenTransition::renderFadeStep(int step, int totalSteps)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    ESP_LOGD(TAG, "Fade step %d/%d (Ultra-Light)", step, totalSteps);

    // 3段階フェード：ソース → フェード色 → ターゲット
    float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);
    
    if (progress < 0.4f) {
        // 前期：ソース画面をそのまま表示
        _sourceCanvas->pushSprite(0, 0);
    } else if (progress < 0.6f) {
        // 中期：フェード色を表示（1回のfillSprite + pushSpriteのみ）
        _workCanvas->fillSprite(_config.fade_color);
        _workCanvas->pushSprite(0, 0);
    } else {
        // 後期：ターゲット画面を表示
        _targetCanvas->pushSprite(0, 0);
    }
}

// SLIDE効果（軽量版）
void ScreenTransition::renderSlideStep(int step, int totalSteps)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    ESP_LOGD(TAG, "Slide step %d/%d (Light)", step, totalSteps);

    float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);
    int offset_x = 0, offset_y = 0;

    // 移動量計算
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

    // 作業キャンバスを黒でクリア
    _workCanvas->fillSprite(TFT_BLACK);

    // ソース画面をオフセット位置に描画（pushSprite使用で高速化）
    _sourceCanvas->pushSprite(_workCanvas, offset_x, offset_y);

    // ターゲット画面を反対側に描画
    int target_x = offset_x > 0 ? offset_x - TRANSITION_WIDTH : offset_x + TRANSITION_WIDTH;
    int target_y = offset_y > 0 ? offset_y - TRANSITION_HEIGHT : offset_y + TRANSITION_HEIGHT;
    _targetCanvas->pushSprite(_workCanvas, target_x, target_y);

    // 結果を画面に表示（1回のpushSpriteのみ）
    _workCanvas->pushSprite(0, 0);
}

// WIPE効果（軽量版）
void ScreenTransition::renderWipeStep(int step, int totalSteps)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    ESP_LOGD(TAG, "Wipe step %d/%d (Light)", step, totalSteps);

    float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);

    // ソース画面をベースとしてコピー
    _sourceCanvas->pushSprite(_workCanvas, 0, 0);

    int wipe_pos = 0;
    if (_config.type == TransitionType::WIPE_LEFT)
    {
        // 左からワイプ
        wipe_pos = static_cast<int>(TRANSITION_WIDTH * progress);
        if (wipe_pos > 0)
        {
            copyCanvasRegion(_targetCanvas, _workCanvas, 0, 0, wipe_pos, TRANSITION_HEIGHT, 0, 0);
        }
    }
    else
    { // WIPE_RIGHT
        // 右からワイプ
        wipe_pos = static_cast<int>(TRANSITION_WIDTH * (1.0f - progress));
        if (wipe_pos < TRANSITION_WIDTH)
        {
            copyCanvasRegion(_targetCanvas, _workCanvas, wipe_pos, 0, 
                           TRANSITION_WIDTH - wipe_pos, TRANSITION_HEIGHT, wipe_pos, 0);
        }
    }

    // 結果を画面に表示（1回のpushSpriteのみ）
    _workCanvas->pushSprite(0, 0);
}

// VENETIAN_BLIND効果（中軽量版）
void ScreenTransition::renderVenetianBlindStep(int step, int totalSteps)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    ESP_LOGD(TAG, "Venetian blind step %d/%d (Medium-Light)", step, totalSteps);

    float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);
    const int blind_count = 8; // ブラインドの枚数（軽量化のため削減）
    const int blind_height = TRANSITION_HEIGHT / blind_count;

    // ソース画面をベース
    _sourceCanvas->pushSprite(_workCanvas, 0, 0);

    // 段階的にブラインドを開く
    int revealed_blinds = static_cast<int>(blind_count * progress);

    for (int i = 0; i < revealed_blinds; i++)
    {
        int y_start = i * blind_height;
        int y_end = std::min(y_start + blind_height, TRANSITION_HEIGHT);
        copyCanvasRegion(_targetCanvas, _workCanvas, 0, y_start, 
                        TRANSITION_WIDTH, y_end - y_start, 0, y_start);
    }

    // 結果を画面に表示（1回のpushSpriteのみ）
    _workCanvas->pushSprite(0, 0);
}

// CUT_IN効果（新規追加）
void ScreenTransition::renderCutInStep(int step, int totalSteps)
{
    if (!_sourceCanvas || !_targetCanvas) return;

    ESP_LOGD(TAG, "Cut-in step %d/%d (Ultra-Light)", step, totalSteps);

    // カットイン効果：短時間だけターゲット画面を表示
    if (step < totalSteps / 3) {
        // 前期：ソース画面
        _sourceCanvas->pushSprite(0, 0);
    } else if (step < totalSteps * 2 / 3) {
        // 中期：ターゲット画面（カットイン）
        _targetCanvas->pushSprite(0, 0);
    } else {
        // 後期：ソース画面に戻る
        _sourceCanvas->pushSprite(0, 0);
    }
}

// PUSH効果（新規追加）
void ScreenTransition::renderPushStep(int step, int totalSteps)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    ESP_LOGD(TAG, "Push step %d/%d (Light)", step, totalSteps);

    float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);
    int push_offset = 0;

    if (_config.type == TransitionType::PUSH_LEFT) {
        push_offset = static_cast<int>(-TRANSITION_WIDTH * progress);
    } else { // PUSH_RIGHT
        push_offset = static_cast<int>(TRANSITION_WIDTH * progress);
    }

    // 作業キャンバスをクリア
    _workCanvas->fillSprite(TFT_BLACK);

    // ソース画面を押し出される位置に描画
    _sourceCanvas->pushSprite(_workCanvas, push_offset, 0);

    // ターゲット画面を押し込んでくる位置に描画
    int target_x = (_config.type == TransitionType::PUSH_LEFT) ? 
                   push_offset + TRANSITION_WIDTH : push_offset - TRANSITION_WIDTH;
    _targetCanvas->pushSprite(_workCanvas, target_x, 0);

    // 結果を画面に表示（1回のpushSpriteのみ）
    _workCanvas->pushSprite(0, 0);
}

// グレースケール色取得（E-Paper用）
uint16_t ScreenTransition::getGrayscaleColor(float intensity)
{
    // intensity: 0.0(黒) - 1.0(白)
    intensity = std::max(0.0f, std::min(1.0f, intensity));

    // E-Paperの16階調グレースケール
    int gray_level = static_cast<int>(intensity * 15.0f);
    
    // RGB565形式でグレースケール色を作成
    uint16_t gray_5bit = (gray_level * 31) / 15;  // 5bit
    uint16_t gray_6bit = (gray_level * 63) / 15;  // 6bit
    
    return (gray_5bit << 11) | (gray_6bit << 5) | gray_5bit;
}

// キャンバス領域コピー（軽量版）
void ScreenTransition::copyCanvasRegion(M5Canvas *src, M5Canvas *dst, 
                                       int sx, int sy, int sw, int sh, int dx, int dy)
{
    if (!src || !dst) return;

    // 境界チェック
    if (sx < 0 || sy < 0 || dx < 0 || dy < 0) return;
    if (sx + sw > TRANSITION_WIDTH || sy + sh > TRANSITION_HEIGHT) return;
    if (dx + sw > TRANSITION_WIDTH || dy + sh > TRANSITION_HEIGHT) return;

    // 中サイズブロック単位でコピー（WDT対策 + 高速化）
    const int block_size = 32;

    for (int y = 0; y < sh; y += block_size)
    {
        for (int x = 0; x < sw; x += block_size)
        {
            int copy_width = std::min(block_size, sw - x);
            int copy_height = std::min(block_size, sh - y);

            // M5GFXのpushImageを使用して高速コピー
            uint16_t* buffer = new uint16_t[copy_width * copy_height];
            if (buffer) {
                // ソースから読み取り（効率化のため一括読み取り）
                for (int by = 0; by < copy_height; by++) {
                    for (int bx = 0; bx < copy_width; bx++) {
                        buffer[by * copy_width + bx] = 
                            src->readPixel(sx + x + bx, sy + y + by);
                    }
                }
                
                // デスティネーションに一括書き込み
                dst->pushImage(dx + x, dy + y, copy_width, copy_height, buffer);
                delete[] buffer;
            }
        }

        // WDTタイムアウト防止（軽量化のため頻度を下げる）
        if (y % (block_size * 4) == 0) {
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
        ESP_LOGI(TAG, "軽量版transition cancelled");
    }
}

// 即座に画面切り替え
void ScreenTransition::switchImmediate()
{
    if (_targetCanvas)
    {
        _targetCanvas->pushSprite(0, 0);
        ESP_LOGI(TAG, "Immediate screen switch");
    }
}

// 簡易トランジション実行
void ScreenTransition::transition(std::function<void(M5Canvas *)> prepare_func, 
                                 const TransitionConfig &config)
{
    if (!_initialized)
    {
        ESP_LOGE(TAG, "Not initialized");
        return;
    }

    // 現在の画面をキャプチャ
    captureSource();

    // 次画面を準備
    prepareTarget(prepare_func);

    // トランジション開始
    startTransition(config);
}

// 元画面のキャプチャ
void ScreenTransition::captureSource()
{
    if (!_initialized || !_sourceCanvas)
    {
        ESP_LOGE(TAG, "Not initialized or source canvas not available");
        return;
    }

    ESP_LOGI(TAG, "Capturing source screen...");
    // 通常は呼び出し元で画面内容を事前に準備
    _sourceCanvas->fillSprite(TFT_BLACK);
    ESP_LOGI(TAG, "Source screen captured");
}

// 次画面の準備
void ScreenTransition::prepareTarget(std::function<void(M5Canvas *)> prepare_func)
{
    if (!_initialized || !_targetCanvas)
    {
        ESP_LOGE(TAG, "Not initialized or target canvas not available");
        return;
    }

    ESP_LOGI(TAG, "Preparing target screen...");
    _targetCanvas->fillSprite(TFT_BLACK);

    if (prepare_func)
    {
        prepare_func(_targetCanvas);
    }

    ESP_LOGI(TAG, "Target screen prepared");
}

// カスタム設定作成
TransitionConfig ScreenTransition::createCustomConfig(TransitionType type, int steps, 
                                                     uint32_t step_delay_ms)
{
    TransitionConfig config = TransitionConfig::defaultConfig();
    config.type = type;
    config.speed = static_cast<TransitionSpeed>(steps);
    config.step_delay_ms = step_delay_ms;
    return config;
}

// リソース解放
void ScreenTransition::cleanup()
{
    ESP_LOGI(TAG, "Cleaning up transition resources...");

    if (_sourceCanvas)
    {
        _sourceCanvas->deleteSprite();
        delete _sourceCanvas;
        _sourceCanvas = nullptr;
    }

    if (_targetCanvas)
    {
        _targetCanvas->deleteSprite();
        delete _targetCanvas;
        _targetCanvas = nullptr;
    }

    if (_workCanvas)
    {
        _workCanvas->deleteSprite();
        delete _workCanvas;
        _workCanvas = nullptr;
    }

    _initialized = false;
    _state = TransitionState::IDLE;
    ESP_LOGI(TAG, "Cleanup completed");
}