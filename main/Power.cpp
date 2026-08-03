// main/Power.cpp - 電源制御と電池残量

#include "Power.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "POWER";

// グローバル実体
Power power;

namespace {

// 電池電圧のばらつきを均すための平均回数。
// ADC は1回読みだと数十mV揺れるので、表示用にはこの程度ならしておく。
constexpr int SAMPLE_COUNT = 16;

// 電圧から残量への換算に使う定数。
//
// M5Unified の Power_Class::getBatteryLevel() をそのまま踏襲している:
//     level = (mv - 3300) * 100 / (4150 - 3350)
// 分子の基準(3300)と分母の下限(3350)が噛み合っていないが、
// 独自に直すと M5 系のツールと表示がずれるため、あえて合わせてある。
constexpr int BATTERY_MV_BASE  = 3300;
constexpr int BATTERY_MV_UPPER = 4150;
constexpr int BATTERY_MV_LOWER = 3350;

}  // namespace

Power::~Power()
{
    if (_cali) {
        adc_cali_delete_scheme_curve_fitting(_cali);
        _cali = nullptr;
    }
    if (_adc) {
        adc_oneshot_del_unit(_adc);
        _adc = nullptr;
    }
}

bool Power::begin()
{
    if (_ready) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }

    // 電源制御線を出力に設定し、LOW で待たせる。
    // M5GFX も起動時に同じ状態にしているが、こちらでも明示しておく
    // （powerOff() のパルス列が LOW から始まる前提のため）。
    gpio_config_t holdConfig = {};
    holdConfig.pin_bit_mask = 1ULL << POWER_HOLD_PIN;
    holdConfig.mode = GPIO_MODE_OUTPUT;
    holdConfig.pull_up_en = GPIO_PULLUP_DISABLE;
    holdConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    holdConfig.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&holdConfig);
    gpio_set_level(POWER_HOLD_PIN, 0);

    // --- 電池監視の ADC ---
    adc_oneshot_unit_init_cfg_t unitConfig = {};
    unitConfig.unit_id = BATTERY_ADC_UNIT;
    unitConfig.ulp_mode = ADC_ULP_MODE_DISABLE;

    esp_err_t err = adc_oneshot_new_unit(&unitConfig, &_adc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit failed: %s", esp_err_to_name(err));
        return false;
    }

    // 電池 4.2V が 1/2 に分圧されて約 2.1V で来る。
    // 12dB 減衰なら約 3.1V まで測れるので余裕がある。
    adc_oneshot_chan_cfg_t chanConfig = {};
    chanConfig.atten = ADC_ATTEN_DB_12;
    chanConfig.bitwidth = ADC_BITWIDTH_DEFAULT;

    err = adc_oneshot_config_channel(_adc, BATTERY_ADC_CHANNEL, &chanConfig);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_config_channel failed: %s", esp_err_to_name(err));
        adc_oneshot_del_unit(_adc);
        _adc = nullptr;
        return false;
    }

    // 較正。ESP32-S3 は curve fitting に対応している。
    // 較正できないと生値からの粗い近似になるので、失敗したことを残しておく。
    adc_cali_curve_fitting_config_t caliConfig = {};
    caliConfig.unit_id = BATTERY_ADC_UNIT;
    caliConfig.chan = BATTERY_ADC_CHANNEL;
    caliConfig.atten = ADC_ATTEN_DB_12;
    caliConfig.bitwidth = ADC_BITWIDTH_DEFAULT;

    err = adc_cali_create_scheme_curve_fitting(&caliConfig, &_cali);
    if (err == ESP_OK) {
        _caliEnabled = true;
    } else {
        ESP_LOGW(TAG, "ADC calibration unavailable (%s). "
                      "Battery voltage will be approximated from raw counts",
                 esp_err_to_name(err));
        _cali = nullptr;
        _caliEnabled = false;
    }

    _ready = true;
    ESP_LOGI(TAG, "Power ready (battery on ADC1 ch%d, calibration %s)",
             static_cast<int>(BATTERY_ADC_CHANNEL),
             _caliEnabled ? "on" : "off");
    return true;
}

int Power::batteryMilliVolts()
{
    if (!_ready || !_adc) {
        return -1;
    }

    int64_t total = 0;
    int samples = 0;

    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        int raw = 0;
        if (adc_oneshot_read(_adc, BATTERY_ADC_CHANNEL, &raw) != ESP_OK) {
            continue;
        }

        int mv = 0;
        if (_caliEnabled) {
            if (adc_cali_raw_to_voltage(_cali, raw, &mv) != ESP_OK) {
                continue;
            }
        } else {
            // 較正が無い場合の粗い近似。
            // 12bit(0..4095) が 0..3100mV に対応するものとして換算する。
            mv = raw * 3100 / 4095;
        }

        total += mv;
        ++samples;
    }

    if (samples == 0) {
        ESP_LOGW(TAG, "Failed to read battery ADC");
        return -1;
    }

    // ピンの電圧を分圧比で戻すと電池電圧になる
    const int pinMv = static_cast<int>(total / samples);
    return static_cast<int>(pinMv * BATTERY_DIVIDER_RATIO);
}

int Power::batteryPercent()
{
    const int mv = batteryMilliVolts();
    if (mv < 0) {
        return -1;
    }

    const int level = (mv - BATTERY_MV_BASE) * 100 /
                      (BATTERY_MV_UPPER - BATTERY_MV_LOWER);

    if (level < 0)   { return 0; }
    if (level >= 100) { return 100; }
    return level;
}

void Power::powerOff()
{
    ESP_LOGI(TAG, "Powering off via GPIO%d", static_cast<int>(POWER_HOLD_PIN));

    // PMS150G へのパルス列。M5Unified の Power_Class::powerOff() と同じ。
    // 最初のパルスで切れるが、確実に落とすため5回繰り返す。
    for (int i = 0; i < 5; ++i) {
        gpio_set_level(POWER_HOLD_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(POWER_HOLD_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // ここへ到達するのは電源が落ちなかった場合。
    // USB 給電中はハードウェア遮断が効かないことがある。
    ESP_LOGW(TAG, "Still running after the power-off pulse. "
                  "Hardware shutdown may not work while USB is connected");

    gpio_set_level(POWER_HOLD_PIN, 0);
}
