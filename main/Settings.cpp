// main/Settings.cpp - 本体設定の読み書き（system/settings.json）

#include "Settings.hpp"

#include "SDcard.hpp"
#include "cJSON.h"
#include "esp_log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static const char* TAG = "SETTINGS";

// グローバル実体
Settings settings;

void Settings::setLastScenario(const char* id)
{
    if (!id) {
        _lastScenario[0] = '\0';
        return;
    }
    snprintf(_lastScenario, sizeof(_lastScenario), "%s", id);
}

bool Settings::load()
{
    size_t len = 0;
    char* text = SD.readFileToBuffer(SETTINGS_PATH, &len);
    if (!text) {
        // 初回起動やSD無しはここに来る。既定値のまま進む。
        ESP_LOGI(TAG, "No settings file. Using defaults");
        return false;
    }

    cJSON* root = cJSON_ParseWithLength(text, len);
    free(text);

    if (!root) {
        ESP_LOGW(TAG, "settings.json is malformed. Using defaults");
        return false;
    }

    const cJSON* sound = cJSON_GetObjectItemCaseSensitive(root, "sound_enabled");
    if (cJSON_IsBool(sound)) {
        _soundEnabled = cJSON_IsTrue(sound);
    }

    const cJSON* last = cJSON_GetObjectItemCaseSensitive(root, "last_scenario");
    if (cJSON_IsString(last) && last->valuestring) {
        setLastScenario(last->valuestring);
    }

    cJSON_Delete(root);

    ESP_LOGI(TAG, "Loaded (sound=%s, last='%s')",
             _soundEnabled ? "on" : "off", _lastScenario);
    return true;
}

bool Settings::save()
{
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return false;
    }

    cJSON_AddNumberToObject(root, "format_version", FORMAT_VERSION);
    cJSON_AddBoolToObject(root, "sound_enabled", _soundEnabled);
    cJSON_AddStringToObject(root, "last_scenario", _lastScenario);

    // 人が読んで直せるよう整形して書く。
    // 設定ファイルは USB MSC 経由で PC から覗かれる前提。
    char* text = cJSON_Print(root);
    cJSON_Delete(root);

    if (!text) {
        ESP_LOGE(TAG, "Failed to serialize settings");
        return false;
    }

    // 置き場が無ければ作る。既にあれば mkdir は失敗するが、それでよい。
    SD.mkdir(SYSTEM_DIR);

    const bool ok = SD.writeFileFromBuffer(SETTINGS_PATH, text, strlen(text));
    free(text);

    if (ok) {
        ESP_LOGI(TAG, "Saved (sound=%s)", _soundEnabled ? "on" : "off");
    } else {
        ESP_LOGW(TAG, "Failed to save settings");
    }
    return ok;
}
