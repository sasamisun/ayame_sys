// main/Button.cpp
#include "Button.hpp"
#include "esp_log.h"

// ログタグ
static const char *TAG = "BUTTON";

// Button クラスの実装
Button::Button(M5GFX *display, int x, int y, int width, int height, const char *label)
    : _x(x), _y(y), _width(width), _height(height), _state(ButtonState::Normal),
      _display(display), _font(nullptr), _textSize(1.0f), _visible(true),
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

void Button::draw()
{
    if (!_visible || !_display)
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

    // 背景を描画
    if (_style.cornerRadius > 0)
    {
        // 角が丸いボタン
        _display->fillRoundRect(_x, _y, _width, _height, _style.cornerRadius, bgColor);

        // 枠線を描画
        if (_style.borderWidth > 0)
        {
            for (int i = 0; i < _style.borderWidth; i++)
            {
                _display->drawRoundRect(_x + i, _y + i, _width - i * 2, _height - i * 2,
                                        _style.cornerRadius, borderColor);
            }
        }
    }
    else
    {
        // 角が四角いボタン
        _display->fillRect(_x, _y, _width, _height, bgColor);

        // 枠線を描画
        if (_style.borderWidth > 0)
        {
            for (int i = 0; i < _style.borderWidth; i++)
            {
                _display->drawRect(_x + i, _y + i, _width - i * 2, _height - i * 2, borderColor);
            }
        }
    }

    // テキストを描画
    if (_label[0] != '\0')
    {
        // フォントやテキストサイズを設定
        if (_font)
        {
            _display->setFont(_font);
        }
        _display->setTextColor(textColor);
        _display->setTextSize(_textSize);

        // テキスト中央寄せで描画
        _display->setTextDatum(middle_center);
        _display->drawString(_label, _x + _width / 2, _y + _height / 2);

        // テキスト配置を元に戻す
        _display->setTextDatum(top_left);
    }
}

// 更新関数 - ExtendedTouchPointを使用するように変更
bool Button::update(const ExtendedTouchPoint &touchPoint, bool isTouched)
{
    if (!_visible || _state == ButtonState::Disabled)
        return false;

    bool wasPressed = (_state == ButtonState::Pressed);
    bool containsTouch = containsPoint(touchPoint.x, touchPoint.y);

    // タッチの状態に応じてボタンの状態を更新
    if (isTouched && containsTouch)
    {
        // タッチされている場合
        if (!wasPressed)
        {
            // 押下状態に変更
            _state = ButtonState::Pressed;

            // 押されたイベントを発火
            if (_onPressed)
            {
                _onPressed(this);
            }

            // 再描画
            draw();
            return true;
        }
    }
    else if (wasPressed)
    {
        // タッチが離された場合
        _state = ButtonState::Normal;

        // 離されたイベントを発火
        if (_onReleased)
        {
            _onReleased(this);
        }

        // 再描画
        draw();
        return true;
    }

    return false;
}

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

// ButtonManager クラスの実装
ButtonManager::ButtonManager(M5GFX *display, TouchHandler *touchHandler)
    : _buttonCount(0), _display(display), _touchHandler(touchHandler)
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
}

void ButtonManager::handleTouch()
{
    // タッチハンドラが有効でなければ何もしない
    if (!_touchHandler)
        return;

    // タッチイベントを処理
    bool isTouched = _touchHandler->isTouched();
    const ExtendedTouchPoint &touchPoint = _touchHandler->getLastPoint();

    // 各ボタンの状態を更新
    for (int i = 0; i < _buttonCount; i++)
    {
        if (_buttons[i] && _buttons[i]->isVisible())
        {
            _buttons[i]->update(touchPoint, isTouched);
        }
    }
}

void ButtonManager::update()
{
    // タッチハンドラの更新
    if (!_touchHandler)
        return;

    // タッチハンドラを更新
    if (_touchHandler->update())
    {
        // タッチイベントが発生した場合

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
            // タッチ終了イベント
            const ExtendedTouchPoint &point = _touchHandler->getLastPoint();

            // 押下状態のボタンを探して、リリース処理を行う
            for (int i = 0; i < _buttonCount; i++)
            {
                if (_buttons[i] && _buttons[i]->isVisible() &&
                    _buttons[i]->getState() == ButtonState::Pressed)
                {

                    // ボタンを通常状態に戻す
                    _buttons[i]->setState(ButtonState::Normal);

                    // ボタンを再描画
                    _buttons[i]->draw();

                    // タッチ終了位置がボタン内ならReleasedイベントを発火
                    if (_buttons[i]->containsPoint(point.x, point.y))
                    {
                        if (_buttons[i]->getOnReleased())
                        {
                            _buttons[i]->getOnReleased()(_buttons[i]);
                        }
                    }
                }
            }
        }
        else if (_touchHandler->isSwipeEvent())
        {
            // スワイプイベント
            const ExtendedTouchPoint &startPoint = _touchHandler->getTouchStartPoint();
            SwipeDirection direction = _touchHandler->getLastSwipe();

            // スワイプ開始位置を含むボタンを探す
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