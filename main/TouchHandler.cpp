// main/TouchHandler.cpp
#include "TouchHandler.hpp"
#include "esp_timer.h"

// ログタグ定義
const char* TouchHandler::TAG = "TOUCH_HANDLER";

TouchHandler::TouchHandler()
    : _display(nullptr)
    , _initialized(false)
    , _lastTouchTime(0)
    , _touchDebounceMs(100) // デフォルトのデバウンス時間 100ms
{
    // 初期化
    _lastPoint.x = 0;
    _lastPoint.y = 0;
    _lastPoint.touched = false;
    _lastPoint.timestamp = 0;
    
    // タッチキャリブレーションデータを初期化
    for (int i = 0; i < 8; i++) {
        _touchPoints[i] = 0;
    }
}

TouchHandler::~TouchHandler()
{
    // 特に何もしない
}

bool TouchHandler::init(M5GFX* display)
{
    if (!display) {
        ESP_LOGE(TAG, "Display pointer is null");
        return false;
    }
    
    _display = display;
    ESP_LOGI(TAG, "Touch handler initialized");
    _initialized = true;
    
    return true;
}

bool TouchHandler::update()
{
    if (!_initialized || !_display) {
        ESP_LOGW(TAG, "Touch handler not initialized");
        return false;
    }
    
    // 現在の時間を取得
    uint32_t currentTime = (uint32_t)(esp_timer_get_time() / 1000ULL); // マイクロ秒からミリ秒に変換
    
    // タッチ情報を取得
    lgfx::v1::touch_point_t tp;
    uint_fast8_t count = _display->getTouch(&tp, 1);
    
    // 以前のタッチ状態を保存
    bool wasTouched = _lastPoint.touched;
    
    // タッチがあるかどうか
    if (count > 0) {
        // デバウンス処理（前回のタッチからの経過時間をチェック）
        if (!wasTouched || (currentTime - _lastTouchTime >= _touchDebounceMs)) {
            _lastPoint.x = tp.x;
            _lastPoint.y = tp.y;
            _lastPoint.touched = true;
            _lastPoint.timestamp = currentTime;
            _lastTouchTime = currentTime;
            
            ESP_LOGI(TAG, "Touch detected at (%d, %d)", _lastPoint.x, _lastPoint.y);
            return true;
        }
    } else {
        // タッチがない場合
        if (wasTouched) {
            _lastPoint.touched = false;
            ESP_LOGI(TAG, "Touch released");
        }
    }
    
    return false;
}

void TouchHandler::drawCircleAtTouch(uint16_t radius, uint32_t color)
{
    if (!_initialized || !_display) {
        ESP_LOGW(TAG, "Touch handler not initialized");
        return;
    }
    
    if (!_lastPoint.touched) {
        ESP_LOGD(TAG, "No touch detected, not drawing circle");
        return;
    }
    
    // タッチされた位置に円を描画
    _display->fillCircle(_lastPoint.x, _lastPoint.y, radius, color);
    ESP_LOGI(TAG, "Drew circle at (%d, %d) with radius %d", _lastPoint.x, _lastPoint.y, radius);
}

bool TouchHandler::calibrate(uint16_t bg_color, uint16_t fg_color)
{
    if (!_initialized || !_display) {
        ESP_LOGW(TAG, "Touch handler not initialized");
        return false;
    }
    
    ESP_LOGI(TAG, "Starting touch calibration");
    
    // タッチキャリブレーションを実行
    _display->calibrateTouch(_touchPoints, fg_color, bg_color);
    
    // キャリブレーションが成功したかどうかの確認は難しいが、
    // とりあえずタッチポイントの値がすべて0でないことを確認
    bool success = false;
    for (int i = 0; i < 8; i++) {
        /*
        if (_touchPoints[i] != 0) {
            success = true;
            break;
        }
            */
           _touchPoints[i] = 0;
    }
    /*
    _touchPoints[0] = 0;
    _touchPoints[1] = 500;
    _touchPoints[2] = 0;
    _touchPoints[3] = 0;
    _touchPoints[4] = 500;
    _touchPoints[5] = 500;
    _touchPoints[6] = 500;
    _touchPoints[7] = 0;
    */
    if (success) {
        ESP_LOGI(TAG, "Touch calibration successful");
        _display->setTouchCalibrate(_touchPoints); // キャリブレーション値を設定
    } else {
        ESP_LOGE(TAG, "Touch calibration failed");
    }
    
    return success;
}