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
 * 2. startTransition()でトランジション開始
 *    （ステップ数は [1, 8] にクランプされる。それ以上の「自動最適化」は行わない）
 * 3. loop()で毎回update()を呼ぶ
 * 4. 全ステップ完了後に showImmediate() で最終画面を表示
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

    // ========== EPD描画モードの一時切り替え ==========
    //
    // Panel_EPD のリフレッシュは「LUTのステップ数」だけパネル全体を走査する。
    // 走査回数はモードで決まり、更新範囲の広さには依存しない。
    //
    //   epd_quality : 21ステップ
    //   epd_text    : 18ステップ
    //   epd_fast    : 11ステップ
    //   epd_fastest :  7ステップ
    //
    // トランジションの中間フレームは一瞬しか表示されないため画質は不要。
    // 中間は高速モードで描き、完了時に元のモードへ戻して最終画面を描き直す。
    lgfx::v1::epd_mode_t _transitionEpdMode;  // トランジション中に使うモード
    lgfx::v1::epd_mode_t _savedEpdMode;       // 開始前のモード（復帰用）
    bool _epdModeOverridden;                  // モードを退避中かどうか

    // EPDモードを高速モードへ切り替える／元に戻す
    void beginFastEpdMode();
    void endFastEpdMode();
    
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

    /**
     * @brief 現在のステップ進捗（0.0〜1.0）を返す
     *
     * 各描画メソッドが個別に
     * `_currentStep / (_totalSteps - 1)` を計算しており、
     * _totalSteps == 1 のとき 0除算で NaN になっていた（5箇所に複製）。
     * 本メソッドに集約し、境界も含めて 0.0〜1.0 にクランプする。
     */
    float calcStepProgress() const;

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

    // 進捗（0.0〜1.0）。内部の描画処理と同じ定義を使う。
    // 従来はここだけ _currentStep / _totalSteps で、内部描画は
    // _currentStep / (_totalSteps - 1) と2つの進捗定義が併存していた。
    float getProgress() const { return calcStepProgress(); }
    
    // コールバック設定
    void setOnComplete(std::function<void()> callback) { _onComplete = callback; }
    void setOnStep(std::function<void(int, int)> callback) { _onStep = callback; }

    /**
     * @brief トランジション中に使うEPD描画モードを設定する
     *
     * 中間フレームの画質を落として速度を稼ぐ。既定は epd_fast。
     * 完了時には開始前のモードへ戻し、そのモードで最終画面を描き直すため、
     * 最終的な表示品質は変わらない。
     *
     * 8ステップの場合の目安（パネル走査回数）:
     *   epd_quality のまま : 8 x 21           = 168
     *   epd_fast + 最終品質: 8 x 11 + 21      = 109  （約1.5倍速）
     *   epd_fastest + 同上 : 8 x  7 + 21      =  77  （約2.2倍速）
     *
     * @param mode epd_quality / epd_text / epd_fast / epd_fastest
     */
    void setTransitionEpdMode(lgfx::v1::epd_mode_t mode) { _transitionEpdMode = mode; }

    /** @brief トランジション中のEPD描画モードを取得する */
    lgfx::v1::epd_mode_t getTransitionEpdMode() const { return _transitionEpdMode; }

    /**
     * @brief 残像（焼き付き）を除去する（画面は白で終わる）
     *
     * 白→黒→白の反転シーケンスで全画素を強制的に駆動し、
     * 蓄積した残像を消す。キャンバスの内容は描き直さない。
     *
     * **static メソッドなので `SimpleTransition` のインスタンスが無くても呼べる。**
     * 起動直後（`SimpleTransition` を生成する前）に呼びたいためこの形にしている。
     *
     * ```cpp
     * display.begin();
     * SimpleTransition::clearGhosting(&display);   // インスタンス生成前でも可
     * ```
     *
     * 電子ペーパーは「変化した画素だけを駆動する」部分更新を繰り返すと、
     * 駆動されなかった画素に前の像が薄く残る（ghosting）。
     * また片方向の塗りつぶし1回では粒子が完全にリセットされないため、
     * 反転を挟む必要がある。
     *
     * **起動直後に呼ぶことを強く推奨する。**
     * `Panel_EPD` は初期化時に「画面は全白」と仮定して内部バッファを埋めるが
     * （`_buf` を 0xFF、`_step_framebuf` を 0xFFFF で初期化）、
     * E-Paper はリセットしても直前の像を保持している。
     * この不一致があると、実際は黒い画素に「白から」の波形がかかり
     * 駆動しきれずに前の像が残る。
     * これが「コールド起動は比較的きれいだが、リセットすると残像がひどい」の原因。
     * 起動時に本メソッドを呼んで物理状態をドライバの仮定（全白）へ合わせると、
     * リセット後の残像を大幅に減らせる。
     *
     * @param display 対象のディスプレイ
     * @param mode    除去に使うEPDモード。既定の epd_quality が最も効果が高い
     */
    static void clearGhosting(M5GFX* display,
                              lgfx::v1::epd_mode_t mode = lgfx::v1::epd_mode_t::epd_quality);

    /**
     * @brief 画面全体を再駆動してコントラストを回復する（内容は変えない・フラッシュなし）
     *
     * **static メソッド。インスタンス不要。**
     *
     * 電子ペーパーの更新は `_range_mod`（描画範囲のバウンディングボックス）に対して
     * のみ行われ、範囲外の画素には電圧がかからない。
     * そのため一部だけを描き続けると、**描画していない領域が徐々に薄くなる**。
     * 例えば文字を書き換え続けると、文字の周囲だけがドリフトして淡くなる。
     *
     * 本メソッドは更新範囲を全画面に指定し、
     * 内容を変えないまま全画素へ波形をかけ直す。
     *
     * @note `display()` に渡すのは**物理**サイズ（`config().panel_width/panel_height`
     *       = 960x540）であり、論理サイズ（`width()/height()` = 540x960）ではない。
     *       理由は実装のコメントを参照。
     * `clearGhosting()` と違い白黒反転を行わないので**画面のフラッシュがなく**、
     * コストも約1/3（フルリフレッシュ1回分）で済む。
     *
     * | 用途 | 使うもの | フラッシュ | コスト目安 |
     * |---|---|---|---|
     * | 描画していない領域の退色を戻す | `refreshScreen()` | なし | 走査21回 |
     * | 蓄積した残像を消す・起動時の同期 | `clearGhosting()` | あり | 走査63回 |
     *
     * テキストやタッチ結果を描くたびに呼ぶ必要はない。
     * 数回に一度、あるいは操作待ちに入る直前などで十分。
     *
     * @param display 対象のディスプレイ
     * @param mode    使用するEPDモード。既定の epd_quality が最も濃く戻る
     */
    static void refreshScreen(M5GFX* display,
                              lgfx::v1::epd_mode_t mode = lgfx::v1::epd_mode_t::epd_quality);

    /**
     * @brief 残像を除去してからメインキャンバスの内容を描き直す
     *
     * `clearGhosting()` の後にメインキャンバスを転送する。
     * 表示中の画面を維持したまま残像だけ消したい場合に使う。
     *
     * 時間がかかる（フルリフレッシュ数回分）ので、
     * 場面の区切りやアイドル時など、遅延が許容できる場面で呼ぶこと。
     *
     * @param mode 除去に使うEPDモード
     */
    void refreshDisplay(lgfx::v1::epd_mode_t mode = lgfx::v1::epd_mode_t::epd_quality);
    
    /**
     * @brief トランジションのプリセット
     *
     * 注意: 現状どこからも呼ばれていない（main は startTransition() を直接呼ぶ）。
     * また括弧内の所要時間は実測値ではないため、目安として扱わないこと
     * （1ステップごとに fillScreen + 部分転送で画面更新が2回走るため、
     *   実際にはこれより大幅に遅い可能性が高い）。
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
    
    // 補足:
    // かつてここに getOptimalTypeForEPaper() / getOptimalStepsForEPaper() という
    // static ヘルパーがあったが、どこからも呼ばれておらず、
    // 「E-Paper向けに自動調整する」という記述だけが残っている状態だった。
    // 実際に startTransition() が行うのはステップ数を [1, 8] にクランプすることのみ。
    // 誤解を招くため削除した。自動調整が必要になった場合は
    // startTransition() 側に実装し、呼び出し経路を必ず用意すること。
};

#endif // _SIMPLE_TRANSITION_HPP_