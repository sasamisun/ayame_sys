// main/Power.hpp - 電源制御と電池残量
#pragma once

#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"

/**
 * @brief M5PaperS3 の電源と電池を扱う
 *
 * ## 電源 OFF は真のハードウェア遮断
 *
 * GPIO44 は **PMS150G**（電源管理チップ）へ繋がる `power_hold` 線で、
 * 特定のパルス列を送ると主電源が落ちる。
 * ESP32 のディープスリープとは違い、消費電流がマイクロアンペア級まで下がる。
 * 復帰は本体側面の電源ボタン。
 *
 * パルス列は M5Unified の `Power_Class::powerOff()` に合わせてある
 * （LOW 50ms → HIGH 50ms を5回）。最初のパルスで切れるが、確実を期して繰り返す。
 *
 * ## 電池は ADC で測る
 *
 * GPIO3（ADC1 チャンネル2）に 1/2 の分圧で電池電圧が来ている。
 * ピン電圧を2倍したものが電池電圧。
 *
 * これらの値は M5Unified の実装から取ったもので、推測ではない。
 *
 * @note グローバル実体 `power` を1つ用意してある（`SD` や `buzzer` と同じ流儀）。
 */
class Power {
public:
    Power() = default;
    ~Power();

    Power(const Power&) = delete;
    Power& operator=(const Power&) = delete;

    /// 電池監視用の ADC。M5PaperS3 は GPIO3 = ADC1 チャンネル2
    static constexpr adc_unit_t     BATTERY_ADC_UNIT    = ADC_UNIT_1;
    static constexpr adc_channel_t  BATTERY_ADC_CHANNEL = ADC_CHANNEL_2;

    /// 分圧比。ピンの電圧を2倍すると電池電圧になる
    static constexpr float BATTERY_DIVIDER_RATIO = 2.0f;

    /// 電源制御線（PMS150G へ）
    static constexpr gpio_num_t POWER_HOLD_PIN = GPIO_NUM_44;

    /**
     * @brief ADC を初期化する
     * @return 成功したか。失敗しても電源 OFF は使える
     */
    bool begin();

    /**
     * @brief 電源を切る
     *
     * **この関数は戻ってこない**（電源が落ちるため）。
     *
     * @warning 電子ペーパーは電源を切っても最後の像が残り続ける。
     *          呼ぶ前に「電源を切った」と分かる画面を描いておくこと。
     *          そうしないと、次に入れるまで直前の画面が見えたままになる。
     * @warning USB 給電中はハードウェア遮断が効かないことがある。
     *          その場合は戻ってくるので、呼び出し側で案内を出すとよい。
     */
    void powerOff();

    /**
     * @brief 電池電圧
     * @return ミリボルト。取得できなければ -1
     */
    int batteryMilliVolts();

    /**
     * @brief 電池残量
     * @return 0〜100 の百分率。取得できなければ -1
     *
     * @note 換算式は M5Unified と同じにしてある。
     *       独自に直すと M5 系のツールと表示がずれるため。
     */
    int batteryPercent();

    bool isReady() const { return _ready; }

private:
    bool _ready = false;
    adc_oneshot_unit_handle_t _adc = nullptr;
    adc_cali_handle_t _cali = nullptr;
    bool _caliEnabled = false;
};

/// グローバル実体
extern Power power;
