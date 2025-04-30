// main/TouchHandler.hpp
#ifndef _TOUCH_HANDLER_HPP_
#define _TOUCH_HANDLER_HPP_

#include <M5GFX.h>
#include <functional>

// M5GFXのタッチポイント構造体を拡張した構造体
struct ExtendedTouchPoint
{
    int x;              // X座標
    int y;              // Y座標
    uint32_t timestamp; // タイムスタンプ（msec）
};

// スワイプ方向の列挙型
enum class SwipeDirection
{
    None, // スワイプなし
    Up,   // 上方向
    Down, // 下方向
    Left, // 左方向
    Right // 右方向
};

// タッチイベントの列挙型
enum class TouchEvent
{
    None,    // イベントなし
    Touch,   // タッチ開始
    Release, // タッチ終了
    Swipe    // スワイプ
};

// タッチ状態を管理するクラス
class TouchHandler
{
private:
    M5GFX *_display;                     // ディスプレイへの参照
    lgfx::v1::touch_point_t _rawPoint;   // M5GFXのタッチポイント
    ExtendedTouchPoint _lastPoint;       // 最後のタッチ位置
    ExtendedTouchPoint _touchStartPoint; // タッチ開始位置
    ExtendedTouchPoint _touchEndPoint;   // タッチ終了位置
    bool _touched;                       // タッチされている状態
    bool _wasTouched;                    // 前回のタッチ状態
    bool _calibrated;                    // キャリブレーション済みフラグ
    uint16_t _touchCalibration[8];       // タッチキャリブレーションデータ
    TouchEvent _lastEvent;               // 最後に発生したイベント
    SwipeDirection _lastSwipe;           // 最後のスワイプ方向
    uint32_t _minSwipeDistance;          // スワイプと認識する最小距離

    // タッチイベントコールバック関数の型定義
    using TouchCallback = std::function<void(const ExtendedTouchPoint &)>;
    using SwipeCallback = std::function<void(SwipeDirection, const ExtendedTouchPoint &, const ExtendedTouchPoint &)>;

    TouchCallback _onTouchStart; // タッチ開始時のコールバック
    TouchCallback _onTouchEnd;   // タッチ終了時のコールバック
    SwipeCallback _onSwipe;      // スワイプ時のコールバック

    // スワイプ方向を判定
    SwipeDirection detectSwipe(const ExtendedTouchPoint &start, const ExtendedTouchPoint &end);

public:
    // コンストラクタ
    TouchHandler();

    // 初期化
    bool init(M5GFX *display);

    // タッチ情報の更新
    bool update();

    // タッチキャリブレーション
    void calibrate(uint32_t bg_color = TFT_BLACK, uint32_t fg_color = TFT_WHITE);

    // タッチされているかを取得
    bool isTouched() const { return _touched; }

    // 最後のタッチ位置を取得
    const ExtendedTouchPoint &getLastPoint() const { return _lastPoint; }

    // タッチ開始位置を取得
    const ExtendedTouchPoint &getTouchStartPoint() const { return _touchStartPoint; }

    // タッチ終了位置を取得
    const ExtendedTouchPoint &getTouchEndPoint() const { return _touchEndPoint; }

    // 最後のイベントを取得
    TouchEvent getLastEvent() const { return _lastEvent; }

    // 最後のスワイプ方向を取得
    SwipeDirection getLastSwipe() const { return _lastSwipe; }

    // 画面上のタッチ位置に円を描画（デバッグ用）
    void drawCircleAtTouch(int radius = 5, uint32_t color = TFT_RED);

    // キャリブレーション済みかを取得
    bool isCalibrated() const { return _calibrated; }

    // スワイプと認識する最小距離を設定
    void setMinSwipeDistance(uint32_t distance) { _minSwipeDistance = distance; }

    // イベントコールバックを設定
    void setOnTouchStart(TouchCallback callback) { _onTouchStart = callback; }
    void setOnTouchEnd(TouchCallback callback) { _onTouchEnd = callback; }
    void setOnSwipe(SwipeCallback callback) { _onSwipe = callback; }

    // イベント発生判定ヘルパー関数
    bool isTouchEvent() const { return _lastEvent == TouchEvent::Touch; }
    bool isReleaseEvent() const { return _lastEvent == TouchEvent::Release; }
    bool isSwipeEvent() const { return _lastEvent == TouchEvent::Swipe; }

    // スワイプ方向判定ヘルパー関数
    bool isSwipeUp() const { return _lastSwipe == SwipeDirection::Up; }
    bool isSwipeDown() const { return _lastSwipe == SwipeDirection::Down; }
    bool isSwipeLeft() const { return _lastSwipe == SwipeDirection::Left; }
    bool isSwipeRight() const { return _lastSwipe == SwipeDirection::Right; }
};

#endif // _TOUCH_HANDLER_HPP_