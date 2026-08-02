// main/Buzzer.cpp - ブザー出力（LEDC の PWM で矩形波を鳴らす）

#include "Buzzer.hpp"

#include "esp_log.h"
#include "freertos/task.h"

#include <algorithm>

static const char* TAG = "BUZZER";

// グローバル実体
Buzzer buzzer;

Buzzer::~Buzzer()
{
    end();
}

bool Buzzer::begin(gpio_num_t pin)
{
    if (_ready) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }

    _pin = pin;

    _mutex = xSemaphoreCreateMutex();
    if (!_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return false;
    }

    ledc_timer_config_t timerConfig = {};
    timerConfig.speed_mode      = LEDC_MODE;
    timerConfig.duty_resolution = LEDC_RES;
    timerConfig.timer_num       = LEDC_TIMER;
    timerConfig.freq_hz         = 1000;   // 仮。鳴らすたびに設定し直す
    timerConfig.clk_cfg         = LEDC_AUTO_CLK;

    esp_err_t err = ledc_timer_config(&timerConfig);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config failed: %s", esp_err_to_name(err));
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
        return false;
    }

    ledc_channel_config_t channelConfig = {};
    channelConfig.gpio_num   = _pin;
    channelConfig.speed_mode = LEDC_MODE;
    channelConfig.channel    = LEDC_CHANNEL;
    channelConfig.intr_type  = LEDC_INTR_DISABLE;
    channelConfig.timer_sel  = LEDC_TIMER;
    channelConfig.duty       = 0;         // 無音から始める
    channelConfig.hpoint     = 0;

    err = ledc_channel_config(&channelConfig);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config failed: %s", esp_err_to_name(err));
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
        return false;
    }

    // 音の切り替えに使うワンショットタイマ。
    // 既定の dispatch method はタイマタスク上で呼ばれるので、
    // コールバックから LEDC の API を叩いてよい（ISR ではない）。
    esp_timer_create_args_t timerArgs = {};
    timerArgs.callback = &Buzzer::onTimerFired;
    timerArgs.arg      = this;
    timerArgs.name     = "buzzer";

    err = esp_timer_create(&timerArgs, &_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_create failed: %s", esp_err_to_name(err));
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
        return false;
    }

    _notes.reserve(MAX_NOTES);
    _ready = true;

    ESP_LOGI(TAG, "Buzzer initialized on GPIO%d", static_cast<int>(_pin));
    return true;
}

void Buzzer::end()
{
    if (!_ready) {
        return;
    }

    stop();

    if (_timer) {
        esp_timer_delete(_timer);
        _timer = nullptr;
    }

    ledc_stop(LEDC_MODE, LEDC_CHANNEL, 0);

    if (_mutex) {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }

    _ready = false;
    ESP_LOGI(TAG, "Buzzer released");
}

void Buzzer::lock()
{
    if (_mutex) {
        xSemaphoreTake(_mutex, portMAX_DELAY);
    }
}

void Buzzer::unlock()
{
    if (_mutex) {
        xSemaphoreGive(_mutex);
    }
}

void Buzzer::setOutput(uint32_t freq)
{
    if (freq == 0) {
        // 休符。周波数はそのままに duty を落として黙らせる。
        // ledc_set_freq(0) は不正なので周波数側は触らない。
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        return;
    }

    const uint32_t clamped = std::min(std::max(freq, MIN_FREQ), MAX_FREQ);
    if (clamped != freq) {
        ESP_LOGW(TAG, "Frequency %u Hz out of range, clamped to %u Hz",
                 static_cast<unsigned>(freq), static_cast<unsigned>(clamped));
    }

    const esp_err_t err = ledc_set_freq(LEDC_MODE, LEDC_TIMER, clamped);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_set_freq(%u) failed: %s",
                 static_cast<unsigned>(clamped), esp_err_to_name(err));
        return;
    }

    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, DUTY_50);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

    // 要求した周波数と、LEDC が実際に設定できた周波数・duty。
    // 食い違えばソフト側（分解能やクロック源の選択）、
    // 一致しているのに鳴らなければハード側（ピンや結線）と切り分けられる。
    ESP_LOGD(TAG, "setOutput: req=%uHz actual=%luHz duty=%lu/%u pin=GPIO%d",
             static_cast<unsigned>(clamped),
             static_cast<unsigned long>(ledc_get_freq(LEDC_MODE, LEDC_TIMER)),
             static_cast<unsigned long>(ledc_get_duty(LEDC_MODE, LEDC_CHANNEL)),
             static_cast<unsigned>(1u << 10),
             static_cast<int>(_pin));
}

void Buzzer::startCurrentNote()
{
    const Note& note = _notes[_index];
    setOutput(note.freq);

    if (note.duration == 0) {
        // 長さ指定なし。stop() されるまで鳴らし続ける
        return;
    }

    esp_timer_start_once(_timer, static_cast<uint64_t>(note.duration) * 1000ULL);
}

void Buzzer::onTimerFired(void* arg)
{
    static_cast<Buzzer*>(arg)->advance();
}

void Buzzer::advance()
{
    lock();

    ++_index;
    if (_index >= _notes.size()) {
        // 最後まで鳴らし終えた
        setOutput(0);
        _playing = false;
        unlock();
        return;
    }

    startCurrentNote();
    unlock();
}

void Buzzer::tone(uint32_t freq, uint32_t durationMs)
{
    const Note single = { freq, durationMs };
    playMelody(&single, 1, false);
}

void Buzzer::playMelody(const Note* notes, size_t count, bool blocking)
{
    if (!_ready) {
        ESP_LOGW(TAG, "Not initialized. Call begin() first");
        return;
    }
    if (!notes || count == 0) {
        return;
    }

    if (count > MAX_NOTES) {
        ESP_LOGW(TAG, "Melody has %u notes, truncated to %u",
                 static_cast<unsigned>(count), static_cast<unsigned>(MAX_NOTES));
        count = MAX_NOTES;
    }

    // 前の再生を止めてから差し替える。
    // stop() 側でタイマを止めるので、advance() と競合しない。
    stop();

    lock();
    // 非同期再生では呼び出し側の配列がすぐ消えても構わないよう、内部にコピーを持つ
    _notes.assign(notes, notes + count);
    _index = 0;
    _playing = true;
    startCurrentNote();
    unlock();

    if (!blocking) {
        return;
    }

    // 鳴り終わるまで待つ。
    // タイマタスクが _playing を下ろすのを見張るだけなので、
    // ここで LEDC を触ってはいけない。
    while (_playing) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void Buzzer::stop()
{
    if (!_ready) {
        return;
    }

    // タイマはロックの外で止める。
    // esp_timer_stop() はコールバックの完了を待つため、
    // ロックを持ったまま呼ぶと advance() と相互待ちになる。
    if (_timer) {
        esp_timer_stop(_timer);
    }

    lock();
    setOutput(0);
    _playing = false;
    _notes.clear();
    _index = 0;
    unlock();
}
