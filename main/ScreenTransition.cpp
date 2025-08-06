// main/ScreenTransition.cpp
// ステップベース画面遷移システムの実装（E-Paper最適化版）

#include "ScreenTransition.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include <cmath>

// ログタグ
static const char *TAG = "STEP_TRANSITION";

// コンストラクタ
ScreenTransition::ScreenTransition(M5GFX *display)
    : _display(display), _initialized(false), _use_psram(true),
      _sourceCanvas(nullptr), _targetCanvas(nullptr), _workCanvas(nullptr),
      _state(TransitionState::IDLE), _currentStep(0), _totalSteps(0), _lastStepTime(0),
      _onTransitionStart(nullptr), _onTransitionStep(nullptr), _onTransitionComplete(nullptr)
{
    ESP_LOGI(TAG, "Step-based ScreenTransition constructor called");
}

// デストラクタ
ScreenTransition::~ScreenTransition()
{
    cleanup();
    ESP_LOGI(TAG, "Step-based ScreenTransition destructor called");
}

// 初期化処理（前回と同じ）
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
    ESP_LOGI(TAG, "Initializing Step-based ScreenTransition with PSRAM: %s",
             _use_psram ? "enabled" : "disabled");

    // PSRAMチェック（前回と同じ）
    if (_use_psram)
    {
        size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t canvas_size = TRANSITION_WIDTH * TRANSITION_HEIGHT * 2;
        size_t total_required = canvas_size * 3;

        if (psram_free < total_required)
        {
            ESP_LOGE(TAG, "Insufficient PSRAM memory. Required: %zu, Available: %zu",
                     total_required, psram_free);
            return false;
        }
    }

    // キャンバス作成（前回と同じ）
    if (!createCanvases())
    {
        ESP_LOGE(TAG, "Failed to create canvases");
        cleanup();
        return false;
    }

    _initialized = true;
    _state = TransitionState::IDLE;

    ESP_LOGI(TAG, "Step-based ScreenTransition initialized successfully");
    return true;
}

// キャンバス作成（前回と同じ）
bool ScreenTransition::createCanvases()
{
    ESP_LOGI(TAG, "Creating transition canvases...");

    // ソースキャンバス作成
    _sourceCanvas = new M5Canvas(_display);
    if (!_sourceCanvas)
    {
        ESP_LOGE(TAG, "Failed to allocate source canvas");
        return false;
    }
    if (_use_psram)
        _sourceCanvas->setPsram(true);
    if (!_sourceCanvas->createSprite(TRANSITION_WIDTH, TRANSITION_HEIGHT))
    {
        ESP_LOGE(TAG, "Failed to create source sprite");
        return false;
    }

    // ターゲットキャンバス作成
    _targetCanvas = new M5Canvas(_display);
    if (!_targetCanvas)
    {
        ESP_LOGE(TAG, "Failed to allocate target canvas");
        return false;
    }
    if (_use_psram)
        _targetCanvas->setPsram(true);
    if (!_targetCanvas->createSprite(TRANSITION_WIDTH, TRANSITION_HEIGHT))
    {
        ESP_LOGE(TAG, "Failed to create target sprite");
        return false;
    }

    // 作業用キャンバス作成
    _workCanvas = new M5Canvas(_display);
    if (!_workCanvas)
    {
        ESP_LOGE(TAG, "Failed to allocate work canvas");
        return false;
    }
    if (_use_psram)
        _workCanvas->setPsram(true);
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

    ESP_LOGI(TAG, "Starting step-based transition: type=%d, steps=%d, delay=%lums",
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

    // トランジション効果を描画（全てCanvas-Only方式）
    switch (_config.type) {
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
            
        default:
            ESP_LOGW(TAG, "Unknown transition type: %d", static_cast<int>(_config.type));
            renderFadeStep(_currentStep, _totalSteps); // フォールバック
            break;
    }
    
    // ✅ E-Paper更新は完全削除
    // updateEPaperDisplay(); // <-- 削除！
    
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

        ESP_LOGI(TAG, "Step-based transition completed");
        return false;
    }

    return true;
}

// FADE_BLACK/FADE_WHITE効果（ステップベース）
void ScreenTransition::renderFadeStep(int step, int totalSteps)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    ESP_LOGD(TAG, "Fade step %d/%d (Canvas-Only)", step, totalSteps);

    if (totalSteps <= 2) {
        // 2ステップの場合：即座切り替え
        if (step == 0) {
            _sourceCanvas->pushSprite(0, 0);
        } else {
            _targetCanvas->pushSprite(0, 0);
        }
    } else if (totalSteps <= 4) {
        // 4ステップの場合：ソース→フェード色→ターゲット
        if (step < totalSteps / 2) {
            // ✅ ソース画面をそのまま表示
            _sourceCanvas->pushSprite(0, 0);
        } else if (step == totalSteps / 2) {
            // ✅ フェード色をCanvasで作成してpushSprite
            _workCanvas->fillSprite(_config.fade_color);
            _workCanvas->pushSprite(0, 0);
        } else {
            // ✅ ターゲット画面を表示
            _targetCanvas->pushSprite(0, 0);
        }
    } else {
        // 8ステップ以上：グレースケールフェード
        float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);
        
        if (progress < 0.3f) {
            // 前期：ソース画面
            _sourceCanvas->pushSprite(0, 0);
        } else if (progress < 0.7f) {
            // 中期：フェード処理
            float fade_progress = (progress - 0.3f) / 0.4f; // 0.0-1.0に正規化
            
            // ✅ Canvas経由でフェード色を表示
            uint16_t fade_color;
            if (_config.type == TransitionType::FADE_WHITE) {
                fade_color = getGrayscaleColor(fade_progress);
            } else {
                fade_color = getGrayscaleColor(1.0f - fade_progress);
            }
            
            // ✅ 作業Canvasを使用（fillScreen使わない）
            _workCanvas->fillSprite(fade_color);
            _workCanvas->pushSprite(0, 0);
        } else {
            // 後期：ターゲット画面
            _targetCanvas->pushSprite(0, 0);
        }
    }
    
    // ✅ E-Paper更新は削除（pushSpriteのみで十分）
    // updateEPaperDisplay(); // <-- これも削除
}

// グレースケール色取得（E-Paper用）
uint16_t ScreenTransition::getGrayscaleColor(float intensity)
{
    // intensity: 0.0(黒) - 1.0(白)
    intensity = std::max(0.0f, std::min(1.0f, intensity));

    // E-Paperの16階調に対応
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

// キャンバス領域コピー（E-Paper最適化版）
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
                    if (src_x >= 0 && src_x < TRANSITION_WIDTH && src_y >= 0 && src_y < TRANSITION_HEIGHT &&
                        dst_x >= 0 && dst_x < TRANSITION_WIDTH && dst_y >= 0 && dst_y < TRANSITION_HEIGHT)
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
        ESP_LOGI(TAG, "Step-based transition cancelled");
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
    config.step_delay_ms = 100; // スライドは少し速めに
    return config;
}

TransitionConfig ScreenTransition::getCircleConfig(TransitionType circle_type, TransitionSpeed speed)
{
    TransitionConfig config = TransitionConfig::defaultConfig();
    config.type = circle_type;
    config.speed = speed;
    config.step_delay_ms = 200; // 円形効果はゆっくりと
    return config;
}

TransitionConfig ScreenTransition::getEffectConfig(TransitionType effect_type, TransitionSpeed speed)
{
    TransitionConfig config = TransitionConfig::defaultConfig();
    config.type = effect_type;
    config.speed = speed;
    config.step_delay_ms = 150; // 標準的な速度
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

// 簡易トランジション実行
void ScreenTransition::transition(std::function<void(M5Canvas *)> prepare_func, const TransitionConfig &config)
{
    if (!_initialized)
    {
        ESP_LOGE(TAG, "Not initialized");
        return;
    }

    // 現在の画面をキャプチャ（実装依存）
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

    // 現在の画面をソースキャンバスにコピー
    // M5GFXでは画面からの読み取りは制限があるため、
    // 通常は呼び出し元で画面内容を事前に準備してもらう
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

    // ターゲットキャンバスをクリア
    _targetCanvas->fillSprite(TFT_BLACK);

    // 準備関数を実行してターゲット画面を描画
    if (prepare_func)
    {
        prepare_func(_targetCanvas);
    }

    ESP_LOGI(TAG, "Target screen prepared");
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
}

// スライド効果（ステップベース）
void ScreenTransition::renderSlideStep(int step, int totalSteps)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas)
        return;

    ESP_LOGD(TAG, "Slide step %d/%d", step, totalSteps);

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

// ワイプ効果（ステップベース）
void ScreenTransition::renderWipeStep(int step, int totalSteps)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas)
        return;

    ESP_LOGD(TAG, "Wipe step %d/%d", step, totalSteps);

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

// 円形効果（ステップベース - 簡略版）
void ScreenTransition::renderCircleStep(int step, int totalSteps)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    ESP_LOGD(TAG, "Circle step %d/%d (Canvas-Only)", step, totalSteps);

    float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);

    // 簡略版：中央部分だけ段階的に切り替え
    int center_x = TRANSITION_WIDTH / 2;
    int center_y = TRANSITION_HEIGHT / 2;
    int max_radius = std::min(center_x, center_y);
    int current_radius = static_cast<int>(max_radius * progress);

    if (_config.type == TransitionType::CIRCLE_EXPAND) {
        // ✅ ソース画面をベースとしてworkCanvasにコピー
        copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);
        
        // 中央の矩形部分をターゲット画面で置き換え
        int rect_size = current_radius * 2;
        int rect_x = center_x - current_radius;
        int rect_y = center_y - current_radius;
        
        if (rect_size > 0 && rect_x >= 0 && rect_y >= 0) {
            copyCanvasRegion(_targetCanvas, _workCanvas, rect_x, rect_y, rect_size, rect_size, rect_x, rect_y);
        }
    } else { // CIRCLE_SHRINK
        // ✅ ターゲット画面をベースとしてworkCanvasにコピー
        copyCanvasRegion(_targetCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);
        
        // 中央の矩形部分をソース画面で置き換え
        int rect_size = (max_radius - current_radius) * 2;
        int rect_x = center_x - (max_radius - current_radius);
        int rect_y = center_y - (max_radius - current_radius);
        
        if (rect_size > 0 && rect_x >= 0 && rect_y >= 0) {
            copyCanvasRegion(_sourceCanvas, _workCanvas, rect_x, rect_y, rect_size, rect_size, rect_x, rect_y);
        }
    }

    // ✅ Canvas経由で表示
    _workCanvas->pushSprite(0, 0);
}

// ピクセル化効果（ステップベース - 簡略版）
void ScreenTransition::renderPixelateStep(int step, int totalSteps)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas)
        return;

    ESP_LOGD(TAG, "Pixelate step %d/%d", step, totalSteps);

    float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);

    if (progress < 0.5f)
    {
        // 前半：ソース画面を表示
        copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);
    }
    else
    {
        // 後半：ターゲット画面を表示
        copyCanvasRegion(_targetCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);
    }

    // 結果を画面に表示
    _workCanvas->pushSprite(0, 0);
}

// ブラインド効果（ステップベース）
void ScreenTransition::renderVenetianBlindStep(int step, int totalSteps)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas)
        return;

    ESP_LOGD(TAG, "Venetian blind step %d/%d", step, totalSteps);

    float progress = static_cast<float>(step) / static_cast<float>(totalSteps - 1);
    const int blind_count = 10; // ブラインドの枚数
    const int blind_height = TRANSITION_HEIGHT / blind_count;

    // ソース画面をベース
    copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);

    // 段階的にブラインドを開く
    int revealed_blinds = static_cast<int>(blind_count * progress);

    for (int i = 0; i < revealed_blinds; i++)
    {
        int y_start = i * blind_height;
        int y_end = std::min(y_start + blind_height, TRANSITION_HEIGHT);
        copyCanvasRegion(_targetCanvas, _workCanvas, 0, y_start, TRANSITION_WIDTH, y_end - y_start, 0, y_start);
    }

    // 結果を画面に表示
    _workCanvas->pushSprite(0, 0);
}

// 残りの効果も同様に簡略化（後で個別に実装可能）
void ScreenTransition::renderCheckerboardStep(int step, int totalSteps)
{
    // 簡略版：フェードで代用
    renderFadeStep(step, totalSteps);
}

void ScreenTransition::renderSpiralStep(int step, int totalSteps)
{
    // 簡略版：フェードで代用
    renderFadeStep(step, totalSteps);
}

void ScreenTransition::renderRippleStep(int step, int totalSteps)
{
    // 簡略版：フェードで代用
    renderFadeStep(step, totalSteps);
}

void ScreenTransition::renderMosaicStep(int step, int totalSteps)
{
    // 簡略版：フェードで代用
    renderFadeStep(step, totalSteps);
}

void ScreenTransition::renderPageTurnStep(int step, int totalSteps)
{
    // 簡略版：ワイプで代用
    renderWipeStep(step, totalSteps);
}