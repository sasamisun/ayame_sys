// main/ScreenTransition.hpp
#ifndef _SCREEN_TRANSITION_HPP_
#define _SCREEN_TRANSITION_HPP_

#include <M5GFX.h>
#include <functional>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"    // ヒープメモリ情報取得用
#include "esp_system.h"       // システム情報取得用
#include <new>                // std::nothrow用

// タイル分割設定
struct TileConfig {
    int tiles_x;          // 水平方向のタイル数
    int tiles_y;          // 垂直方向のタイル数
    int tile_width;       // 各タイルの幅
    int tile_height;      // 各タイルの高さ
    size_t tile_memory;   // 各タイルのメモリ使用量
    bool is_viable;       // 実現可能かどうか
};

// 最適化モードの列挙型（メモリ不足対策）
enum class OptimizationMode {
    FULL_SCREEN,        // フルスクリーントランジション
    COLOR_DEPTH_4BIT,   // 4bit色深度で最適化
    PARTIAL_SCREEN,     // 部分画面トランジション
    DIRECT_DRAW         // スプライトを使わず直接描画
};

// トランジション効果の種類
enum class TransitionType {
    NONE,           // トランジションなし（即座に切り替え）
    FADE,           // フェードイン/フェードアウト
    SLIDE_LEFT,     // 左からスライドイン
    SLIDE_RIGHT,    // 右からスライドイン
    SLIDE_UP,       // 上からスライドイン
    SLIDE_DOWN,     // 下からスライドイン
    WIPE_LEFT,      // 左からワイプ
    WIPE_RIGHT,     // 右からワイプ
    WIPE_UP,        // 上からワイプ
    WIPE_DOWN,      // 下からワイプ
    DISSOLVE,       // ディゾルブ（ランダムドット）
    PUSH_LEFT,      // 左にプッシュ（現在の画面を押し出す）
    PUSH_RIGHT,     // 右にプッシュ
    PUSH_UP,        // 上にプッシュ
    PUSH_DOWN,      // 下にプッシュ
    IRIS_IN,        // アイリス（円形）イン
    IRIS_OUT,       // アイリス（円形）アウト
    BLINDS_H,       // 水平ブラインド
    BLINDS_V,       // 垂直ブラインド
    CHECKERBOARD,   // チェッカーボード
    SPIRAL,         // スパイラル
    PIXEL_SCATTER   // ピクセル散布（E-Ink特化）
};

// トランジションの状態
enum class TransitionState {
    IDLE,           // アイドル状態
    TRANSITIONING,  // トランジション実行中
    COMPLETED       // トランジション完了
};

// トランジション設定構造体
struct TransitionConfig {
    TransitionType type;        // トランジションタイプ
    uint32_t duration;          // 継続時間（ミリ秒）
    bool useEasing;             // イージング使用フラグ
    float easingPower;          // イージングの強さ（1.0 = リニア、2.0 = クアドラティック）
    uint32_t stepDelay;         // ステップ間の遅延（E-Ink用、ミリ秒）
    bool preserveOldScreen;     // 古い画面を保持するかどうか
    
    // デフォルト設定を返す静的メソッド
    static TransitionConfig defaultConfig() {
        return {
            TransitionType::FADE,   // デフォルトはフェード
            1000,                   // 1秒間
            true,                   // イージング使用
            2.0f,                   // クアドラティックイージング
            50,                     // E-Ink用50ms遅延
            true                    // 古い画面を保持
        };
    }
};

// 画面トランジション管理クラス
class ScreenTransition {
private:
    static const char* TAG;                     // ログタグ
    
    M5GFX* _display;                           // ディスプレイへの参照
    lgfx::LGFX_Sprite* _oldScreen;             // 古い画面のスプライト
    lgfx::LGFX_Sprite* _newScreen;             // 新しい画面のスプライト
    lgfx::LGFX_Sprite* _workBuffer;            // 作業用バッファ
    
    // タイル分割用スプライト
    lgfx::LGFX_Sprite* _oldTile;               // 古い画面のタイル
    lgfx::LGFX_Sprite* _newTile;               // 新しい画面のタイル
    lgfx::LGFX_Sprite* _workTile;              // 作業用タイル
    TileConfig _tileConfig;                    // タイル分割設定
    
    TransitionState _state;                     // 現在の状態
    TransitionConfig _config;                   // 現在の設定
    OptimizationMode _optimizedMode;            // 最適化モード
    
    uint64_t _startTime;                       // トランジション開始時刻
    uint64_t _lastStepTime;                    // 最後のステップ実行時刻
    float _progress;                           // 進行度（0.0 - 1.0）
    
    int _screenWidth;                          // 画面幅
    int _screenHeight;                         // 画面高さ
    
    // コールバック関数の型定義
    using TransitionCallback = std::function<void()>;
    TransitionCallback _onTransitionStart;      // トランジション開始時のコールバック
    TransitionCallback _onTransitionComplete;  // トランジション完了時のコールバック
    TransitionCallback _onTransitionStep;      // 各ステップでのコールバック
    
    // 内部メソッド
    void initializeSprites();                  // スプライトの初期化
    bool initializeSprites(int colorDepth);    // 色深度指定版
    bool initPartialTransition();              // 部分トランジション初期化
    bool initTileBasedTransition();            // タイル分割初期化
    void cleanupSprites();                     // スプライトの解放
    void cleanupTileSprites();                 // タイルスプライトの解放
    
    // タイル分割関連メソッド
    TileConfig selectOptimalTileConfig();      // 最適なタイル設定選択
    void executeTileBasedTransition();         // タイル分割トランジション実行
    void executeTileFade(int tilesToUpdate, int totalTiles);     // タイルフェード
    void executeTileWipeLeft(int tilesToUpdate);                 // タイル左ワイプ
    void executeTileWipeRight(int tilesToUpdate);                // タイル右ワイプ
    void executeTileDissolve(int tilesToUpdate, int totalTiles); // タイルディゾルブ
    void executeTileRandom(int tilesToUpdate, int totalTiles);   // タイルランダム
    void updateSingleTile(int tile_x, int tile_y, bool useNewScreen); // 単一タイル更新
    float calculateEasing(float t);            // イージング計算
    uint64_t getCurrentTime();                 // 現在時刻取得（マイクロ秒）
    
    // 各トランジション効果の実装
    void executeTransition();                  // トランジション実行
    void executePartialTransition();           // 部分トランジション実行
    void drawFadeTransition();                 // フェードトランジション
    void drawSlideTransition();                // スライドトランジション
    void drawWipeTransition();                 // ワイプトランジション
    void drawDissolveTransition();             // ディゾルブトランジション
    void drawPushTransition();                 // プッシュトランジション
    void drawIrisTransition();                 // アイリストランジション
    void drawBlindsTransition();               // ブラインドトランジション
    void drawCheckerboardTransition();         // チェッカーボードトランジション
    void drawSpiralTransition();               // スパイラルトランジション
    void drawPixelScatterTransition();         // ピクセル散布トランジション（E-Ink特化）
    
    // ヘルパーメソッド
    void blendSprites(float alpha);            // スプライトのアルファブレンド
    void copyScreenToSprite(lgfx::LGFX_Sprite* sprite); // 現在の画面をスプライトにコピー
    bool shouldStepNow();                      // ステップを実行すべきかどうか
    
    // デバッグ・調査用メソッド
    void debugMemoryInfo();                    // 詳細なメモリ情報表示
    void testColorDepthSupport();              // M5GFXの色深度サポートテスト
    void testSpriteAllocation();               // スプライト割り当て限界テスト
    bool createSpriteInPSRAM(lgfx::LGFX_Sprite** sprite, int width, int height, int colorDepth); // PSRAM使用
    
    // PSRAM詳細分析用メソッド
    void analyzePSRAMUsage();                  // PSRAM使用状況詳細分析
    void checkCurrentMemoryUsers();            // 現在のメモリ使用者確認
    void testM5GFXPSRAMCompatibility();        // M5GFXのPSRAM互換性テスト
    void testDividedSpriteStrategy();          // 分割スプライト戦略テスト
    void checkMemoryFragmentation();           // メモリ断片化確認
    
public:
    // コンストラクタ・デストラクタ
    ScreenTransition(M5GFX* display);
    ~ScreenTransition();
    
    // 初期化
    bool init();
    
    // トランジション開始
    bool startTransition(TransitionType type, uint32_t duration = 1000);
    bool startTransition(const TransitionConfig& config);
    
    // 更新処理（メインループで呼び出す）
    bool update();
    
    // 状態取得
    TransitionState getState() const { return _state; }
    bool isTransitioning() const { return _state == TransitionState::TRANSITIONING; }
    bool isCompleted() const { return _state == TransitionState::COMPLETED; }
    float getProgress() const { return _progress; }
    
    // 画面キャプチャ
    bool captureCurrentScreen();               // 現在の画面をキャプチャ
    bool captureOldScreen();                   // 古い画面として現在の画面をキャプチャ
    bool setNewScreen(lgfx::LGFX_Sprite* newScreen); // 新しい画面を設定
    
    // 設定
    void setConfig(const TransitionConfig& config) { _config = config; }
    const TransitionConfig& getConfig() const { return _config; }
    
    // コールバック設定
    void setOnTransitionStart(TransitionCallback callback) { _onTransitionStart = callback; }
    void setOnTransitionComplete(TransitionCallback callback) { _onTransitionComplete = callback; }
    void setOnTransitionStep(TransitionCallback callback) { _onTransitionStep = callback; }
    
    // 即座に切り替え（トランジションなし）
    void immediateTransition();
    
    // トランジション停止
    void stopTransition();
    
    // リセット
    void reset();
    
    // E-Ink最適化関連
    void setEInkOptimization(bool enable);     // E-Ink最適化の有効/無効
    bool isEInkOptimized() const;              // E-Ink最適化が有効かどうか
    
    // デバッグ用
    void debugDrawProgress();                  // 進行度の描画（デバッグ用）
    void logTransitionInfo();                  // トランジション情報のログ出力
};

#endif // _SCREEN_TRANSITION_HPP_