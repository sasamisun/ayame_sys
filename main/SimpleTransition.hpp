// main/SimpleTransition.hpp
// シンプルトランジションシステム - メインキャンバス1つで実現

#ifndef _SIMPLE_TRANSITION_HPP_
#define _SIMPLE_TRANSITION_HPP_

#include <M5GFX.h>
#include <functional>

// M5PaperS3の画面解像度
#define TRANSITION_WIDTH  540
#define TRANSITION_HEIGHT 960

/**
 * @brief シンプルトランジション効果の種類
 * メインキャンバスから段階的に描画する方式
 */
enum class SimpleTransitionType {
    NONE,              // トランジションなし（即座に切り替え）
    FADE_IN,           // フェードイン（徐々に表示）
    SLIDE_DOWN,        // 上から下にスライド表示
    SLIDE_UP,          // 下から上にスライド表示
    SLIDE_LEFT,        // 右から左にスライド表示
    SLIDE_RIGHT,       // 左から右にスライド表示
    WIPE_DOWN,         // 上から下にワイプ表示
    WIPE_UP,           // 下から上にワイプ表示
    WIPE_LEFT,         // 右から左にワイプ表示
    WIPE_RIGHT,        // 左から右にワイプ表示
    CENTER_OUT,        // 中央から外に向かって表示
    RANDOM_BLOCKS,     // ランダムブロック表示
    LINE_SCAN,         // 線形スキャン表示
    TYPEWRITER         // タイプライター風表示（文字単位）
};

/**
 * @brief トランジション状態
 */
enum class SimpleTransitionState {
    IDLE,       // 待機中
    RUNNING,    // 実行中
    FINISHED    // 完了
};

/**
 * @brief シンプルトランジション設定
 */
struct SimpleTransitionConfig {
    SimpleTransitionType type;      // トランジション種類
    int total_steps;                // 総ステップ数
    bool clear_before_step;         // ステップ前に画面をクリアするか
    uint32_t clear_color;          // クリア色
    bool reverse_direction;         // 逆方向フラグ
    
    // デフォルト設定
    static SimpleTransitionConfig defaultConfig() {
        return {
            SimpleTransitionType::FADE_IN,  // フェードイン
            20,                             // 20ステップ
            true,                           // ステップ前クリア
            TFT_BLACK,                      // 黒でクリア
            false                           // 通常方向
        };
    }
    
    // プリセット設定
    static SimpleTransitionConfig fastFade() {
        return { SimpleTransitionType::FADE_IN, 10, true, TFT_BLACK, false };
    }
    
    static SimpleTransitionConfig slowSlide() {
        return { SimpleTransitionType::SLIDE_DOWN, 30, true, TFT_BLACK, false };
    }
    
    static SimpleTransitionConfig typewriter() {
        return { SimpleTransitionType::TYPEWRITER, 50, false, TFT_BLACK, false };
    }
};

/**
 * @brief シンプルトランジションクラス
 * メインキャンバス1つで実現するシンプルな画面遷移
 */
class SimpleTransition {
private:
    M5GFX* _display;                    // 描画先ディスプレイ
    M5Canvas* _mainCanvas;              // メインキャンバス（アプリから渡される）
    SimpleTransitionState _state;      // 現在の状態
    SimpleTransitionConfig _config;    // 現在の設定
    int _currentStep;                   // 現在のステップ（0から開始）
    
    // ランダムブロック用データ
    struct BlockInfo {
        int x, y, w, h;
        int step_to_show;
    };
    std::vector<BlockInfo> _randomBlocks;
    
    // コールバック関数
    std::function<void()> _onStart;                             // 開始時コールバック
    std::function<void(int, int)> _onStep;                      // ステップ進行コールバック
    std::function<void()> _onComplete;                          // 完了時コールバック
    
    // 内部メソッド
    void executeCurrentStep();                                  // 現在ステップを実行
    void prepareRandomBlocks();                                 // ランダムブロック準備
    
    // 各トランジション効果の実装
    void renderFadeStep();              // フェードイン効果
    void renderSlideStep();             // スライド効果
    void renderWipeStep();              // ワイプ効果
    void renderCenterOutStep();         // 中央から外への効果
    void renderRandomBlocksStep();      // ランダムブロック効果
    void renderLineScanStep();          // 線形スキャン効果
    void renderTypewriterStep();        // タイプライター効果

public:
    /**
     * @brief コンストラクタ
     * @param display M5GFXディスプレイオブジェクト
     */
    SimpleTransition(M5GFX* display);
    
    /**
     * @brief デストラクタ
     */
    ~SimpleTransition();
    
    /**
     * @brief トランジション開始
     * @param mainCanvas 表示したい最終画面を描画済みのキャンバス
     * @param config トランジション設定
     * @return 成功時true
     */
    bool start(M5Canvas* mainCanvas, const SimpleTransitionConfig& config = SimpleTransitionConfig::defaultConfig());
    
    /**
     * @brief 1ステップ実行（メインループから毎回呼び出し）
     * @return 継続中ならtrue、完了ならfalse
     */
    bool step();
    
    /**
     * @brief トランジション停止
     */
    void stop();
    
    /**
     * @brief 即座に完成画面を表示（トランジションなし）
     */
    void showImmediate();
    
    /**
     * @brief トランジション完了まで一括実行
     * 内部でstep()を繰り返し呼び出す
     */
    void runToCompletion();
    
    // ゲッター
    SimpleTransitionState getState() const { return _state; }
    int getCurrentStep() const { return _currentStep; }
    int getTotalSteps() const { return _config.total_steps; }
    float getProgress() const;
    bool isRunning() const { return _state == SimpleTransitionState::RUNNING; }
    bool isFinished() const { return _state == SimpleTransitionState::FINISHED; }
    bool isIdle() const { return _state == SimpleTransitionState::IDLE; }
    
    // コールバック設定
    void setOnStart(std::function<void()> callback) { _onStart = callback; }
    void setOnStep(std::function<void(int, int)> callback) { _onStep = callback; }
    void setOnComplete(std::function<void()> callback) { _onComplete = callback; }
    
    /**
     * @brief 簡単な使用例
     * 
     * // 使用方法：
     * M5Canvas canvas(&display);
     * canvas.createSprite(540, 960);
     * 
     * // キャンバスに最終画面を描画
     * canvas.fillSprite(TFT_BLUE);
     * canvas.drawString("Hello!", 100, 100);
     * 
     * // トランジション開始
     * SimpleTransitionConfig config = SimpleTransitionConfig::slowSlide();
     * transition.start(&canvas, config);
     * 
     * // メインループで実行
     * while(transition.step()) {
     *     vTaskDelay(pdMS_TO_TICKS(50)); // 50ms間隔
     * }
     */
};

#endif // _SIMPLE_TRANSITION_HPP_