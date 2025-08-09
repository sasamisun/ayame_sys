// main/ScreenTransition.hpp
// 軽量版ステップベース画面遷移システム（E-Paper最適化版）

#ifndef _SCREEN_TRANSITION_HPP_
#define _SCREEN_TRANSITION_HPP_

#include <M5GFX.h>
#include <functional>
#include <vector>

// M5PaperS3の画面解像度
#define TRANSITION_WIDTH  540
#define TRANSITION_HEIGHT 960

/**
 * @brief トランジション効果の種類（軽量版）
 * アドベンチャーゲームに特化した軽量な効果のみ
 */
enum class TransitionType {
    NONE,              // トランジションなし（即座に切り替え）
    
    // フェード系（超軽量 - 単色塗りつぶしのみ）
    FADE_BLACK,        // 黒フェード（場面転換の定番）
    FADE_WHITE,        // 白フェード（回想シーン、夢のシーン）
    
    // スライド系（軽量 - 2キャンバスの位置変更のみ）
    SLIDE_LEFT,        // 左スライド（右への移動感）
    SLIDE_RIGHT,       // 右スライド（左への移動感、時間遡行）
    SLIDE_UP,          // 上スライド（階段を上る、上昇感）
    SLIDE_DOWN,        // 下スライド（落下、地下へ降りる）
    
    // ワイプ系（軽量 - 領域コピーのみ）
    WIPE_LEFT,         // 左ワイプ（ページめくり風）
    WIPE_RIGHT,        // 右ワイプ（ページめくり風逆）
    
    // シンプル演出系（中軽量）
    VENETIAN_BLIND,    // ブラインド効果（サスペンス、緊張感）
    
    // 特殊効果（必要に応じて追加）
    CUT_IN,            // カットイン効果（瞬間切り替え + 短時間表示）
    PUSH_LEFT,         // プッシュ効果（新画面が古い画面を押し出す）
    PUSH_RIGHT         // プッシュ効果（右方向）
};

/**
 * @brief トランジション速度設定
 * ステップ数で速度を制御（軽量化のため選択肢を絞る）
 */
enum class TransitionSpeed {
    INSTANT = 1,       // 1ステップ（瞬間切り替え）
    VERY_FAST = 3,     // 3ステップ（超高速）
    FAST = 5,          // 5ステップ（高速）
    NORMAL = 8,        // 8ステップ（標準）
    SLOW = 12,         // 12ステップ（ゆっくり）
    VERY_SLOW = 16     // 16ステップ（演出用）
};

/**
 * @brief トランジション方向指定
 */
enum class TransitionDirection {
    IN,     // 新しい画面が入ってくる
    OUT,    // 現在の画面が出ていく
    BOTH    // 両方向同時
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
 * @brief 軽量版ステップベーストランジション設定構造体
 */
struct TransitionConfig {
    TransitionType type;           // トランジション種類
    TransitionSpeed speed;         // 速度（ステップ数）
    TransitionDirection direction; // 方向
    uint32_t step_delay_ms;        // ステップ間の待機時間（ミリ秒）
    uint32_t fade_color;          // フェード色（白黒のみ）
    bool use_psram;               // PSRAM使用フラグ
    bool reverse;                 // 逆方向フラグ
    
    // アドベンチャーゲーム用デフォルト設定
    static TransitionConfig defaultConfig() {
        return {
            TransitionType::FADE_BLACK,   // 標準的な黒フェード
            TransitionSpeed::NORMAL,      // 8ステップ
            TransitionDirection::BOTH,    // 両方向
            120,                          // 120ms間隔（E-Paper最適）
            TFT_BLACK,                    // 黒色
            true,                         // PSRAM使用
            false                         // 通常方向
        };
    }
    
    // 高速切り替え用設定
    static TransitionConfig fastConfig() {
        return {
            TransitionType::FADE_BLACK,
            TransitionSpeed::FAST,
            TransitionDirection::BOTH,
            80,
            TFT_BLACK,
            true,
            false
        };
    }
    
    // 演出用ゆっくり設定
    static TransitionConfig slowConfig() {
        return {
            TransitionType::FADE_BLACK,
            TransitionSpeed::SLOW,
            TransitionDirection::BOTH,
            200,
            TFT_BLACK,
            true,
            false
        };
    }
};

/**
 * @brief 軽量版ScreenTransitionクラス
 * アドベンチャーゲーム特化・E-Paper最適化版
 */
class ScreenTransition {
private:
    M5GFX* _display;              // 描画先ディスプレイ
    bool _initialized;            // 初期化フラグ
    bool _use_psram;              // PSRAM使用フラグ
    
    // ダブルバッファリング用キャンバス
    M5Canvas* _sourceCanvas;      // 元画面のキャンバス
    M5Canvas* _targetCanvas;      // 次画面のキャンバス
    M5Canvas* _workCanvas;        // 作業用キャンバス
    
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
    void cleanup();                               // リソース解放
    bool createCanvases();                        // キャンバス作成
    float getStepProgress() const;                      // 現在ステップの進行度取得
    bool isStepReady();                          // 次ステップ実行可能かチェック
    void executeStep();                          // 1ステップ実行
    
    // 軽量版トランジション効果の実装
    void renderFadeStep(int step, int totalSteps);              // フェード効果（超軽量）
    void renderSlideStep(int step, int totalSteps);             // スライド効果（軽量）
    void renderWipeStep(int step, int totalSteps);              // ワイプ効果（軽量）
    void renderVenetianBlindStep(int step, int totalSteps);     // ブラインド効果（中軽量）
    void renderCutInStep(int step, int totalSteps);             // カットイン効果
    void renderPushStep(int step, int totalSteps);              // プッシュ効果
    
    // E-Paper最適化ユーティリティ関数
    void copyCanvasRegion(M5Canvas* src, M5Canvas* dst, int sx, int sy, int sw, int sh, int dx, int dy);
    uint16_t getGrayscaleColor(float intensity);                // 0.0(黒) - 1.0(白)

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
     * @brief 初期化処理
     * @param use_psram PSRAM使用フラグ
     * @return 成功時true
     */
    bool init(bool use_psram = true);
    
    /**
     * @brief 元画面のキャプチャ
     * 現在の画面をソースキャンバスに保存
     */
    void captureSource();
    
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
     * @brief 簡易トランジション実行
     * キャプチャ→準備→実行を一括で行う便利関数
     * @param prepare_func 次画面を描画する関数
     * @param config トランジション設定
     */
    void transition(std::function<void(M5Canvas*)> prepare_func, 
                   const TransitionConfig& config = TransitionConfig::defaultConfig());
    
    // ゲッター
    TransitionState getState() const { return _state; }
    int getCurrentStep() const { return _currentStep; }
    int getTotalSteps() const { return _totalSteps; }
    float getProgress() const { return getStepProgress(); }
    bool isRunning() const { return _state == TransitionState::RUNNING; }
    bool isFinished() const { return _state == TransitionState::FINISHED; }
    
    // コールバック設定
    void setOnTransitionStart(std::function<void()> callback) { _onTransitionStart = callback; }
    void setOnTransitionStep(std::function<void(int, int)> callback) { _onTransitionStep = callback; }
    void setOnTransitionComplete(std::function<void()> callback) { _onTransitionComplete = callback; }
    
    // ユーティリティ
    M5Canvas* getSourceCanvas() { return _sourceCanvas; }
    M5Canvas* getTargetCanvas() { return _targetCanvas; }
    M5Canvas* getWorkCanvas() { return _workCanvas; }
    
    /**
     * @brief アドベンチャーゲーム用プリセット設定
     */
    
    // 基本的な場面転換用
    static TransitionConfig sceneChange() {
        return { TransitionType::FADE_BLACK, TransitionSpeed::NORMAL, TransitionDirection::BOTH, 150, TFT_BLACK, true, false };
    }
    
    // 移動演出用
    static TransitionConfig moveLeft() {
        return { TransitionType::SLIDE_LEFT, TransitionSpeed::FAST, TransitionDirection::BOTH, 100, TFT_BLACK, true, false };
    }
    
    static TransitionConfig moveRight() {
        return { TransitionType::SLIDE_RIGHT, TransitionSpeed::FAST, TransitionDirection::BOTH, 100, TFT_BLACK, true, false };
    }
    
    // 回想・夢シーン用
    static TransitionConfig flashback() {
        return { TransitionType::FADE_WHITE, TransitionSpeed::SLOW, TransitionDirection::BOTH, 200, TFT_WHITE, true, false };
    }
    
    // ページめくり用
    static TransitionConfig pageNext() {
        return { TransitionType::WIPE_LEFT, TransitionSpeed::NORMAL, TransitionDirection::BOTH, 120, TFT_BLACK, true, false };
    }
    
    static TransitionConfig pagePrev() {
        return { TransitionType::WIPE_RIGHT, TransitionSpeed::NORMAL, TransitionDirection::BOTH, 120, TFT_BLACK, true, false };
    }
    
    // サスペンス演出用
    static TransitionConfig suspense() {
        return { TransitionType::VENETIAN_BLIND, TransitionSpeed::SLOW, TransitionDirection::BOTH, 180, TFT_BLACK, true, false };
    }
    
    // 瞬間切り替え用
    static TransitionConfig instant() {
        return { TransitionType::NONE, TransitionSpeed::INSTANT, TransitionDirection::BOTH, 0, TFT_BLACK, true, false };
    }
                                          
    /**
     * @brief カスタム設定作成
     */
    static TransitionConfig createCustomConfig(TransitionType type, int steps, 
                                             uint32_t step_delay_ms = 120);
};

#endif // _SCREEN_TRANSITION_HPP_