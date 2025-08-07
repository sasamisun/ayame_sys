// main/Button.hpp - Canvas Drawing Support Version
// Canvas描画対応ボタンシステム
#ifndef _BUTTON_HPP_
#define _BUTTON_HPP_

#include <M5GFX.h>
#include <functional>
#include "TouchHandler.hpp" // ExtendedTouchPointの定義を含む

// ボタンの状態を表す列挙型
enum class ButtonState
{
    Normal,  // 通常状態
    Pressed, // 押下状態
    Disabled // 無効状態
};

// ボタンのスタイルを定義する構造体
struct ButtonStyle
{
    uint32_t bgColor;             // 背景色
    uint32_t bgColorPressed;      // 押下時の背景色
    uint32_t bgColorDisabled;     // 無効時の背景色
    uint32_t textColor;           // テキスト色
    uint32_t textColorPressed;    // 押下時のテキスト色
    uint32_t textColorDisabled;   // 無効時のテキスト色
    uint32_t borderColor;         // 枠線の色
    uint32_t borderColorPressed;  // 押下時の枠線の色
    uint32_t borderColorDisabled; // 無効時の枠線の色
    uint8_t borderWidth;          // 枠線の幅
    uint8_t cornerRadius;         // 角の丸み

    // デフォルトのボタンスタイル
    static ButtonStyle defaultStyle()
    {
        return {
            TFT_WHITE,     // 背景色
            TFT_LIGHTGRAY, // 押下時の背景色
            TFT_DARKGRAY,  // 無効時の背景色
            TFT_BLACK,     // テキスト色
            TFT_BLACK,     // 押下時のテキスト色
            TFT_LIGHTGRAY, // 無効時のテキスト色
            TFT_BLACK,     // 枠線の色
            TFT_BLACK,     // 押下時の枠線の色
            TFT_DARKGRAY,  // 無効時の枠線の色
            2,             // 枠線の幅
            5,             // 角の丸み
        };
    }
};

// ボタンクラス（Canvas対応版）
class Button
{
private:
    int _x;                       // X座標
    int _y;                       // Y座標
    int _width;                   // 幅
    int _height;                  // 高さ
    char _label[64];              // ボタンラベル
    ButtonState _state;           // 現在の状態
    ButtonStyle _style;           // ボタンのスタイル
    M5GFX *_display;              // ディスプレイへの参照（フォールバック用）
    lgfx::LGFX_Sprite *_drawTarget; // Canvas描画先（nullptrでディスプレイに直接描画）
    lgfx::IFont *_font;           // フォント
    float _textSize;              // テキストサイズ
    bool _visible;                // 表示/非表示フラグ

    // タッチイベントコールバック関数の型定義
    using TouchCallback = std::function<void(Button *)>;
    TouchCallback _onPressed;  // 押された時のコールバック
    TouchCallback _onReleased; // 離された時のコールバック

    // スワイプイベントコールバック関数の型定義
    using SwipeCallback = std::function<void(Button *, SwipeDirection)>;
    SwipeCallback _onSwipeUp;    // 上スワイプのコールバック
    SwipeCallback _onSwipeDown;  // 下スワイプのコールバック
    SwipeCallback _onSwipeLeft;  // 左スワイプのコールバック
    SwipeCallback _onSwipeRight; // 右スワイプのコールバック

    /**
     * @brief Canvas（lgfx::LGFX_Sprite）への描画処理
     * @param canvas 描画先Canvas
     * @param bgColor 背景色
     * @param textColor テキスト色
     * @param borderColor 枠線色
     */
    void drawToCanvas(lgfx::LGFX_Sprite* canvas, uint32_t bgColor, uint32_t textColor, uint32_t borderColor);

    /**
     * @brief Display（M5GFX）への描画処理
     * @param display 描画先Display
     * @param bgColor 背景色
     * @param textColor テキスト色
     * @param borderColor 枠線色
     */
    void drawToDisplay(M5GFX* display, uint32_t bgColor, uint32_t textColor, uint32_t borderColor);

public:
    /**
     * @brief コンストラクタ
     * @param display M5GFXディスプレイオブジェクト
     * @param x ボタンのX座標
     * @param y ボタンのY座標
     * @param width ボタンの幅
     * @param height ボタンの高さ
     * @param label ボタンのラベル文字列
     */
    Button(M5GFX *display, int x, int y, int width, int height, const char *label = "");

    /**
     * @brief デストラクタ
     */
    ~Button() = default;

    /**
     * @brief 描画先Canvas/Spriteを設定
     * @param canvas 描画先Canvas（nullptrでディスプレイに直接描画）
     */
    void setDrawTarget(lgfx::LGFX_Sprite *canvas);

    /**
     * @brief 描画関数
     * 設定された描画先（Canvas優先、なければDisplay）に描画
     */
    void draw();

    /**
     * @brief 状態更新関数 - ExtendedTouchPointを使用
     * @param touchPoint タッチ座標情報
     * @param isTouched タッチされているかのフラグ
     * @return 状態が変更された場合はtrue
     */
    bool update(const ExtendedTouchPoint &touchPoint, bool isTouched);

    // ゲッターとセッター
    int getX() const { return _x; }
    int getY() const { return _y; }
    int getWidth() const { return _width; }
    int getHeight() const { return _height; }
    const char *getLabel() const { return _label; }
    ButtonState getState() const { return _state; }
    bool isVisible() const { return _visible; }
    lgfx::LGFX_Sprite *getDrawTarget() const { return _drawTarget; }

    void setX(int x) { _x = x; }
    void setY(int y) { _y = y; }
    void setWidth(int width) { _width = width; }
    void setHeight(int height) { _height = height; }
    void setLabel(const char *label);
    void setState(ButtonState state) { _state = state; }
    void setVisible(bool visible) { _visible = visible; }
    void setFont(lgfx::IFont *font) { _font = font; }
    void setTextSize(float size) { _textSize = size; }
    void setStyle(const ButtonStyle &style) { _style = style; }

    // 有効/無効の切り替え
    void setEnabled(bool enabled) { _state = enabled ? ButtonState::Normal : ButtonState::Disabled; }
    bool isEnabled() const { return _state != ButtonState::Disabled; }

    /**
     * @brief タッチ領域内かどうかを判定
     * @param x X座標
     * @param y Y座標
     * @return 領域内ならtrue
     */
    bool containsPoint(int x, int y) const;

    // イベントハンドラを設定
    void setOnPressed(TouchCallback callback) { _onPressed = callback; }
    void setOnReleased(TouchCallback callback) { _onReleased = callback; }

    // イベントハンドラを取得
    TouchCallback getOnPressed() const { return _onPressed; }
    TouchCallback getOnReleased() const { return _onReleased; }

    // スワイプイベントハンドラを設定
    void setOnSwipeUp(SwipeCallback callback) { _onSwipeUp = callback; }
    void setOnSwipeDown(SwipeCallback callback) { _onSwipeDown = callback; }
    void setOnSwipeLeft(SwipeCallback callback) { _onSwipeLeft = callback; }
    void setOnSwipeRight(SwipeCallback callback) { _onSwipeRight = callback; }

    // スワイプイベントハンドラを取得
    SwipeCallback getOnSwipeUp() const { return _onSwipeUp; }
    SwipeCallback getOnSwipeDown() const { return _onSwipeDown; }
    SwipeCallback getOnSwipeLeft() const { return _onSwipeLeft; }
    SwipeCallback getOnSwipeRight() const { return _onSwipeRight; }

    /**
     * @brief スワイプイベントを処理
     * @param direction スワイプ方向
     * @return 処理された場合はtrue
     */
    bool handleSwipe(SwipeDirection direction);
};

// ボタンマネージャークラス - 複数のボタンを管理（Canvas対応版）
class ButtonManager
{
private:
    static const int MAX_BUTTONS = 32;          // 最大ボタン数
    Button *_buttons[MAX_BUTTONS];              // ボタン配列
    int _buttonCount;                           // 現在のボタン数
    M5GFX *_display;                            // ディスプレイへの参照（フォールバック用）
    TouchHandler *_touchHandler;                // タッチハンドラへの参照
    lgfx::LGFX_Sprite *_drawTarget;             // Canvas描画先（nullptrでディスプレイに直接描画）

public:
    /**
     * @brief コンストラクタ
     * @param display M5GFXディスプレイオブジェクト
     * @param touchHandler タッチハンドラオブジェクト
     */
    ButtonManager(M5GFX *display, TouchHandler *touchHandler);

    /**
     * @brief デストラクタ
     */
    ~ButtonManager();

    /**
     * @brief 描画先Canvas/Spriteを設定
     * 管理している全ボタンの描画先も同時に変更される
     * @param canvas 描画先Canvas（nullptrでディスプレイに直接描画）
     */
    void setDrawTarget(lgfx::LGFX_Sprite *canvas);

    /**
     * @brief 描画先Canvas/Spriteを取得
     * @return 現在の描画先Canvas（nullptrの場合はディスプレイ）
     */
    lgfx::LGFX_Sprite *getDrawTarget() const { return _drawTarget; }

    /**
     * @brief ボタンの追加
     * @param button 追加するボタンオブジェクト
     * @return 成功時はtrue
     */
    bool addButton(Button *button);

    /**
     * @brief ボタンの削除
     * @param button 削除するボタンオブジェクト
     * @return 成功時はtrue
     */
    bool removeButton(Button *button);

    /**
     * @brief 全ボタンのクリア
     */
    void clearButtons();

    /**
     * @brief 全ボタンの描画
     * 設定された描画先（Canvas優先、なければDisplay）に描画
     */
    void drawButtons();

    /**
     * @brief 指定したCanvas/Displayに全ボタンを描画
     * 一時的に描画先を変更して描画する
     * @param target 描画先Canvas/Display
     */
    void drawButtonsToTarget(lgfx::LGFX_Sprite *target);

    /**
     * @brief タッチイベントの処理
     * 非推奨：update()メソッドを使用してください
     */
    void handleTouch();

    /**
     * @brief 定期的な更新処理
     * メインループから呼び出す
     */
    void update();

    /**
     * @brief ボタン数を取得
     * @return 現在管理しているボタン数
     */
    int getButtonCount() const { return _buttonCount; }

    /**
     * @brief 指定インデックスのボタンを取得
     * @param index ボタンのインデックス（0から始まる）
     * @return ボタンオブジェクト（範囲外の場合はnullptr）
     */
    Button* getButton(int index) const 
    { 
        return (index >= 0 && index < _buttonCount) ? _buttons[index] : nullptr; 
    }

    /**
     * @brief ラベルでボタンを検索
     * @param label 検索するラベル文字列
     * @return 見つかったボタンオブジェクト（見つからない場合はnullptr）
     */
    Button* findButtonByLabel(const char* label) const
    {
        if (!label) return nullptr;
        
        for (int i = 0; i < _buttonCount; i++) {
            if (_buttons[i] && strcmp(_buttons[i]->getLabel(), label) == 0) {
                return _buttons[i];
            }
        }
        return nullptr;
    }
};

#endif // _BUTTON_HPP_