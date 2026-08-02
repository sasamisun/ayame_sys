// main/Buzzer.hpp - ブザー出力（LEDC の PWM で矩形波を鳴らす）
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/**
 * @brief 1音ぶんの指定
 */
struct Note {
    uint32_t freq;      //!< 周波数[Hz]。0 で休符（無音のまま時間だけ経過する）
    uint32_t duration;  //!< 長さ[ms]
};

/**
 * @brief M5PaperS3 のブザーを鳴らす
 *
 * GPIO21 に繋がったブザーを LEDC（PWM）で駆動する。
 * 出力は矩形波で、**周波数で音程を決める**。
 *
 * ## 音量は変えられない
 *
 * 圧電ブザーを PWM で叩いているだけなので、音量調整の手段が無い。
 * duty を 50% から動かすと音量ではなく音色（倍音の乗り方）が変わるため、
 * duty は 50% 固定にしてある。
 *
 * ## 再生は非同期
 *
 * `tone()` と `playMelody()` は**鳴らし始めたらすぐ返る**。
 * シナリオの `beep` コマンドが `wait: false` を既定にしているため、
 * 音が鳴っている間も次のコマンドへ進めるようにしてある。
 * 鳴り終わりは `esp_timer` が検出して自動的に止める。
 *
 * 鳴り終わるまで待ちたい場合は `playMelody(..., blocking = true)` を使うか、
 * `isPlaying()` を見る。
 *
 * ```cpp
 * buzzer.begin();
 * buzzer.tone(880, 120);            // ラの音を0.12秒
 *
 * static const Note fanfare[] = {
 *     { 523, 150 }, { 659, 150 }, { 784, 300 }, { 0, 100 },
 * };
 * buzzer.playMelody(fanfare, 4);    // 鳴らしっぱなしで即座に返る
 * ```
 *
 * @note グローバル実体 `buzzer` を1つ用意してある（`SD` と同じ流儀）。
 *       LEDC のタイマとチャンネルを占有するため、複数生成は想定していない。
 */
class Buzzer {
public:
    Buzzer() = default;
    ~Buzzer();

    Buzzer(const Buzzer&) = delete;
    Buzzer& operator=(const Buzzer&) = delete;

    /// メロディで保持できる最大音数。これを超えた分は切り捨てる
    static constexpr size_t MAX_NOTES = 64;

    /// 鳴らせる周波数の範囲。範囲外はこの値に丸める
    static constexpr uint32_t MIN_FREQ = 30;
    static constexpr uint32_t MAX_FREQ = 10000;

    /**
     * @brief LEDC を初期化する
     * @param pin ブザーの GPIO。M5PaperS3 は GPIO21
     * @return 成功したか。失敗時は以降の呼び出しが無視される
     */
    bool begin(gpio_num_t pin = GPIO_NUM_21);

    /// LEDC を解放する
    void end();

    /**
     * @brief 単音を鳴らす（非同期。すぐ返る）
     * @param freq     周波数[Hz]。0 なら鳴らさず無音時間だけ取る
     * @param durationMs 長さ[ms]。0 なら stop() されるまで鳴り続ける
     */
    void tone(uint32_t freq, uint32_t durationMs);

    /**
     * @brief 音列を鳴らす
     * @param notes    音の配列
     * @param count    音数（MAX_NOTES まで）
     * @param blocking true なら鳴り終わるまで返らない
     *
     * @note 非同期時は内部に**コピー**を持つので、呼び出し側の配列が
     *       すぐ消えても構わない。
     */
    void playMelody(const Note* notes, size_t count, bool blocking = false);

    /// 鳴っていれば止める
    void stop();

    /// 再生中か
    bool isPlaying() const { return _playing; }

    /// begin() 済みか
    bool isReady() const { return _ready; }

private:
    // esp_timer から呼ばれる。次の音へ進める
    static void onTimerFired(void* arg);
    void advance();

    // 出力を freq に切り替える（0 なら無音）。ロックは呼び出し側が持つこと
    void setOutput(uint32_t freq);

    // 現在の音を鳴らし、その長さぶんのタイマを仕掛ける
    void startCurrentNote();

    void lock();
    void unlock();

    static constexpr ledc_mode_t      LEDC_MODE    = LEDC_LOW_SPEED_MODE;
    static constexpr ledc_timer_t     LEDC_TIMER   = LEDC_TIMER_0;
    static constexpr ledc_channel_t   LEDC_CHANNEL = LEDC_CHANNEL_0;
    // 10bit あれば可聴域は十分に出せる（80MHz / 2^10 ≒ 78kHz まで）。
    // 分解能を上げすぎると高い周波数が出せなくなる。
    static constexpr ledc_timer_bit_t LEDC_RES     = LEDC_TIMER_10_BIT;
    static constexpr uint32_t         DUTY_50      = (1u << 10) / 2;

    bool _ready = false;
    volatile bool _playing = false;
    gpio_num_t _pin = GPIO_NUM_NC;

    esp_timer_handle_t _timer = nullptr;
    SemaphoreHandle_t _mutex = nullptr;

    std::vector<Note> _notes;
    size_t _index = 0;
};

/// グローバル実体
extern Buzzer buzzer;
