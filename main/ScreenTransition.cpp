// main/ScreenTransition.cpp
// main/ScreenTransition.cpp に追加するPSRAM詳細分析コード
#include "esp_psram.h"      // PSRAM専用関数
#include "esp_heap_caps.h"  // ヒープ管理
// 完全版 - 画面トランジション機能の実装
#include "ScreenTransition.hpp"
#include "esp_log.h"       // ESP-IDFログ機能
#include "esp_heap_caps.h" // ヒープメモリ情報
#include "esp_system.h"    // システム情報
#include <cmath>           // 数学関数
#include <cstdlib>         // 標準ライブラリ
#include <ctime>           // 時間関数
#include <algorithm>       // std::min, std::max

// ログタグ
const char *ScreenTransition::TAG = "SCREEN_TRANSITION";

// コンストラクタ
ScreenTransition::ScreenTransition(M5GFX *display)
    : _display(display), _oldScreen(nullptr), _newScreen(nullptr), _workBuffer(nullptr),
      _state(TransitionState::IDLE), _config(TransitionConfig::defaultConfig()),
      _startTime(0), _lastStepTime(0), _progress(0.0f),
      _screenWidth(0), _screenHeight(0),
      _onTransitionStart(nullptr), _onTransitionComplete(nullptr), _onTransitionStep(nullptr)
{
    if (_display)
    {
        _screenWidth = _display->width();
        _screenHeight = _display->height();
    }

    // 乱数の初期化
    srand(esp_timer_get_time() / 1000);

    ESP_LOGI(TAG, "ScreenTransition initialized with screen size: %dx%d", _screenWidth, _screenHeight);
}

// デストラクタ
ScreenTransition::~ScreenTransition()
{
    cleanupSprites();
    ESP_LOGI(TAG, "ScreenTransition destroyed");
}

// 初期化
/*
bool ScreenTransition::init()
{
    if (!_display)
    {
        ESP_LOGE(TAG, "Display not initialized");
        return false;
    }

    // 色深度を8bitに強制設定（E-Ink用最適化）
    int colorDepth = 8; // 16階調グレースケール用に8bitを使用

    // メモリ使用量の正しい計算
    size_t spriteSize = static_cast<size_t>(_screenWidth) *
                        static_cast<size_t>(_screenHeight) *
                        (colorDepth / 8);
    size_t totalMemoryNeeded = spriteSize * 3; // 3つのスプライト

    // 利用可能なヒープメモリをチェック
    size_t freeHeap = esp_get_free_heap_size();
    size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);

    ESP_LOGI(TAG, "Memory check - Screen: %dx%d, ColorDepth: %d bit",
             _screenWidth, _screenHeight, colorDepth);
    ESP_LOGI(TAG, "Sprite size: %zu bytes (%.2f MB)",
             spriteSize, spriteSize / (1024.0f * 1024.0f));
    ESP_LOGI(TAG, "Total needed: %zu bytes (%.2f MB), Free heap: %zu bytes (%.2f MB)",
             totalMemoryNeeded, totalMemoryNeeded / (1024.0f * 1024.0f),
             freeHeap, freeHeap / (1024.0f * 1024.0f));
    ESP_LOGI(TAG, "Largest block: %zu bytes (%.2f MB)",
             largestBlock, largestBlock / (1024.0f * 1024.0f));

    // メモリ不足の場合は代替案を提示
    if (totalMemoryNeeded > freeHeap)
    {
        ESP_LOGW(TAG, "Insufficient memory for full-screen sprites");
        ESP_LOGI(TAG, "Trying memory optimization strategies...");

        // 戦略1: 色深度を4bitに削減（16階調グレースケール）
        colorDepth = 4;
        spriteSize = static_cast<size_t>(_screenWidth) *
                     static_cast<size_t>(_screenHeight) *
                     (colorDepth / 8);
        totalMemoryNeeded = spriteSize * 3;

        ESP_LOGI(TAG, "Strategy 1 - 4bit color depth: %zu bytes needed", totalMemoryNeeded);

        if (totalMemoryNeeded <= freeHeap)
        {
            ESP_LOGI(TAG, "Using 4-bit color depth for E-Ink optimization");
            _optimizedMode = OptimizationMode::COLOR_DEPTH_4BIT;
        }
        else
        {
            // 戦略2: 部分トランジション（画面を分割）
            ESP_LOGI(TAG, "Strategy 2 - Using partial screen transitions");
            _optimizedMode = OptimizationMode::PARTIAL_SCREEN;
            return initPartialTransition();
        }
    }
    else
    {
        _optimizedMode = OptimizationMode::FULL_SCREEN;
        ESP_LOGI(TAG, "Using full-screen 8-bit transitions");
    }

    // スプライトの初期化
    return initializeSprites(colorDepth);
}
// 修正版の初期化関数（デバッグ情報付き）
bool ScreenTransition::init() {
    if (!_display) {
        ESP_LOGE(TAG, "Display not initialized");
        return false;
    }
    
    // 詳細なメモリ情報を表示
    debugMemoryInfo();
    
    // M5GFXの色深度サポートをテスト
    testColorDepthSupport();
    
    // スプライト割り当て限界をテスト
    testSpriteAllocation();
    
    // 通常の初期化を試行
    ESP_LOGI(TAG, "Attempting normal sprite initialization...");
    
    // 色深度を8bitに設定
    int colorDepth = 8;
    size_t spriteSize = static_cast<size_t>(_screenWidth) * 
                       static_cast<size_t>(_screenHeight) * 
                       (colorDepth / 8);
    
    ESP_LOGI(TAG, "Target sprite size: %dx%d, %d-bit, %zu bytes", 
             _screenWidth, _screenHeight, colorDepth, spriteSize);
    
    // まず1つのスプライトだけで試してみる
    ESP_LOGI(TAG, "Testing single sprite creation...");
    
    _oldScreen = new lgfx::LGFX_Sprite(_display);
    if (!_oldScreen) {
        ESP_LOGE(TAG, "Failed to allocate sprite object");
        return false;
    }
    
    _oldScreen->setColorDepth(colorDepth);
    
    ESP_LOGI(TAG, "Sprite object created, attempting createSprite...");
    size_t free_before = esp_get_free_heap_size();
    
    bool success = _oldScreen->createSprite(_screenWidth, _screenHeight);
    
    size_t free_after = esp_get_free_heap_size();
    size_t used_memory = free_before - free_after;
    
    ESP_LOGI(TAG, "createSprite result: %s", success ? "SUCCESS" : "FAILED");
    ESP_LOGI(TAG, "Memory used: %zu bytes (expected: %zu bytes)", used_memory, spriteSize);
    
    if (!success) {
        ESP_LOGE(TAG, "Failed to create sprite - investigating alternatives...");
        
        // PSRAMでの作成を試してみる
        delete _oldScreen;
        _oldScreen = nullptr;
        
        ESP_LOGI(TAG, "Trying PSRAM allocation...");
        if (createSpriteInPSRAM(&_oldScreen, _screenWidth, _screenHeight, colorDepth)) {
            ESP_LOGI(TAG, "PSRAM allocation successful!");
        } else {
            ESP_LOGE(TAG, "PSRAM allocation also failed");
            return false;
        }
    }
    
    // 成功した場合はクリーンアップして終了
    if (_oldScreen) {
        _oldScreen->deleteSprite();
        delete _oldScreen;
        _oldScreen = nullptr;
    }
    
    ESP_LOGI(TAG, "Debug analysis complete");
    return false; // とりあえずfalseを返してデバッグ情報だけ見る
}


// 修正版の初期化関数（安全なデバッグ版）
bool ScreenTransition::init() {
    if (!_display) {
        ESP_LOGE(TAG, "Display not initialized");
        return false;
    }
    
    // 各種分析を実行
    ESP_LOGI(TAG, "Starting comprehensive memory analysis...");
    
    analyzePSRAMUsage();
    checkCurrentMemoryUsers();
    checkMemoryFragmentation();
    testM5GFXPSRAMCompatibility();
    testDividedSpriteStrategy();
    
    ESP_LOGI(TAG, "Analysis complete - no sprite creation attempted");
    
    // 実際の初期化は行わず、分析のみ
    return false;
}
*/

bool ScreenTransition::init() {
    if (!_display) {
        ESP_LOGE(TAG, "Display not initialized");
        return false;
    }
    
    ESP_LOGI(TAG, "Initializing transition with tile-based approach");
    
    // タイル分割方式で初期化
    bool success = initTileBasedTransition();
    
    if (success) {
        _optimizedMode = OptimizationMode::PARTIAL_SCREEN;
        ESP_LOGI(TAG, "Screen transition initialized successfully with tiles");
    } else {
        ESP_LOGE(TAG, "Failed to initialize tile-based transition");
    }
    
    return success;
}

// 最適なタイル分割設定を決定
TileConfig ScreenTransition::selectOptimalTileConfig() {
    // ログから分かった実現可能な設定
    TileConfig candidates[] = {
        {2, 2, 270, 480, 270*480*1, false},  // 126KB - 一応テスト
        {3, 2, 180, 480, 180*480*1, false},  // 84KB - VIABLE
        {2, 3, 270, 320, 270*320*1, false},  // 84KB - VIABLE  
        {3, 3, 180, 320, 180*320*1, false},  // 56KB - VIABLE
        {4, 3, 135, 320, 135*320*1, false},  // 42KB - VIABLE
        {4, 4, 135, 240, 135*240*1, false},  // 31KB - VIABLE
    };
    
    // 各設定の実現可能性をテスト
    for (auto& config : candidates) {
        lgfx::LGFX_Sprite* test_sprite = new lgfx::LGFX_Sprite(_display);
        if (test_sprite) {
            test_sprite->setColorDepth(8);
            config.is_viable = test_sprite->createSprite(config.tile_width, config.tile_height);
            
            if (config.is_viable) {
                test_sprite->deleteSprite();
            }
            delete test_sprite;
        }
    }
    
    // 最適な設定を選択（メモリ効率と品質のバランス）
    for (auto& config : candidates) {
        if (config.is_viable) {
            ESP_LOGI(TAG, "Selected tile config: %dx%d tiles (%dx%d each, %lu KB)", 
                     config.tiles_x, config.tiles_y, 
                     config.tile_width, config.tile_height,
                     (unsigned long)(config.tile_memory / 1024));
            return config;
        }
    }
    
    // フォールバック設定
    ESP_LOGW(TAG, "Using fallback tile config");
    return {4, 4, 135, 240, 135*240*1, true};
}
// タイル分割方式での初期化
bool ScreenTransition::initTileBasedTransition() {
    ESP_LOGI(TAG, "Initializing tile-based transition system");
    
    // 最適なタイル設定を決定
    _tileConfig = selectOptimalTileConfig();
    
    if (!_tileConfig.is_viable) {
        ESP_LOGE(TAG, "No viable tile configuration found");
        return false;
    }
    
    // タイル用スプライトを作成（old, new, work の3つ）
    _oldTile = new lgfx::LGFX_Sprite(_display);
    _newTile = new lgfx::LGFX_Sprite(_display);
    _workTile = new lgfx::LGFX_Sprite(_display);
    
    if (!_oldTile || !_newTile || !_workTile) {
        ESP_LOGE(TAG, "Failed to allocate tile sprites");
        cleanupTileSprites();
        return false;
    }
    
    // スプライトを設定
    bool success = true;
    
    _oldTile->setColorDepth(8);
    if (!_oldTile->createSprite(_tileConfig.tile_width, _tileConfig.tile_height)) {
        ESP_LOGE(TAG, "Failed to create old tile sprite");
        success = false;
    }
    
    _newTile->setColorDepth(8);
    if (success && !_newTile->createSprite(_tileConfig.tile_width, _tileConfig.tile_height)) {
        ESP_LOGE(TAG, "Failed to create new tile sprite");
        success = false;
    }
    
    _workTile->setColorDepth(8);
    if (success && !_workTile->createSprite(_tileConfig.tile_width, _tileConfig.tile_height)) {
        ESP_LOGE(TAG, "Failed to create work tile sprite");
        success = false;
    }
    
    if (!success) {
        cleanupTileSprites();
        return false;
    }
    
    ESP_LOGI(TAG, "Tile-based transition initialized successfully");
    ESP_LOGI(TAG, "Using %dx%d tiles, each %dx%d pixels", 
             _tileConfig.tiles_x, _tileConfig.tiles_y,
             _tileConfig.tile_width, _tileConfig.tile_height);
    
    return true;
}

// タイルスプライトのクリーンアップ
void ScreenTransition::cleanupTileSprites() {
    if (_oldTile) {
        _oldTile->deleteSprite();
        delete _oldTile;
        _oldTile = nullptr;
    }
    
    if (_newTile) {
        _newTile->deleteSprite();
        delete _newTile;
        _newTile = nullptr;
    }
    
    if (_workTile) {
        _workTile->deleteSprite();
        delete _workTile;
        _workTile = nullptr;
    }
}

// タイル単位でのトランジション実行
void ScreenTransition::executeTileBasedTransition() {
    if (!_oldTile || !_newTile || !_workTile) return;
    
    float easedProgress = calculateEasing(_progress);
    
    // 進行度に応じて更新するタイル数を決定
    int totalTiles = _tileConfig.tiles_x * _tileConfig.tiles_y;
    int tilesToUpdate = static_cast<int>(totalTiles * easedProgress);
    
    // トランジション効果に応じてタイル更新順序を決定
    switch (_config.type) {
        case TransitionType::FADE:
            executeTileFade(tilesToUpdate, totalTiles);
            break;
        case TransitionType::WIPE_LEFT:
            executeTileWipeLeft(tilesToUpdate);
            break;
        case TransitionType::WIPE_RIGHT:
            executeTileWipeRight(tilesToUpdate);
            break;
        case TransitionType::DISSOLVE:
            executeTileDissolve(tilesToUpdate, totalTiles);
            break;
        default:
            // デフォルトはランダム更新
            executeTileRandom(tilesToUpdate, totalTiles);
            break;
    }
}

// タイルフェード効果
void ScreenTransition::executeTileFade(int tilesToUpdate, int totalTiles) {
    // ランダムな順序でタイルを更新
    static std::vector<int> tileOrder;
    
    if (tileOrder.empty()) {
        // 初回実行時にランダム順序を生成
        for (int i = 0; i < totalTiles; i++) {
            tileOrder.push_back(i);
        }
        // ランダムシャッフル
        for (int i = 0; i < totalTiles; i++) {
            int j = rand() % totalTiles;
            std::swap(tileOrder[i], tileOrder[j]);
        }
    }
    
    for (int i = 0; i < tilesToUpdate && i < totalTiles; i++) {
        int tileIndex = tileOrder[i];
        int tile_x = tileIndex % _tileConfig.tiles_x;
        int tile_y = tileIndex / _tileConfig.tiles_x;
        
        updateSingleTile(tile_x, tile_y, true); // 新しい画面で更新
    }
    
    // トランジション完了時にリセット
    if (tilesToUpdate >= totalTiles) {
        tileOrder.clear();
    }
}

// 左ワイプ効果
void ScreenTransition::executeTileWipeLeft(int tilesToUpdate) {
    //int tilesPerRow = _tileConfig.tiles_x;
    int completedTiles = 0;
    
    for (int y = 0; y < _tileConfig.tiles_y && completedTiles < tilesToUpdate; y++) {
        for (int x = 0; x < _tileConfig.tiles_x && completedTiles < tilesToUpdate; x++) {
            updateSingleTile(x, y, true);
            completedTiles++;
        }
    }
}

// 右ワイプ効果
void ScreenTransition::executeTileWipeRight(int tilesToUpdate) {
    int completedTiles = 0;
    
    for (int y = 0; y < _tileConfig.tiles_y && completedTiles < tilesToUpdate; y++) {
        for (int x = _tileConfig.tiles_x - 1; x >= 0 && completedTiles < tilesToUpdate; x--) {
            updateSingleTile(x, y, true);
            completedTiles++;
        }
    }
}

// ディゾルブ効果
void ScreenTransition::executeTileDissolve(int tilesToUpdate, int totalTiles) {
    // ランダムなタイルを選択して更新
    for (int i = 0; i < tilesToUpdate; i++) {
        int tile_x = rand() % _tileConfig.tiles_x;
        int tile_y = rand() % _tileConfig.tiles_y;
        updateSingleTile(tile_x, tile_y, true);
    }
}

// ランダム更新効果
void ScreenTransition::executeTileRandom(int tilesToUpdate, int totalTiles) {
    for (int i = 0; i < tilesToUpdate; i++) {
        int tileIndex = rand() % totalTiles;
        int tile_x = tileIndex % _tileConfig.tiles_x;
        int tile_y = tileIndex / _tileConfig.tiles_x;
        updateSingleTile(tile_x, tile_y, true);
    }
}

// 単一タイルの更新
void ScreenTransition::updateSingleTile(int tile_x, int tile_y, bool useNewScreen) {
    int pixel_x = tile_x * _tileConfig.tile_width;
    int pixel_y = tile_y * _tileConfig.tile_height;
    
    // タイルサイズを調整（画面端の場合）
    int actual_width = std::min(_tileConfig.tile_width, _screenWidth - pixel_x);
    int actual_height = std::min(_tileConfig.tile_height, _screenHeight - pixel_y);
    
    if (useNewScreen) {
        // 新しい画面の内容でタイルを更新
        // 実際の実装では、新しい画面データからタイル部分を描画
        _display->fillRect(pixel_x, pixel_y, actual_width, actual_height, TFT_WHITE);
    } else {
        // 古い画面の内容でタイルを更新
        _display->fillRect(pixel_x, pixel_y, actual_width, actual_height, TFT_BLACK);
    }
}

// スプライトの初期化
bool ScreenTransition::initializeSprites(int colorDepth)
{
    cleanupSprites();

    bool success = true;

    // 最適化モードに応じてスプライトサイズを調整
    int spriteWidth = _screenWidth;
    int spriteHeight = _screenHeight;

    if (_optimizedMode == OptimizationMode::PARTIAL_SCREEN)
    {
        // 画面を4分割して処理
        spriteWidth = _screenWidth / 2;
        spriteHeight = _screenHeight / 2;
        ESP_LOGI(TAG, "Using partial screen mode: %dx%d sprites", spriteWidth, spriteHeight);
    }

    // 古い画面用スプライト
    _oldScreen = new lgfx::LGFX_Sprite(_display);
    if (!_oldScreen)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for old screen sprite");
        success = false;
    }
    else
    {
        _oldScreen->setColorDepth(colorDepth);
        if (!_oldScreen->createSprite(spriteWidth, spriteHeight))
        {
            ESP_LOGE(TAG, "Failed to create old screen sprite");
            success = false;
        }
    }

    // 新しい画面用スプライト
    if (success)
    {
        _newScreen = new lgfx::LGFX_Sprite(_display);
        if (!_newScreen)
        {
            ESP_LOGE(TAG, "Failed to allocate memory for new screen sprite");
            success = false;
        }
        else
        {
            _newScreen->setColorDepth(colorDepth);
            if (!_newScreen->createSprite(spriteWidth, spriteHeight))
            {
                ESP_LOGE(TAG, "Failed to create new screen sprite");
                success = false;
            }
        }
    }

    // 作業用バッファ
    if (success)
    {
        _workBuffer = new lgfx::LGFX_Sprite(_display);
        if (!_workBuffer)
        {
            ESP_LOGE(TAG, "Failed to allocate memory for work buffer sprite");
            success = false;
        }
        else
        {
            _workBuffer->setColorDepth(colorDepth);
            if (!_workBuffer->createSprite(spriteWidth, spriteHeight))
            {
                ESP_LOGE(TAG, "Failed to create work buffer sprite");
                success = false;
            }
        }
    }

    if (success)
    {
        size_t actualSpriteSize = spriteWidth * spriteHeight * (colorDepth / 8);
        ESP_LOGI(TAG, "Sprites initialized successfully");
        ESP_LOGI(TAG, "Each sprite: %dx%d, %d-bit, %zu bytes",
                 spriteWidth, spriteHeight, colorDepth, actualSpriteSize);
        ESP_LOGI(TAG, "Total memory used: %zu bytes", actualSpriteSize * 3);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize sprites");
        cleanupSprites();
    }

    return success;
}

bool ScreenTransition::initPartialTransition()
{
    ESP_LOGI(TAG, "Initializing partial transition mode");

    // 非常に小さなスプライトで部分的なトランジション効果を実現
    int tileSize = 64; // 64x64ピクセルのタイル

    _oldScreen = new lgfx::LGFX_Sprite(_display);
    if (_oldScreen)
    {
        _oldScreen->setColorDepth(4); // 4bit色深度
        if (!_oldScreen->createSprite(tileSize, tileSize))
        {
            delete _oldScreen;
            _oldScreen = nullptr;
        }
    }

    _newScreen = new lgfx::LGFX_Sprite(_display);
    if (_newScreen)
    {
        _newScreen->setColorDepth(4);
        if (!_newScreen->createSprite(tileSize, tileSize))
        {
            delete _newScreen;
            _newScreen = nullptr;
        }
    }

    // 作業用バッファは使わずに直接描画
    _workBuffer = nullptr;

    bool success = (_oldScreen != nullptr && _newScreen != nullptr);

    if (success)
    {
        ESP_LOGI(TAG, "Partial transition initialized with %dx%d tiles", tileSize, tileSize);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize even partial transition");
        cleanupSprites();
    }

    return success;
}

// スプライトの解放
void ScreenTransition::cleanupSprites()
{
    if (_oldScreen)
    {
        _oldScreen->deleteSprite();
        delete _oldScreen;
        _oldScreen = nullptr;
    }

    if (_newScreen)
    {
        _newScreen->deleteSprite();
        delete _newScreen;
        _newScreen = nullptr;
    }

    if (_workBuffer)
    {
        _workBuffer->deleteSprite();
        delete _workBuffer;
        _workBuffer = nullptr;
    }

    ESP_LOGD(TAG, "Sprites cleaned up");
}

// 現在時刻取得（マイクロ秒）
uint64_t ScreenTransition::getCurrentTime()
{
    return esp_timer_get_time();
}

// イージング計算
float ScreenTransition::calculateEasing(float t)
{
    if (!_config.useEasing)
    {
        return t; // リニア
    }

    // イーズイン・アウト（クアドラティック）
    if (_config.easingPower == 2.0f)
    {
        return t < 0.5f ? 2.0f * t * t : 1.0f - pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
    }

    // 汎用パワーイージング
    float power = _config.easingPower;
    return t < 0.5f ? pow(2.0f, power - 1.0f) * pow(t, power) : 1.0f - pow(-2.0f * t + 2.0f, power) / 2.0f;
}

// トランジション開始
bool ScreenTransition::startTransition(TransitionType type, uint32_t duration)
{
    TransitionConfig config = _config;
    config.type = type;
    config.duration = duration;
    return startTransition(config);
}

bool ScreenTransition::startTransition(const TransitionConfig &config)
{
    if (_state == TransitionState::TRANSITIONING)
    {
        ESP_LOGW(TAG, "Transition already in progress");
        return false;
    }

    if (!_display || !_oldScreen || !_newScreen)
    {
        ESP_LOGE(TAG, "Not properly initialized");
        return false;
    }

    // 設定を保存
    _config = config;

    // 現在の画面を古い画面としてキャプチャ
    if (_config.preserveOldScreen)
    {
        captureOldScreen();
    }

    // 状態初期化
    _state = TransitionState::TRANSITIONING;
    _startTime = getCurrentTime();
    _lastStepTime = _startTime;
    _progress = 0.0f;

    // コールバック呼び出し
    if (_onTransitionStart)
    {
        _onTransitionStart();
    }

    ESP_LOGI(TAG, "Transition started: type=%d, duration=%lu ms",
             static_cast<int>(_config.type), _config.duration);

    return true;
}

// ステップを実行すべきかどうか
bool ScreenTransition::shouldStepNow()
{
    uint64_t currentTime = getCurrentTime();
    uint64_t elapsed = (currentTime - _lastStepTime) / 1000; // マイクロ秒からミリ秒に変換

    if (elapsed >= _config.stepDelay)
    {
        _lastStepTime = currentTime;
        return true;
    }

    return false;
}

// 更新処理
bool ScreenTransition::update()
{
    if (_state != TransitionState::TRANSITIONING)
    {
        return false;
    }

    // E-Ink最適化が有効な場合はステップ遅延をチェック
    if (_config.stepDelay > 0 && !shouldStepNow())
    {
        return true; // まだ実行タイミングではない
    }

    uint64_t currentTime = getCurrentTime();
    uint64_t elapsed = (currentTime - _startTime) / 1000; // ミリ秒に変換

    // 進行度を計算
    _progress = std::min(1.0f, static_cast<float>(elapsed) / static_cast<float>(_config.duration));

    // トランジション実行
    executeTransition();

    // ステップコールバック
    if (_onTransitionStep)
    {
        _onTransitionStep();
    }

    // 完了チェック
    if (_progress >= 1.0f)
    {
        _state = TransitionState::COMPLETED;

        // 最終画面を描画
        if (_newScreen)
        {
            _newScreen->pushSprite(_display, 0, 0);
        }

        // 完了コールバック
        if (_onTransitionComplete)
        {
            _onTransitionComplete();
        }

        ESP_LOGI(TAG, "Transition completed");
        return false; // 完了
    }

    return true; // 継続中
}

// トランジション実行
void ScreenTransition::executeTransition()
{
    if (!_workBuffer)
        return;

    // 作業バッファをクリア
    _workBuffer->fillSprite(TFT_BLACK);

    // トランジションタイプに応じて処理
    switch (_config.type)
    {
    case TransitionType::FADE:
        drawFadeTransition();
        break;
    case TransitionType::SLIDE_LEFT:
    case TransitionType::SLIDE_RIGHT:
    case TransitionType::SLIDE_UP:
    case TransitionType::SLIDE_DOWN:
        drawSlideTransition();
        break;
    case TransitionType::WIPE_LEFT:
    case TransitionType::WIPE_RIGHT:
    case TransitionType::WIPE_UP:
    case TransitionType::WIPE_DOWN:
        drawWipeTransition();
        break;
    case TransitionType::DISSOLVE:
        drawDissolveTransition();
        break;
    case TransitionType::PUSH_LEFT:
    case TransitionType::PUSH_RIGHT:
    case TransitionType::PUSH_UP:
    case TransitionType::PUSH_DOWN:
        drawPushTransition();
        break;
    case TransitionType::IRIS_IN:
    case TransitionType::IRIS_OUT:
        drawIrisTransition();
        break;
    case TransitionType::BLINDS_H:
    case TransitionType::BLINDS_V:
        drawBlindsTransition();
        break;
    case TransitionType::CHECKERBOARD:
        drawCheckerboardTransition();
        break;
    case TransitionType::SPIRAL:
        drawSpiralTransition();
        break;
    case TransitionType::PIXEL_SCATTER:
        drawPixelScatterTransition();
        break;
    default:
        // 即座に新しい画面を表示
        if (_newScreen)
        {
            _newScreen->pushSprite(_workBuffer, 0, 0);
        }
        break;
    }

    // 作業バッファを画面に描画
    _workBuffer->pushSprite(_display, 0, 0);
}

void ScreenTransition::executePartialTransition()
{
    if (_optimizedMode != OptimizationMode::PARTIAL_SCREEN)
    {
        executeTransition(); // 通常のトランジション
        return;
    }

    // タイルベースのトランジション
    int tileSize = 64;
    int tilesX = (_screenWidth + tileSize - 1) / tileSize;
    int tilesY = (_screenHeight + tileSize - 1) / tileSize;

    float easedProgress = calculateEasing(_progress);
    int completedTiles = static_cast<int>((tilesX * tilesY) * easedProgress);

    // ランダムな順序でタイルを更新
    for (int i = 0; i < completedTiles; i++)
    {
        int tileIndex = (i + static_cast<int>(_progress * 1000)) % (tilesX * tilesY);
        int tileX = (tileIndex % tilesX) * tileSize;
        int tileY = (tileIndex / tilesX) * tileSize;

        int actualWidth = std::min(tileSize, _screenWidth - tileX);
        int actualHeight = std::min(tileSize, _screenHeight - tileY);

        // 新しい画面の該当部分を直接描画
        // ここでは簡単な色変更で代用
        uint32_t newColor = TFT_WHITE; // 実際は新しい画面のデータを使用
        _display->fillRect(tileX, tileY, actualWidth, actualHeight, newColor);
    }
}

// フェードトランジション
void ScreenTransition::drawFadeTransition()
{
    if (!_oldScreen || !_newScreen)
        return;

    float easedProgress = calculateEasing(_progress);

    // アルファブレンドでフェード効果を実現
    // E-Inkでは実際のアルファブレンドは難しいので、グレースケール値で近似

    // 古い画面を基準にして、新しい画面を重ねる
    _oldScreen->pushSprite(_workBuffer, 0, 0);

    // 簡易的なアルファブレンド（E-Ink向け）
    for (int y = 0; y < _screenHeight; y += 4)
    { // 4ピクセルごとに処理（高速化）
        for (int x = 0; x < _screenWidth; x += 4)
        {
            if (static_cast<float>(rand()) / RAND_MAX < easedProgress)
            {
                // 新しい画面のピクセルを描画
                uint32_t color = _newScreen->readPixel(x, y);
                _workBuffer->fillRect(x, y, 4, 4, color);
            }
        }
    }
}

// スライドトランジション
void ScreenTransition::drawSlideTransition()
{
    if (!_oldScreen || !_newScreen)
        return;

    float easedProgress = calculateEasing(_progress);
    int offset;

    switch (_config.type)
    {
    case TransitionType::SLIDE_LEFT:
        offset = static_cast<int>(_screenWidth * (1.0f - easedProgress));
        _oldScreen->pushSprite(_workBuffer, 0, 0);
        _newScreen->pushSprite(_workBuffer, -offset, 0);
        break;

    case TransitionType::SLIDE_RIGHT:
        offset = static_cast<int>(_screenWidth * (1.0f - easedProgress));
        _oldScreen->pushSprite(_workBuffer, 0, 0);
        _newScreen->pushSprite(_workBuffer, offset, 0);
        break;

    case TransitionType::SLIDE_UP:
        offset = static_cast<int>(_screenHeight * (1.0f - easedProgress));
        _oldScreen->pushSprite(_workBuffer, 0, 0);
        _newScreen->pushSprite(_workBuffer, 0, -offset);
        break;

    case TransitionType::SLIDE_DOWN:
        offset = static_cast<int>(_screenHeight * (1.0f - easedProgress));
        _oldScreen->pushSprite(_workBuffer, 0, 0);
        _newScreen->pushSprite(_workBuffer, 0, offset);
        break;

    default:
        break;
    }
}

// ワイプトランジション
void ScreenTransition::drawWipeTransition()
{
    if (!_oldScreen || !_newScreen)
        return;

    float easedProgress = calculateEasing(_progress);

    // 古い画面を全体に描画
    _oldScreen->pushSprite(_workBuffer, 0, 0);

    // 進行度に応じて新しい画面の一部を描画
    switch (_config.type)
    {
    case TransitionType::WIPE_LEFT:
    {
        int wipeWidth = static_cast<int>(_screenWidth * easedProgress);
        _workBuffer->setClipRect(0, 0, wipeWidth, _screenHeight);
        _newScreen->pushSprite(_workBuffer, 0, 0);
        _workBuffer->clearClipRect();
        break;
    }

    case TransitionType::WIPE_RIGHT:
    {
        int wipeStart = static_cast<int>(_screenWidth * (1.0f - easedProgress));
        _workBuffer->setClipRect(wipeStart, 0, _screenWidth - wipeStart, _screenHeight);
        _newScreen->pushSprite(_workBuffer, 0, 0);
        _workBuffer->clearClipRect();
        break;
    }

    case TransitionType::WIPE_UP:
    {
        int wipeHeight = static_cast<int>(_screenHeight * easedProgress);
        _workBuffer->setClipRect(0, 0, _screenWidth, wipeHeight);
        _newScreen->pushSprite(_workBuffer, 0, 0);
        _workBuffer->clearClipRect();
        break;
    }

    case TransitionType::WIPE_DOWN:
    {
        int wipeStart = static_cast<int>(_screenHeight * (1.0f - easedProgress));
        _workBuffer->setClipRect(0, wipeStart, _screenWidth, _screenHeight - wipeStart);
        _newScreen->pushSprite(_workBuffer, 0, 0);
        _workBuffer->clearClipRect();
        break;
    }

    default:
        break;
    }
}

// ディゾルブトランジション
void ScreenTransition::drawDissolveTransition()
{
    if (!_oldScreen || !_newScreen)
        return;

    float easedProgress = calculateEasing(_progress);

    // 古い画面を基準に描画
    _oldScreen->pushSprite(_workBuffer, 0, 0);

    // ランダムなピクセルを新しい画面の色に置き換え
    int pixelsToChange = static_cast<int>((_screenWidth * _screenHeight) * easedProgress / 16); // 16分の1に間引き

    for (int i = 0; i < pixelsToChange; i++)
    {
        int x = rand() % _screenWidth;
        int y = rand() % _screenHeight;

        uint32_t newColor = _newScreen->readPixel(x, y);
        _workBuffer->fillRect(x - 1, y - 1, 3, 3, newColor); // 3x3のブロックで描画
    }
}

// プッシュトランジション
void ScreenTransition::drawPushTransition()
{
    if (!_oldScreen || !_newScreen)
        return;

    float easedProgress = calculateEasing(_progress);

    switch (_config.type)
    {
    case TransitionType::PUSH_LEFT:
    {
        int offset = static_cast<int>(_screenWidth * easedProgress);
        _oldScreen->pushSprite(_workBuffer, -offset, 0);
        _newScreen->pushSprite(_workBuffer, _screenWidth - offset, 0);
        break;
    }

    case TransitionType::PUSH_RIGHT:
    {
        int offset = static_cast<int>(_screenWidth * easedProgress);
        _oldScreen->pushSprite(_workBuffer, offset, 0);
        _newScreen->pushSprite(_workBuffer, -_screenWidth + offset, 0);
        break;
    }

    case TransitionType::PUSH_UP:
    {
        int offset = static_cast<int>(_screenHeight * easedProgress);
        _oldScreen->pushSprite(_workBuffer, 0, -offset);
        _newScreen->pushSprite(_workBuffer, 0, _screenHeight - offset);
        break;
    }

    case TransitionType::PUSH_DOWN:
    {
        int offset = static_cast<int>(_screenHeight * easedProgress);
        _oldScreen->pushSprite(_workBuffer, 0, offset);
        _newScreen->pushSprite(_workBuffer, 0, -_screenHeight + offset);
        break;
    }

    default:
        break;
    }
}

// アイリストランジション
void ScreenTransition::drawIrisTransition()
{
    if (!_oldScreen || !_newScreen)
        return;

    float easedProgress = calculateEasing(_progress);

    // 古い画面を全体に描画
    _oldScreen->pushSprite(_workBuffer, 0, 0);

    // 円形のマスクで新しい画面を描画
    int centerX = _screenWidth / 2;
    int centerY = _screenHeight / 2;
    int maxRadius = std::max(_screenWidth, _screenHeight);

    if (_config.type == TransitionType::IRIS_IN)
    {
        int radius = static_cast<int>(maxRadius * easedProgress);

        // 円形領域に新しい画面を描画（効率化のため4ピクセルごと）
        for (int y = centerY - radius; y <= centerY + radius; y += 2)
        {
            for (int x = centerX - radius; x <= centerX + radius; x += 2)
            {
                if (x >= 0 && x < _screenWidth && y >= 0 && y < _screenHeight)
                {
                    int dx = x - centerX;
                    int dy = y - centerY;
                    if (dx * dx + dy * dy <= radius * radius)
                    {
                        uint32_t newColor = _newScreen->readPixel(x, y);
                        _workBuffer->fillRect(x, y, 2, 2, newColor);
                    }
                }
            }
        }
    }
    else
    { // IRIS_OUT
        int radius = static_cast<int>(maxRadius * (1.0f - easedProgress));
        // 外側から内側に向かって新しい画面を描画
        for (int y = 0; y < _screenHeight; y += 2)
        {
            for (int x = 0; x < _screenWidth; x += 2)
            {
                int dx = x - centerX;
                int dy = y - centerY;
                if (dx * dx + dy * dy >= radius * radius)
                {
                    uint32_t newColor = _newScreen->readPixel(x, y);
                    _workBuffer->fillRect(x, y, 2, 2, newColor);
                }
            }
        }
    }
}

// ブラインドトランジション
void ScreenTransition::drawBlindsTransition()
{
    if (!_oldScreen || !_newScreen)
        return;

    float easedProgress = calculateEasing(_progress);

    // 古い画面を基準に描画
    _oldScreen->pushSprite(_workBuffer, 0, 0);

    if (_config.type == TransitionType::BLINDS_H)
    {
        // 水平ブラインド
        int blindCount = 8; // ブラインドの数
        int blindHeight = _screenHeight / blindCount;
        int openHeight = static_cast<int>(blindHeight * easedProgress);

        for (int i = 0; i < blindCount; i++)
        {
            int blindY = i * blindHeight;
            int clipY = blindY + (blindHeight - openHeight) / 2;

            if (openHeight > 0)
            {
                _workBuffer->setClipRect(0, clipY, _screenWidth, openHeight);
                _newScreen->pushSprite(_workBuffer, 0, 0);
                _workBuffer->clearClipRect();
            }
        }
    }
    else
    { // BLINDS_V
        // 垂直ブラインド
        int blindCount = 8;
        int blindWidth = _screenWidth / blindCount;
        int openWidth = static_cast<int>(blindWidth * easedProgress);

        for (int i = 0; i < blindCount; i++)
        {
            int blindX = i * blindWidth;
            int clipX = blindX + (blindWidth - openWidth) / 2;

            if (openWidth > 0)
            {
                _workBuffer->setClipRect(clipX, 0, openWidth, _screenHeight);
                _newScreen->pushSprite(_workBuffer, 0, 0);
                _workBuffer->clearClipRect();
            }
        }
    }
}

// チェッカーボードトランジション
void ScreenTransition::drawCheckerboardTransition()
{
    if (!_oldScreen || !_newScreen)
        return;

    float easedProgress = calculateEasing(_progress);

    // 古い画面を基準に描画
    _oldScreen->pushSprite(_workBuffer, 0, 0);

    int checkSize = 32; // チェッカーボードのサイズ

    for (int y = 0; y < _screenHeight; y += checkSize)
    {
        for (int x = 0; x < _screenWidth; x += checkSize)
        {
            // チェッカーボードパターンで新しい画面を表示
            bool isEvenCheck = ((x / checkSize) + (y / checkSize)) % 2 == 0;
            float threshold = isEvenCheck ? easedProgress : easedProgress * 0.5f;

            if (static_cast<float>(rand()) / RAND_MAX < threshold)
            {
                int width = std::min(checkSize, _screenWidth - x);
                int height = std::min(checkSize, _screenHeight - y);

                _workBuffer->setClipRect(x, y, width, height);
                _newScreen->pushSprite(_workBuffer, 0, 0);
                _workBuffer->clearClipRect();
            }
        }
    }
}

// スパイラルトランジション
void ScreenTransition::drawSpiralTransition()
{
    if (!_oldScreen || !_newScreen)
        return;

    float easedProgress = calculateEasing(_progress);

    // 古い画面を基準に描画
    _oldScreen->pushSprite(_workBuffer, 0, 0);

    int centerX = _screenWidth / 2;
    int centerY = _screenHeight / 2;
    int maxRadius = std::max(_screenWidth, _screenHeight) / 2;

    // スパイラル描画（効率化のため間引き）
    for (int r = 0; r < maxRadius; r += 4)
    {
        float angleStep = 0.2f; // 角度のステップ
        for (float angle = 0; angle < 6.28f; angle += angleStep)
        {
            float spiralProgress = static_cast<float>(r) / maxRadius;

            if (spiralProgress <= easedProgress)
            {
                int x = centerX + static_cast<int>(r * cos(angle));
                int y = centerY + static_cast<int>(r * sin(angle));

                if (x >= 0 && x < _screenWidth && y >= 0 && y < _screenHeight)
                {
                    uint32_t newColor = _newScreen->readPixel(x, y);
                    _workBuffer->fillRect(x - 2, y - 2, 5, 5, newColor);
                }
            }
        }
    }
}

// ピクセル散布トランジション（E-Ink特化）
void ScreenTransition::drawPixelScatterTransition()
{
    if (!_oldScreen || !_newScreen)
        return;

    float easedProgress = calculateEasing(_progress);

    // 古い画面を基準に描画
    _oldScreen->pushSprite(_workBuffer, 0, 0);

    // E-Inkに最適化されたピクセル散布
    // 大きなブロック単位で処理してE-Inkの更新を効率化
    int blockSize = 8;
    int totalBlocks = (_screenWidth / blockSize) * (_screenHeight / blockSize);
    int blocksToUpdate = static_cast<int>(totalBlocks * easedProgress);

    for (int i = 0; i < blocksToUpdate; i++)
    {
        int blockX = (rand() % (_screenWidth / blockSize)) * blockSize;
        int blockY = (rand() % (_screenHeight / blockSize)) * blockSize;

        _workBuffer->setClipRect(blockX, blockY, blockSize, blockSize);
        _newScreen->pushSprite(_workBuffer, 0, 0);
        _workBuffer->clearClipRect();
    }
}

// 現在の画面をキャプチャ
bool ScreenTransition::captureCurrentScreen()
{
    if (!_display || !_oldScreen)
    {
        return false;
    }

    // 現在の画面内容を古い画面スプライトにコピー
    copyScreenToSprite(_oldScreen);
    ESP_LOGD(TAG, "Current screen captured");
    return true;
}

// 古い画面として現在の画面をキャプチャ
bool ScreenTransition::captureOldScreen()
{
    return captureCurrentScreen();
}

// 新しい画面を設定
bool ScreenTransition::setNewScreen(lgfx::LGFX_Sprite *newScreen)
{
    if (!newScreen || !_newScreen)
    {
        return false;
    }

    // 新しい画面のデータを内部スプライトにコピー
    newScreen->pushSprite(_newScreen, 0, 0);
    ESP_LOGD(TAG, "New screen set");
    return true;
}

// 現在の画面をスプライトにコピー
void ScreenTransition::copyScreenToSprite(lgfx::LGFX_Sprite *sprite)
{
    if (!sprite || !_display)
        return;

    // M5GFXの制限により、実際の画面読み取りは困難
    // 代替として、現在のフレームバッファの内容をコピー
    // 実装では、描画前に明示的にキャプチャを要求する方式を推奨
    sprite->fillSprite(TFT_BLACK); // 暫定的に黒で埋める

    ESP_LOGD(TAG, "Screen copied to sprite");
}

// 即座に切り替え
void ScreenTransition::immediateTransition()
{
    if (_newScreen)
    {
        _newScreen->pushSprite(_display, 0, 0);
    }

    _state = TransitionState::COMPLETED;
    ESP_LOGI(TAG, "Immediate transition completed");
}

// トランジション停止
void ScreenTransition::stopTransition()
{
    if (_state == TransitionState::TRANSITIONING)
    {
        _state = TransitionState::IDLE;
        ESP_LOGI(TAG, "Transition stopped");
    }
}

// リセット
void ScreenTransition::reset()
{
    _state = TransitionState::IDLE;
    _progress = 0.0f;
    _startTime = 0;
    _lastStepTime = 0;
    ESP_LOGI(TAG, "Transition reset");
}

// E-Ink最適化設定
void ScreenTransition::setEInkOptimization(bool enable)
{
    if (enable)
    {
        _config.stepDelay = 100; // E-Ink用に遅延を増加
        ESP_LOGI(TAG, "E-Ink optimization enabled");
    }
    else
    {
        _config.stepDelay = 16; // 通常の60FPS相当
        ESP_LOGI(TAG, "E-Ink optimization disabled");
    }
}

bool ScreenTransition::isEInkOptimized() const
{
    return _config.stepDelay >= 50; // 50ms以上なら最適化モードと判定
}

// デバッグ用進行度描画
void ScreenTransition::debugDrawProgress()
{
    if (!_display)
        return;

    int barWidth = 200;
    int barHeight = 10;
    int barX = (_screenWidth - barWidth) / 2;
    int barY = _screenHeight - 30;

    // プログレスバーの枠
    _display->drawRect(barX, barY, barWidth, barHeight, TFT_WHITE);

    // プログレスバーの中身
    int fillWidth = static_cast<int>(barWidth * _progress);
    _display->fillRect(barX + 1, barY + 1, fillWidth, barHeight - 2, TFT_WHITE);

    // 進行度のテキスト表示
    _display->setTextColor(TFT_WHITE);
    _display->setTextSize(1);
    _display->setCursor(barX, barY - 15);
    _display->printf("Progress: %.1f%%", _progress * 100.0f);
}

// トランジション情報のログ出力
void ScreenTransition::logTransitionInfo()
{
    ESP_LOGI(TAG, "=== Transition Info ===");
    ESP_LOGI(TAG, "State: %d", static_cast<int>(_state));
    ESP_LOGI(TAG, "Type: %d", static_cast<int>(_config.type));
    ESP_LOGI(TAG, "Duration: %lu ms", _config.duration);
    ESP_LOGI(TAG, "Progress: %.2f", _progress);
    ESP_LOGI(TAG, "E-Ink optimized: %s", isEInkOptimized() ? "Yes" : "No");
    ESP_LOGI(TAG, "====================");
}

// main/ScreenTransition.cpp に追加するデバッグ関数

// 詳細なメモリ情報を表示する関数
void ScreenTransition::debugMemoryInfo() {
    ESP_LOGI(TAG, "=== Detailed Memory Analysis ===");
    
    // 基本的なヒープ情報
    size_t total_heap = esp_get_free_heap_size();
    size_t min_heap = esp_get_minimum_free_heap_size();
    size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    
    ESP_LOGI(TAG, "General Heap:");
    ESP_LOGI(TAG, "  Free: %zu bytes (%.2f MB)", total_heap, total_heap / (1024.0f * 1024.0f));
    ESP_LOGI(TAG, "  Min free: %zu bytes (%.2f MB)", min_heap, min_heap / (1024.0f * 1024.0f));
    ESP_LOGI(TAG, "  Largest block: %zu bytes (%.2f MB)", largest_block, largest_block / (1024.0f * 1024.0f));
    
    // PSRAM情報
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    
    ESP_LOGI(TAG, "PSRAM:");
    ESP_LOGI(TAG, "  Total: %zu bytes (%.2f MB)", psram_total, psram_total / (1024.0f * 1024.0f));
    ESP_LOGI(TAG, "  Free: %zu bytes (%.2f MB)", psram_free, psram_free / (1024.0f * 1024.0f));
    ESP_LOGI(TAG, "  Largest block: %zu bytes (%.2f MB)", psram_largest, psram_largest / (1024.0f * 1024.0f));
    
    // 内部RAM情報
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    size_t internal_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    
    ESP_LOGI(TAG, "Internal RAM:");
    ESP_LOGI(TAG, "  Total: %zu bytes (%.2f MB)", internal_total, internal_total / (1024.0f * 1024.0f));
    ESP_LOGI(TAG, "  Free: %zu bytes (%.2f MB)", internal_free, internal_free / (1024.0f * 1024.0f));
    ESP_LOGI(TAG, "  Largest block: %zu bytes (%.2f MB)", internal_largest, internal_largest / (1024.0f * 1024.0f));
    
    ESP_LOGI(TAG, "==============================");
}

// M5GFXの対応色深度をテストする関数
void ScreenTransition::testColorDepthSupport() {
    ESP_LOGI(TAG, "=== Testing M5GFX Color Depth Support ===");
    
    // 小さなテストスプライトで各色深度をテスト
    int test_sizes[] = {32, 64, 128, 256};
    int color_depths[] = {1, 4, 8, 16, 24};
    
    for (int depth : color_depths) {
        ESP_LOGI(TAG, "Testing %d-bit color depth:", depth);
        
        for (int size : test_sizes) {
            lgfx::LGFX_Sprite* testSprite = new lgfx::LGFX_Sprite(_display);
            
            if (testSprite) {
                testSprite->setColorDepth(depth);
                bool success = testSprite->createSprite(size, size);
                
                ESP_LOGI(TAG, "  %dx%d sprite: %s", size, size, success ? "SUCCESS" : "FAILED");
                
                if (success) {
                    testSprite->deleteSprite();
                }
                delete testSprite;
            } else {
                ESP_LOGE(TAG, "  Failed to allocate test sprite");
                break;
            }
        }
    }
    
    ESP_LOGI(TAG, "=====================================");
}

// 段階的にスプライトサイズを増やしてテストする関数
void ScreenTransition::testSpriteAllocation() {
    ESP_LOGI(TAG, "=== Testing Sprite Allocation Limits ===");
    
    // 8bit色深度で段階的にサイズを増やしてテスト
    int test_widths[] = {100, 200, 300, 400, 500, 540};
    int test_heights[] = {100, 200, 400, 600, 800, 960};
    
    for (int w : test_widths) {
        for (int h : test_heights) {
            size_t sprite_size = w * h * 1; // 8bit = 1byte
            
            ESP_LOGI(TAG, "Testing %dx%d sprite (%.2f KB):", 
                     w, h, sprite_size / 1024.0f);
            
            // メモリ使用量を事前チェック
            size_t free_before = esp_get_free_heap_size();
            
            lgfx::LGFX_Sprite* testSprite = new lgfx::LGFX_Sprite(_display);
            
            if (testSprite) {
                testSprite->setColorDepth(8);
                bool success = testSprite->createSprite(w, h);
                
                size_t free_after = esp_get_free_heap_size();
                size_t used_memory = free_before - free_after;
                
                ESP_LOGI(TAG, "  Result: %s, Used memory: %zu bytes", 
                         success ? "SUCCESS" : "FAILED", used_memory);
                
                if (success) {
                    testSprite->deleteSprite();
                } else {
                    ESP_LOGE(TAG, "  FAILED at %dx%d - this is our limit!", w, h);
                    delete testSprite;
                    ESP_LOGI(TAG, "=====================================");
                    return;
                }
                delete testSprite;
            } else {
                ESP_LOGE(TAG, "  Failed to allocate sprite object");
                ESP_LOGI(TAG, "=====================================");
                return;
            }
        }
    }
    
    ESP_LOGI(TAG, "=====================================");
}

// PSRAMを明示的に使用してスプライトを作成する関数
bool ScreenTransition::createSpriteInPSRAM(lgfx::LGFX_Sprite** sprite, int width, int height, int colorDepth) {
    ESP_LOGI(TAG, "Attempting to create %dx%d sprite in PSRAM", width, height);
    
    // PSRAMにスプライトオブジェクトを確保
    *sprite = (lgfx::LGFX_Sprite*)heap_caps_malloc(sizeof(lgfx::LGFX_Sprite), MALLOC_CAP_SPIRAM);
    
    if (*sprite) {
        // プレースメント new でコンストラクタ呼び出し
        new lgfx::LGFX_Sprite(_display);
        
        (*sprite)->setColorDepth(colorDepth);
        
        bool success = (*sprite)->createSprite(width, height);
        
        if (success) {
            ESP_LOGI(TAG, "Successfully created sprite in PSRAM");
            return true;
        } else {
            // 失敗時のクリーンアップ
            (*sprite)->~LGFX_Sprite();  // デストラクタ呼び出し
            heap_caps_free(*sprite);
            *sprite = nullptr;
            ESP_LOGE(TAG, "Failed to create sprite in PSRAM");
            return false;
        }
    } else {
        ESP_LOGE(TAG, "Failed to allocate sprite object in PSRAM");
        return false;
    }
}


// PSRAM の詳細状況を分析する関数
void ScreenTransition::analyzePSRAMUsage() {
    ESP_LOGI(TAG, "=== PSRAM Detailed Analysis ===");
    
    // 基本的なPSRAM情報
    if (esp_psram_is_initialized()) {
        ESP_LOGI(TAG, "PSRAM is initialized: YES");
        size_t psram_size = esp_psram_get_size();
        ESP_LOGI(TAG, "PSRAM total size: %zu bytes (%.2f MB)", 
                 psram_size, psram_size / (1024.0f * 1024.0f));
    } else {
        ESP_LOGE(TAG, "PSRAM is NOT initialized!");
        return;
    }
    
    // ヒープ統計情報
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_SPIRAM);
    
    ESP_LOGI(TAG, "PSRAM Heap Statistics:");
    ESP_LOGI(TAG, "  Total free bytes: %zu", heap_info.total_free_bytes);
    ESP_LOGI(TAG, "  Total allocated bytes: %zu", heap_info.total_allocated_bytes);
    ESP_LOGI(TAG, "  Largest free block: %zu", heap_info.largest_free_block);
    ESP_LOGI(TAG, "  Minimum free bytes: %zu", heap_info.minimum_free_bytes);
    ESP_LOGI(TAG, "  Allocated blocks: %zu", heap_info.allocated_blocks);
    ESP_LOGI(TAG, "  Free blocks: %zu", heap_info.free_blocks);
    ESP_LOGI(TAG, "  Total blocks: %zu", heap_info.total_blocks);
    
    // 現在のメモリ使用者を特定
    ESP_LOGI(TAG, "Memory allocation test:");
    
    // PSRAMに大きなメモリブロックを確保してテスト
    size_t test_sizes[] = {100*1024, 500*1024, 1024*1024, 2*1024*1024, 4*1024*1024};
    
    for (size_t test_size : test_sizes) {
        void* test_ptr = heap_caps_malloc(test_size, MALLOC_CAP_SPIRAM);
        if (test_ptr) {
            ESP_LOGI(TAG, "  Successfully allocated %zu KB in PSRAM", test_size / 1024);
            heap_caps_free(test_ptr);
        } else {
            ESP_LOGE(TAG, "  Failed to allocate %zu KB in PSRAM", test_size / 1024);
        }
    }
    
    ESP_LOGI(TAG, "===============================");
}

// 既存のメモリ使用量を確認する関数（修正版）
void ScreenTransition::checkCurrentMemoryUsers() {
    ESP_LOGI(TAG, "=== Current Memory Users Analysis ===");
    
    // vTaskListの代わりにタスク数などの基本情報を取得
    ESP_LOGI(TAG, "FreeRTOS Task Information:");
    ESP_LOGI(TAG, "  Number of tasks: %d", uxTaskGetNumberOfTasks());
    ESP_LOGI(TAG, "  Free heap size: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "  Minimum free heap: %lu bytes", esp_get_minimum_free_heap_size());
    
    // 現在実行中のタスクの情報
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    if (current_task) {
        ESP_LOGI(TAG, "  Current task: %s", pcTaskGetName(current_task));
        ESP_LOGI(TAG, "  Current task priority: %d", (int)uxTaskPriorityGet(current_task));
    }
    
    // ヒープ統計（内部RAM）
    multi_heap_info_t internal_heap;
    heap_caps_get_info(&internal_heap, MALLOC_CAP_INTERNAL);
    
    ESP_LOGI(TAG, "Internal RAM usage:");
    ESP_LOGI(TAG, "  Used: %zu KB", internal_heap.total_allocated_bytes / 1024);
    ESP_LOGI(TAG, "  Free: %zu KB", internal_heap.total_free_bytes / 1024);
    ESP_LOGI(TAG, "  Largest block: %zu KB", internal_heap.largest_free_block / 1024);
    ESP_LOGI(TAG, "  Allocated blocks: %zu", internal_heap.allocated_blocks);
    ESP_LOGI(TAG, "  Free blocks: %zu", internal_heap.free_blocks);
    
    // PSRAM統計
    multi_heap_info_t psram_heap;
    heap_caps_get_info(&psram_heap, MALLOC_CAP_SPIRAM);
    
    ESP_LOGI(TAG, "PSRAM usage:");
    ESP_LOGI(TAG, "  Used: %zu KB", psram_heap.total_allocated_bytes / 1024);
    ESP_LOGI(TAG, "  Free: %zu KB", psram_heap.total_free_bytes / 1024);
    ESP_LOGI(TAG, "  Largest block: %zu KB", psram_heap.largest_free_block / 1024);
    ESP_LOGI(TAG, "  Allocated blocks: %zu", psram_heap.allocated_blocks);
    ESP_LOGI(TAG, "  Free blocks: %zu", psram_heap.free_blocks);
    
    // 各メモリタイプの詳細情報
    ESP_LOGI(TAG, "Memory capabilities breakdown:");
    
    // DMA可能メモリ
    size_t dma_free = heap_caps_get_free_size(MALLOC_CAP_DMA);
    size_t dma_largest = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    ESP_LOGI(TAG, "  DMA capable: %zu KB free, %zu KB largest block", 
             dma_free / 1024, dma_largest / 1024);
    
    // 32bit aligned メモリ
    size_t aligned_free = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    size_t aligned_largest = heap_caps_get_largest_free_block(MALLOC_CAP_32BIT);
    ESP_LOGI(TAG, "  32-bit aligned: %zu KB free, %zu KB largest block", 
             aligned_free / 1024, aligned_largest / 1024);
    
    // 実行可能メモリ
    size_t exec_free = heap_caps_get_free_size(MALLOC_CAP_EXEC);
    ESP_LOGI(TAG, "  Executable: %zu KB free", exec_free / 1024);
    
    ESP_LOGI(TAG, "====================================");
}

// M5GFXがPSRAMを使用できるかテストする関数
void ScreenTransition::testM5GFXPSRAMCompatibility() {
    ESP_LOGI(TAG, "=== Testing M5GFX PSRAM Compatibility ===");
    
    // 段階的にサイズを増やしてPSRAM強制でテスト
    int test_widths[] = {200, 250, 300, 350, 400, 450, 500, 540};
    int test_heights[] = {600, 700, 800, 900, 960};
    
    for (int w : test_widths) {
        for (int h : test_heights) {
            size_t sprite_memory = w * h * 1; // 8bit
            
            if (sprite_memory > 250*1024) break; // 250KB以上はスキップ
            
            ESP_LOGI(TAG, "Testing %dx%d (%.1f KB) with PSRAM preference", 
                     w, h, sprite_memory / 1024.0f);
            
            // M5GFXが内部でPSRAMを使うように誘導
            // （M5GFXの内部実装次第だが...）
            
            lgfx::LGFX_Sprite* test_sprite = new lgfx::LGFX_Sprite(_display);
            if (test_sprite) {
                test_sprite->setColorDepth(8);
                
                // 大きなメモリを事前確保してからスプライト作成
                void* dummy_alloc = heap_caps_malloc(1024*1024, MALLOC_CAP_SPIRAM);
                
                bool success = test_sprite->createSprite(w, h);
                
                if (dummy_alloc) heap_caps_free(dummy_alloc);
                
                ESP_LOGI(TAG, "  Result: %s", success ? "SUCCESS" : "FAILED");
                
                if (success) {
                    test_sprite->deleteSprite();
                }
                delete test_sprite;
                
                if (!success) {
                    ESP_LOGE(TAG, "M5GFX limit reached at %dx%d", w, h);
                    break;
                }
            }
        }
    }
    
    ESP_LOGI(TAG, "========================================");
}

// 分割スプライト戦略のテスト
void ScreenTransition::testDividedSpriteStrategy() {
    ESP_LOGI(TAG, "=== Testing Divided Sprite Strategy ===");
    
    // 画面を分割する戦略をテスト
    struct DivisionStrategy {
        int divisions_x;
        int divisions_y; 
        const char* name;
    };
    
    DivisionStrategy strategies[] = {
        {2, 2, "4 tiles (270x480 each)"},
        {3, 2, "6 tiles (180x480 each)"},
        {2, 3, "6 tiles (270x320 each)"},
        {3, 3, "9 tiles (180x320 each)"},
        {4, 3, "12 tiles (135x320 each)"},
        {4, 4, "16 tiles (135x240 each)"}
    };
    
    for (auto& strategy : strategies) {
        int tile_width = _screenWidth / strategy.divisions_x;
        int tile_height = _screenHeight / strategy.divisions_y;
        size_t tile_size = tile_width * tile_height * 1; // 8bit
        size_t total_memory = tile_size * 3; // 3つのスプライト（old, new, work）
        
        ESP_LOGI(TAG, "Strategy: %s", strategy.name);
        ESP_LOGI(TAG, "  Tile size: %dx%d (%zu KB)", 
                 tile_width, tile_height, tile_size / 1024);
        ESP_LOGI(TAG, "  Total memory: %zu KB", total_memory / 1024);
        
        // 実際にタイルサイズでスプライト作成をテスト
        lgfx::LGFX_Sprite* test_tile = new lgfx::LGFX_Sprite(_display);
        if (test_tile) {
            test_tile->setColorDepth(8);
            bool success = test_tile->createSprite(tile_width, tile_height);
            
            ESP_LOGI(TAG, "  Feasibility: %s", success ? "VIABLE" : "NOT VIABLE");
            
            if (success) {
                test_tile->deleteSprite();
            }
            delete test_tile;
        }
        
        ESP_LOGI(TAG, "");
    }
    
    ESP_LOGI(TAG, "======================================");
}

// メモリ断片化の状況を確認
void ScreenTransition::checkMemoryFragmentation() {
    ESP_LOGI(TAG, "=== Memory Fragmentation Analysis ===");
    
    // 断片化をテストするため、様々なサイズで確保・解放を試す
    size_t test_sizes[] = {1024, 4096, 16384, 65536, 262144, 1048576}; // 1KB～1MB
    
    ESP_LOGI(TAG, "PSRAM fragmentation test:");
    for (size_t size : test_sizes) {
        size_t successful_allocs = 0;
        void* ptrs[100]; // 最大100個まで確保
        
        // 指定サイズのメモリを可能な限り確保
        for (int i = 0; i < 100; i++) {
            ptrs[i] = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
            if (ptrs[i]) {
                successful_allocs++;
            } else {
                break;
            }
        }
        
        // 確保したメモリを解放
        for (size_t i = 0; i < successful_allocs; i++) {
            heap_caps_free(ptrs[i]);
        }
        
        ESP_LOGI(TAG, "  %zu byte blocks: %zu successful allocations", 
                 size, successful_allocs);
    }
    
    ESP_LOGI(TAG, "===================================");
}