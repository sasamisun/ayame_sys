// main/ScreenTransition.hpp
// 完全外部キャンバス専用版画面遷移システム（最大メモリ節約版）

#ifndef _SCREEN_TRANSITION_HPP_
#define _SCREEN_TRANSITION_HPP_

#include <M5GFX.h>
#include <functional>
#include <vector>

// M5PaperS3の画面解像度
#define TRANSITION_WIDTH  540
#define TRANSITION_HEIGHT 960

/**
 * @brief トランジション効果の種類
 * E-Paper最適化版
 */
enum class TransitionType {
    NONE,              // トランジションなし（即座に切り替え）
    FADE_BLACK,        // 黒フェード（クラシック）
    FADE_WHITE,        // 白フェード（回想シーン用）
    SLIDE_LEFT,        // 左スライド（場面転換）
    SLIDE_RIGHT,       // 右スライド（時間遡行）
    SLIDE_UP,          // 上スライド（上昇感）
    SLIDE_DOWN,        // 下スライド（落下感）
    WIPE_LEFT,         // 左ワイプ（ページめくり風）
    WIPE_RIGHT,        // 右ワイプ（ページめくり風逆）
    CIRCLE_EXPAND,     // 円形拡大（フォーカス効果）
    CIRCLE_SHRINK,     // 円形縮小（望遠鏡効果）
    PIXELATE,          // ピクセル化（デジタル効果）
    VENETIAN_BLIND,    // ブラインド効果（サスペンス）
    CHECKERBOARD,      // チェッカーボード（パズル風）
    SPIRAL,            // 螺旋効果（幻想的）
    RIPPLE,            // 波紋効果（水面のような）
    MOSAIC,            // モザイク効果（記憶の断片）
    PAGE_TURN          // ページめくり効果（本を読む感覚）
};

/**
 * @brief トランジション速度設定
 * ステップ数で速度を制御
 */
enum class TransitionSpeed {
    VERY_FAST = 2,     // 2ステップ（超高速）
    FAST = 4,          // 4ステップ（高速）
    NORMAL = 8,        // 8ステップ（標準）
    SLOW = 16,         // 16ステップ（ゆっくり）
    VERY_SLOW = 32     // 32ステップ（非常にゆっくり）
};

/**
 * @brief トランジション方向指定
 */
enum class TransitionDirection {
    IN,     // 新しい画面が入ってくる
    OUT,    // 現在の画面が出ていく
    BOTH    // 両方向同時（クロスフェード等）
};

/**
 * @brief トランジション状態
 */
enum class TransitionState {
    IDLE,       // 待機中
    RUNNING,    // 実行中
    FINISHED,   // 完了
    CANCELLED   // キャンセル
};

/**
 * @brief ステップベーストランジション設定構造体
 */
struct TransitionConfig {
    TransitionType type;           // トランジション種類
    TransitionSpeed speed;         // 速度（ステップ数）
    TransitionDirection direction; // 方向
    uint32_t step_delay_ms;        // ステップ間の待機時間（ミリ秒）
    uint32_t fade_color;          // フェード色（白黒のみ）
    bool reverse;                 // 逆方向フラグ
    
    // デフォルト設定
    static TransitionConfig defaultConfig() {
        return {
            TransitionType::FADE_BLACK,   // 標準的な黒フェード
            TransitionSpeed::NORMAL,      // 8ステップ
            TransitionDirection::BOTH,    // 両方向
            150,                          // 150ms間隔（E-Paper最適）
            TFT_BLACK,                    // 黒色
            false                         // 通常方向
        };
    }
};

/**
 * @brief 完全外部キャンバス専用ScreenTransitionクラス
 * 最大メモリ節約版 - 内部キャンバス一切なし、外部キャンバス必須
 */
class ScreenTransition {
private:
    M5GFX* _display;              // 描画先ディスプレイ
    bool _initialized;            // 初期化フラグ
    
    // 外部キャンバスへの参照（所有権なし、必須）
    M5Canvas* _sourceCanvas;      // 元画面のキャンバス（外部必須）
    M5Canvas* _targetCanvas;      // 次画面のキャンバス（外部必須）
    M5Canvas* _workCanvas;        // 作業用キャンバス（必要時のみ一時作成）
    
    // 作業用キャンバスの管理フラグ
    bool _owns_work_canvas;       // 作業用キャンバスを一時作成したか
    
    // ステップベース状態管理
    TransitionState _state;       // 現在の状態
    TransitionConfig _config;     // 現在の設定
    int _currentStep;             // 現在のステップ（0から開始）
    int _totalSteps;              // 総ステップ数
    int64_t _lastStepTime;        // 最後のステップ実行時刻
    
    // コールバック関数
    std::function<void()> _onTransitionStart;                    // 開始時コールバック
    std::function<void(int, int)> _onTransitionStep;            // ステップ進行コールバック
    std::function<void()> _onTransitionComplete;                // 完了時コールバック
    
    // 内部メソッド
    void cleanup();                               // リソース解放（作業用キャンバスのみ）
    bool ensureWorkCanvas();                      // 必要時に作業用キャンバス作成
    void releaseWorkCanvas();                     // 作業用キャンバス解放
    float getStepProgress() const;                // 現在ステップの進行度取得
    bool isStepReady();                          // 次ステップ実行可能かチェック
    void executeStep();                          // 1ステップ実行
    
    // 各トランジション効果の実装（ステップベース）
    void renderFadeStep(int step, int totalSteps);              // フェード効果
    void renderSlideStep(int step, int totalSteps);             // スライド効果
    void renderWipeStep(int step, int totalSteps);              // ワイプ効果
    void renderCircleStep(int step, int totalSteps);            // 円形効果
    void renderPixelateStep(int step, int totalSteps);          // ピクセル化効果
    void renderVenetianBlindStep(int step, int totalSteps);     // ブラインド効果
    void renderCheckerboardStep(int step, int totalSteps);      // チェッカーボード効果
    void renderSpiralStep(int step, int totalSteps);            // 螺旋効果
    void renderRippleStep(int step, int totalSteps);            // 波紋効果
    void renderMosaicStep(int step, int totalSteps);            // モザイク効果
    void renderPageTurnStep(int step, int totalSteps);          // ページめくり効果
    
    // E-Paper最適化ユーティリティ関数
    void copyCanvasRegion(M5Canvas* src, M5Canvas* dst, int sx, int sy, int sw, int sh, int dx, int dy);
    void blendCanvasesGrayscale(M5Canvas* src, M5Canvas* dst, float alpha);
    uint16_t getGrayscaleColor(float intensity);                // 0.0(黒) - 1.0(白)
    void fillCanvasGradient(M5Canvas* canvas, uint16_t color1, uint16_t color2, bool horizontal);

public:
    /**
     * @brief コンストラクタ
     * @param display M5GFXディスプレイオブジェクト
     */
    ScreenTransition(M5GFX* display);
    
    /**
     * @brief デストラクタ
     */
    ~ScreenTransition();
    
    /**
     * @brief 初期化処理（外部キャンバス必須）
     * @param sourceCanvas 元画面用のキャンバス（540x960必須）
     * @param targetCanvas 次画面用のキャンバス（540x960必須）
     * @return 成功時true
     * @note 両方のキャンバスは外部で作成・管理される必要があります
     */
    bool init(M5Canvas* sourceCanvas, M5Canvas* targetCanvas);
    
    /**
     * @brief 外部キャンバスを変更（実行時変更用）
     * @param sourceCanvas 元画面用のキャンバス（540x960必須）
     * @param targetCanvas 次画面用のキャンバス（540x960必須）
     * @note 通常は init() で設定するため、実行中の変更時のみ使用
     */
    void setCanvases(M5Canvas* sourceCanvas, M5Canvas* targetCanvas);
    
    /**
     * @brief 元画面のキャプチャ
     * 現在の画面をソースキャンバスに保存
     * @param captureFromDisplay 画面から直接キャプチャするか（false推奨）
     */
    void captureSource(bool captureFromDisplay = false);
    
    /**
     * @brief 次画面の準備
     * @param prepare_func 次画面を描画する関数
     */
    void prepareTarget(std::function<void(M5Canvas*)> prepare_func);
    
    /**
     * @brief トランジション開始
     * @param config トランジション設定
     * @return 成功時true
     */
    bool startTransition(const TransitionConfig& config = TransitionConfig::defaultConfig());
    
    /**
     * @brief トランジション更新（メインループで呼び出し）
     * @return 継続中ならtrue、完了ならfalse
     */
    bool updateTransition();
    
    /**
     * @brief トランジション停止
     */
    void stopTransition();
    
    /**
     * @brief 即座に画面切り替え（トランジションなし）
     */
    void switchImmediate();
    
    /**
     * @brief ワンステップトランジション実行
     * キャプチャ→準備→実行を一括で行う便利関数
     * @param prepare_func 次画面を描画する関数
     * @param config トランジション設定
     */
    void executeTransition(std::function<void(M5Canvas*)> prepare_func, 
                          const TransitionConfig& config = TransitionConfig::defaultConfig());
    
    // ゲッター
    TransitionState getState() const { return _state; }
    int getCurrentStep() const { return _currentStep; }
    int getTotalSteps() const { return _totalSteps; }
    float getProgress() const { return getStepProgress(); }
    bool isRunning() const { return _state == TransitionState::RUNNING; }
    bool isFinished() const { return _state == TransitionState::FINISHED; }
    bool isInitialized() const { return _initialized; }
    
    // コールバック設定
    void setOnTransitionStart(std::function<void()> callback) { _onTransitionStart = callback; }
    void setOnTransitionStep(std::function<void(int, int)> callback) { _onTransitionStep = callback; }
    void setOnTransitionComplete(std::function<void()> callback) { _onTransitionComplete = callback; }
    
    // キャンバス取得（デバッグ・高度な用途）
    M5Canvas* getSourceCanvas() { return _sourceCanvas; }
    M5Canvas* getTargetCanvas() { return _targetCanvas; }
    M5Canvas* getWorkCanvas() { return _workCanvas; }
    
    /**
     * @brief プリセット設定の取得
     */
    static TransitionConfig getFadeConfig(TransitionSpeed speed = TransitionSpeed::NORMAL, 
                                        uint32_t color = TFT_BLACK);
    static TransitionConfig getSlideConfig(TransitionType slide_type, 
                                         TransitionSpeed speed = TransitionSpeed::NORMAL);
    static TransitionConfig getCircleConfig(TransitionType circle_type, 
                                          TransitionSpeed speed = TransitionSpeed::SLOW);
    static TransitionConfig getEffectConfig(TransitionType effect_type, 
                                          TransitionSpeed speed = TransitionSpeed::NORMAL);
                                          
    /**
     * @brief ステップ数を直接指定してコンフィグ作成
     */
    static TransitionConfig createCustomConfig(TransitionType type, int steps, 
                                             uint32_t step_delay_ms = 150);
};

#endif // _SCREEN_TRANSITION_HPP_