// main/Settings.hpp - 本体設定の読み書き（system/settings.json）
#pragma once

/**
 * @brief シナリオに属さない本体設定
 *
 * SD の `system/settings.json` に置く。仕様は `SCENARIO_SPEC.md` の 7 章。
 *
 * ## 保存は明示的に
 *
 * 値を変えただけでは書かれない。`save()` を呼んだときだけ SD へ書く。
 * 設定を変えるたびに書くと SD への書き込みが増え、
 * 電源断のタイミングで壊れる機会も増えるため。
 *
 * ## 読めなくても動く
 *
 * SD が無い、ファイルが無い、JSON が壊れている、USB MSC 中——
 * どの場合も既定値で動き続ける。設定は本質的に「あれば嬉しい」ものなので、
 * 起動を止める理由にはしない。
 *
 * @note グローバル実体 `settings` を1つ用意してある。
 */
class Settings {
public:
    Settings() = default;

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    /// 設定ファイルの置き場（`/sdcard` 起点）
    static constexpr const char* SYSTEM_DIR = "system";
    static constexpr const char* SETTINGS_PATH = "system/settings.json";

    static constexpr int FORMAT_VERSION = 1;

    /**
     * @brief SD から読み込む
     * @return 読めたか。読めなくても既定値で使える
     */
    bool load();

    /**
     * @brief SD へ書き出す
     *
     * `system/` が無ければ作る。
     *
     * @return 書けたか。USB MSC 中は必ず false
     */
    bool save();

    // ---- 設定項目 ----

    /// ブザーを鳴らすか
    bool soundEnabled() const { return _soundEnabled; }
    void setSoundEnabled(bool enabled) { _soundEnabled = enabled; }

    /// 最後に遊んだシナリオのID（空なら無し）
    const char* lastScenario() const { return _lastScenario; }
    void setLastScenario(const char* id);

    /**
     * @brief 「続きから」で読むセーブスロット
     *
     * `suspend` で中断したときに書かれる。`-1` なら続きは無い。
     * 一度再開したら消す（同じ栞を何度も使えると、
     * いつの状態から始まるのか分からなくなるため）。
     */
    int resumeSlot() const { return _resumeSlot; }
    void setResumeSlot(int slot) { _resumeSlot = slot; }
    bool hasResume() const { return _resumeSlot >= 0 && _lastScenario[0] != '\0'; }
    void clearResume() { _resumeSlot = -1; }

private:
    bool _soundEnabled = true;
    char _lastScenario[64] = {0};
    int _resumeSlot = -1;
};

/// グローバル実体
extern Settings settings;
