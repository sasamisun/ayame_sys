// main/ScreenTransition.cpp
// アドベンチャーゲーム用画面トランジションシステムの実装

#include "ScreenTransition.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include <cmath>

// ログタグ
static const char* TAG = "TRANSITION";

// 数学定数
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// コンストラクタ
ScreenTransition::ScreenTransition(M5GFX* display)
    : _display(display), _initialized(false), _use_psram(true),
      _sourceCanvas(nullptr), _targetCanvas(nullptr), _workCanvas(nullptr),
      _state(TransitionState::IDLE), _startTime(0), _currentTime(0), _progress(0.0f),
      _onTransitionStart(nullptr), _onTransitionProgress(nullptr), _onTransitionComplete(nullptr)
{
    ESP_LOGI(TAG, "ScreenTransition constructor called");
}

// デストラクタ
ScreenTransition::~ScreenTransition()
{
    cleanup();
    ESP_LOGI(TAG, "ScreenTransition destructor called");
}

// 初期化処理
bool ScreenTransition::init(bool use_psram)
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

    ESP_LOGI(TAG, "Initializing ScreenTransition with PSRAM: %s", 
             _use_psram ? "enabled" : "disabled");

    // PSRAMの使用可能容量をチェック
    if (_use_psram) {
        size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t canvas_size = TRANSITION_WIDTH * TRANSITION_HEIGHT * 2; // RGB565
        size_t total_required = canvas_size * 3; // 3つのキャンバス

        ESP_LOGI(TAG, "PSRAM free: %zu bytes, required: %zu bytes", psram_free, total_required);

        if (psram_free < total_required) {
            ESP_LOGE(TAG, "Insufficient PSRAM memory. Required: %zu, Available: %zu", 
                     total_required, psram_free);
            return false;
        }
    }

    // キャンバスを作成
    if (!createCanvases()) {
        ESP_LOGE(TAG, "Failed to create canvases");
        cleanup();
        return false;
    }

    _initialized = true;
    _state = TransitionState::IDLE;

    ESP_LOGI(TAG, "ScreenTransition initialized successfully");
    return true;
}

// キャンバス作成
bool ScreenTransition::createCanvases()
{
    ESP_LOGI(TAG, "Creating transition canvases...");

    // ソースキャンバス作成
    _sourceCanvas = new M5Canvas(_display);
    if (!_sourceCanvas) {
        ESP_LOGE(TAG, "Failed to allocate source canvas");
        return false;
    }

    if (_use_psram) {
        _sourceCanvas->setPsram(true);
    }

    if (!_sourceCanvas->createSprite(TRANSITION_WIDTH, TRANSITION_HEIGHT)) {
        ESP_LOGE(TAG, "Failed to create source sprite");
        return false;
    }
    ESP_LOGI(TAG, "Source canvas created");

    // ターゲットキャンバス作成
    _targetCanvas = new M5Canvas(_display);
    if (!_targetCanvas) {
        ESP_LOGE(TAG, "Failed to allocate target canvas");
        return false;
    }

    if (_use_psram) {
        _targetCanvas->setPsram(true);
    }

    if (!_targetCanvas->createSprite(TRANSITION_WIDTH, TRANSITION_HEIGHT)) {
        ESP_LOGE(TAG, "Failed to create target sprite");
        return false;
    }
    ESP_LOGI(TAG, "Target canvas created");

    // 作業用キャンバス作成
    _workCanvas = new M5Canvas(_display);
    if (!_workCanvas) {
        ESP_LOGE(TAG, "Failed to allocate work canvas");
        return false;
    }

    if (_use_psram) {
        _workCanvas->setPsram(true);
    }

    if (!_workCanvas->createSprite(TRANSITION_WIDTH, TRANSITION_HEIGHT)) {
        ESP_LOGE(TAG, "Failed to create work sprite");
        return false;
    }
    ESP_LOGI(TAG, "Work canvas created");

    return true;
}

// リソース解放
void ScreenTransition::cleanup()
{
    ESP_LOGI(TAG, "Cleaning up transition resources...");

    if (_sourceCanvas) {
        _sourceCanvas->deleteSprite();
        delete _sourceCanvas;
        _sourceCanvas = nullptr;
    }

    if (_targetCanvas) {
        _targetCanvas->deleteSprite();
        delete _targetCanvas;
        _targetCanvas = nullptr;
    }

    if (_workCanvas) {
        _workCanvas->deleteSprite();
        delete _workCanvas;
        _workCanvas = nullptr;
    }

    _initialized = false;
    _state = TransitionState::IDLE;
}

// 元画面のキャプチャ
void ScreenTransition::captureSource()
{
    if (!_initialized || !_sourceCanvas) {
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
void ScreenTransition::prepareTarget(std::function<void(M5Canvas*)> prepare_func)
{
    if (!_initialized || !_targetCanvas) {
        ESP_LOGE(TAG, "Not initialized or target canvas not available");
        return;
    }

    ESP_LOGI(TAG, "Preparing target screen...");

    // ターゲットキャンバスをクリア
    _targetCanvas->fillSprite(TFT_BLACK);

    // 準備関数を実行してターゲット画面を描画
    if (prepare_func) {
        prepare_func(_targetCanvas);
    }

    ESP_LOGI(TAG, "Target screen prepared");
}

// トランジション開始
bool ScreenTransition::startTransition(const TransitionConfig& config)
{
    if (!_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return false;
    }

    if (_state == TransitionState::RUNNING) {
        ESP_LOGW(TAG, "Transition already running");
        return false;
    }

    ESP_LOGI(TAG, "Starting transition: type=%d, duration=%lu ms", 
             static_cast<int>(config.type), config.duration_ms);

    _config = config;
    _state = TransitionState::RUNNING;
    _startTime = esp_timer_get_time();
    _progress = 0.0f;

    // 開始コールバックを実行
    if (_onTransitionStart) {
        _onTransitionStart();
    }

    return true;
}

// トランジション更新
bool ScreenTransition::updateTransition()
{
    if (_state != TransitionState::RUNNING) {
        return false;
    }

    // 進行度を更新
    updateProgress();

    // トランジション効果を描画
    switch (_config.type) {
        case TransitionType::NONE:
            // 即座に切り替え
            _progress = 1.0f;
            break;
            
        case TransitionType::FADE_BLACK:
        case TransitionType::FADE_WHITE:
            renderFade(_progress);
            break;
            
        case TransitionType::SLIDE_LEFT:
        case TransitionType::SLIDE_RIGHT:
        case TransitionType::SLIDE_UP:
        case TransitionType::SLIDE_DOWN:
            renderSlide(_progress);
            break;
            
        case TransitionType::WIPE_LEFT:
        case TransitionType::WIPE_RIGHT:
            renderWipe(_progress);
            break;
            
        case TransitionType::CIRCLE_EXPAND:
        case TransitionType::CIRCLE_SHRINK:
            renderCircle(_progress);
            break;
            
        case TransitionType::PIXELATE:
            renderPixelate(_progress);
            break;
            
        case TransitionType::VENETIAN_BLIND:
            renderVenetianBlind(_progress);
            break;
            
        case TransitionType::CHECKERBOARD:
            renderCheckerboard(_progress);
            break;
            
        case TransitionType::SPIRAL:
            renderSpiral(_progress);
            break;
            
        case TransitionType::RIPPLE:
            renderRipple(_progress);
            break;
            
        case TransitionType::MOSAIC:
            renderMosaic(_progress);
            break;
            
        case TransitionType::PAGE_TURN:
            renderPageTurn(_progress);
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown transition type: %d", static_cast<int>(_config.type));
            renderFade(_progress); // フォールバック
            break;
    }

    // 進行コールバックを実行
    if (_onTransitionProgress) {
        _onTransitionProgress(_progress);
    }

    // 完了チェック
    if (_progress >= 1.0f) {
        _state = TransitionState::FINISHED;
        
        // 最終画面を表示
        if (_targetCanvas) {
            _targetCanvas->pushSprite(0, 0);
        }

        // 完了コールバックを実行
        if (_onTransitionComplete) {
            _onTransitionComplete();
        }

        ESP_LOGI(TAG, "Transition completed");
        return false;
    }

    return true;
}

// 進行度更新
void ScreenTransition::updateProgress()
{
    _currentTime = esp_timer_get_time();
    int64_t elapsed = _currentTime - _startTime;
    float raw_progress = static_cast<float>(elapsed) / (_config.duration_ms * 1000.0f);
    
    // 進行度を0.0-1.0にクランプ
    raw_progress = std::max(0.0f, std::min(1.0f, raw_progress));
    
    // 速度倍率を適用
    raw_progress *= _config.speed_multiplier;
    
    // 逆方向フラグを適用
    if (_config.reverse) {
        raw_progress = 1.0f - raw_progress;
    }
    
    // イージングを適用
    _progress = applyEasing(raw_progress);
}

// イージング適用
float ScreenTransition::applyEasing(float t)
{
    // tを0.0-1.0にクランプ
    t = std::max(0.0f, std::min(1.0f, t));
    
    switch (_config.easing) {
        case EasingType::LINEAR:
            return t;
            
        case EasingType::EASE_IN:
            return t * t;
            
        case EasingType::EASE_OUT:
            return 1.0f - (1.0f - t) * (1.0f - t);
            
        case EasingType::EASE_IN_OUT:
            if (t < 0.5f) {
                return 2.0f * t * t;
            } else {
                return -1.0f + (4.0f - 2.0f * t) * t;
            }
            
        case EasingType::BOUNCE:
            if (t < 1.0f / 2.75f) {
                return 7.5625f * t * t;
            } else if (t < 2.0f / 2.75f) {
                t -= 1.5f / 2.75f;
                return 7.5625f * t * t + 0.75f;
            } else if (t < 2.5f / 2.75f) {
                t -= 2.25f / 2.75f;
                return 7.5625f * t * t + 0.9375f;
            } else {
                t -= 2.625f / 2.75f;
                return 7.5625f * t * t + 0.984375f;
            }
            
        case EasingType::ELASTIC:
            if (t == 0.0f) return 0.0f;
            if (t == 1.0f) return 1.0f;
            return powf(2.0f, -10.0f * t) * sinf((t - 0.1f) * (2.0f * M_PI) / 0.4f) + 1.0f;
            
        case EasingType::BACK:
        {
            float c1 = 1.70158f;
            float c3 = c1 + 1.0f;
            return c3 * t * t * t - c1 * t * t;
        }
        
        case EasingType::CUBIC:
            return t * t * (3.0f - 2.0f * t);
            
        default:
            return t;
    }
}

// フェード効果
void ScreenTransition::renderFade(float progress)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    // 作業キャンバスをクリア
    _workCanvas->fillSprite(_config.color);

    if (progress < 0.5f) {
        // 前半：ソース画面からフェード色へ
        float alpha = progress * 2.0f;
        blendCanvases(_sourceCanvas, _workCanvas, alpha);
    } else {
        // 後半：フェード色からターゲット画面へ
        float alpha = (progress - 0.5f) * 2.0f;
        blendCanvases(_workCanvas, _targetCanvas, alpha);
    }

    // 結果を画面に表示
    _workCanvas->pushSprite(0, 0);
}

// スライド効果
void ScreenTransition::renderSlide(float progress)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    int offset_x = 0, offset_y = 0;

    switch (_config.type) {
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

// ワイプ効果
void ScreenTransition::renderWipe(float progress)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    // ソース画面をベースとしてコピー
    copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);

    int wipe_pos = 0;
    if (_config.type == TransitionType::WIPE_LEFT) {
        wipe_pos = static_cast<int>(TRANSITION_WIDTH * progress);
        // 左からワイプ
        copyCanvasRegion(_targetCanvas, _workCanvas, 0, 0, wipe_pos, TRANSITION_HEIGHT, 0, 0);
    } else { // WIPE_RIGHT
        wipe_pos = static_cast<int>(TRANSITION_WIDTH * (1.0f - progress));
        // 右からワイプ
        copyCanvasRegion(_targetCanvas, _workCanvas, wipe_pos, 0, TRANSITION_WIDTH - wipe_pos, TRANSITION_HEIGHT, wipe_pos, 0);
    }

    // 結果を画面に表示
    _workCanvas->pushSprite(0, 0);
}

// 円形効果
void ScreenTransition::renderCircle(float progress)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    int center_x = TRANSITION_WIDTH / 2;
    int center_y = TRANSITION_HEIGHT / 2;
    int max_radius = static_cast<int>(sqrt(center_x * center_x + center_y * center_y));

    if (_config.type == TransitionType::CIRCLE_EXPAND) {
        // 円形拡大：中心から外側へ
        int radius = static_cast<int>(max_radius * progress);
        
        // ソース画面をベースとしてコピー
        copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);
        
        // ターゲット画面を円形にマスクして描画
        for (int y = 0; y < TRANSITION_HEIGHT; y++) {
            for (int x = 0; x < TRANSITION_WIDTH; x++) {
                int dx = x - center_x;
                int dy = y - center_y;
                int distance = static_cast<int>(sqrt(dx * dx + dy * dy));
                
                if (distance <= radius) {
                    // 円の内側はターゲット画面
                    uint16_t pixel = _targetCanvas->readPixel(x, y);
                    _workCanvas->writePixel(x, y, pixel);
                }
            }
        }
    } else { // CIRCLE_SHRINK
        // 円形縮小：外側から中心へ
        int radius = static_cast<int>(max_radius * (1.0f - progress));
        
        // ターゲット画面をベースとしてコピー
        copyCanvasRegion(_targetCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);
        
        // ソース画面を円形にマスクして描画
        for (int y = 0; y < TRANSITION_HEIGHT; y++) {
            for (int x = 0; x < TRANSITION_WIDTH; x++) {
                int dx = x - center_x;
                int dy = y - center_y;
                int distance = static_cast<int>(sqrt(dx * dx + dy * dy));
                
                if (distance <= radius) {
                    // 円の内側はソース画面
                    uint16_t pixel = _sourceCanvas->readPixel(x, y);
                    _workCanvas->writePixel(x, y, pixel);
                }
            }
        }
    }

    // 結果を画面に表示
    _workCanvas->pushSprite(0, 0);
}

// ピクセル化効果
void ScreenTransition::renderPixelate(float progress)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    // ピクセル化レベル（1.0で元サイズ、0.0で最大ピクセル化）
    float pixel_level = 1.0f - progress * 0.9f; // 完全には0にしない
    int pixel_size = static_cast<int>(1.0f / pixel_level);
    pixel_size = std::max(1, std::min(pixel_size, 32)); // 1-32ピクセルの範囲

    // 進行度に応じてソースとターゲットをブレンド
    if (progress < 0.5f) {
        // 前半：ソース画面をピクセル化
        copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);
    } else {
        // 後半：ターゲット画面をピクセル化しながら表示
        float blend_ratio = (progress - 0.5f) * 2.0f;
        
        for (int y = 0; y < TRANSITION_HEIGHT; y += pixel_size) {
            for (int x = 0; x < TRANSITION_WIDTH; x += pixel_size) {
                // ピクセルブロックの色を取得
                uint16_t src_color = _sourceCanvas->readPixel(x, y);
                uint16_t tgt_color = _targetCanvas->readPixel(x, y);
                uint16_t blended_color = interpolateColor(src_color, tgt_color, blend_ratio);
                
                // ピクセルブロックを描画
                for (int dy = 0; dy < pixel_size && (y + dy) < TRANSITION_HEIGHT; dy++) {
                    for (int dx = 0; dx < pixel_size && (x + dx) < TRANSITION_WIDTH; dx++) {
                        _workCanvas->writePixel(x + dx, y + dy, blended_color);
                    }
                }
            }
        }
    }

    // 結果を画面に表示
    _workCanvas->pushSprite(0, 0);
}

// ブラインド効果
void ScreenTransition::renderVenetianBlind(float progress)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    const int blind_count = 20; // ブラインドの枚数
    const int blind_height = TRANSITION_HEIGHT / blind_count;

    // ソース画面をベース
    copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);

    for (int i = 0; i < blind_count; i++) {
        int y_start = i * blind_height;
        int y_end = std::min(y_start + blind_height, TRANSITION_HEIGHT);
        
        // 各ブラインドの開き具合を計算（少しずつ時間差をつける）
        float blind_progress = progress + (static_cast<float>(i) / blind_count) * 0.3f;
        blind_progress = std::max(0.0f, std::min(1.0f, blind_progress));
        
        int reveal_height = static_cast<int>((y_end - y_start) * blind_progress);
        
        if (reveal_height > 0) {
            // ターゲット画面の該当部分を描画
            copyCanvasRegion(_targetCanvas, _workCanvas, 0, y_start, TRANSITION_WIDTH, reveal_height, 0, y_start);
        }
    }

    // 結果を画面に表示
    _workCanvas->pushSprite(0, 0);
}

// チェッカーボード効果
void ScreenTransition::renderCheckerboard(float progress)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    const int checker_size = 32; // チェッカーのサイズ
    
    // ベースとしてソース画面をコピー
    copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);

    for (int y = 0; y < TRANSITION_HEIGHT; y += checker_size) {
        for (int x = 0; x < TRANSITION_WIDTH; x += checker_size) {
            // チェッカーパターンを計算
            bool is_checker = ((x / checker_size) + (y / checker_size)) % 2 == 0;
            
            // 各チェッカーの変化タイミングをずらす
            float checker_progress = progress;
            if (is_checker) {
                checker_progress += 0.3f; // 先に変化
            }
            checker_progress = std::max(0.0f, std::min(1.0f, checker_progress));
            
            if (checker_progress > 0.5f) {
                // チェッカー領域をターゲット画面に置き換え
                int width = std::min(checker_size, TRANSITION_WIDTH - x);
                int height = std::min(checker_size, TRANSITION_HEIGHT - y);
                copyCanvasRegion(_targetCanvas, _workCanvas, x, y, width, height, x, y);
            }
        }
    }

    // 結果を画面に表示
    _workCanvas->pushSprite(0, 0);
}

// 螺旋効果
void ScreenTransition::renderSpiral(float progress)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    int center_x = TRANSITION_WIDTH / 2;
    int center_y = TRANSITION_HEIGHT / 2;
    float max_angle = progress * 8.0f * M_PI; // 螺旋の角度

    // ソース画面をベース
    copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);

    // 螺旋の描画
    for (int y = 0; y < TRANSITION_HEIGHT; y++) {
        for (int x = 0; x < TRANSITION_WIDTH; x++) {
            int dx = x - center_x;
            int dy = y - center_y;
            float angle = atan2f(dy, dx) + M_PI;
            float distance = sqrtf(dx * dx + dy * dy);
            
            // 螺旋の進行度を計算
            float spiral_progress = (angle + distance * 0.01f) / (2.0f * M_PI);
            spiral_progress = fmodf(spiral_progress, 1.0f);
            
            if (spiral_progress < (max_angle / (8.0f * M_PI))) {
                uint16_t pixel = _targetCanvas->readPixel(x, y);
                _workCanvas->writePixel(x, y, pixel);
            }
        }
    }

    // 結果を画面に表示
    _workCanvas->pushSprite(0, 0);
}

// 波紋効果
void ScreenTransition::renderRipple(float progress)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    int center_x = TRANSITION_WIDTH / 2;
    int center_y = TRANSITION_HEIGHT / 2;
    float max_radius = sqrtf(center_x * center_x + center_y * center_y);
    float current_radius = max_radius * progress;

    // ベースとしてソース画面をコピー
    copyCanvasRegion(_sourceCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);

    // 波紋効果
    for (int y = 0; y < TRANSITION_HEIGHT; y++) {
        for (int x = 0; x < TRANSITION_WIDTH; x++) {
            int dx = x - center_x;
            int dy = y - center_y;
            float distance = sqrtf(dx * dx + dy * dy);
            
            // 波紋の位置での歪み計算
            if (distance <= current_radius) {
                float wave_factor = sinf((distance / current_radius) * 2.0f * M_PI * 3.0f) * 0.1f;
                float blend_ratio = (distance / current_radius) + wave_factor;
                blend_ratio = std::max(0.0f, std::min(1.0f, blend_ratio));
                
                uint16_t src_color = _sourceCanvas->readPixel(x, y);
                uint16_t tgt_color = _targetCanvas->readPixel(x, y);
                uint16_t blended_color = interpolateColor(src_color, tgt_color, blend_ratio);
                _workCanvas->writePixel(x, y, blended_color);
            }
        }
    }

    // 結果を画面に表示
    _workCanvas->pushSprite(0, 0);
}

// モザイク効果
void ScreenTransition::renderMosaic(float progress)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    const int max_mosaic_size = 16;
    int mosaic_size = static_cast<int>(max_mosaic_size * (1.0f - progress)) + 1;

    // モザイク効果を適用
    for (int y = 0; y < TRANSITION_HEIGHT; y += mosaic_size) {
        for (int x = 0; x < TRANSITION_WIDTH; x += mosaic_size) {
            // モザイクブロックの代表色を計算
            uint16_t src_color = _sourceCanvas->readPixel(x, y);
            uint16_t tgt_color = _targetCanvas->readPixel(x, y);
            uint16_t blend_color = interpolateColor(src_color, tgt_color, progress);
            
            // モザイクブロックを描画
            for (int dy = 0; dy < mosaic_size && (y + dy) < TRANSITION_HEIGHT; dy++) {
                for (int dx = 0; dx < mosaic_size && (x + dx) < TRANSITION_WIDTH; dx++) {
                    _workCanvas->writePixel(x + dx, y + dy, blend_color);
                }
            }
        }
    }

    // 結果を画面に表示
    _workCanvas->pushSprite(0, 0);
}

// ページめくり効果
void ScreenTransition::renderPageTurn(float progress)
{
    if (!_workCanvas || !_sourceCanvas || !_targetCanvas) return;

    // ページがめくれる位置を計算
    int fold_x = static_cast<int>(TRANSITION_WIDTH * progress);
    
    // ベースとしてターゲット画面をコピー
    copyCanvasRegion(_targetCanvas, _workCanvas, 0, 0, TRANSITION_WIDTH, TRANSITION_HEIGHT, 0, 0);

    // めくれていない部分はソース画面を表示
    if (fold_x < TRANSITION_WIDTH) {
        copyCanvasRegion(_sourceCanvas, _workCanvas, fold_x, 0, TRANSITION_WIDTH - fold_x, TRANSITION_HEIGHT, fold_x, 0);
        
        // ページの影を追加
        if (fold_x > 10) {
            for (int x = fold_x - 10; x < fold_x; x++) {
                if (x >= 0 && x < TRANSITION_WIDTH) {
                    float shadow_intensity = static_cast<float>(fold_x - x) / 10.0f;
                    uint32_t shadow_color = interpolateColor(TFT_BLACK, _sourceCanvas->readPixel(x, TRANSITION_HEIGHT / 2), 1.0f - shadow_intensity * 0.5f);
                    
                    for (int y = 0; y < TRANSITION_HEIGHT; y++) {
                        _workCanvas->writePixel(x, y, shadow_color);
                    }
                }
            }
        }
    }

    // 結果を画面に表示
    _workCanvas->pushSprite(0, 0);
}

// ユーティリティ関数：キャンバスブレンド
void ScreenTransition::blendCanvases(M5Canvas* src, M5Canvas* dst, float alpha)
{
    if (!src || !dst) return;
    
    alpha = std::max(0.0f, std::min(1.0f, alpha));
    
    for (int y = 0; y < TRANSITION_HEIGHT; y++) {
        for (int x = 0; x < TRANSITION_WIDTH; x++) {
            uint16_t src_color = src->readPixel(x, y);
            uint16_t dst_color = dst->readPixel(x, y);
            uint16_t blended = interpolateColor(src_color, dst_color, alpha);
            dst->writePixel(x, y, blended);
        }
    }
}

// ユーティリティ関数：キャンバス領域コピー
void ScreenTransition::copyCanvasRegion(M5Canvas* src, M5Canvas* dst, int sx, int sy, int sw, int sh, int dx, int dy)
{
    if (!src || !dst) return;
    
    for (int y = 0; y < sh; y++) {
        for (int x = 0; x < sw; x++) {
            int src_x = sx + x;
            int src_y = sy + y;
            int dst_x = dx + x;
            int dst_y = dy + y;
            
            // 境界チェック
            if (src_x >= 0 && src_x < TRANSITION_WIDTH && src_y >= 0 && src_y < TRANSITION_HEIGHT &&
                dst_x >= 0 && dst_x < TRANSITION_WIDTH && dst_y >= 0 && dst_y < TRANSITION_HEIGHT) {
                uint16_t pixel = src->readPixel(src_x, src_y);
                dst->writePixel(dst_x, dst_y, pixel);
            }
        }
    }
}

// ユーティリティ関数：色補間
uint32_t ScreenTransition::interpolateColor(uint32_t color1, uint32_t color2, float t)
{
    t = std::max(0.0f, std::min(1.0f, t));
    
    // RGB565からRGB888に変換
    uint8_t r1 = (color1 >> 11) & 0x1F;
    uint8_t g1 = (color1 >> 5) & 0x3F;
    uint8_t b1 = color1 & 0x1F;
    
    uint8_t r2 = (color2 >> 11) & 0x1F;
    uint8_t g2 = (color2 >> 5) & 0x3F;
    uint8_t b2 = color2 & 0x1F;
    
    // 補間
    uint8_t r = static_cast<uint8_t>(r1 + (r2 - r1) * t);
    uint8_t g = static_cast<uint8_t>(g1 + (g2 - g1) * t);
    uint8_t b = static_cast<uint8_t>(b1 + (b2 - b1) * t);
    
    // RGB565に戻す
    return ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F);
}

// トランジション停止
void ScreenTransition::stopTransition()
{
    if (_state == TransitionState::RUNNING) {
        _state = TransitionState::CANCELLED;
        ESP_LOGI(TAG, "Transition cancelled");
    }
}

// 即座に画面切り替え
void ScreenTransition::switchImmediate()
{
    if (_targetCanvas) {
        _targetCanvas->pushSprite(0, 0);
        ESP_LOGI(TAG, "Immediate screen switch");
    }
}

// 簡易トランジション実行
void ScreenTransition::transition(std::function<void(M5Canvas*)> prepare_func, const TransitionConfig& config)
{
    if (!_initialized) {
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

// プリセット設定関数
TransitionConfig ScreenTransition::getFadeConfig(uint32_t duration_ms, uint32_t color)
{
    TransitionConfig config = TransitionConfig::defaultConfig();
    config.type = (color == TFT_BLACK) ? TransitionType::FADE_BLACK : TransitionType::FADE_WHITE;
    config.duration_ms = duration_ms;
    config.color = color;
    return config;
}

TransitionConfig ScreenTransition::getSlideConfig(TransitionType slide_type, uint32_t duration_ms)
{
    TransitionConfig config = TransitionConfig::defaultConfig();
    config.type = slide_type;
    config.duration_ms = duration_ms;
    config.easing = EasingType::EASE_OUT;
    return config;
}

TransitionConfig ScreenTransition::getCircleConfig(TransitionType circle_type, uint32_t duration_ms)
{
    TransitionConfig config = TransitionConfig::defaultConfig();
    config.type = circle_type;
    config.duration_ms = duration_ms;
    config.easing = EasingType::EASE_IN_OUT;
    return config;
}

TransitionConfig ScreenTransition::getEffectConfig(TransitionType effect_type, uint32_t duration_ms)
{
    TransitionConfig config = TransitionConfig::defaultConfig();
    config.type = effect_type;
    config.duration_ms = duration_ms;
    config.easing = EasingType::EASE_IN_OUT;
    return config;
}