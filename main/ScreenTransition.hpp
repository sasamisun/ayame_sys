// main/ScreenTransition.hpp
// アドベンチャーゲーム用画面トランジションシステム
// M5Canvasを使用したPSRAMダブルバッファリング対応

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
 * アドベンチャーゲームでよく使われる効果を網羅
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
 * @brief トランジションの方向指定
 */
enum class TransitionDirection {
    IN,     // 新しい画面が入ってくる
    OUT,    // 現在の画面が出ていく
    BOTH    // 両方向同時（クロスフェード等）
};

/**
 * @brief イージング関数の種類
 * アニメーションの動きに変化をつける
 */
enum class EasingType {
    LINEAR,        // 線形（一定速度）
    EASE_IN,       // ゆっくり始まって加速
    EASE_OUT,      // 速く始まってゆっくり終わる
    EASE_IN_OUT,   // ゆっくり始まってゆっくり終わる
    BOUNCE,        // バウンス（跳ね返り）
    ELASTIC,       // エラスティック（ゴムのような）
    BACK,          // バック（少し逆方向に動いてから進む）
    CUBIC          // 3次ベジェ曲線
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
 * @brief トランジション設定構造体
 */
struct TransitionConfig {
    TransitionType type;           // トランジション種類
    uint32_t duration_ms;          // 継続時間（ミリ秒）
    EasingType easing;            // イージング関数
    TransitionDirection direction; // 方向
    uint32_t color;               // フェード色等で使用
    bool use_psram;               // PSRAM使用フラグ
    float speed_multiplier;       // 速度倍率（1.0が標準）
    bool reverse;                 // 逆方向フラグ
    
    // デフォルト設定
    static TransitionConfig defaultConfig() {
        return {
            TransitionType::FADE_BLACK,   // 標準的な黒フェード
            1000,                         // 1秒
            EasingType::EASE_IN_OUT,      // スムーズな動き
            TransitionDirection::BOTH,    // 両方向
            TFT_BLACK,                    // 黒色
            true,                         // PSRAM使用
            1.0f,                         // 標準速度
            false                         // 通常方向
        };
    }
};

/**
 * @brief ScreenTransitionクラス
 * アドベンチャーゲーム用の高機能画面遷移システム
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
    
    // トランジション状態管理
    TransitionState _state;       // 現在の状態
    TransitionConfig _config;     // 現在の設定
    int64_t _startTime;           // 開始時刻
    int64_t _currentTime;         // 現在時刻
    float _progress;              // 進行度（0.0-1.0）
    
    // コールバック関数
    std::function<void()> _onTransitionStart;     // 開始時コールバック
    std::function<void(float)> _onTransitionProgress; // 進行中コールバック
    std::function<void()> _onTransitionComplete;  // 完了時コールバック
    
    // 内部メソッド
    void cleanup();                               // リソース解放
    bool createCanvases();                        // キャンバス作成
    void updateProgress();                        // 進行度更新
    float applyEasing(float t);                   // イージング適用
    
    // 各トランジション効果の実装
    void renderFade(float progress);              // フェード効果
    void renderSlide(float progress);             // スライド効果
    void renderWipe(float progress);              // ワイプ効果
    void renderCircle(float progress);            // 円形効果
    void renderPixelate(float progress);          // ピクセル化効果
    void renderVenetianBlind(float progress);     // ブラインド効果
    void renderCheckerboard(float progress);      // チェッカーボード効果
    void renderSpiral(float progress);            // 螺旋効果
    void renderRipple(float progress);            // 波紋効果
    void renderMosaic(float progress);            // モザイク効果
    void renderPageTurn(float progress);          // ページめくり効果
    
    // ユーティリティ関数
    void blendCanvases(M5Canvas* src, M5Canvas* dst, float alpha);
    void copyCanvasRegion(M5Canvas* src, M5Canvas* dst, int sx, int sy, int sw, int sh, int dx, int dy);
    uint32_t interpolateColor(uint32_t color1, uint32_t color2, float t);
    void applyImageEffect(M5Canvas* canvas, float intensity);

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
    float getProgress() const { return _progress; }
    bool isRunning() const { return _state == TransitionState::RUNNING; }
    bool isFinished() const { return _state == TransitionState::FINISHED; }
    
    // コールバック設定
    void setOnTransitionStart(std::function<void()> callback) { _onTransitionStart = callback; }
    void setOnTransitionProgress(std::function<void(float)> callback) { _onTransitionProgress = callback; }
    void setOnTransitionComplete(std::function<void()> callback) { _onTransitionComplete = callback; }
    
    // ユーティリティ
    M5Canvas* getSourceCanvas() { return _sourceCanvas; }
    M5Canvas* getTargetCanvas() { return _targetCanvas; }
    M5Canvas* getWorkCanvas() { return _workCanvas; }
    
    /**
     * @brief プリセット設定の取得
     */
    static TransitionConfig getFadeConfig(uint32_t duration_ms = 1000, uint32_t color = TFT_BLACK);
    static TransitionConfig getSlideConfig(TransitionType slide_type, uint32_t duration_ms = 800);
    static TransitionConfig getCircleConfig(TransitionType circle_type, uint32_t duration_ms = 1200);
    static TransitionConfig getEffectConfig(TransitionType effect_type, uint32_t duration_ms = 1500);
};

#endif // _SCREEN_TRANSITION_HPP_