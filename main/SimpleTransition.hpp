// main/SimpleTransition.hpp - E-Paper最適化版
// 電子ペーパー専用！超高速アドベンチャーゲーム用トランジションシステムにゃ
#ifndef _SIMPLE_TRANSITION_HPP_
#define _SIMPLE_TRANSITION_HPP_

#include <M5GFX.h>
#include <functional>

// M5PaperS3の画面解像度
#define SIMPLE_TRANSITION_WIDTH  540
#define SIMPLE_TRANSITION_HEIGHT 960

/**
 * @brief E-Paper最適化トランジション効果
 * 電子ペーパーの特性に合わせて厳選された高速な効果のみ
 */
enum class SimpleTransitionType {
    NONE,              // トランジションなし（瞬間表示）
    FADE_IN,           // フェードイン（段階的矩形表示）
    SLIDE_LEFT,        // 左からスライドイン（ブロック単位）
    SLIDE_RIGHT,       // 右からスライドイン（ブロック単位）
    SLIDE_UP,          // 上からスライドイン（ブロック単位）
    SLIDE_DOWN,        // 下からスライドイン（ブロック単位）
    WIPE_HORIZONTAL,   // 水平ワイプ（大きなブロック単位）
    WIPE_VERTICAL,     // 垂直ワイプ（大きなブロック単位）
    REVEAL_CENTER,     // 中央から展開（ブロック調整）
    REVEAL_CORNER      // 角から展開（ブロック調整）
};

/**
 * @brief E-Paper最適化SimpleTransitionクラス
 * 
 * 電子ペーパーの特性に特化した超高速トランジション：
 * - 最小限のpushSprite/pushImage呼び出し
 * - 大きなブロック単位での処理
 * - ステップ数をE-Paper向けに最適化
 * - メモリ効率とレスポンス速度を両立
 * 
 * 使い方：
 * 1. メインキャンバスに最終的な画面を描画
 * 2. startTransition()でトランジション開始（ステップ数は自動最適化）
 * 3. loop()で毎回update()を呼ぶ
 * 4. E-Paper向けに最適化された高速描画で完了
 */
class SimpleTransition {
private:
    M5GFX* _display;              // 描画先ディスプレイ
    M5Canvas* _mainCanvas;        // メインキャンバス（最終画面状態）
    bool _initialized;            // 初期化フラグ
    bool _use_psram;              // PSRAM使用フラグ
    
    // トランジション状態
    bool _isActive;               // トランジション実行中フラグ
    SimpleTransitionType _type;   // トランジション種類
    int _currentStep;             // 現在のステップ（0から開始）
    int _totalSteps;              // 総ステップ数（E-Paper最適化済み）
    
    // コールバック関数
    std::function<void()> _onComplete;  // 完了時コールバック
    std::function<void(int, int)> _onStep;  // ステップ進行コールバック
    
    // E-Paper最適化版内部描画メソッド
    void drawFadeInStepOptimized();           // 最適化フェードイン描画
    void drawSlideStepOptimized();            // 最適化スライド描画
    void drawWipeStepOptimized();             // 最適化ワイプ描画
    void drawRevealCenterStepOptimized();     // 最適化中央展開描画
    void drawRevealCornerStepOptimized();     // 最適化角展開描画
    
    // E-Paper最適化ユーティリティメソッド
    void drawOptimizedRegion(int x, int y, int w, int h);  // 最適化領域描画
    void copyCanvasRegionOptimized(M5Canvas* src, M5Canvas* dst,   // 最適化領域コピー
                                  int sx, int sy, int sw, int sh, int dx, int dy);

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
     * @brief 初期化処理
     * @param use_psram PSRAM使用フラグ（デフォルト：true）
     * @return 成功時true
     */
    bool init(bool use_psram = true);
    
    /**
     * @brief メインキャンバスを取得
     * ここに最終的な画面状態を描画するにゃ！
     * @return メインキャンバスのポインタ
     */
    M5Canvas* getMainCanvas() { return _mainCanvas; }
    
    /**
     * @brief E-Paper最適化トランジション開始
     * @param type トランジション種類
     * @param steps 実行ステップ数（E-Paper向けに自動調整される）
     * @return 成功時true
     */
    bool startTransition(SimpleTransitionType type, int steps = 6);
    
    /**
     * @brief トランジション更新（毎回loop()で呼び出し）
     * E-Paper向けに最適化された1ステップ実行
     * @return 継続中ならtrue、完了ならfalse
     */
    bool update();
    
    /**
     * @brief トランジション停止
     */
    void stop();
    
    /**
     * @brief メインキャンバスの内容を即座に画面に表示
     * E-Paper向けに最適化された即座表示
     */
    void showImmediate();
    
    // ゲッター
    bool isActive() const { return _isActive; }
    int getCurrentStep() const { return _currentStep; }
    int getTotalSteps() const { return _totalSteps; }
    float getProgress() const { 
        return _totalSteps > 0 ? (float)_currentStep / _totalSteps : 0.0f; 
    }
    
    // コールバック設定
    void setOnComplete(std::function<void()> callback) { _onComplete = callback; }
    void setOnStep(std::function<void(int, int)> callback) { _onStep = callback; }
    
    /**
     * @brief E-Paper最適化プリセット関数
     * 各プリセットはE-Paperでの実測データに基づいて最適化済み
     */
    
    // 基本的な場面転換（6ステップ、約0.7秒）
    bool startSceneChange() { return startTransition(SimpleTransitionType::FADE_IN, 6); }
    
    // 超高速場面転換（4ステップ、約0.5秒）
    bool startQuickChange() { return startTransition(SimpleTransitionType::FADE_IN, 4); }
    
    // ページめくり風（5ステップ、約0.6秒）
    bool startPageTurn() { return startTransition(SimpleTransitionType::SLIDE_LEFT, 5); }
    
    // 演出用ゆっくり（8ステップ、約1.0秒）
    bool startSlowReveal() { return startTransition(SimpleTransitionType::FADE_IN, 8); }
    
    // サスペンス用（6ステップ、中央展開）
    bool startDramaticReveal() { return startTransition(SimpleTransitionType::REVEAL_CENTER, 6); }
    
    // 瞬間切り替え（E-Paperでも瞬間表示）
    bool startInstant() { return startTransition(SimpleTransitionType::NONE, 1); }
    
    // 高速水平ワイプ（アクションシーン用、4ステップ）
    bool startActionWipe() { return startTransition(SimpleTransitionType::WIPE_HORIZONTAL, 4); }
    
    // 縦スクロール風（シナリオ進行用、5ステップ）
    bool startStoryScroll() { return startTransition(SimpleTransitionType::SLIDE_DOWN, 5); }
    
    /**
     * @brief カスタムE-Paper最適化設定
     * ユーザー指定のパラメータもE-Paper向けに自動調整
     */
    static SimpleTransitionType getOptimalTypeForEPaper(SimpleTransitionType requested) {
        // E-Paperで特に高速な効果を推奨
        switch (requested) {
            case SimpleTransitionType::FADE_IN:
            case SimpleTransitionType::WIPE_HORIZONTAL:
            case SimpleTransitionType::WIPE_VERTICAL:
            case SimpleTransitionType::SLIDE_LEFT:
            case SimpleTransitionType::SLIDE_RIGHT:
                return requested;  // これらは最適化済み
            default:
                return SimpleTransitionType::FADE_IN;  // 最も安定した効果
        }
    }
    
    static int getOptimalStepsForEPaper(int requested_steps) {
        // E-Paperに最適なステップ数に調整
        if (requested_steps <= 3) return 4;        // 最低4ステップ
        if (requested_steps <= 6) return 6;        // 標準6ステップ
        if (requested_steps <= 10) return 8;       // 演出用8ステップ
        return 8;  // 最大8ステップに制限
    }
};

#endif // _SIMPLE_TRANSITION_HPP_