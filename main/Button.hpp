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
    lgfx::IFont *_font;           // フォント（静的なもの）
    const uint8_t *_vlwFont;      // VLWフォント（日本語用。loadFont で読む）
    const uint8_t *_iconData;     // アイコンPNG（埋め込み。nullptr なら通常描画）
    size_t _iconLen;              // アイコンPNGのバイト数
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
     * @brief 描画先への描画処理（Canvas / Display 共通）
     *
     * lgfx::LGFX_Sprite と M5GFX はいずれも lgfx::LovyanGFX の派生なので、
     * 基底クラスのポインタで受けて1本の実装で処理する。
     * （以前は drawToCanvas() / drawToDisplay() として同一内容を2重に持っていた）
     *
     * @param target 描画先
     * @param bgColor 背景色
     * @param textColor テキスト色
     * @param borderColor 枠線色
     */
    void drawTo(lgfx::LovyanGFX* target, uint32_t bgColor, uint32_t textColor, uint32_t borderColor);

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

    // 注: update(const ExtendedTouchPoint&, bool) は ButtonManager::update() と
    //     同じ状態遷移の2重実装だったため削除した。入力処理は ButtonManager::update() に一本化。

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

    /**
     * @brief VLW フォント（日本語）を使う
     *
     * `setFont()` は静的な `IFont*` 用で、**VLW バイナリには使えない**。
     * VLW は `loadFont()` で読み込む必要があるため、経路を分けてある。
     * これを設定しないと、ラベルの日本語が既定フォントで豆腐になる。
     *
     * @param vlwData VLW バイナリの先頭。`nullptr` で既定フォントに戻る
     *
     * @note データは呼び出し側が保持し続けること（コピーしない）。
     * @note 描画のたびに `loadFont()` が走る。ボタンは画面ごとに1回しか
     *       描かないので問題にならないが、頻繁に描き直す用途では注意。
     */
    void setVlwFont(const uint8_t *vlwData) { _vlwFont = vlwData; }

    /**
     * @brief ボタンの見た目を画像にする
     *
     * 設定すると、背景・枠線・ラベルの代わりに PNG を描く。
     * 画像はボタンの寸法に合わせて用意すること（拡大縮小はしない）。
     *
     * @param pngData PNG のバイト列。`nullptr` で通常の描画に戻る
     * @param pngLen  バイト数
     *
     * @note **画像はファームウェアに埋め込むこと。SD から読んではいけない。**
     *       USB MSC が有効な間は全てのファイル操作が失敗するため、
     *       SD 由来のアイコンは MSC 中に描けなくなる。
     *       「USB を切る」ボタンが見えなくなって操作不能になる。
     * @note データは呼び出し側が保持し続けること（コピーしない）。
     */
    void setIcon(const uint8_t *pngData, size_t pngLen)
    {
        _iconData = pngData;
        _iconLen = pngLen;
    }

    bool hasIcon() const { return _iconData != nullptr && _iconLen > 0; }
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
    // const参照で返す。値返しだと呼び出し側の
    // 「if (getOnPressed()) getOnPressed()(btn);」というパターンで
    // std::function のコピー（ヒープ確保を伴う場合がある）が毎イベント発生していた。
    const TouchCallback &getOnPressed() const { return _onPressed; }
    const TouchCallback &getOnReleased() const { return _onReleased; }

    // スワイプイベントハンドラを設定
    void setOnSwipeUp(SwipeCallback callback) { _onSwipeUp = callback; }
    void setOnSwipeDown(SwipeCallback callback) { _onSwipeDown = callback; }
    void setOnSwipeLeft(SwipeCallback callback) { _onSwipeLeft = callback; }
    void setOnSwipeRight(SwipeCallback callback) { _onSwipeRight = callback; }

    // スワイプイベントハンドラを取得（同様にconst参照で返す）
    const SwipeCallback &getOnSwipeUp() const { return _onSwipeUp; }
    const SwipeCallback &getOnSwipeDown() const { return _onSwipeDown; }
    const SwipeCallback &getOnSwipeLeft() const { return _onSwipeLeft; }
    const SwipeCallback &getOnSwipeRight() const { return _onSwipeRight; }

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
     *
     * 保持しているボタンは delete しない（下記「所有権」を参照）。
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
     * @brief ボタンを預ける（**所有権を渡す**）
     *
     * 以後の解放は `ButtonManager` が行う。
     * `clearButtons()` / `removeButton()` / デストラクタが `delete` する。
     *
     * **呼び出し側で `delete` しないこと。** 二重解放になる。
     *
     * 画面ごとにボタンの集合を作り直す作りなので、
     * 「manager から外す」と「解放する」を別々に書くと必ず漏れる。
     * そのため所有させている。
     */
    bool addButton(Button *button);

    /**
     * @brief ボタンを1つ外して**解放する**
     * @param button 外すボタン。以後そのポインタは無効
     * @return 見つかって外せたか
     */
    bool removeButton(Button *button);

    /**
     * @brief 全ボタンの登録解除（delete はしない）
     */
    void clearButtons();

    /**
     * @brief 全ボタンの描画
     * 設定された描画先（Canvas優先、なければDisplay）に描画
     */
    void drawButtons();

    // 注: drawButtonsToTarget() は呼び出し元が存在しない死蔵コードだったため削除した。

    // 注: handleTouch() は呼び出し元が存在しない死蔵コードだったため削除した。

    /**
     * @brief 入力イベントをボタンへ配送する
     *
     * **呼び出す前に `TouchHandler::update()` を1回だけ実行しておくこと。**
     * 本メソッドは内部でポーリングせず、`TouchHandler` が保持している
     * 現在のイベント状態を読むだけである。
     *
     * `TouchHandler::update()` はハードウェアを読んで内部状態を更新し
     * イベントを1回だけ返す破壊的メソッドなので、1ループにつき1回しか呼べない。
     * 本メソッドがポーリングまで行うと、呼び出し側が別途ポーリングした際に
     * どちらかがイベントを取りこぼす。
     *
     * ```cpp
     * touchHandler.update();      // ポーリングは呼び出し側で1回だけ
     * buttonManager->update();    // その結果を配送する
     * ```
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