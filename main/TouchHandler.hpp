// main/TouchHandler.hpp
#ifndef _TOUCH_HANDLER_HPP_
#define _TOUCH_HANDLER_HPP_

#include <M5GFX.h>
#include "esp_log.h"

// タッチポイント情報を保持する構造体
struct TouchPoint {
    int16_t x;            // X座標
    int16_t y;            // Y座標
    bool touched;         // タッチされているかどうか
    uint32_t timestamp;   // タイムスタンプ (ms)
};

// タッチ操作を管理するクラス
class TouchHandler {
private:
    static const char* TAG;  // ログタグ
    M5GFX* _display;         // ディスプレイへの参照
    TouchPoint _lastPoint;   // 最後に検出されたタッチポイント
    bool _initialized;       // 初期化フラグ
    uint16_t _touchPoints[8]; // タッチキャリブレーション用データ
    uint32_t _lastTouchTime;  // 最後にタッチされた時間
    uint32_t _touchDebounceMs; // タッチのデバウンス時間(ms)
    
public:
    TouchHandler();
    ~TouchHandler();
    
    // 初期化
    bool init(M5GFX* display);
    
    // タッチ状態の更新と取得
    bool update();
    
    // 最後に検出されたタッチポイントを取得
    const TouchPoint& getLastPoint() const { return _lastPoint; }
    
    // 指定した位置に円を描画
    void drawCircleAtTouch(uint16_t radius, uint32_t color);
    
    // タッチされたかどうかを取得
    bool isTouched() const { return _lastPoint.touched; }
    
    // タッチキャリブレーションを実行
    bool calibrate(uint16_t bg_color = TFT_BLACK, uint16_t fg_color = TFT_WHITE);
    
    // タッチデバウンス時間の設定
    void setTouchDebounce(uint32_t ms) { _touchDebounceMs = ms; }
};

#endif // _TOUCH_HANDLER_HPP_