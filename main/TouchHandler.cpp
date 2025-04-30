// main/TouchHandler.cpp
#include "TouchHandler.hpp"
#include "esp_log.h"
#include "esp_timer.h"

// ログタグ
static const char* TAG = "TOUCH";

// 現在の時間をミリ秒で取得するヘルパー関数
static uint32_t millis() {
    return esp_timer_get_time() / 1000;
}

TouchHandler::TouchHandler()
    : _display(nullptr), _touched(false), _wasTouched(false), _calibrated(false),
      _lastEvent(TouchEvent::None), _lastSwipe(SwipeDirection::None),
      _minSwipeDistance(30) {
    
    // rawPointの初期化
    _rawPoint.x = 0;
    _rawPoint.y = 0;
    _rawPoint.id = 0;
    _rawPoint.size = 0;
    
    // 拡張タッチポイント初期化
    _lastPoint = {0, 0, 0};
    _touchStartPoint = {0, 0, 0};
    _touchEndPoint = {0, 0, 0};
    
    // キャリブレーションデータ初期化
    for (int i = 0; i < 8; i++) {
        _touchCalibration[i] = 0;
    }
}

bool TouchHandler::init(M5GFX* display) {
    _display = display;
    
    if (!_display) {
        ESP_LOGE(TAG, "Display not initialized");
        return false;
    }
    
    // タッチパネルの初期化
    bool touchAvailable = _display->touch() != nullptr;
    
    if (!touchAvailable) {
        ESP_LOGE(TAG, "Touch panel not available");
        return false;
    }
    
    ESP_LOGI(TAG, "Touch handler initialized successfully");
    return true;
}

bool TouchHandler::update() {
    if (!_display) return false;
    
    // 前回の状態を保存
    _wasTouched = _touched;
    
    // タッチポイントを取得
    _touched = _display->getTouch(&_rawPoint) > 0;
    
    // イベントの初期化
    _lastEvent = TouchEvent::None;
    
    if (_touched) {
        // タッチ位置の更新
        _lastPoint.x = _rawPoint.x;
        _lastPoint.y = _rawPoint.y;
        _lastPoint.timestamp = millis();
        
        // タッチ開始検出
        if (!_wasTouched) {
            _touchStartPoint = _lastPoint;
            _lastEvent = TouchEvent::Touch;
            
            // タッチ開始コールバック実行
            if (_onTouchStart) {
                _onTouchStart(_touchStartPoint);
            }
            
            ESP_LOGI(TAG, "Touch start at (%d, %d)", _touchStartPoint.x, _touchStartPoint.y);
        }
    } else if (_wasTouched) {
        // タッチ終了を検出
        _touchEndPoint = _lastPoint;
        _lastEvent = TouchEvent::Release;
        
        // スワイプ検出
        _lastSwipe = detectSwipe(_touchStartPoint, _touchEndPoint);
        
        if (_lastSwipe != SwipeDirection::None) {
            _lastEvent = TouchEvent::Swipe;
            
            // スワイプコールバック実行
            if (_onSwipe) {
                _onSwipe(_lastSwipe, _touchStartPoint, _touchEndPoint);
            }
            
            ESP_LOGI(TAG, "Swipe detected: %d", static_cast<int>(_lastSwipe));
        }
        
        // タッチ終了コールバック実行
        if (_onTouchEnd) {
            _onTouchEnd(_touchEndPoint);
        }
        
        ESP_LOGI(TAG, "Touch end at (%d, %d)", _touchEndPoint.x, _touchEndPoint.y);
    }
    
    return _lastEvent != TouchEvent::None;
}

void TouchHandler::calibrate(uint32_t bg_color, uint32_t fg_color) {
    if (!_display) return;
    
    ESP_LOGI(TAG, "Starting touch calibration");
    
    _display->fillScreen(bg_color);
    _display->calibrateTouch(_touchCalibration, fg_color, bg_color);
    _calibrated = true;
    
    ESP_LOGI(TAG, "Touch calibration completed");
    
    // キャリブレーションデータをログに出力（保存用）
    ESP_LOGI(TAG, "Calibration data:");
    for (int i = 0; i < 8; i++) {
        ESP_LOGI(TAG, "  data[%d] = 0x%04x", i, _touchCalibration[i]);
    }
}

void TouchHandler::drawCircleAtTouch(int radius, uint32_t color) {
    if (!_display || !_touched) return;
    
    _display->fillCircle(_lastPoint.x, _lastPoint.y, radius, color);
}

SwipeDirection TouchHandler::detectSwipe(const ExtendedTouchPoint& start, const ExtendedTouchPoint& end) {
    // 水平方向と垂直方向の移動距離を計算
    int dx = end.x - start.x;
    int dy = end.y - start.y;
    
    // 絶対値を計算
    int absDx = abs(dx);
    int absDy = abs(dy);
    
    // 最小スワイプ距離よりも短い場合はスワイプではない
    if (absDx < _minSwipeDistance && absDy < _minSwipeDistance) {
        return SwipeDirection::None;
    }
    
    // 水平方向のスワイプが垂直方向より大きい場合
    if (absDx > absDy) {
        return dx > 0 ? SwipeDirection::Right : SwipeDirection::Left;
    }
    // 垂直方向のスワイプ
    else {
        return dy > 0 ? SwipeDirection::Down : SwipeDirection::Up;
    }
}