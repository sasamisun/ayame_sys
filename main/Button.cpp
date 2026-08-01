// main/Button.cpp - Type Safe Canvas Drawing Support Version
// 型安全Canvas描画対応ボタンシステム
#include "Button.hpp"
#include "esp_log.h"

// ログタグ
static const char *TAG = "BUTTON";

// Button クラスの実装
Button::Button(M5GFX *display, int x, int y, int width, int height, const char *label)
    : _x(x), _y(y), _width(width), _height(height), _state(ButtonState::Normal),
      _display(display), _drawTarget(nullptr), _font(nullptr), _textSize(1.0f), _visible(true),
      _onPressed(nullptr), _onReleased(nullptr),
      _onSwipeUp(nullptr), _onSwipeDown(nullptr), _onSwipeLeft(nullptr), _onSwipeRight(nullptr)
{

    // ラベルの設定
    setLabel(label);

    // デフォルトスタイルの設定
    _style = ButtonStyle::defaultStyle();
}

void Button::setLabel(const char *label)
{
    // ラベルを安全にコピー
    if (label)
    {
        strncpy(_label, label, sizeof(_label) - 1);
        _label[sizeof(_label) - 1] = '\0';
    }
    else
    {
        _label[0] = '\0';
    }
}

bool Button::containsPoint(int x, int y) const
{
    // 点がボタン領域内にあるかをチェック
    return _visible && x >= _x && x < (_x + _width) && y >= _y && y < (_y + _height);
}

void Button::setDrawTarget(lgfx::LGFX_Sprite *canvas)
{
    _drawTarget = canvas;
    // ボタン追加時や一括切替時にボタン数だけ呼ばれるため ESP_LOGD にする
    // （まとめた件数は ButtonManager::setDrawTarget() が ESP_LOGI で出す）
    ESP_LOGD(TAG, "Button draw target set to %s", canvas ? "canvas" : "display");
}

void Button::draw()
{
    if (!_visible)
        return;

    // 現在の状態に応じた色を取得
    uint32_t bgColor, textColor, borderColor;

    switch (_state)
    {
    case ButtonState::Pressed:
        bgColor = _style.bgColorPressed;
        textColor = _style.textColorPressed;
        borderColor = _style.borderColorPressed;
        break;
    case ButtonState::Disabled:
        bgColor = _style.bgColorDisabled;
        textColor = _style.textColorDisabled;
        borderColor = _style.borderColorDisabled;
        break;
    default:
        bgColor = _style.bgColor;
        textColor = _style.textColor;
        borderColor = _style.borderColor;
        break;
    }

    // 描画先を決定（Canvas優先、なければDisplay）。
    // lgfx::LGFX_Sprite も M5GFX も lgfx::LovyanGFX 派生なので、
    // 基底ポインタに束ねて単一の描画実装へ渡す。
    lgfx::LovyanGFX *target = nullptr;
    const char *targetName = nullptr;

    if (_drawTarget != nullptr)
    {
        target = _drawTarget;
        targetName = "canvas";
    }
    else if (_display != nullptr)
    {
        target = _display;
        targetName = "display";
    }

    if (target == nullptr)
    {
        ESP_LOGE(TAG, "No valid draw target available for button '%s'", _label);
        return;
    }

    drawTo(target, bgColor, textColor, borderColor);
    ESP_LOGD(TAG, "Button '%s' drawn to %s at (%d,%d)", _label, targetName, _x, _y);
}

void Button::drawTo(lgfx::LovyanGFX* target, uint32_t bgColor, uint32_t textColor, uint32_t borderColor)
{
    // 背景を描画
    if (_style.cornerRadius > 0)
    {
        // 角が丸いボタン
        target->fillRoundRect(_x, _y, _width, _height, _style.cornerRadius, bgColor);

        // 枠線を描画
        if (_style.borderWidth > 0)
        {
            for (int i = 0; i < _style.borderWidth; i++)
            {
                target->drawRoundRect(_x + i, _y + i, _width - i * 2, _height - i * 2,
                                      _style.cornerRadius, borderColor);
            }
        }
    }
    else
    {
        // 角が四角いボタン
        target->fillRect(_x, _y, _width, _height, bgColor);

        // 枠線を描画
        if (_style.borderWidth > 0)
        {
            for (int i = 0; i < _style.borderWidth; i++)
            {
                target->drawRect(_x + i, _y + i, _width - i * 2, _height - i * 2, borderColor);
            }
        }
    }

    // テキストを描画
    if (_label[0] != '\0')
    {
        // フォントやテキストサイズを設定
        if (_font)
        {
            target->setFont(_font);
        }
        target->setTextColor(textColor);
        target->setTextSize(_textSize);

        // テキスト中央寄せで描画
        target->setTextDatum(middle_center);
        target->drawString(_label, _x + _width / 2, _y + _height / 2);

        // テキスト配置を元に戻す
        // TODO: 呼び出し前の datum を保存して復元するのが正しい（現状は top_left 決め打ち）
        target->setTextDatum(top_left);
    }
}

// 補足: Button::update(const ExtendedTouchPoint&, bool) をここに実装していたが削除した。
//       ButtonManager::update() が同じ状態遷移をインラインで実装しており、
//       2重実装になっていた（領域外で指を離したときの挙動が両者で異なっていた）。
//       唯一の呼び出し元だった ButtonManager::handleTouch() を削除した時点で
//       呼び出し元0件になったため、ButtonManager::update() 側に一本化した。


// スワイプイベントを処理
bool Button::handleSwipe(SwipeDirection direction)
{
    if (!_visible || _state == ButtonState::Disabled)
        return false;

    bool handled = false;

    // 方向に応じたコールバックを実行
    switch (direction)
    {
    case SwipeDirection::Up:
        if (_onSwipeUp)
        {
            _onSwipeUp(this, direction);
            handled = true;
        }
        break;
    case SwipeDirection::Down:
        if (_onSwipeDown)
        {
            _onSwipeDown(this, direction);
            handled = true;
        }
        break;
    case SwipeDirection::Left:
        if (_onSwipeLeft)
        {
            _onSwipeLeft(this, direction);
            handled = true;
        }
        break;
    case SwipeDirection::Right:
        if (_onSwipeRight)
        {
            _onSwipeRight(this, direction);
            handled = true;
        }
        break;
    default:
        break;
    }

    return handled;
}

// ButtonManager クラスの実装（Canvas対応版）
ButtonManager::ButtonManager(M5GFX *display, TouchHandler *touchHandler)
    : _buttonCount(0), _display(display), _touchHandler(touchHandler), _drawTarget(nullptr)
{

    // ボタン配列を初期化
    for (int i = 0; i < MAX_BUTTONS; i++)
    {
        _buttons[i] = nullptr;
    }
}

ButtonManager::~ButtonManager()
{
    // 全ボタンのクリア
    clearButtons();
}

void ButtonManager::setDrawTarget(lgfx::LGFX_Sprite *canvas)
{
    _drawTarget = canvas;
    
    // 管理している全ボタンの描画先も変更
    for (int i = 0; i < _buttonCount; i++)
    {
        if (_buttons[i])
        {
            _buttons[i]->setDrawTarget(canvas);
        }
    }
    
    ESP_LOGI(TAG, "ButtonManager draw target set to %s for %d buttons", 
             canvas ? "canvas" : "display", _buttonCount);
}

bool ButtonManager::addButton(Button *button)
{
    if (!button || _buttonCount >= MAX_BUTTONS)
    {
        ESP_LOGE(TAG, "Failed to add button: %s",
                 !button ? "Null button" : "Maximum buttons reached");
        return false;
    }

    // 既に追加済みのボタンかチェック
    for (int i = 0; i < _buttonCount; i++)
    {
        if (_buttons[i] == button)
        {
            ESP_LOGW(TAG, "Button already added");
            return false;
        }
    }

    // ボタンを追加
    _buttons[_buttonCount++] = button;
    
    // ボタンの描画先を現在の設定に合わせる
    button->setDrawTarget(_drawTarget);
    
    ESP_LOGI(TAG, "Button added, count: %d", _buttonCount);
    return true;
}

bool ButtonManager::removeButton(Button *button)
{
    if (!button)
        return false;

    for (int i = 0; i < _buttonCount; i++)
    {
        if (_buttons[i] == button)
        {
            // ボタンを削除し、配列を詰める
            for (int j = i; j < _buttonCount - 1; j++)
            {
                _buttons[j] = _buttons[j + 1];
            }
            _buttons[--_buttonCount] = nullptr;
            ESP_LOGI(TAG, "Button removed, count: %d", _buttonCount);
            return true;
        }
    }

    ESP_LOGW(TAG, "Button not found for removal");
    return false;
}

void ButtonManager::clearButtons()
{
    // ボタン配列をクリア
    for (int i = 0; i < _buttonCount; i++)
    {
        _buttons[i] = nullptr;
    }
    _buttonCount = 0;
    ESP_LOGI(TAG, "All buttons cleared");
}

void ButtonManager::drawButtons()
{
    // 全ボタンを描画
    for (int i = 0; i < _buttonCount; i++)
    {
        if (_buttons[i] && _buttons[i]->isVisible())
        {
            _buttons[i]->draw();
        }
    }
    
    ESP_LOGD(TAG, "Drew %d buttons to %s", _buttonCount, 
             _drawTarget ? "canvas" : "display");
}

// 補足: drawButtonsToTarget() をここに実装していたが、呼び出し元が存在しない死蔵コードだった。
//       実装にも問題があり、全ボタンの描画先を一時変更する際にボタン数だけ
//       setDrawTarget() のログが出るうえ、_drawTarget メンバ自体は変更しないため
//       drawButtons() 末尾のログが誤った描画先を表示していた。削除した。


// 補足: handleTouch() をここに実装していたが、ヘッダで「非推奨」と明記されたうえ
//       呼び出し元も存在しない死蔵コードだったため削除した。
//       実際の入力処理は ButtonManager::update() が行っている。
//       なお handleTouch() は Button::update() の唯一の呼び出し元だったため、
//       この削除により Button::update() は未使用になる（§12.3 #27 の判断待ち）。


void ButtonManager::update()
{
    if (!_touchHandler)
        return;

    // 注意: ここでは TouchHandler::update() を呼ばない。
    //
    // TouchHandler::update() はハードウェアを読んで内部状態を更新し、
    // イベントを1回だけ返す破壊的メソッドである。
    // 2回呼ぶと2回目は必ず None を返し、_wasTouched も潰れてイベントを取りこぼす。
    //
    // 以前はこのメソッドが内部でポーリングしており、呼び出し側の loop() でも
    // 別途 touchHandler.update() を呼んでいたため、
    // どちらか一方がイベントを取り逃していた。
    // ポーリングは呼び出し側で1回だけ行い、本メソッドはその結果を読むだけにする。
    {
        if (_touchHandler->isTouchEvent())
        {
            // タッチ開始イベント
            const ExtendedTouchPoint &point = _touchHandler->getLastPoint();

            // タッチ位置を含むボタンを探す
            for (int i = 0; i < _buttonCount; i++)
            {
                if (_buttons[i] && _buttons[i]->isVisible() && _buttons[i]->isEnabled() &&
                    _buttons[i]->containsPoint(point.x, point.y))
                {

                    // ボタンを押下状態に更新
                    _buttons[i]->setState(ButtonState::Pressed);

                    // ボタンを再描画
                    _buttons[i]->draw();

                    // ボタンのPressedイベントを発火
                    if (_buttons[i]->getOnPressed())
                    {
                        _buttons[i]->getOnPressed()(_buttons[i]);
                    }
                }
            }
        }
        else if (_touchHandler->isReleaseEvent())
        {
            // タッチ終了イベント。
            // スワイプが成立した場合も TouchEvent::Release として通知されるため、
            // ここでリリース処理とスワイプ配送の両方を行う。
            // 以前は else if (isSwipeEvent()) を連ねていたので、
            // スワイプ時にリリース処理が丸ごとスキップされ、
            // 押下状態のボタンが押されたままの表示で固まっていた。
            const ExtendedTouchPoint &point = _touchHandler->getLastPoint();
            const bool swiped = _touchHandler->isSwipeEvent();

            // 押下状態のボタンを探して、リリース処理を行う
            for (int i = 0; i < _buttonCount; i++)
            {
                if (_buttons[i] && _buttons[i]->isVisible() &&
                    _buttons[i]->getState() == ButtonState::Pressed)
                {

                    // ボタンを通常状態に戻す（スワイプでも必ず実行する）
                    _buttons[i]->setState(ButtonState::Normal);

                    // ボタンを再描画
                    _buttons[i]->draw();

                    // タッチ終了位置がボタン内ならReleasedイベントを発火。
                    // ただしスワイプが成立した場合はタップではないので発火しない。
                    if (!swiped && _buttons[i]->containsPoint(point.x, point.y))
                    {
                        if (_buttons[i]->getOnReleased())
                        {
                            _buttons[i]->getOnReleased()(_buttons[i]);
                        }
                    }
                }
            }

            // スワイプはタッチ開始位置のボタンへ配送する
            if (swiped)
            {
                const ExtendedTouchPoint &startPoint = _touchHandler->getTouchStartPoint();
                SwipeDirection direction = _touchHandler->getLastSwipe();

                for (int i = 0; i < _buttonCount; i++)
                {
                    if (_buttons[i] && _buttons[i]->isVisible() && _buttons[i]->isEnabled() &&
                        _buttons[i]->containsPoint(startPoint.x, startPoint.y))
                    {

                        // ボタンのスワイプイベントを処理
                        if (_buttons[i]->handleSwipe(direction))
                        {
                            // イベントが処理された場合は終了
                            break;
                        }
                    }
                }
            }
        }
    }
}