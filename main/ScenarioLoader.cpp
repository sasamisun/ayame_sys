// main/ScenarioLoader.cpp - シナリオJSONの読み込み・検証

#include "ScenarioLoader.hpp"

#include "SDcard.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include <cstdlib>
#include <cstring>

static const char* TAG = "SCENARIO";

namespace {

// cJSON に渡す確保関数。PSRAM を明示指定する。
void* psramMalloc(size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
}

void psramFree(void* ptr)
{
    heap_caps_free(ptr);
}

// オブジェクトから文字列を引く。無ければ既定値
std::string getString(const cJSON* obj, const char* key, const char* fallback = "")
{
    if (!obj) {
        return fallback;
    }
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsString(item) || !item->valuestring) {
        return fallback;
    }
    return item->valuestring;
}

}  // namespace

ScenarioLoader::~ScenarioLoader()
{
    unload();
}

std::string ScenarioLoader::peekTitle(const char* scenarioId)
{
    if (!scenarioId || scenarioId[0] == '\0') {
        return "";
    }

    const std::string fallback = scenarioId;
    const std::string path = std::string(SCENARIOS_ROOT) + "/" + scenarioId +
                             "/scenario.json";

    // 先頭だけ読む。PSRAM を明示するのは、この大きさ（4KB）だと
    // 既定の malloc では内部RAMに載ってしまうため。
    char* buffer = static_cast<char*>(
        heap_caps_malloc(TITLE_PEEK_BYTES, MALLOC_CAP_SPIRAM));
    if (!buffer) {
        return fallback;
    }

    const size_t got = SD.readFilePrefix(path.c_str(), buffer, TITLE_PEEK_BYTES);
    if (got == 0) {
        free(buffer);
        return fallback;
    }

    // "title" を探して、その後ろの文字列値を取り出す。
    //
    // 読んだのは途中で切れた断片なので cJSON では解析できない。
    // ここでは素朴に走査する。誤検出を避けるため、
    // キーとしての "title" （後ろにコロンが続くもの）だけを拾う。
    std::string title;
    const char* p = buffer;
    const char* end = buffer + got;

    while ((p = strstr(p, "\"title\"")) != nullptr) {
        p += 7;   // "title" の直後

        // コロンまでの空白を飛ばす
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) { ++p; }
        if (p >= end || *p != ':') {
            continue;   // キーではなかった。次の候補へ
        }
        ++p;
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) { ++p; }
        if (p >= end || *p != '"') {
            continue;   // 文字列値ではない
        }
        ++p;

        // 閉じ引用符まで。バックスラッシュで逃がされたものは終端としない。
        while (p < end && *p != '"') {
            if (*p == '\\' && (p + 1) < end) {
                // \" や \\ をそのまま通す。日本語（UTF-8）は素通しでよい
                title += *p;
                ++p;
            }
            title += *p;
            ++p;
        }

        if (p < end) {
            break;   // 閉じ引用符まで届いた。取れた
        }

        // 4KB の途中で切れていた。中途半端な値は使わない
        title.clear();
        break;
    }

    free(buffer);

    if (title.empty()) {
        ESP_LOGD(TAG, "No title found in the first %u bytes of %s. Using the folder name",
                 static_cast<unsigned>(TITLE_PEEK_BYTES), scenarioId);
        return fallback;
    }
    return title;
}

void ScenarioLoader::initAllocator()
{
    cJSON_Hooks hooks;
    hooks.malloc_fn = psramMalloc;
    hooks.free_fn   = psramFree;
    cJSON_InitHooks(&hooks);

    ESP_LOGI(TAG, "cJSON allocator directed to PSRAM");
}

void ScenarioLoader::unload()
{
    if (_root) {
        cJSON_Delete(_root);
        _root = nullptr;
    }
    _scenes = nullptr;
    _scenarioId.clear();
    _basePath.clear();
    _issueCount = 0;
}

bool ScenarioLoader::load(const char* scenarioId)
{
    if (!scenarioId || scenarioId[0] == '\0') {
        ESP_LOGE(TAG, "Scenario id is empty");
        return false;
    }

    unload();

    _scenarioId = scenarioId;
    _basePath   = std::string(SCENARIOS_ROOT) + "/" + scenarioId;

    const std::string jsonPath = _basePath + "/scenario.json";

    // パス長の上限は buildFullPath() の char[256]。
    // ここで弾いておかないと、後段で切り詰められて別のファイルを開きかねない。
    if (jsonPath.size() >= 200) {
        ESP_LOGE(TAG, "Path too long (%u chars): %s",
                 static_cast<unsigned>(jsonPath.size()), jsonPath.c_str());
        unload();
        return false;
    }

    // 巨大なシナリオへの備え。
    //
    // 解析中は「原文バッファ」と「cJSON ツリー」が同時に生きるので、
    // ピークはファイルサイズの約 2.6 倍になる（SCENARIO_SPEC.md の 9.7 参照）。
    // 足りなければ readFileToBuffer() か cJSON_Parse() が失敗して
    // nullptr が返るだけだが、理由が分かりにくいので先に警告を出しておく。
    //
    // 超えていても**読み込みは試す**。ここで止めると、
    // 実際には入るのに弾いてしまう可能性があるため。
    const size_t fileSize = SD.size(jsonPath.c_str());
    const size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const size_t estimatedPeak = fileSize * 26 / 10;
    if (fileSize > 0 && estimatedPeak > freePsram) {
        ESP_LOGW(TAG, "Scenario may not fit: file %u bytes needs about %u bytes "
                      "at peak, but only %u bytes of PSRAM are free. Trying anyway",
                 static_cast<unsigned>(fileSize),
                 static_cast<unsigned>(estimatedPeak),
                 static_cast<unsigned>(freePsram));
    }

    const int64_t internalBefore =
        static_cast<int64_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    const int64_t psramBefore = static_cast<int64_t>(freePsram);

    size_t textLen = 0;
    char* text = SD.readFileToBuffer(jsonPath.c_str(), &textLen);
    if (!text) {
        ESP_LOGE(TAG, "Failed to read %s", jsonPath.c_str());
        unload();
        return false;
    }

    _root = cJSON_ParseWithLength(text, textLen);

    // cJSON は文字列を自前のツリーへ複製するので、原文はもう要らない。
    // 先に解放しておけばピーク使用量が半分で済む。
    const char* parseError = _root ? nullptr : cJSON_GetErrorPtr();
    const size_t errorOffset =
        (parseError && parseError >= text) ? static_cast<size_t>(parseError - text) : 0;
    free(text);
    text = nullptr;

    if (!_root) {
        ESP_LOGE(TAG, "JSON parse failed at byte %u of %s",
                 static_cast<unsigned>(errorOffset), jsonPath.c_str());
        unload();
        return false;
    }

    const int64_t internalAfter =
        static_cast<int64_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    const int64_t psramAfter =
        static_cast<int64_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    // ここでの psram 差分は「ツリーだけ」の実測値になる。
    // 原文バッファは既に解放済みだから。
    //
    // 見ておきたいのは2点:
    //   ・internal delta が小さいこと
    //       大きければ initAllocator() の呼び忘れ。PSRAM フックが効いていない
    //   ・tree / source の倍率
    //       SCENARIO_SPEC.md 9.7 の見積もり式が実機と合っているかの検証になる
    const int64_t treeBytes = psramBefore - psramAfter;
    ESP_LOGI(TAG, "Parsed %u bytes. Tree: %ld bytes in PSRAM (x%.2f of source), "
                  "internal delta: %ld bytes",
             static_cast<unsigned>(textLen),
             static_cast<long>(treeBytes),
             textLen ? static_cast<double>(treeBytes) / static_cast<double>(textLen) : 0.0,
             static_cast<long>(internalBefore - internalAfter));

    const cJSON* formatVersion =
        cJSON_GetObjectItemCaseSensitive(_root, "format_version");
    if (!cJSON_IsNumber(formatVersion)) {
        ESP_LOGW(TAG, "format_version is missing. Assuming %d", SUPPORTED_FORMAT_VERSION);
    } else if (formatVersion->valueint > SUPPORTED_FORMAT_VERSION) {
        // 前方互換の方針: 知らないバージョンでも読んでみる。
        // 未知のキーやコマンドは実行側が無視する。
        ESP_LOGW(TAG, "format_version %d is newer than supported %d. Trying anyway",
                 formatVersion->valueint, SUPPORTED_FORMAT_VERSION);
    }

    _scenes = cJSON_GetObjectItemCaseSensitive(_root, "scenes");
    if (!cJSON_IsObject(_scenes)) {
        ESP_LOGE(TAG, "'scenes' is missing or not an object");
        unload();
        return false;
    }

    _issueCount = validate();

    ESP_LOGI(TAG, "Loaded '%s' (title=%s, version=%s, issues=%d)",
             _scenarioId.c_str(), title().c_str(), version().c_str(), _issueCount);

    return true;
}

std::string ScenarioLoader::title() const
{
    const cJSON* meta = cJSON_GetObjectItemCaseSensitive(_root, "meta");
    const std::string t = getString(meta, "title");
    return t.empty() ? _scenarioId : t;
}

std::string ScenarioLoader::version() const
{
    return getString(cJSON_GetObjectItemCaseSensitive(_root, "meta"), "version");
}

std::string ScenarioLoader::startSceneId() const
{
    return getString(_root, "start");
}

std::string ScenarioLoader::defaultTextDirection() const
{
    return getString(cJSON_GetObjectItemCaseSensitive(_root, "meta"),
                     "text_direction", "VERTICAL");
}

const cJSON* ScenarioLoader::variablesNode() const
{
    return cJSON_GetObjectItemCaseSensitive(_root, "variables");
}

const cJSON* ScenarioLoader::findScene(const std::string& sceneId) const
{
    if (!_scenes || sceneId.empty()) {
        return nullptr;
    }
    return cJSON_GetObjectItemCaseSensitive(_scenes, sceneId.c_str());
}

bool ScenarioLoader::sceneExists(const char* sceneId) const
{
    if (!_scenes || !sceneId) {
        return false;
    }
    return cJSON_GetObjectItemCaseSensitive(_scenes, sceneId) != nullptr;
}

bool ScenarioLoader::resolveBackgroundPath(const char* logicalName,
                                           std::string& outPath) const
{
    outPath.clear();
    if (!logicalName) {
        return false;
    }

    const cJSON* assets = cJSON_GetObjectItemCaseSensitive(_root, "assets");
    const cJSON* backgrounds =
        cJSON_GetObjectItemCaseSensitive(assets, "backgrounds");
    const cJSON* entry =
        cJSON_GetObjectItemCaseSensitive(backgrounds, logicalName);

    if (!cJSON_IsString(entry) || !entry->valuestring) {
        return false;
    }

    outPath = _basePath + "/" + entry->valuestring;

    // buildFullPath() が char[256] なので、余裕を見て弾く
    if (outPath.size() >= 200) {
        ESP_LOGW(TAG, "Background path too long (%u chars): %s",
                 static_cast<unsigned>(outPath.size()), outPath.c_str());
        outPath.clear();
        return false;
    }

    return true;
}

void ScenarioLoader::validateCommands(const cJSON* commands,
                                      const char* sceneId, int& issues)
{
    if (!cJSON_IsArray(commands)) {
        return;
    }

    const cJSON* cmd = nullptr;
    cJSON_ArrayForEach(cmd, commands) {
        const std::string type = getString(cmd, "type");

        if (type == "jump") {
            const std::string next = getString(cmd, "next");
            if (next.empty() || !sceneExists(next.c_str())) {
                ESP_LOGW(TAG, "  scene '%s': jump to undefined scene '%s'",
                         sceneId, next.c_str());
                ++issues;
            }
        } else if (type == "bg") {
            const std::string image = getString(cmd, "image");
            std::string path;
            if (!resolveBackgroundPath(image.c_str(), path)) {
                ESP_LOGW(TAG, "  scene '%s': background '%s' is not in assets",
                         sceneId, image.c_str());
                ++issues;
            } else if (!SD.exists(path.c_str())) {
                ESP_LOGW(TAG, "  scene '%s': background file not found: %s",
                         sceneId, path.c_str());
                ++issues;
            }
        } else if (type == "choice") {
            const cJSON* options = cJSON_GetObjectItemCaseSensitive(cmd, "options");
            const cJSON* opt = nullptr;
            cJSON_ArrayForEach(opt, options) {
                const std::string next = getString(opt, "next");
                if (next.empty() || !sceneExists(next.c_str())) {
                    ESP_LOGW(TAG, "  scene '%s': choice leads to undefined scene '%s'",
                             sceneId, next.c_str());
                    ++issues;
                }
            }
        } else if (type == "if") {
            // then / else の中も同じ規則で確かめる
            validateCommands(cJSON_GetObjectItemCaseSensitive(cmd, "then"),
                             sceneId, issues);
            validateCommands(cJSON_GetObjectItemCaseSensitive(cmd, "else"),
                             sceneId, issues);
        }
    }
}

int ScenarioLoader::validate()
{
    int issues = 0;

    const std::string start = startSceneId();
    if (start.empty()) {
        ESP_LOGW(TAG, "  'start' is missing");
        ++issues;
    } else if (!sceneExists(start.c_str())) {
        ESP_LOGW(TAG, "  'start' points to undefined scene '%s'", start.c_str());
        ++issues;
    }

    const cJSON* scene = nullptr;
    cJSON_ArrayForEach(scene, _scenes) {
        const char* sceneId = scene->string ? scene->string : "(unnamed)";

        const cJSON* commands = cJSON_GetObjectItemCaseSensitive(scene, "commands");
        if (!cJSON_IsArray(commands)) {
            ESP_LOGW(TAG, "  scene '%s': 'commands' is missing or not an array", sceneId);
            ++issues;
            continue;
        }

        validateCommands(commands, sceneId, issues);

        // 出口があるか。next も終端コマンドも無いと、そこで話が止まる。
        const std::string next = getString(scene, "next");
        if (!next.empty() && !sceneExists(next.c_str())) {
            ESP_LOGW(TAG, "  scene '%s': next points to undefined scene '%s'",
                     sceneId, next.c_str());
            ++issues;
        }
    }

    if (issues > 0) {
        ESP_LOGW(TAG, "Validation found %d issue(s). Loading anyway", issues);
    } else {
        ESP_LOGI(TAG, "Validation passed");
    }

    return issues;
}
