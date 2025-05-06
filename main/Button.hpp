// main/Button.hpp
#ifndef _BUTTON_HPP_
#define _BUTTON_HPP_

#include <M5GFX.h>
#include <functional>
#include "TouchHandler.hpp" // ExtendedTouchPointの定義を含む
#include "TouchHandler.hpp" // ExtendedTouchPointの定義を含む

// ボタンの状態を表す列挙型
enum class ButtonState
{
    Normal,  // 通常状態
    Pressed, // 押下状態
    Disabled // 無効状態
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
            5              // 角の丸み
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
            5              // 角の丸み
        };
    }
};

// ボタンクラス
class Button
{
class Button
{
private:
    int _x;             // X座標
    int _y;             // Y座標
    int _width;         // 幅
    int _height;        // 高さ
    char _label[64];    // ボタンラベル
    ButtonState _state; // 現在の状態
    ButtonStyle _style; // ボタンのスタイル
    M5GFX *_display;    // ディスプレイへの参照
    lgfx::IFont *_font; // フォント
    float _textSize;    // テキストサイズ
    bool _visible;      // 表示/非表示フラグ

    int _x;             // X座標
    int _y;             // Y座標
    int _width;         // 幅
    int _height;        // 高さ
    char _label[64];    // ボタンラベル
    ButtonState _state; // 現在の状態
    ButtonStyle _style; // ボタンのスタイル
    M5GFX *_display;    // ディスプレイへの参照
    lgfx::IFont *_font; // フォント
    float _textSize;    // テキストサイズ
    bool _visible;      // 表示/非表示フラグ

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

    using TouchCallback = std::function<void(Button *)>;
    TouchCallback _onPressed;  // 押された時のコールバック
    TouchCallback _onReleased; // 離された時のコールバック

    // スワイプイベントコールバック関数の型定義
    using SwipeCallback = std::function<void(Button *, SwipeDirection)>;
    SwipeCallback _onSwipeUp;    // 上スワイプのコールバック
    SwipeCallback _onSwipeDown;  // 下スワイプのコールバック
    SwipeCallback _onSwipeLeft;  // 左スワイプのコールバック
    SwipeCallback _onSwipeRight; // 右スワイプのコールバック

public:
    // コンストラクタ
    Button(M5GFX *display, int x, int y, int width, int height, const char *label = "");

    Button(M5GFX *display, int x, int y, int width, int height, const char *label = "");

    // デストラクタ
    ~Button() = default;


    // 描画関数
    void draw();


    // 状態更新関数 - ExtendedTouchPointを使用
    bool update(const ExtendedTouchPoint &touchPoint, bool isTouched);

    bool update(const ExtendedTouchPoint &touchPoint, bool isTouched);

    // ゲッターとセッター
    int getX() const { return _x; }
    int getY() const { return _y; }
    int getWidth() const { return _width; }
    int getHeight() const { return _height; }
    const char *getLabel() const { return _label; }
    const char *getLabel() const { return _label; }
    ButtonState getState() const { return _state; }
    bool isVisible() const { return _visible; }


    void setX(int x) { _x = x; }
    void setY(int y) { _y = y; }
    void setWidth(int width) { _width = width; }
    void setHeight(int height) { _height = height; }
    void setLabel(const char *label);
    void setLabel(const char *label);
    void setState(ButtonState state) { _state = state; }
    void setVisible(bool visible) { _visible = visible; }
    void setFont(lgfx::IFont *font) { _font = font; }
    void setFont(lgfx::IFont *font) { _font = font; }
    void setTextSize(float size) { _textSize = size; }
    void setStyle(const ButtonStyle &style) { _style = style; }

    void setStyle(const ButtonStyle &style) { _style = style; }

    // 有効/無効の切り替え
    void setEnabled(bool enabled) { _state = enabled ? ButtonState::Normal : ButtonState::Disabled; }
    bool isEnabled() const { return _state != ButtonState::Disabled; }


    // タッチ領域内かどうかを判定
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

    // スワイプイベントを処理
    bool handleSwipe(SwipeDirection direction);
};

// ボタンマネージャークラス - 複数のボタンを管理
class ButtonManager
{
class ButtonManager
{
private:
    static const int MAX_BUTTONS = 32; // 最大ボタン数
    Button *_buttons[MAX_BUTTONS];     // ボタン配列
    int _buttonCount;                  // 現在のボタン数
    M5GFX *_display;                   // ディスプレイへの参照
    TouchHandler *_touchHandler;       // タッチハンドラへの参照

    static const int MAX_BUTTONS = 32; // 最大ボタン数
    Button *_buttons[MAX_BUTTONS];     // ボタン配列
    int _buttonCount;                  // 現在のボタン数
    M5GFX *_display;                   // ディスプレイへの参照
    TouchHandler *_touchHandler;       // タッチハンドラへの参照

public:
    // コンストラクタ
    ButtonManager(M5GFX *display, TouchHandler *touchHandler);

    ButtonManager(M5GFX *display, TouchHandler *touchHandler);

    // デストラクタ
    ~ButtonManager();


    // ボタンの追加
    bool addButton(Button *button);

    bool addButton(Button *button);

    // ボタンの削除
    bool removeButton(Button *button);

    bool removeButton(Button *button);

    // 全ボタンのクリア
    void clearButtons();


    // 全ボタンの描画
    void drawButtons();


    // タッチイベントの処理
    void handleTouch();


    // 定期的な更新処理
    void update();


    // ボタン数を取得
    int getButtonCount() const { return _buttonCount; }
};

#endif // _BUTTON_HPP_