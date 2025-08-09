// main/SimpleTransition.cpp
// シンプルトランジションシステムの実装（M5GFX API修正版）

#include "SimpleTransition.hpp"
#include "esp_log.h"
#include "esp_random.h"
#include <algorithm>
#include <cmath>

// ログタグ
static const char* TAG = "SIMPLE_TRANSITION";

// コンストラクタ
SimpleTransition::SimpleTransition(M5GFX* display)
    : _display(display), _mainCanvas(nullptr), _state(SimpleTransitionState::IDLE),
      _currentStep(0), _onStart(nullptr), _onStep(nullptr), _onComplete(nullptr) {
    ESP_LOGI(TAG, "SimpleTransition constructor called");
}

// デストラクタ
SimpleTransition::~SimpleTransition() {
    stop();
    ESP_LOGI(TAG, "SimpleTransition destructor called");
}

// トランジション開始
bool SimpleTransition::start(M5Canvas* mainCanvas, const SimpleTransitionConfig& config) {
    if (!_display || !mainCanvas) {
        ESP_LOGE(TAG, "Invalid display or canvas");
        return false;
    }
    
    if (_state == SimpleTransitionState::RUNNING) {
        ESP_LOGW(TAG, "Transition already running, stopping current transition");
        stop();
    }
    
    _mainCanvas = mainCanvas;
    _config = config;
    _currentStep = 0;
    _state = SimpleTransitionState::RUNNING;
    
    ESP_LOGI(TAG, "Starting simple transition: type=%d, steps=%d", 
             static_cast<int>(config.type), config.total_steps);
    
    // ランダムブロック用の準備
    if (config.type == SimpleTransitionType::RANDOM_BLOCKS) {
        prepareRandomBlocks();
    }
    
    // 初期状態の画面をクリア
    if (config.clear_before_step) {
        _display->fillScreen(config.clear_color);
    }
    
    // 開始コールバックを実行
    if (_onStart) {
        _onStart();
    }
    
    return true;
}

// 1ステップ実行
bool SimpleTransition::step() {
    if (_state != SimpleTransitionState::RUNNING) {
        return false;
    }
    
    if (!_mainCanvas) {
        ESP_LOGE(TAG, "Main canvas is null");
        stop();
        return false;
    }
    
    // 現在ステップを実行
    executeCurrentStep();
    
    // ステップコールバックを実行
    if (_onStep) {
        _onStep(_currentStep, _config.total_steps);
    }
    
    // 次ステップに進む
    _currentStep++;
    
    // 完了チェック
    if (_currentStep >= _config.total_steps) {
        _state = SimpleTransitionState::FINISHED;
        
        // 最終画面を確実に表示
        _mainCanvas->pushSprite(0, 0);
        
        // 完了コールバックを実行
        if (_onComplete) {
            _onComplete();
        }
        
        ESP_LOGI(TAG, "Transition completed");
        return false;
    }
    
    return true;
}

// 現在ステップを実行
void SimpleTransition::executeCurrentStep() {
    ESP_LOGD(TAG, "Executing step %d/%d (%.1f%%)", 
             _currentStep, _config.total_steps, getProgress() * 100.0f);
    
    // ステップ前のクリア処理
    if (_config.clear_before_step && _currentStep > 0) {
        _display->fillScreen(_config.clear_color);
    }
    
    // トランジション効果に応じた描画処理
    switch (_config.type) {
        case SimpleTransitionType::NONE:
            // トランジションなし：即座に完成画面を表示
            _mainCanvas->pushSprite(0, 0);
            _currentStep = _config.total_steps; // 即座に完了
            break;
            
        case SimpleTransitionType::FADE_IN:
            renderFadeStep();
            break;
            
        case SimpleTransitionType::SLIDE_DOWN:
        case SimpleTransitionType::SLIDE_UP:
        case SimpleTransitionType::SLIDE_LEFT:
        case SimpleTransitionType::SLIDE_RIGHT:
            renderSlideStep();
            break;
            
        case SimpleTransitionType::WIPE_DOWN:
        case SimpleTransitionType::WIPE_UP:
        case SimpleTransitionType::WIPE_LEFT:
        case SimpleTransitionType::WIPE_RIGHT:
            renderWipeStep();
            break;
            
        case SimpleTransitionType::CENTER_OUT:
            renderCenterOutStep();
            break;
            
        case SimpleTransitionType::RANDOM_BLOCKS:
            renderRandomBlocksStep();
            break;
            
        case SimpleTransitionType::LINE_SCAN:
            renderLineScanStep();
            break;
            
        case SimpleTransitionType::TYPEWRITER:
            renderTypewriterStep();
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown transition type: %d", static_cast<int>(_config.type));
            renderFadeStep(); // フォールバック
            break;
    }
}

// 進行度取得
float SimpleTransition::getProgress() const {
    if (_config.total_steps <= 0) return 0.0f;
    return static_cast<float>(_currentStep) / static_cast<float>(_config.total_steps);
}

// フェードイン効果（クリッピングを使用）
void SimpleTransition::renderFadeStep() {
    float progress = getProgress();
    
    // プログレッシブ描画：上から下に段階的に表示
    int reveal_height = static_cast<int>(TRANSITION_HEIGHT * progress);
    
    if (reveal_height > 0) {
        // クリッピング領域を設定して部分描画
        _display->setClipRect(0, 0, TRANSITION_WIDTH, reveal_height);
        _mainCanvas->pushSprite(0, 0);
        _display->clearClipRect();
    }
}

// スライド効果（オフセット描画を使用）
void SimpleTransition::renderSlideStep() {
    float progress = getProgress();
    int offset_x = 0, offset_y = 0;
    
    switch (_config.type) {
        case SimpleTransitionType::SLIDE_DOWN:
            // 上から下にスライド
            offset_y = static_cast<int>(-TRANSITION_HEIGHT * (1.0f - progress));
            break;
            
        case SimpleTransitionType::SLIDE_UP:
            // 下から上にスライド
            offset_y = static_cast<int>(TRANSITION_HEIGHT * (1.0f - progress));
            break;
            
        case SimpleTransitionType::SLIDE_RIGHT:
            // 左から右にスライド
            offset_x = static_cast<int>(-TRANSITION_WIDTH * (1.0f - progress));
            break;
            
        case SimpleTransitionType::SLIDE_LEFT:
            // 右から左にスライド
            offset_x = static_cast<int>(TRANSITION_WIDTH * (1.0f - progress));
            break;
            
        default:
            break;
    }
    
    // オフセット位置に描画
    _mainCanvas->pushSprite(offset_x, offset_y);
}

// ワイプ効果（クリッピングを使用）
void SimpleTransition::renderWipeStep() {
    float progress = getProgress();
    
    switch (_config.type) {
        case SimpleTransitionType::WIPE_DOWN: {
            // 上から下にワイプ
            int reveal_height = static_cast<int>(TRANSITION_HEIGHT * progress);
            if (reveal_height > 0) {
                _display->setClipRect(0, 0, TRANSITION_WIDTH, reveal_height);
                _mainCanvas->pushSprite(0, 0);
                _display->clearClipRect();
            }
            break;
        }
        
        case SimpleTransitionType::WIPE_UP: {
            // 下から上にワイプ
            int reveal_height = static_cast<int>(TRANSITION_HEIGHT * progress);
            int start_y = TRANSITION_HEIGHT - reveal_height;
            if (reveal_height > 0) {
                _display->setClipRect(0, start_y, TRANSITION_WIDTH, reveal_height);
                _mainCanvas->pushSprite(0, 0);
                _display->clearClipRect();
            }
            break;
        }
        
        case SimpleTransitionType::WIPE_RIGHT: {
            // 左から右にワイプ
            int reveal_width = static_cast<int>(TRANSITION_WIDTH * progress);
            if (reveal_width > 0) {
                _display->setClipRect(0, 0, reveal_width, TRANSITION_HEIGHT);
                _mainCanvas->pushSprite(0, 0);
                _display->clearClipRect();
            }
            break;
        }
        
        case SimpleTransitionType::WIPE_LEFT: {
            // 右から左にワイプ
            int reveal_width = static_cast<int>(TRANSITION_WIDTH * progress);
            int start_x = TRANSITION_WIDTH - reveal_width;
            if (reveal_width > 0) {
                _display->setClipRect(start_x, 0, reveal_width, TRANSITION_HEIGHT);
                _mainCanvas->pushSprite(0, 0);
                _display->clearClipRect();
            }
            break;
        }
        
        default:
            break;
    }
}

// 中央から外への効果（クリッピングを使用）
void SimpleTransition::renderCenterOutStep() {
    float progress = getProgress();
    
    // 中央から外に向かって矩形で表示
    int center_x = TRANSITION_WIDTH / 2;
    int center_y = TRANSITION_HEIGHT / 2;
    
    int reveal_width = static_cast<int>(TRANSITION_WIDTH * progress);
    int reveal_height = static_cast<int>(TRANSITION_HEIGHT * progress);
    
    int start_x = center_x - reveal_width / 2;
    int start_y = center_y - reveal_height / 2;
    
    // 境界チェック
    start_x = std::max(0, start_x);
    start_y = std::max(0, start_y);
    reveal_width = std::min(reveal_width, TRANSITION_WIDTH - start_x);
    reveal_height = std::min(reveal_height, TRANSITION_HEIGHT - start_y);
    
    if (reveal_width > 0 && reveal_height > 0) {
        _display->setClipRect(start_x, start_y, reveal_width, reveal_height);
        _mainCanvas->pushSprite(0, 0);
        _display->clearClipRect();
    }
}

// ランダムブロック効果の準備
void SimpleTransition::prepareRandomBlocks() {
    _randomBlocks.clear();
    
    const int block_size = 40; // ブロックサイズ
    int blocks_x = (TRANSITION_WIDTH + block_size - 1) / block_size;
    int blocks_y = (TRANSITION_HEIGHT + block_size - 1) / block_size;
    
    // 全ブロックを作成
    for (int y = 0; y < blocks_y; y++) {
        for (int x = 0; x < blocks_x; x++) {
            BlockInfo block;
            block.x = x * block_size;
            block.y = y * block_size;
            block.w = std::min(block_size, TRANSITION_WIDTH - block.x);
            block.h = std::min(block_size, TRANSITION_HEIGHT - block.y);
            
            // ランダムなステップで表示するように設定
            block.step_to_show = esp_random() % _config.total_steps;
            
            _randomBlocks.push_back(block);
        }
    }
    
    ESP_LOGI(TAG, "Prepared %zu random blocks", _randomBlocks.size());
}

// ランダムブロック効果（複数のクリッピング領域）
void SimpleTransition::renderRandomBlocksStep() {
    // 現在のステップで表示すべきブロックを順次描画
    for (const auto& block : _randomBlocks) {
        if (block.step_to_show <= _currentStep) {
            _display->setClipRect(block.x, block.y, block.w, block.h);
            _mainCanvas->pushSprite(0, 0);
            _display->clearClipRect();
        }
    }
}

// 線形スキャン効果（ライン単位のクリッピング）
void SimpleTransition::renderLineScanStep() {
    float progress = getProgress();
    
    // 水平線で上から下にスキャン（5ライン間隔）
    int scan_lines = static_cast<int>(TRANSITION_HEIGHT * progress);
    
    for (int y = 0; y < scan_lines; y += 5) {
        int line_height = std::min(5, TRANSITION_HEIGHT - y);
        if (line_height > 0) {
            _display->setClipRect(0, y, TRANSITION_WIDTH, line_height);
            _mainCanvas->pushSprite(0, 0);
            _display->clearClipRect();
        }
    }
}

// タイプライター効果（上から下、左から右に段階描画）
void SimpleTransition::renderTypewriterStep() {
    float progress = getProgress();
    
    // 上から下に行単位で表示
    int reveal_lines = static_cast<int>(TRANSITION_HEIGHT * progress / 20); // 20ピクセル単位
    int reveal_height = reveal_lines * 20;
    
    if (reveal_height > 0 && reveal_height <= TRANSITION_HEIGHT) {
        _display->setClipRect(0, 0, TRANSITION_WIDTH, reveal_height);
        _mainCanvas->pushSprite(0, 0);
        _display->clearClipRect();
    }
    
    // 最後の部分的な行の処理
    if (reveal_height < TRANSITION_HEIGHT) {
        float line_progress = fmod(progress * TRANSITION_HEIGHT / 20, 1.0f);
        int partial_width = static_cast<int>(TRANSITION_WIDTH * line_progress);
        
        if (partial_width > 0) {
            _display->setClipRect(0, reveal_height, partial_width, 20);
            _mainCanvas->pushSprite(0, 0);
            _display->clearClipRect();
        }
    }
}

// トランジション停止
void SimpleTransition::stop() {
    if (_state == SimpleTransitionState::RUNNING) {
        _state = SimpleTransitionState::IDLE;
        ESP_LOGI(TAG, "Transition stopped");
    }
    _randomBlocks.clear();
}

// 即座に完成画面を表示
void SimpleTransition::showImmediate() {
    if (_mainCanvas) {
        _mainCanvas->pushSprite(0, 0);
        ESP_LOGI(TAG, "Immediate show completed");
    }
    _state = SimpleTransitionState::FINISHED;
}

// トランジション完了まで一括実行
void SimpleTransition::runToCompletion() {
    if (_state != SimpleTransitionState::RUNNING) {
        ESP_LOGW(TAG, "Transition not running");
        return;
    }
    
    ESP_LOGI(TAG, "Running transition to completion...");
    
    while (step()) {
        vTaskDelay(pdMS_TO_TICKS(50)); // 50ms間隔で実行
    }
    
    ESP_LOGI(TAG, "Transition completed");
}