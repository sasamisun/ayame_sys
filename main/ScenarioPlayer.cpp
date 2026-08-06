// main/ScenarioPlayer.cpp - シナリオのシーンとコマンドを実行する

#include "ScenarioPlayer.hpp"

#include "Buzzer.hpp"
#include "Power.hpp"
#include "SDcard.hpp"
#include "Settings.hpp"
#include "TextSystem.hpp"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <vector>

static const char* TAG = "PLAYER";

namespace {

std::string getString(const cJSON* obj, const char* key, const char* fallback = "")
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsString(item) || !item->valuestring) {
        return fallback;
    }
    return item->valuestring;
}

int getInt(const cJSON* obj, const char* key, int fallback)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(item) ? item->valueint : fallback;
}

bool getBool(const cJSON* obj, const char* key, bool fallback)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item);
    }
    return fallback;
}

float getFloat(const cJSON* obj, const char* key, float fallback)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(item) ? static_cast<float>(item->valuedouble) : fallback;
}

/**
 * @brief JSON の文字列を epd_mode_t に直す
 *
 * 値は M5GFX の enum 名と同じ綴りにしてある（変換表を仕様書と二重に持たないため）。
 */
lgfx::v1::epd_mode_t parseEpdMode(const std::string& name)
{
    if (name == "epd_text")    { return lgfx::v1::epd_mode_t::epd_text; }
    if (name == "epd_fast")    { return lgfx::v1::epd_mode_t::epd_fast; }
    if (name == "epd_fastest") { return lgfx::v1::epd_mode_t::epd_fastest; }
    if (name != "epd_quality") {
        ESP_LOGW(TAG, "Unknown epd mode '%s'. Using epd_quality", name.c_str());
    }
    return lgfx::v1::epd_mode_t::epd_quality;
}

/**
 * @brief JSON の文字列を SimpleTransitionType に直す
 *
 * 仕様上、値は enum のメンバー名と同じ綴りにしてある（変換表を作らないため）。
 */
SimpleTransitionType parseTransition(const std::string& name, bool& found)
{
    struct Entry { const char* name; SimpleTransitionType type; };
    static const Entry TABLE[] = {
        { "NONE",            SimpleTransitionType::NONE },
        { "FADE_IN",         SimpleTransitionType::FADE_IN },
        { "SLIDE_LEFT",      SimpleTransitionType::SLIDE_LEFT },
        { "SLIDE_RIGHT",     SimpleTransitionType::SLIDE_RIGHT },
        { "SLIDE_UP",        SimpleTransitionType::SLIDE_UP },
        { "SLIDE_DOWN",      SimpleTransitionType::SLIDE_DOWN },
        { "WIPE_HORIZONTAL", SimpleTransitionType::WIPE_HORIZONTAL },
        { "WIPE_VERTICAL",   SimpleTransitionType::WIPE_VERTICAL },
        { "REVEAL_CENTER",   SimpleTransitionType::REVEAL_CENTER },
        { "REVEAL_CORNER",   SimpleTransitionType::REVEAL_CORNER },
    };

    for (const Entry& e : TABLE) {
        if (name == e.name) {
            found = true;
            return e.type;
        }
    }
    found = false;
    return SimpleTransitionType::NONE;
}

}  // namespace

// ========================================
// 変数
// ========================================

std::string ScenarioPlayer::Value::toString() const
{
    switch (type) {
    case Type::Bool:
        return boolValue ? "true" : "false";
    case Type::Number: {
        char buf[32];
        snprintf(buf, sizeof(buf), "%g", numberValue);
        return buf;
    }
    case Type::String:
    default:
        return stringValue;
    }
}

ScenarioPlayer::Value ScenarioPlayer::valueFromJson(const cJSON* item)
{
    Value v;
    if (cJSON_IsBool(item)) {
        v.type = Value::Type::Bool;
        v.boolValue = cJSON_IsTrue(item);
    } else if (cJSON_IsNumber(item)) {
        v.type = Value::Type::Number;
        v.numberValue = item->valuedouble;
    } else if (cJSON_IsString(item) && item->valuestring) {
        v.type = Value::Type::String;
        v.stringValue = item->valuestring;
    }
    return v;
}

void ScenarioPlayer::initVariables()
{
    _variables.clear();
    _persistentNames.clear();

    const cJSON* vars = _loader->variablesNode();
    if (!cJSON_IsObject(vars)) {
        return;
    }

    const cJSON* item = nullptr;
    cJSON_ArrayForEach(item, vars) {
        if (!item->string) {
            continue;
        }

        // 2通りの書き方を受ける。
        //
        //   "affection": 0
        //   "got_end":   { "value": false, "persistent": true }
        //
        // オブジェクトで `persistent` を付けたものは、
        // ニューゲームでもリセットせず周回をまたいで残す。
        // エンディングの回収記録などがこれで書ける。
        if (cJSON_IsObject(item)) {
            const cJSON* value = cJSON_GetObjectItemCaseSensitive(item, "value");
            _variables[item->string] = valueFromJson(value);
            if (getBool(item, "persistent", false)) {
                _persistentNames.insert(item->string);
            }
        } else {
            _variables[item->string] = valueFromJson(item);
        }
    }

    // 前回までの値を上から被せる
    loadPersistent();

    ESP_LOGI(TAG, "Variables initialized: %u (%u persistent)",
             static_cast<unsigned>(_variables.size()),
             static_cast<unsigned>(_persistentNames.size()));
}

std::string ScenarioPlayer::persistentPath() const
{
    return _loader->basePath() + "/saves/persistent.json";
}

void ScenarioPlayer::loadPersistent()
{
    if (_persistentNames.empty()) {
        return;
    }

    size_t len = 0;
    char* text = SD.readFileToBuffer(persistentPath().c_str(), &len);
    if (!text) {
        return;   // まだ1度も保存していない。宣言した初期値のまま
    }

    cJSON* root = cJSON_ParseWithLength(text, len);
    free(text);
    if (!root) {
        ESP_LOGW(TAG, "persistent.json is malformed. Ignored");
        return;
    }

    int restored = 0;
    const cJSON* item = nullptr;
    cJSON_ArrayForEach(item, root) {
        if (!item->string) {
            continue;
        }
        // **宣言に無い名前・永続でない名前は捨てる。**
        // シナリオ側で persistent を外したのに古い値が残り続けるのを防ぐ。
        if (_persistentNames.find(item->string) == _persistentNames.end()) {
            continue;
        }
        auto it = _variables.find(item->string);
        if (it == _variables.end()) {
            continue;
        }
        it->second = valueFromJson(item);
        ++restored;
    }
    cJSON_Delete(root);

    ESP_LOGI(TAG, "Restored %d persistent variable(s)", restored);
}

bool ScenarioPlayer::savePersistent()
{
    if (_persistentNames.empty()) {
        return true;
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return false;
    }

    for (const std::string& name : _persistentNames) {
        const auto it = _variables.find(name);
        if (it == _variables.end()) {
            continue;
        }
        const Value& v = it->second;
        switch (v.type) {
        case Value::Type::Bool:
            cJSON_AddBoolToObject(root, name.c_str(), v.boolValue);
            break;
        case Value::Type::Number:
            cJSON_AddNumberToObject(root, name.c_str(), v.numberValue);
            break;
        case Value::Type::String:
            cJSON_AddStringToObject(root, name.c_str(), v.stringValue.c_str());
            break;
        }
    }

    char* text = cJSON_Print(root);
    cJSON_Delete(root);
    if (!text) {
        return false;
    }

    SD.mkdir((_loader->basePath() + "/saves").c_str());
    const bool ok = SD.writeFileFromBuffer(persistentPath().c_str(),
                                           text, strlen(text));
    free(text);

    if (ok) {
        ESP_LOGI(TAG, "Persistent variables saved");
    } else {
        // USB MSC 中や SD 無し。物語は続けられる。
        ESP_LOGW(TAG, "Could not save the persistent variables");
    }
    return ok;
}

std::string ScenarioPlayer::interpolate(const std::string& text) const
{
    // `{` を含まないなら何もしない。ほとんどの本文はここで抜ける。
    if (text.find('{') == std::string::npos) {
        return text;
    }

    std::string out;
    out.reserve(text.size());

    size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];

        if (c == '{') {
            // `{{` は `{` そのもの
            if (i + 1 < text.size() && text[i + 1] == '{') {
                out += '{';
                i += 2;
                continue;
            }

            const size_t close = text.find('}', i + 1);
            if (close == std::string::npos) {
                // 閉じ括弧が無い。記号として扱い、本文を失わない。
                out += c;
                ++i;
                continue;
            }

            const std::string name = text.substr(i + 1, close - i - 1);
            const auto it = _variables.find(name);
            if (it != _variables.end()) {
                out += it->second.toString();
            } else {
                // 宣言されていない変数。書き間違いに気づけるよう、
                // 空にせず記法のまま残す。
                ESP_LOGW(TAG, "Text uses undeclared variable '{%s}'", name.c_str());
                out += text.substr(i, close - i + 1);
            }
            i = close + 1;
            continue;
        }

        if (c == '}' && i + 1 < text.size() && text[i + 1] == '}') {
            out += '}';
            i += 2;
            continue;
        }

        out += c;
        ++i;
    }

    return out;
}

// ========================================
// 条件式
// ========================================

bool ScenarioPlayer::evaluateComparison(const cJSON* cond) const
{
    const std::string name = getString(cond, "var");
    const auto it = _variables.find(name);
    if (it == _variables.end()) {
        // 宣言されていない変数。タイプミスの可能性が高いので気づけるようにする。
        ESP_LOGE(TAG, "Condition uses undeclared variable '%s' -> false", name.c_str());
        return false;
    }

    const Value& current = it->second;
    const Value expected = valueFromJson(cJSON_GetObjectItemCaseSensitive(cond, "value"));
    const std::string op = getString(cond, "op", "==");

    // 大小比較は数値だけ。bool と文字列は == / != のみ。
    if (op == "<" || op == "<=" || op == ">" || op == ">=") {
        if (current.type != Value::Type::Number) {
            ESP_LOGE(TAG, "Operator '%s' needs a number, but '%s' is not -> false",
                     op.c_str(), name.c_str());
            return false;
        }
        const double a = current.numberValue;
        const double b = expected.numberValue;
        if (op == "<")  { return a <  b; }
        if (op == "<=") { return a <= b; }
        if (op == ">")  { return a >  b; }
        return a >= b;
    }

    bool equal = false;
    if (current.type != expected.type) {
        // 型が違えば等しくない。JSON の書き間違いに気づけるよう警告する。
        ESP_LOGW(TAG, "Type mismatch comparing '%s'", name.c_str());
    } else {
        switch (current.type) {
        case Value::Type::Bool:   equal = (current.boolValue   == expected.boolValue);   break;
        case Value::Type::Number: equal = (current.numberValue == expected.numberValue); break;
        case Value::Type::String: equal = (current.stringValue == expected.stringValue); break;
        }
    }

    if (op == "!=") {
        return !equal;
    }
    if (op != "==") {
        ESP_LOGW(TAG, "Unknown operator '%s', treating as '=='", op.c_str());
    }
    return equal;
}

bool ScenarioPlayer::evaluateCondition(const cJSON* cond) const
{
    // 条件そのものが無い場合は「常に成立」。
    // cond を省いた選択肢が常に出るようにするため。
    if (!cJSON_IsObject(cond)) {
        return true;
    }

    const cJSON* all = cJSON_GetObjectItemCaseSensitive(cond, "all");
    if (cJSON_IsArray(all)) {
        const cJSON* sub = nullptr;
        cJSON_ArrayForEach(sub, all) {
            if (!evaluateCondition(sub)) {
                return false;
            }
        }
        return true;
    }

    const cJSON* any = cJSON_GetObjectItemCaseSensitive(cond, "any");
    if (cJSON_IsArray(any)) {
        const cJSON* sub = nullptr;
        cJSON_ArrayForEach(sub, any) {
            if (evaluateCondition(sub)) {
                return true;
            }
        }
        return false;
    }

    const cJSON* negated = cJSON_GetObjectItemCaseSensitive(cond, "not");
    if (cJSON_IsObject(negated)) {
        return !evaluateCondition(negated);
    }

    return evaluateComparison(cond);
}

// ========================================
// 実行位置
// ========================================

ScenarioPlayer::Frame* ScenarioPlayer::currentFrame()
{
    return _frames.empty() ? nullptr : &_frames.back();
}

const cJSON* ScenarioPlayer::currentCommand()
{
    Frame* frame = currentFrame();
    if (!frame || !frame->commands) {
        return nullptr;
    }
    if (frame->index >= cJSON_GetArraySize(const_cast<cJSON*>(frame->commands))) {
        return nullptr;
    }
    return cJSON_GetArrayItem(const_cast<cJSON*>(frame->commands), frame->index);
}

void ScenarioPlayer::begin(M5GFX* display,
                           ScenarioLoader* loader,
                           TypoWrite* vertical,
                           TypoWrite* horizontal,
                           SimpleTransition* transition)
{
    _display    = display;
    _loader     = loader;
    _vertical   = vertical;
    _horizontal = horizontal;
    _transition = transition;
}

bool ScenarioPlayer::prepare()
{
    if (!_display || !_loader || !_loader->isLoaded()) {
        ESP_LOGE(TAG, "Not ready (display=%p loader=%p loaded=%d)",
                 _display, _loader, _loader ? _loader->isLoaded() : 0);
        _state = State::Finished;
        return false;
    }

    _endingId.clear();
    _endPending = false;
    _pageOffset = 0;
    _frames.clear();
    _charas.clear();
    _currentBackground.clear();
    _backgroundX = 0;
    _backgroundY = 0;
    _backgroundScale = 1.0f;
    _foreground = ForegroundState{};
    _callStack.clear();
    clearCharaCache();
    _history.clear();

    // **控えは必ず捨てる。**
    // Frame は読み込んだ JSON への生ポインタなので、
    // 前のシナリオの控えを残すと解放済みの領域を指したままになる。
    _backStack.clear();
    _lastBody.clear();
    _lastBoxName.clear();
    _lastPageOffset = 0;
    releaseCheckpoint();
    initVariables();
    buildTextBoxes();

    if (!gotoScene(_loader->startSceneId())) {
        ESP_LOGE(TAG, "Start scene '%s' not found", _loader->startSceneId().c_str());
        _state = State::Finished;
        return false;
    }

    return true;
}

bool ScenarioPlayer::start()
{
    if (!prepare()) {
        return false;
    }

    ESP_LOGI(TAG, "Playing '%s' from scene '%s'",
             _loader->title().c_str(), _sceneId.c_str());

    run();
    return true;
}

bool ScenarioPlayer::resumeFrom(int slot)
{
    // まず通常の開始と同じ状態を作る。
    // 変数の宣言とテキストボックスはセーブに含まれないので、
    // シナリオ側の定義から作り直す必要がある。
    //
    // prepare() は run() を含まない。start() を使うと
    // **冒頭が一瞬描かれてから位置が飛ぶ**ため。
    if (!prepare()) {
        return false;
    }

    if (!loadFromSlot(slot)) {
        // セーブが無い・壊れている・別シナリオのもの。
        // prepare() は済んでいるので、冒頭から遊べる状態にはなっている。
        ESP_LOGW(TAG, "Could not resume from slot %d. Starting from the beginning", slot);
        run();
        return false;
    }

    ESP_LOGI(TAG, "Resumed '%s' at scene '%s'",
             _loader->title().c_str(), _sceneId.c_str());

    run();
    return true;
}

bool ScenarioPlayer::gotoScene(const std::string& sceneId)
{
    const cJSON* scene = _loader->findScene(sceneId);
    if (!scene) {
        return false;
    }

    const cJSON* commands = cJSON_GetObjectItemCaseSensitive(scene, "commands");
    if (!cJSON_IsArray(commands)) {
        ESP_LOGW(TAG, "Scene '%s' has no commands array", sceneId.c_str());
        return false;
    }

    _sceneId    = sceneId;
    _scene      = scene;
    _pageOffset = 0;

    // シーンを移るときは入れ子（`if` の中）から抜けきる。
    // 前のシーンのフレームが残っていると、そちらへ戻ってしまう。
    _frames.clear();
    _frames.push_back(Frame{commands, 0});

    ESP_LOGI(TAG, "Scene -> '%s' (%d commands)",
             _sceneId.c_str(), cJSON_GetArraySize(commands));
    return true;
}

TypoWrite* ScenarioPlayer::writerFor(const cJSON* cmd) const
{
    // 名前付きボックスの指定があればそれを使う。
    const std::string boxName = getString(cmd, "box");
    if (!boxName.empty()) {
        if (TypoWrite* named = textSystem.box(boxName)) {
            return named;
        }
        // 定義されていない名前。既定へ落として再生は続ける。
        ESP_LOGW(TAG, "Text box '%s' is not defined. Using the default box",
                 boxName.c_str());
    }

    // 指定が無ければ従来どおり。
    // 優先順位: コマンドの direction > meta.text_direction > 縦書き
    std::string dir = getString(cmd, "direction");
    if (dir.empty() && _loader) {
        dir = _loader->defaultTextDirection();
    }

    return (dir == "HORIZONTAL") ? _horizontal : _vertical;
}

void ScenarioPlayer::buildTextBoxes()
{
    textSystem.clearBoxes();

    const cJSON* boxes = _loader->textBoxesNode();
    if (!cJSON_IsObject(boxes)) {
        return;   // 定義なし。既定の2つだけで動く
    }

    const cJSON* box = nullptr;
    cJSON_ArrayForEach(box, boxes) {
        if (!box->string) {
            continue;
        }

        // 省略された項目は既定値。
        // 位置と大きさだけ書けば使えるようにしてある。
        const std::string dir = getString(box, "direction", "VERTICAL");
        const std::string align = getString(box, "align", "LEFT");
        const bool vertical = (dir != "HORIZONTAL");

        // 本文の色。
        //
        // **既定は下地の反対。** 下地を白にしたのに本文が白のままだと
        // 何も見えない。これは実際に踏んだ（サンプルが真っ白になった）。
        // 背景画像を敷く場合など、明るさが分からないときは
        // `text_color` で明示する。
        const bool whiteBg = (getString(box, "background_color", "BLACK") == "WHITE");
        const std::string textColorName =
            getString(box, "text_color", whiteBg ? "BLACK" : "WHITE");
        const uint16_t textColor = (textColorName == "BLACK") ? TFT_BLACK : TFT_WHITE;

        TextAlignment a = TextAlignment::LEFT;
        if (align == "CENTER") { a = TextAlignment::CENTER; }
        else if (align == "RIGHT") { a = TextAlignment::RIGHT; }

        const cJSON* fs = cJSON_GetObjectItemCaseSensitive(box, "font_size");
        const float fontSize = cJSON_IsNumber(fs)
                                   ? static_cast<float>(fs->valuedouble)
                                   : 1.0f;

        textSystem.defineBox(box->string,
                             getInt(box, "x", 0),
                             getInt(box, "y", 0),
                             getInt(box, "w", 0),
                             getInt(box, "h", 0),
                             vertical,
                             fontSize,
                             getInt(box, "line_spacing", 6),
                             // 字間の既定値は向きで変える。
                             // 縦書きは送りが 1em ちょうどで隙間がほぼ無く、
                             // 0 のままだと既定のボックスより詰まって見える。
                             getInt(box, "char_spacing",
                                    vertical ? DEFAULT_VERTICAL_CHAR_SPACING : 0),
                             a,
                             parsePadding(cJSON_GetObjectItemCaseSensitive(box, "padding")),
                             textColor,
                             getBool(box, "kinsoku", true));
    }
}

TextBoxPadding ScenarioPlayer::parsePadding(const cJSON* node)
{
    TextBoxPadding p;

    if (!node) {
        return p;
    }

    // 数値なら四辺まとめて。枠のない箱ではこれで足りる。
    if (cJSON_IsNumber(node)) {
        p.top = p.right = p.bottom = p.left = node->valueint;
        return p;
    }

    // オブジェクトなら辺ごと。省略した辺は 0。
    if (cJSON_IsObject(node)) {
        p.top    = getInt(node, "top", 0);
        p.right  = getInt(node, "right", 0);
        p.bottom = getInt(node, "bottom", 0);
        p.left   = getInt(node, "left", 0);
        return p;
    }

    ESP_LOGW(TAG, "padding must be a number or an object. Ignored");
    return p;
}

void ScenarioPlayer::fillTextBoxBackground(const std::string& boxName, TypoWrite* writer)
{
    if (!writer) {
        return;
    }

    // 背景画像があれば矩形に敷く。
    // 画像はボックスの寸法に合わせて用意すること（拡大縮小はしない）。
    if (!boxName.empty()) {
        std::string path;
        if (_loader->resolveTextBoxBackground(boxName.c_str(), path)) {
            if (_display->drawPngFile(&SD, path.c_str(),
                                      writer->areaX(), writer->areaY())) {
                return;
            }
            ESP_LOGE(TAG, "Failed to draw the text box background: %s", path.c_str());
        }
    }

    // 画像が無い、または描けなかった場合は色で塗る
    uint16_t color = TFT_BLACK;
    if (!boxName.empty()) {
        const cJSON* box =
            cJSON_GetObjectItemCaseSensitive(_loader->textBoxesNode(), boxName.c_str());
        if (getString(box, "background_color", "BLACK") == "WHITE") {
            color = TFT_WHITE;
        }
    }
    writer->clearArea(color);
}

void ScenarioPlayer::beginBatch()
{
    if (_batchOpen || !_display) {
        return;
    }
    _display->startWrite();
    _batchOpen = true;
}

void ScenarioPlayer::endBatch()
{
    if (!_batchOpen || !_display) {
        return;
    }
    _batchOpen = false;
    _display->endWrite();
}

void ScenarioPlayer::markRefresh(lgfx::v1::epd_mode_t mode)
{
    _needRefresh = true;
    _refreshMode = mode;
}

void ScenarioPlayer::flushScreen()
{
    if (_needRefresh) {
        _needRefresh = false;

        // **バッチは開けたまま渡す。**
        // 先に閉じると「描いた範囲の部分更新」と「全画面の走査」で
        // 2回書き換わる。refreshScreen() は更新範囲を全画面へ広げてから
        // 積むので、開けたまま呼べば1回で済む。
        SimpleTransition::refreshScreen(_display, _refreshMode);
        _refreshMode = lgfx::v1::epd_mode_t::epd_quality;
    }
    endBatch();
}

void ScenarioPlayer::pushSnapshot()
{
    Snapshot s;
    s.sceneId = _sceneId;
    s.frames = _frames;
    s.callStack = _callStack;
    s.pageOffset = _pageOffset;
    s.variables = _variables;
    s.background = _currentBackground;
    s.backgroundX = _backgroundX;
    s.backgroundY = _backgroundY;
    s.backgroundScale = _backgroundScale;
    s.charas = _charas;
    s.foreground = _foreground;
    s.historySize = _history.size();

    _backStack.push_back(std::move(s));

    // 古いものから捨てる。全部持つと長編で際限なく増える。
    if (_backStack.size() > MAX_BACK) {
        _backStack.erase(_backStack.begin());
    }
}

void ScenarioPlayer::restoreSnapshot(const Snapshot& s)
{
    _sceneId = s.sceneId;
    _scene = _loader->findScene(_sceneId);
    _frames = s.frames;
    _callStack = s.callStack;
    _pageOffset = s.pageOffset;
    _variables = s.variables;
    _currentBackground = s.background;
    _backgroundX = s.backgroundX;
    _backgroundY = s.backgroundY;
    _backgroundScale = s.backgroundScale;
    _charas = s.charas;
    _foreground = s.foreground;

    // 戻った先の本文をもう一度積むので、ここで切り詰めておく。
    // やらないと履歴に同じ本文が並ぶ。
    if (s.historySize < _history.size()) {
        _history.resize(s.historySize);
    }

    // 文字送りの途中で戻られても続きを出さない
    _typingWriter = nullptr;
    _typingBody.clear();
    _typingBoxName.clear();

    // `end` のメッセージを出したあとに戻られた場合。
    // 残したままだと、次のタップでシナリオが終わってしまう。
    _endPending = false;
}

bool ScenarioPlayer::goBack()
{
    // **画面が落ち着いているときだけ戻す。**
    // 遷移の途中や選択肢の表示中に位置を動かすと、
    // このあと走るはずの処理（onTransitionFinished / selectChoice）と食い違う。
    if (_state != State::WaitingTap &&
        _state != State::Waiting &&
        _state != State::Typing) {
        ESP_LOGI(TAG, "Not a good moment to go back (state=%d)",
                 static_cast<int>(_state));
        return false;
    }

    // 積んであるのは「今の画面の開始位置」まで。
    // 1つしか無いなら、今出ているのが最初の画面ということ。
    if (_backStack.size() < 2) {
        ESP_LOGI(TAG, "No screen to go back to");
        return false;
    }

    _backStack.pop_back();                  // 今の画面の開始位置は捨てる
    const Snapshot prev = _backStack.back();
    _backStack.pop_back();                  // run() が押し直す

    restoreSnapshot(prev);

    ESP_LOGI(TAG, "Going back to scene '%s' index %d",
             _sceneId.c_str(), _frames.empty() ? -1 : _frames[0].index);

    // **舞台を戻す。**
    // 戻った先のコマンドが背景や立ち絵を描くとは限らないので、
    // 控えた舞台をここで作り直しておかないと今の画面のまま残る。
    beginBatch();
    renderStage(_display);
    markRefresh();

    run();
    return true;
}

void ScenarioPlayer::run()
{
    pushSnapshot();
    beginBatch();
    runUntilWait();
    flushScreen();
}

void ScenarioPlayer::runUntilWait()
{
    int steps = 0;

    for (;;) {
        if (++steps > MAX_STEPS_PER_RUN) {
            // 待ちの入らない jump の輪に落ちた可能性が高い。
            // 放置すると戻ってこないので打ち切る。
            ESP_LOGE(TAG, "Exceeded %d commands without waiting. "
                          "Check for a jump loop in scene '%s'",
                     MAX_STEPS_PER_RUN, _sceneId.c_str());
            _state = State::Finished;
            return;
        }

        if (_frames.empty()) {
            _state = State::Finished;
            return;
        }

        const cJSON* cmd = currentCommand();

        if (!cmd) {
            // 今の配列を使い切った。
            // 入れ子（`if` の中）なら1つ戻って続きから、
            // シーンの底まで来ていたら `next` を辿る。
            if (_frames.size() > 1) {
                _frames.pop_back();
                ++_frames.back().index;   // `if` コマンドの次へ
                continue;
            }

            const std::string next = getString(_scene, "next");
            if (next.empty()) {
                ESP_LOGI(TAG, "Scene '%s' ended with no 'next'. Finishing",
                         _sceneId.c_str());
                _state = State::Finished;
                return;
            }
            if (!gotoScene(next)) {
                ESP_LOGE(TAG, "Scene '%s': next '%s' not found",
                         _sceneId.c_str(), next.c_str());
                _state = State::Finished;
                return;
            }
            continue;
        }

        switch (executeCommand(cmd)) {
        case CmdResult::Next:
            ++currentFrame()->index;
            break;

        case CmdResult::StayAndWaitTap:
            // 同じコマンドのまま待つ（本文の続きが残っている）
            _state = State::WaitingTap;
            return;

        case CmdResult::NextAndWaitTap:
            ++currentFrame()->index;
            _state = State::WaitingTap;
            return;

        case CmdResult::NextAndWaitTransition:
            ++currentFrame()->index;
            _state = State::WaitingTransition;
            return;

        case CmdResult::StayAndType:
            // 位置は据え置く。tickTyping() が出し切ってから進める。
            _state = State::Typing;
            return;

        case CmdResult::NextAndWaitTime:
            ++currentFrame()->index;
            _state = State::Waiting;
            return;

        case CmdResult::NextAndWaitChoice:
            // 選択肢は選ばれた時点でシーンごと移るので、
            // ここで位置を進めておく意味は薄いが、
            // 選択が成立しなかった場合に先へ進めるよう揃えておく。
            ++currentFrame()->index;
            _state = State::WaitingChoice;
            return;

        case CmdResult::Pushed:
            // executeIf() が入れ子の配列を積んだ
            break;

        case CmdResult::Jumped:
            // gotoScene() が位置を設定済み
            break;

        case CmdResult::Finished:
            _state = State::Finished;
            return;
        }
    }
}

ScenarioPlayer::CmdResult ScenarioPlayer::executeCommand(const cJSON* cmd)
{
    const std::string type = getString(cmd, "type");

    if (type == "text") {
        return executeText(cmd);
    }
    if (type == "bg") {
        return executeBackground(cmd);
    }
    if (type == "chara") {
        return executeChara(cmd);
    }
    if (type == "random") {
        return executeRandom(cmd);
    }
    if (type == "call") {
        return executeCall(cmd);
    }
    if (type == "return") {
        return executeReturn(cmd);
    }
    if (type == "image") {
        return executeImage(cmd);
    }
    if (type == "suspend") {
        return executeSuspend(cmd);
    }
    if (type == "checkpoint") {
        return executeCheckpoint(cmd);
    }
    if (type == "save") {
        return executeSave(cmd);
    }
    if (type == "load") {
        return executeLoad(cmd);
    }
    if (type == "choice") {
        return executeChoice(cmd);
    }
    if (type == "set") {
        return executeSet(cmd);
    }
    if (type == "if") {
        return executeIf(cmd);
    }
    if (type == "clear") {
        const std::string color = getString(cmd, "color", "BLACK");
        _display->fillScreen(color == "WHITE" ? TFT_WHITE : TFT_BLACK);

        // 画面を塗ったので、背景も立ち絵も見た目上は消えている。
        // 保持している状態もここで捨てて実態と合わせる。
        // 残したままだと、次に立ち絵を動かした時に
        // 消したはずの背景が renderStage() で復活する。
        _charas.clear();
        _currentBackground.clear();
        _foreground = ForegroundState{};

        // 画面全体が変わったので、出すときは全画面の走査で
        markRefresh();
        return CmdResult::Next;
    }
    if (type == "wait") {
        const int ms = getInt(cmd, "ms", 0);
        if (ms <= 0) {
            return CmdResult::Next;
        }

        // vTaskDelay で止めるとタップを拾えず skippable が実現できない。
        // 期限だけ決めて、経過は tickWait() が見る。
        _waitUntilMs = esp_timer_get_time() / 1000 + ms;
        _waitSkippable = getBool(cmd, "skippable", true);
        return CmdResult::NextAndWaitTime;
    }
    if (type == "beep") {
        const cJSON* melody = cJSON_GetObjectItemCaseSensitive(cmd, "melody");
        if (cJSON_IsArray(melody)) {
            std::vector<Note> notes;
            const cJSON* n = nullptr;
            cJSON_ArrayForEach(n, melody) {
                notes.push_back(Note{
                    static_cast<uint32_t>(getInt(n, "freq", 0)),
                    static_cast<uint32_t>(getInt(n, "duration", 100))
                });
            }
            if (!notes.empty()) {
                buzzer.playMelody(notes.data(), notes.size(),
                                  getBool(cmd, "wait", false));
            }
        } else {
            buzzer.tone(static_cast<uint32_t>(getInt(cmd, "freq", 0)),
                        static_cast<uint32_t>(getInt(cmd, "duration", 100)));
        }
        return CmdResult::Next;
    }
    if (type == "refresh") {
        const lgfx::v1::epd_mode_t mode = parseEpdMode(getString(cmd, "mode", "epd_quality"));
        if (getBool(cmd, "clear_ghost", false)) {
            // 反転を伴うので、**ここまでの描画を先に出しておく**。
            // 残像消去は「今パネルに出ている絵」を反転させる処理なので、
            // 溜めたままだと反転の対象が古い絵になる。
            endBatch();
            SimpleTransition::clearGhosting(_display, mode);
            _needRefresh = false;
            beginBatch();
        } else {
            markRefresh(mode);
        }
        return CmdResult::Next;
    }
    if (type == "jump") {
        const std::string next = getString(cmd, "next");
        if (!gotoScene(next)) {
            ESP_LOGE(TAG, "jump to undefined scene '%s'", next.c_str());
            return CmdResult::Finished;
        }
        return CmdResult::Jumped;
    }
    if (type == "end") {
        _endingId = getString(cmd, "ending");
        ESP_LOGI(TAG, "Scenario finished (ending='%s')", _endingId.c_str());

        // 周回をまたぐ変数を書き出す。
        // `set` のたびに書くと SD への書き込みが増えすぎるので、
        // 区切りのここと `suspend` だけで書く。
        savePersistent();

        // message があれば見せてから終わる。
        // 即座に終わるとメニューへ戻ってしまい、読む間が無い。
        const std::string message = interpolate(getString(cmd, "message"));
        if (!message.empty()) {
            TypoWrite* writer = _vertical;
            if (writer) {
                fillTextBoxBackground("", writer);
                writer->drawTextPaged(message, 0);
                markRefresh();
            }
            _endPending = true;
            return CmdResult::NextAndWaitTap;
        }

        return CmdResult::Finished;
    }

    // 未対応のコマンド。前方互換の方針どおり読み飛ばす。
    ESP_LOGW(TAG, "Unsupported command '%s' in scene '%s' (skipped)",
             type.c_str(), _sceneId.c_str());
    return CmdResult::Next;
}

ScenarioPlayer::CmdResult ScenarioPlayer::executeText(const cJSON* cmd)
{
    TypoWrite* writer = writerFor(cmd);
    if (!writer) {
        ESP_LOGE(TAG, "No text writer available");
        return CmdResult::Next;
    }

    // 変数を差し込む。ページ送りのオフセットは差し込み後の文字列基準なので、
    // ここで一度だけ行い、以降は加工済みの本文を使い回す。
    std::string body = interpolate(getString(cmd, "body"));

    // 話者名は本文の先頭に添える。
    // 専用の名前欄はまだ無いので、まずはこの形で読めるようにしておく。
    //
    // ページ送りのオフセットは「加工後の本文」を基準にしているため、
    // 2ページ目以降も同じ加工をしないと位置がずれる。
    // ここで分岐させず常に付けるのはそのため。
    const std::string speaker = interpolate(getString(cmd, "speaker"));
    if (!speaker.empty()) {
        body = "【" + speaker + "】\n" + body;
    }

    // 背景を透過にしてあるので、前ページの消去は呼び出し側の責任。
    // ボックスに背景画像があれば敷き、無ければ色で塗る。
    const std::string boxName = getString(cmd, "box");
    fillTextBoxBackground(boxName, writer);

    // 描き直し用に控える。バックログを閉じたとき、
    // 舞台だけ描き直しても本文が戻らないため。
    _lastBody = body;
    _lastBoxName = boxName;
    _lastPageOffset = _pageOffset;

    // 履歴。ページ送りの2ページ目以降は同じ本文なので積まない。
    if (_pageOffset == 0) {
        _history.push_back(body);
        if (_history.size() > MAX_HISTORY) {
            _history.erase(_history.begin());
        }
    }

    const int speed = getInt(cmd, "speed", 0);

    // 文字送りあり。1文字だけ出して、あとは tickTyping() が増やす。
    //
    // 電子ペーパーは1文字ごとに全面走査が要るので、
    // 最速の epd_fastest でも1文字約117ms かかる。
    // 送っている間だけそのモードへ落とし、出し切ったら元のモードで描き直す。
    if (speed > 0) {
        const TypoWrite::DrawResult first = writer->drawTextPaged(body, _pageOffset, 1);

        _typingBody = body;
        _typingWriter = writer;
        _typingBoxName = boxName;
        _typedChars = 1;
        _typingPageChars = first.pageChars;
        _typingSpeedMs = speed;
        _lastTypedMs = esp_timer_get_time() / 1000;
        _typingWaitAfter = getBool(cmd, "wait", true);

        // 送っているあいだは最速モード。以後 tickTyping() が1文字ずつ出す。
        _display->setEpdMode(lgfx::v1::epd_mode_t::epd_fastest);
        markRefresh(lgfx::v1::epd_mode_t::epd_fastest);

        ESP_LOGI(TAG, "text: typing %u chars at %d ms/char",
                 static_cast<unsigned>(_typingPageChars), speed);
        return CmdResult::StayAndType;
    }

    const TypoWrite::DrawResult result = writer->drawTextPaged(body, _pageOffset);

    ESP_LOGI(TAG, "text: offset %u -> %u, hasMore=%d",
             static_cast<unsigned>(_pageOffset),
             static_cast<unsigned>(result.nextOffset),
             static_cast<int>(result.hasMore));

    markRefresh();

    if (result.hasMore) {
        // 続きがある。同じコマンドのまま次のページを待つ。
        _pageOffset = result.nextOffset;
        return CmdResult::StayAndWaitTap;
    }

    _pageOffset = 0;
    return getBool(cmd, "wait", true) ? CmdResult::NextAndWaitTap : CmdResult::Next;
}

ScenarioPlayer::CharaState* ScenarioPlayer::findChara(const std::string& id)
{
    for (CharaState& c : _charas) {
        if (c.id == id) {
            return &c;
        }
    }
    return nullptr;
}

std::string ScenarioPlayer::charaBgKey(const CharaState& c) const
{
    // 背景のその部分がどこかを表す。位置と倍率が変われば切り出す場所も変わる。
    char buf[160];
    snprintf(buf, sizeof(buf), "%s,%d,%d,%.3f|%d,%d,%.3f",
             _currentBackground.c_str(), _backgroundX, _backgroundY,
             _backgroundScale, c.x, c.y, c.scale);
    return buf;
}

std::string ScenarioPlayer::charaCacheKey(const CharaState& c) const
{
    std::string key = charaBgKey(c);
    key += "|";
    for (const auto& kv : c.layers) {
        key += kv.first;
        key += "=";
        key += kv.second;
        key += ",";
    }
    return key;
}

bool ScenarioPlayer::charaBounds(const CharaState& c, int& w, int& h) const
{
    const cJSON* size = _loader->characterSize(c.id.c_str());
    if (!cJSON_IsObject(size)) {
        return false;
    }
    w = static_cast<int>(getInt(size, "w", 0) * c.scale);
    h = static_cast<int>(getInt(size, "h", 0) * c.scale);
    return w > 0 && h > 0;
}

bool ScenarioPlayer::charaOverlaps(const CharaState& c) const
{
    int w = 0;
    int h = 0;
    if (!charaBounds(c, w, h)) {
        return true;   // 大きさが分からない。安全側に倒して直接描く
    }

    for (const CharaState& other : _charas) {
        if (other.id == c.id || !other.visible) {
            continue;
        }
        int ow = 0;
        int oh = 0;
        if (!charaBounds(other, ow, oh)) {
            return true;
        }
        const bool apart = (c.x + w <= other.x) || (other.x + ow <= c.x) ||
                           (c.y + h <= other.y) || (other.y + oh <= c.y);
        if (!apart) {
            return true;
        }
    }
    return false;
}

void ScenarioPlayer::clearCharaCache()
{
    for (auto& kv : _charaCache) {
        if (kv.second.bgSlice) {
            kv.second.bgSlice->deleteSprite();
            delete kv.second.bgSlice;
        }
        if (kv.second.composite) {
            kv.second.composite->deleteSprite();
            delete kv.second.composite;
        }
    }
    _charaCache.clear();
}

M5Canvas* ScenarioPlayer::ensureCharaComposite(const CharaState& c,
                                               const cJSON* layerDefs)
{
    int cw = 0;
    int ch = 0;
    if (!charaBounds(c, cw, ch)) {
        CharaCache& cache = _charaCache[c.id];
        if (!cache.warned) {
            cache.warned = true;
            ESP_LOGI(TAG, "chara '%s': no 'size' in assets. "
                          "Drawing directly (add {\"w\",\"h\"} to speed it up)",
                     c.id.c_str());
        }
        return nullptr;
    }

    CharaCache& cache = _charaCache[c.id];

    // 倍率が変わると要る大きさも変わる。
    // 作り直さないと、前の倍率のままの1枚を貼り続けることになる。
    if (cache.bgSlice && (cache.w != cw || cache.h != ch)) {
        cache.bgSlice->deleteSprite();
        cache.composite->deleteSprite();
        delete cache.bgSlice;
        delete cache.composite;
        cache.bgSlice = nullptr;
        cache.composite = nullptr;
    }

    if (!cache.bgSlice) {
        cache.bgSlice = new M5Canvas(_display);
        cache.bgSlice->setPsram(true);
        cache.composite = new M5Canvas(_display);
        cache.composite->setPsram(true);

        if (!cache.bgSlice->createSprite(cw, ch) ||
            !cache.composite->createSprite(cw, ch)) {
            ESP_LOGW(TAG, "Could not hold a composite for '%s' (%dx%d x2)",
                     c.id.c_str(), cw, ch);
            cache.bgSlice->deleteSprite();
            cache.composite->deleteSprite();
            delete cache.bgSlice;
            delete cache.composite;
            _charaCache.erase(c.id);
            return nullptr;
        }
        cache.w = cw;
        cache.h = ch;
        cache.bgKey.clear();
        cache.key.clear();
    }

    // --- 背景のその部分 ---
    //
    // **背景が変わっていなければ読み直さない。**
    // 背景 PNG の読み込みは実測 900ms あり、描画全体の 86% を占めていた。
    // 表情を変えるたびに読み直しては、控えを持つ意味が無い。
    const std::string bgKey = charaBgKey(c);
    if (cache.bgKey != bgKey) {
        cache.bgSlice->fillSprite(TFT_BLACK);
        if (!_currentBackground.empty()) {
            std::string bgPath;
            if (_loader->resolveBackgroundPath(_currentBackground.c_str(), bgPath)) {
                cache.bgSlice->drawPngFile(&SD, bgPath.c_str(),
                                           _backgroundX - c.x, _backgroundY - c.y,
                                           0, 0, 0, 0,
                                           _backgroundScale, _backgroundScale);
            }
        }
        cache.bgKey = bgKey;
        cache.key.clear();   // 下地が変わったので重ね直す
        ESP_LOGI(TAG, "chara '%s': background slice rebuilt", c.id.c_str());
    }

    // --- レイヤーを重ねる ---
    const std::string key = charaCacheKey(c);
    if (cache.key != key) {
        // 下地を写してから重ねる。ここは SD を読まない。
        cache.bgSlice->pushSprite(cache.composite, 0, 0);

        const cJSON* lay = nullptr;
        cJSON_ArrayForEach(lay, layerDefs) {
            const std::string name = getString(lay, "name");
            if (name.empty()) {
                continue;
            }
            const auto it = c.layers.find(name);
            const std::string variant = (it != c.layers.end())
                ? it->second
                : ScenarioLoader::defaultVariant(lay);

            std::string path;
            if (!_loader->resolveLayerPath(c.id.c_str(), name.c_str(),
                                           variant.c_str(), path)) {
                continue;
            }
            cache.composite->drawPngFile(&SD, path.c_str(),
                                         static_cast<int>(getInt(lay, "x", 0) * c.scale),
                                         static_cast<int>(getInt(lay, "y", 0) * c.scale),
                                         0, 0, 0, 0, c.scale, c.scale);
        }
        cache.key = key;
    }

    return cache.composite;
}

bool ScenarioPlayer::drawCharaCached(lgfx::LovyanGFX* target,
                                     const CharaState& c,
                                     const cJSON* layerDefs)
{
    // 控えが使えるのは画面へ直接描くときだけ。
    // トランジションのキャンバスには別の背景が乗っているので食い違う。
    if (target != _display) {
        return false;
    }
    if (charaOverlaps(c)) {
        // 背景ごと貼るので、下にいる立ち絵を消してしまう
        return false;
    }

    M5Canvas* composite = ensureCharaComposite(c, layerDefs);
    if (!composite) {
        return false;
    }

    composite->pushSprite(target, c.x, c.y);
    return true;
}

bool ScenarioPlayer::emergencySave()
{
    if (!_loader || !_loader->isLoaded()) {
        return false;
    }

    ESP_LOGW(TAG, "Battery is low. Saving to the auto slot");

    const bool ok = saveToSlot(0);
    savePersistent();

    if (ok) {
        // 次に電源を入れたとき「続きから」で拾えるようにする
        settings.setLastScenario(_loader->scenarioId().c_str());
        settings.setResumeSlot(0);
        settings.save();
    }
    return ok;
}

void ScenarioPlayer::restoreStageAndText()
{
    renderStage(_display);

    if (_lastBody.empty()) {
        return;
    }

    // **戻せるのは直近に書いた1つの枠だけ。**
    // どの枠に何を書いたかを全部覚えてはいない。
    // 名前や見出しの枠も残したい場合は、シナリオ側で書き直すこと。
    TypoWrite* writer = _lastBoxName.empty()
        ? (_vertical ? _vertical : _horizontal)
        : textSystem.box(_lastBoxName);
    if (!writer) {
        writer = _vertical ? _vertical : _horizontal;
    }
    if (writer) {
        fillTextBoxBackground(_lastBoxName, writer);
        writer->drawTextPaged(_lastBody, _lastPageOffset);
    }
}

void ScenarioPlayer::redrawCurrentScreen()
{
    // 舞台と本文をまとめて1回で出す。
    // バックログを閉じた直後なので、画面全体が変わっている。
    beginBatch();
    restoreStageAndText();
    markRefresh();
    flushScreen();
}

void ScenarioPlayer::renderStage(lgfx::LovyanGFX* target)
{
    if (!target) {
        return;
    }

    // **描き終わるまで画面へ出さない。**
    // 塗りつぶし → 背景 PNG → 立ち絵の順に何度も描くので、
    // そのまま出すと「黒 → 背景だけ → 立ち絵1体」と段階が見えてしまう。
    //
    // startWrite() は入れ子になる（_start_count）。run() が既に開いていれば
    // ここでは増減するだけで、実際に出るのは run() が閉じたときになる。
    target->startWrite();

    // 背景から描き直す。
    // 立ち絵だけを描くと前の立ち絵が消えずに重なってしまう。
    target->fillScreen(TFT_BLACK);

    if (!_currentBackground.empty()) {
        std::string path;
        if (_loader->resolveBackgroundPath(_currentBackground.c_str(), path)) {
            // scale は 1bpp では輪郭が粗くなるので、原寸で用意するのが基本。
            // それでも指定できるようにしてあるのは、素材を作り直さずに
            // 位置合わせを試したい場面があるため。
            if (!target->drawPngFile(&SD, path.c_str(), _backgroundX, _backgroundY,
                                     0, 0, 0, 0,
                                     _backgroundScale, _backgroundScale)) {
                ESP_LOGE(TAG, "Failed to draw background %s", path.c_str());
            }
        }
    }

    // 立ち絵を追加順に重ねる（後のものが手前）
    for (const CharaState& c : _charas) {
        if (!c.visible) {
            continue;
        }

        const cJSON* layerDefs = _loader->characterLayers(c.id.c_str());

        if (!layerDefs) {
            // 単一画像方式（従来）
            std::string path;
            if (!_loader->resolveCharacterPath(c.id.c_str(), c.expression.c_str(), path)) {
                ESP_LOGE(TAG, "Character '%s/%s' is not in assets",
                         c.id.c_str(), c.expression.c_str());
                continue;
            }
            if (!target->drawPngFile(&SD, path.c_str(), c.x, c.y,
                                     0, 0, 0, 0, c.scale, c.scale)) {
                ESP_LOGE(TAG, "Failed to draw character %s", path.c_str());
            }
            continue;
        }

        // 合成の控えが使えるなら貼るだけで済ませる
        if (drawCharaCached(target, c, layerDefs)) {
            continue;
        }

        // レイヤー方式。**配列の順に描く**（先が奥）。
        //
        // 透過はここでの直描きが背景と正しく混ぜてくれる。
        // スプライトへ展開して重ねると透過が色キー1色に落ちるので、
        // 反転や回転が要る場合も素材側で用意すること。
        const cJSON* layer = nullptr;
        cJSON_ArrayForEach(layer, layerDefs) {
            const std::string name = getString(layer, "name");
            if (name.empty()) {
                continue;
            }

            const auto it = c.layers.find(name);
            const std::string variant = (it != c.layers.end())
                                            ? it->second
                                            : ScenarioLoader::defaultVariant(layer);

            std::string path;
            if (!_loader->resolveLayerPath(c.id.c_str(), name.c_str(),
                                           variant.c_str(), path)) {
                ESP_LOGE(TAG, "Layer '%s/%s/%s' is not in assets",
                         c.id.c_str(), name.c_str(), variant.c_str());
                continue;
            }

            // レイヤーの位置はキャラの左上からの相対。
            // 倍率もオフセットに掛けないと、拡大したとき目や口がずれる。
            const int lx = c.x + static_cast<int>(getInt(layer, "x", 0) * c.scale);
            const int ly = c.y + static_cast<int>(getInt(layer, "y", 0) * c.scale);

            if (!target->drawPngFile(&SD, path.c_str(), lx, ly,
                                     0, 0, 0, 0, c.scale, c.scale)) {
                ESP_LOGE(TAG, "Failed to draw layer %s", path.c_str());
            }
        }
    }

    // 前面の一枚絵は立ち絵より手前。イベントCGを想定している。
    if (_foreground.visible && !_foreground.image.empty()) {
        std::string path;
        if (_loader->resolveBackgroundPath(_foreground.image.c_str(), path)) {
            if (!target->drawPngFile(&SD, path.c_str(), _foreground.x, _foreground.y,
                                     0, 0, 0, 0,
                                     _foreground.scale, _foreground.scale)) {
                ESP_LOGE(TAG, "Failed to draw the foreground image %s", path.c_str());
            }
        }
    }

    target->endWrite();
}

ScenarioPlayer::CmdResult ScenarioPlayer::executeChara(const cJSON* cmd)
{
    const std::string id = getString(cmd, "id");
    if (id.empty()) {
        ESP_LOGE(TAG, "chara has no id");
        return CmdResult::Next;
    }

    const bool visible = getBool(cmd, "visible", true);

    CharaState* existing = findChara(id);

    if (!visible) {
        // 消す指定。表示していなければ何もしない。
        if (existing) {
            existing->visible = false;
            ESP_LOGI(TAG, "chara '%s' hidden", id.c_str());
        }
        renderStage(_display);
        markRefresh();   // 背景から描き直したので全画面
        return CmdResult::Next;
    }

    const cJSON* layerDefs = _loader->characterLayers(id.c_str());

    // 変更前の位置と倍率を控える。
    //
    // **位置か倍率が変わったら、控えは使えない。**
    // 控えは「その矩形だけ」を貼るので、元いた場所の画素が残ってしまう。
    // 背景を描き直せるのは renderStage() だけ。
    const bool isNew = (existing == nullptr);
    const int prevX = existing ? existing->x : 0;
    const int prevY = existing ? existing->y : 0;
    const float prevScale = existing ? existing->scale : 1.0f;

    if (existing) {
        // 同じ id を再指定したら差し替える（新しく積まない）。
        // 表情だけ変えたいときに位置を書かなくて済むよう、
        // 省略された項目は今の値を残す。
        existing->expression = getString(cmd, "expression", existing->expression.c_str());
        existing->x = getInt(cmd, "x", existing->x);
        existing->y = getInt(cmd, "y", existing->y);
        existing->scale = getFloat(cmd, "scale", existing->scale);
        existing->visible = true;
    } else {
        CharaState c;
        c.id = id;
        c.expression = getString(cmd, "expression", "normal");
        c.x = getInt(cmd, "x", 0);
        c.y = getInt(cmd, "y", 0);
        c.scale = getFloat(cmd, "scale", 1.0f);
        c.visible = true;

        // 初めて出すときは全レイヤーを既定の差分で埋めておく。
        // 埋めないと、書かれなかったレイヤーが描かれない。
        if (layerDefs) {
            const cJSON* layer = nullptr;
            cJSON_ArrayForEach(layer, layerDefs) {
                const std::string name = getString(layer, "name");
                if (!name.empty()) {
                    c.layers[name] = ScenarioLoader::defaultVariant(layer);
                }
            }
        }

        _charas.push_back(c);
        existing = &_charas.back();
    }

    // 書かれたレイヤーだけ差し替える。
    // 書かなかったものは今の差分のまま（「目だけ変える」を書けるように）。
    if (layerDefs) {
        const cJSON* wanted = cJSON_GetObjectItemCaseSensitive(cmd, "layers");
        const cJSON* item = nullptr;
        cJSON_ArrayForEach(item, wanted) {
            if (!item->string || !cJSON_IsString(item) || !item->valuestring) {
                continue;
            }
            if (!ScenarioLoader::findLayer(layerDefs, item->string)) {
                // レイヤー名の書き間違い。気づけるように残す。
                ESP_LOGW(TAG, "chara '%s' has no layer '%s'",
                         id.c_str(), item->string);
                continue;
            }
            existing->layers[item->string] = item->valuestring;
        }
    }

    if (layerDefs) {
        ESP_LOGI(TAG, "chara '%s' at (%d,%d) with %u layer(s)",
                 existing->id.c_str(), existing->x, existing->y,
                 static_cast<unsigned>(existing->layers.size()));
    } else {
        ESP_LOGI(TAG, "chara '%s/%s' at (%d,%d)",
                 existing->id.c_str(), existing->expression.c_str(),
                 existing->x, existing->y);
    }

    // **この立ち絵の矩形だけ貼り直せるなら、背景は描き直さない。**
    //
    // renderStage() は毎回そこから背景 PNG を読む。実測で 900ms あり、
    // 表情を変えるだけの場面では描画時間の 86% がそれだった。
    // 控えが使えるなら、下地は既にスプライトの中にある。
    //
    // **動かした・拡大縮小したときは使えない。**
    // 貼るのは新しい矩形だけなので、元いた場所に前の姿が残る。
    // その場合は従来どおり全部描き直す。
    const bool moved = !isNew && (existing->x != prevX ||
                                  existing->y != prevY ||
                                  existing->scale != prevScale);

    if (moved || !layerDefs || !drawCharaCached(_display, *existing, layerDefs)) {
        renderStage(_display);
    }

    markRefresh();
    return CmdResult::Next;
}

ScenarioPlayer::CmdResult ScenarioPlayer::executeBackground(const cJSON* cmd)
{
    const std::string image = getString(cmd, "image");

    std::string path;
    if (!_loader->resolveBackgroundPath(image.c_str(), path)) {
        ESP_LOGE(TAG, "Background '%s' is not defined in assets", image.c_str());
        return CmdResult::Next;
    }

    // 背景を覚えておく。
    // 立ち絵を出し入れするたびに背景から描き直すので、
    // 「今どの背景か」を保持しておく必要がある。
    _currentBackground = image;
    _backgroundX = getInt(cmd, "x", 0);
    _backgroundY = getInt(cmd, "y", 0);
    _backgroundScale = getFloat(cmd, "scale", 1.0f);

    const std::string transitionName = getString(cmd, "transition", "NONE");
    bool known = false;
    const SimpleTransitionType type = parseTransition(transitionName, known);
    if (!known) {
        ESP_LOGW(TAG, "Unknown transition '%s'. Drawing without effect",
                 transitionName.c_str());
    }

    // 演出なし、またはトランジションが使えない場合は直接描く
    if (!known || type == SimpleTransitionType::NONE || !_transition) {
        // 背景だけでなく立ち絵も一緒に描き直す。
        // 背景だけ描くと、表示中の立ち絵が消えてしまう。
        renderStage(_display);
        markRefresh();
        return CmdResult::Next;
    }

    // 演出つき。キャンバスに描いてから遷移させる。
    M5Canvas* canvas = _transition->getMainCanvas();
    if (!canvas) {
        ESP_LOGE(TAG, "Transition canvas unavailable. Drawing directly");
        renderStage(_display);
        markRefresh();
        return CmdResult::Next;
    }

    renderStage(canvas);

    // **溜めた分をここで出しておく。**
    // このあとは SimpleTransition が自分でパネルを動かす。
    // 開いたままにすると、遷移が終わったあとで
    // 遷移前の更新範囲が積まれ、一瞬前の絵に戻る。
    _needRefresh = false;
    endBatch();

    _transition->startTransition(type, 16);
    ESP_LOGI(TAG, "bg '%s' with transition %s", image.c_str(), transitionName.c_str());

    // 遷移の完了は onTransitionFinished() が受け、そこから run() が再開する
    return CmdResult::NextAndWaitTransition;
}

ScenarioPlayer::CmdResult ScenarioPlayer::executeSet(const cJSON* cmd)
{
    const std::string name = getString(cmd, "var");
    const auto it = _variables.find(name);
    if (it == _variables.end()) {
        // 宣言されていない変数への代入は受け付けない。
        // 黙って作れてしまうとタイプミスに気づけないため。
        ESP_LOGE(TAG, "set to undeclared variable '%s' (declare it in 'variables')",
                 name.c_str());
        return CmdResult::Next;
    }

    Value& target = it->second;
    const Value operand = valueFromJson(cJSON_GetObjectItemCaseSensitive(cmd, "value"));
    const std::string op = getString(cmd, "op", "=");

    if (op == "=") {
        if (target.type != operand.type) {
            ESP_LOGW(TAG, "set '%s': type changes from the declared one", name.c_str());
        }
        target = operand;
    } else if (op == "+=" || op == "-=") {
        if (target.type != Value::Type::Number || operand.type != Value::Type::Number) {
            ESP_LOGE(TAG, "set '%s': '%s' works on numbers only", name.c_str(), op.c_str());
            return CmdResult::Next;
        }
        target.numberValue += (op == "+=") ? operand.numberValue : -operand.numberValue;
    } else {
        ESP_LOGW(TAG, "set '%s': unknown operator '%s' (ignored)", name.c_str(), op.c_str());
        return CmdResult::Next;
    }

    ESP_LOGI(TAG, "set %s = %s", name.c_str(), target.toString().c_str());
    return CmdResult::Next;
}

ScenarioPlayer::CmdResult ScenarioPlayer::executeIf(const cJSON* cmd)
{
    const cJSON* cond = cJSON_GetObjectItemCaseSensitive(cmd, "cond");
    const bool taken = evaluateCondition(cond);

    const cJSON* branch = cJSON_GetObjectItemCaseSensitive(cmd, taken ? "then" : "else");

    ESP_LOGI(TAG, "if -> %s", taken ? "then" : "else");

    if (!cJSON_IsArray(branch) || cJSON_GetArraySize(const_cast<cJSON*>(branch)) == 0) {
        // 進む先が無い（else 省略など）。この if は素通りする。
        return CmdResult::Next;
    }

    // 選んだ側の配列に入る。
    // 使い切ったら run() が pop して、この if の次から再開する。
    _frames.push_back(Frame{branch, 0});
    return CmdResult::Pushed;
}

ScenarioPlayer::CmdResult ScenarioPlayer::executeRandom(const cJSON* cmd)
{
    const std::string name = getString(cmd, "var");
    const auto it = _variables.find(name);
    if (it == _variables.end()) {
        ESP_LOGE(TAG, "random writes to undeclared variable '%s'", name.c_str());
        return CmdResult::Next;
    }
    if (it->second.type != Value::Type::Number) {
        ESP_LOGE(TAG, "random needs a number variable, but '%s' is not", name.c_str());
        return CmdResult::Next;
    }

    const int lo = getInt(cmd, "min", 0);
    const int hi = getInt(cmd, "max", 100);
    if (hi < lo) {
        ESP_LOGE(TAG, "random: max(%d) is less than min(%d)", hi, lo);
        return CmdResult::Next;
    }

    // esp_random() はハードウェア乱数。種を蒔く必要がない。
    const uint32_t span = static_cast<uint32_t>(hi - lo) + 1;
    it->second.numberValue = lo + static_cast<double>(esp_random() % span);

    ESP_LOGI(TAG, "random %s = %g (%d..%d)",
             name.c_str(), it->second.numberValue, lo, hi);
    return CmdResult::Next;
}

ScenarioPlayer::CmdResult ScenarioPlayer::executeCall(const cJSON* cmd)
{
    const std::string target = getString(cmd, "scene");

    if (_callStack.size() >= MAX_CALL_DEPTH) {
        // 手書きの JSON が互いを呼び合うと止まらなくなる。
        ESP_LOGE(TAG, "call is nested more than %u deep. Ignoring "
                      "(check for scenes calling each other)",
                 static_cast<unsigned>(MAX_CALL_DEPTH));
        return CmdResult::Next;
    }

    // 戻り先を積む。`call` の**次**から再開したいので、位置を進めてから積む。
    if (Frame* frame = currentFrame()) {
        ++frame->index;
    }

    CallSite site;
    site.sceneId = _sceneId;
    site.frames = _frames;
    site.pageOffset = _pageOffset;
    _callStack.push_back(site);

    if (!gotoScene(target)) {
        ESP_LOGE(TAG, "call to undefined scene '%s'", target.c_str());
        _callStack.pop_back();
        return CmdResult::Next;
    }

    ESP_LOGI(TAG, "call -> '%s' (depth %u)",
             target.c_str(), static_cast<unsigned>(_callStack.size()));
    return CmdResult::Jumped;
}

ScenarioPlayer::CmdResult ScenarioPlayer::executeReturn(const cJSON* cmd)
{
    (void)cmd;

    if (_callStack.empty()) {
        // call されていないのに return された。
        // 進みようがないのでシーンを終わらせる。
        ESP_LOGW(TAG, "return without a matching call. Ending the scene");
        return CmdResult::Finished;
    }

    const CallSite site = _callStack.back();
    _callStack.pop_back();

    // 呼び出し元のシーンと位置を戻す。
    // gotoScene() は _frames を作り直してしまうので使わない。
    const cJSON* scene = _loader->findScene(site.sceneId);
    if (!scene) {
        ESP_LOGE(TAG, "The scene that called us ('%s') no longer exists",
                 site.sceneId.c_str());
        return CmdResult::Finished;
    }

    _sceneId = site.sceneId;
    _scene = scene;
    _frames = site.frames;
    _pageOffset = site.pageOffset;

    ESP_LOGI(TAG, "return -> '%s' (depth %u)",
             _sceneId.c_str(), static_cast<unsigned>(_callStack.size()));
    return CmdResult::Jumped;
}

ScenarioPlayer::CmdResult ScenarioPlayer::executeImage(const cJSON* cmd)
{
    // clear: true で消す
    if (getBool(cmd, "clear", false)) {
        _foreground.visible = false;
        _foreground.image.clear();
        ESP_LOGI(TAG, "image cleared");
        renderStage(_display);
        markRefresh();
        return CmdResult::Next;
    }

    const std::string image = getString(cmd, "image");
    std::string path;
    if (!_loader->resolveBackgroundPath(image.c_str(), path)) {
        ESP_LOGE(TAG, "Image '%s' is not defined in assets.backgrounds", image.c_str());
        return CmdResult::Next;
    }

    _foreground.image = image;
    _foreground.x = getInt(cmd, "x", 0);
    _foreground.y = getInt(cmd, "y", 0);
    _foreground.scale = getFloat(cmd, "scale", 1.0f);
    _foreground.visible = true;

    ESP_LOGI(TAG, "image '%s' at (%d,%d)",
             image.c_str(), _foreground.x, _foreground.y);

    renderStage(_display);
    markRefresh();
    return CmdResult::Next;
}

ScenarioPlayer::CmdResult ScenarioPlayer::executeSuspend(const cJSON* cmd)
{
    const int slot = getInt(cmd, "slot", 0);

    ESP_LOGI(TAG, "Suspending (slot %d)", slot);

    // **溜めた描画をここで出し切る。**
    // このあとは残像消去としおり画面を自分でパネルへ出し、
    // 走査の完了を待ってから電源を切る。溜めたままだと
    // 電源が落ちるまでに出せず、直前の画面が残る。
    _needRefresh = false;
    endBatch();

    // 1. 控えが無いときのために、実行位置を1つ進めておく。
    //
    //    run() は「コマンドを実行 → 添字を進める」の順なので、
    //    ここでの位置は **この suspend コマンド自身** を指している。
    //    そのまま保存すると、再開した瞬間に suspend をもう一度実行し、
    //    しおり画面を描いて電源を切る、を繰り返して操作できなくなる。
    //
    //    `checkpoint` があればそちらが書かれるので、ここは触らない。
    //    電源を切る直前なので、状態を書き換えて構わない。
    if (!_checkpoint) {
        ++currentFrame()->index;
    }

    // 2. 中断位置を保存する。
    //    描画より先に保存するのは、状態が変わらないうちに
    //    確実に書き残しておくため。
    //
    //    **必ずここで解決した slot を渡すこと。**
    //    以前は cmd をそのまま executeSave() へ渡しており、
    //    `slot` を省いたシナリオでは save 側の既定値 1 が使われる一方、
    //    栞には suspend 側の既定値 0 が記録されていた。
    //    保存先と栞の指す先がずれ、再開すると必ず冒頭から始まっていた。
    saveToSlot(slot);

    // 周回をまたぐ変数もここで書き出す。
    // 電源が落ちるので、書き残せる機会はここが最後。
    savePersistent();

    // 3. 「続きから」の情報を残す
    settings.setLastScenario(_loader->scenarioId().c_str());
    settings.setResumeSlot(slot);
    if (!settings.save()) {
        // USB MSC 中や SD 無しでは残せない。
        // 画面は出せるので、そのまま電源は切る。
        ESP_LOGW(TAG, "Could not record the resume point");
    }

    // 4. しおりになる画面を作る。
    //
    //    電子ペーパーは電源を切っても像が残るので、
    //    ここで描いたものが次に電源を入れるまで見え続ける。
    //
    //    先に clearGhosting() で全画素を振り切っておく。
    //    これをやらないと、電源を切った後に像が薄くなる
    //    （粒子が端まで動ききらないため）。
    SimpleTransition::clearGhosting(_display);

    const std::string image = getString(cmd, "image");
    if (!image.empty()) {
        std::string path;
        if (_loader->resolveBackgroundPath(image.c_str(), path)) {
            _display->fillScreen(TFT_BLACK);
            _display->drawPngFile(&SD, path.c_str(), 0, 0);
        } else {
            ESP_LOGW(TAG, "Suspend image '%s' is not in assets", image.c_str());
        }
    } else {
        _display->fillScreen(TFT_BLACK);
    }

    const std::string message = interpolate(getString(cmd, "message"));
    if (!message.empty() && _vertical) {
        _vertical->drawTextPaged(message, 0);
    }

    // 5. 走査を終わらせてから電源を切る。
    //    待ちが短いと画面上部に横線が残る（SystemMenu の shutdown と同じ理由）。
    SimpleTransition::refreshScreen(_display);
    _display->waitDisplay();
    vTaskDelay(pdMS_TO_TICKS(SUSPEND_SETTLE_MS));

    power.powerOff();

    // ここへ戻るのは電源が落ちなかったとき（USB 給電中など）。
    // しおりは残っているので、シナリオとしては終わらせてメニューへ返す。
    ESP_LOGW(TAG, "Power off did not take effect. Ending the scenario instead");
    return CmdResult::Finished;
}

std::string ScenarioPlayer::savePath(int slot) const
{
    char name[32];
    if (slot <= 0) {
        snprintf(name, sizeof(name), "auto.json");
    } else {
        snprintf(name, sizeof(name), "slot%02d.json", slot);
    }
    return _loader->basePath() + "/saves/" + name;
}

ScenarioPlayer::CmdResult ScenarioPlayer::executeSave(const cJSON* cmd)
{
    saveToSlot(getInt(cmd, "slot", 1));
    return CmdResult::Next;
}

cJSON* ScenarioPlayer::buildStateObject() const
{
    // `if` の入れ子の中で控えると、再開位置が復元できない。
    //
    // 実行位置はフレームのスタックで持っており、残せるのは底の位置だけ。
    // 底は `if` コマンド自身を指しているので、そこから再開すると
    // 条件が再評価され、分岐の中を**もう一度頭から実行**することになる。
    // その中に `set` の "+=" があれば二重に効いてしまう。
    if (_frames.size() > 1) {
        ESP_LOGW(TAG, "Recording a state inside an 'if' block. The branch will be re-run "
                      "on load. Move the save/checkpoint out of the 'if' to avoid "
                      "double effects");
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create the state object");
        return nullptr;
    }

    cJSON_AddNumberToObject(root, "format_version", 1);
    cJSON_AddStringToObject(root, "scenario_id", _loader->scenarioId().c_str());
    cJSON_AddStringToObject(root, "scenario_version", _loader->version().c_str());
    cJSON_AddStringToObject(root, "scene", _sceneId.c_str());
    cJSON_AddNumberToObject(root, "command_index", _frames.empty() ? 0 : _frames[0].index);

    // 時計を合わせる仕組みがまだ無いので、起動からの経過時間を入れておく。
    // RTC を使うようになったら実時刻に差し替える。
    cJSON_AddNumberToObject(root, "uptime_ms",
                            static_cast<double>(esp_timer_get_time() / 1000));

    // 変数
    cJSON* vars = cJSON_AddObjectToObject(root, "variables");
    for (const auto& kv : _variables) {
        const Value& v = kv.second;
        switch (v.type) {
        case Value::Type::Bool:
            cJSON_AddBoolToObject(vars, kv.first.c_str(), v.boolValue);
            break;
        case Value::Type::Number:
            cJSON_AddNumberToObject(vars, kv.first.c_str(), v.numberValue);
            break;
        case Value::Type::String:
            cJSON_AddStringToObject(vars, kv.first.c_str(), v.stringValue.c_str());
            break;
        }
    }

    // 画面の状態。これが無いと、再開したとき背景も立ち絵も消えた画面になる。
    cJSON* stage = cJSON_AddObjectToObject(root, "stage");
    cJSON_AddStringToObject(stage, "background", _currentBackground.c_str());
    cJSON_AddNumberToObject(stage, "background_x", _backgroundX);
    cJSON_AddNumberToObject(stage, "background_y", _backgroundY);
    cJSON_AddNumberToObject(stage, "background_scale", _backgroundScale);

    cJSON* fg = cJSON_AddObjectToObject(stage, "foreground");
    cJSON_AddStringToObject(fg, "image", _foreground.image.c_str());
    cJSON_AddNumberToObject(fg, "x", _foreground.x);
    cJSON_AddNumberToObject(fg, "y", _foreground.y);
    cJSON_AddNumberToObject(fg, "scale", _foreground.scale);
    cJSON_AddBoolToObject(fg, "visible", _foreground.visible);

    cJSON* charas = cJSON_AddArrayToObject(stage, "charas");
    for (const CharaState& c : _charas) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", c.id.c_str());
        cJSON_AddStringToObject(item, "expression", c.expression.c_str());
        cJSON_AddNumberToObject(item, "x", c.x);
        cJSON_AddNumberToObject(item, "y", c.y);
        cJSON_AddNumberToObject(item, "scale", c.scale);
        cJSON_AddBoolToObject(item, "visible", c.visible);

        // レイヤー方式のときだけ書く。
        // 単一画像方式のセーブに空のオブジェクトが増えないようにする。
        if (!c.layers.empty()) {
            cJSON* layers = cJSON_AddObjectToObject(item, "layers");
            for (const auto& kv : c.layers) {
                cJSON_AddStringToObject(layers, kv.first.c_str(), kv.second.c_str());
            }
        }

        cJSON_AddItemToArray(charas, item);
    }

    return root;
}

bool ScenarioPlayer::writeStateObject(cJSON* root, int slot)
{
    if (!root) {
        return false;
    }

    // スロット番号は状態の一部ではない。
    // 同じ控えを別のスロットへ書けるよう、書き出す直前に足す。
    cJSON_AddNumberToObject(root, "slot", slot);

    // 人が読んで直せるよう整形する。USB MSC で PC から覗かれる前提。
    char* text = cJSON_Print(root);
    const std::string scene = getString(root, "scene");
    const int index = getInt(root, "command_index", 0);
    cJSON_Delete(root);

    if (!text) {
        ESP_LOGE(TAG, "Failed to serialize the save data");
        return false;
    }

    // 置き場が無ければ作る。既にあれば mkdir は失敗するが、それでよい。
    SD.mkdir((_loader->basePath() + "/saves").c_str());

    const std::string path = savePath(slot);
    const bool ok = SD.writeFileFromBuffer(path.c_str(), text, strlen(text));
    free(text);

    if (ok) {
        ESP_LOGI(TAG, "Saved to %s (scene='%s', index=%d)",
                 path.c_str(), scene.c_str(), index);
    } else {
        // USB MSC 中や SD 無しでは保存できない。
        // 物語は続けられるので再生は止めない。
        ESP_LOGW(TAG, "Failed to save to %s. Continuing anyway", path.c_str());
    }

    return ok;
}

bool ScenarioPlayer::saveToSlot(int slot)
{
    // 控えがあればそちらを書く。無ければ今の状態を組む。
    //
    // `checkpoint` が置かれていれば「どの状態を保存するか」は作者が決めている。
    // ここで実行中の状態を書いてしまうと、その意図を無視することになる。
    cJSON* root = _checkpoint ? cJSON_Duplicate(_checkpoint, true)
                              : buildStateObject();

    if (_checkpoint && !root) {
        ESP_LOGE(TAG, "Failed to copy the checkpoint");
        return false;
    }

    return writeStateObject(root, slot);
}

void ScenarioPlayer::releaseCheckpoint()
{
    if (_checkpoint) {
        cJSON_Delete(_checkpoint);
        _checkpoint = nullptr;
    }
}

ScenarioPlayer::CmdResult ScenarioPlayer::executeCheckpoint(const cJSON* cmd)
{
    releaseCheckpoint();

    if (getBool(cmd, "clear", false)) {
        ESP_LOGI(TAG, "Checkpoint cleared. Saves will use the live state again");
        return CmdResult::Next;
    }

    _checkpoint = buildStateObject();
    if (!_checkpoint) {
        // 控えられなくても物語は続けられる。
        // 以後のセーブは実行中の状態になる。
        ESP_LOGE(TAG, "Failed to record the checkpoint");
        return CmdResult::Next;
    }

    ESP_LOGI(TAG, "Checkpoint recorded (scene='%s', index=%d)",
             _sceneId.c_str(), _frames.empty() ? 0 : _frames[0].index);
    return CmdResult::Next;
}

ScenarioPlayer::CmdResult ScenarioPlayer::executeLoad(const cJSON* cmd)
{
    if (!loadFromSlot(getInt(cmd, "slot", 1))) {
        return CmdResult::Next;
    }
    // 位置が変わったので jump と同じ扱いにする
    return CmdResult::Jumped;
}

bool ScenarioPlayer::loadFromSlot(int slot)
{
    const std::string path = savePath(slot);

    size_t len = 0;
    char* text = SD.readFileToBuffer(path.c_str(), &len);
    if (!text) {
        ESP_LOGW(TAG, "No save data at %s. Continuing without loading", path.c_str());
        return false;
    }

    cJSON* root = cJSON_ParseWithLength(text, len);
    free(text);

    if (!root) {
        ESP_LOGE(TAG, "Save data at %s is malformed", path.c_str());
        return false;
    }

    const std::string savedId = getString(root, "scenario_id");
    if (!savedId.empty() && savedId != _loader->scenarioId()) {
        // 別のシナリオのセーブ。読み込むと確実に破綻するので断る。
        ESP_LOGE(TAG, "Save belongs to '%s', not '%s'. Ignored",
                 savedId.c_str(), _loader->scenarioId().c_str());
        cJSON_Delete(root);
        return false;
    }

    const std::string savedVersion = getString(root, "scenario_version");
    if (savedVersion != _loader->version()) {
        // シーンIDやコマンド位置がずれている可能性がある。
        // 読み込みは試みるが、位置がおかしければ後段で丸める。
        ESP_LOGW(TAG, "Save was made with version '%s' but the scenario is '%s'. "
                      "The resume position may be off",
                 savedVersion.c_str(), _loader->version().c_str());
    }

    // --- 変数 ---
    //
    // 宣言に無いキーは捨てる。シナリオ側で変数を消したときに、
    // 古いセーブの残骸が復活しないようにするため。
    const cJSON* vars = cJSON_GetObjectItemCaseSensitive(root, "variables");
    if (cJSON_IsObject(vars)) {
        const cJSON* item = nullptr;
        cJSON_ArrayForEach(item, vars) {
            if (!item->string) {
                continue;
            }
            auto it = _variables.find(item->string);
            if (it == _variables.end()) {
                ESP_LOGW(TAG, "Save has an unknown variable '%s'. Dropped", item->string);
                continue;
            }
            it->second = valueFromJson(item);
        }
    }

    // --- 画面の状態 ---
    _charas.clear();
    _currentBackground.clear();
    _backgroundX = 0;
    _backgroundY = 0;
    _foreground = ForegroundState{};

    // call の途中でセーブされていた場合に備え、戻り先も捨てる。
    // 保存していないので、ロード後は呼び出し元へ戻れない。
    _callStack.clear();

    const cJSON* stage = cJSON_GetObjectItemCaseSensitive(root, "stage");
    if (cJSON_IsObject(stage)) {
        _currentBackground = getString(stage, "background");
        _backgroundX = getInt(stage, "background_x", 0);
        _backgroundY = getInt(stage, "background_y", 0);
        _backgroundScale = getFloat(stage, "background_scale", 1.0f);

        _foreground = ForegroundState{};
        const cJSON* fg = cJSON_GetObjectItemCaseSensitive(stage, "foreground");
        if (cJSON_IsObject(fg)) {
            _foreground.image = getString(fg, "image");
            _foreground.x = getInt(fg, "x", 0);
            _foreground.y = getInt(fg, "y", 0);
            _foreground.scale = getFloat(fg, "scale", 1.0f);
            _foreground.visible = getBool(fg, "visible", false);
        }

        const cJSON* charas = cJSON_GetObjectItemCaseSensitive(stage, "charas");
        const cJSON* c = nullptr;
        cJSON_ArrayForEach(c, charas) {
            CharaState s;
            s.id = getString(c, "id");
            s.expression = getString(c, "expression", "normal");
            s.x = getInt(c, "x", 0);
            s.y = getInt(c, "y", 0);
            s.scale = getFloat(c, "scale", 1.0f);
            s.visible = getBool(c, "visible", true);

            // レイヤーの状態。**無ければ空**なので、
            // レイヤー方式より前に作った古いセーブもそのまま読める。
            const cJSON* layers = cJSON_GetObjectItemCaseSensitive(c, "layers");
            const cJSON* kv = nullptr;
            cJSON_ArrayForEach(kv, layers) {
                if (kv->string && cJSON_IsString(kv) && kv->valuestring) {
                    s.layers[kv->string] = kv->valuestring;
                }
            }

            if (!s.id.empty()) {
                _charas.push_back(s);
            }
        }
    }

    // --- 再開位置 ---
    const std::string scene = getString(root, "scene");
    const int index = getInt(root, "command_index", 0);
    cJSON_Delete(root);

    if (!gotoScene(scene)) {
        ESP_LOGW(TAG, "Saved scene '%s' no longer exists. Starting over",
                 scene.c_str());
        if (!gotoScene(_loader->startSceneId())) {
            return false;
        }
    } else {
        // シナリオが更新されてコマンド数が減っていることがある。
        // 範囲外なら先頭へ丸める（gotoScene が 0 にしてあるのでそのまま）。
        const int count = cJSON_GetArraySize(
            const_cast<cJSON*>(_frames[0].commands));
        if (index >= 0 && index < count) {
            _frames[0].index = index;
        } else {
            ESP_LOGW(TAG, "Saved index %d is out of range (%d commands). Starting the scene over",
                     index, count);
        }
    }

    // 画面を復元する
    renderStage(_display);
    markRefresh();

    ESP_LOGI(TAG, "Loaded %s (scene='%s', index=%d, charas=%u)",
             path.c_str(), _sceneId.c_str(), _frames[0].index,
             static_cast<unsigned>(_charas.size()));

    // 位置が変わったので jump と同じ扱いにする
    return true;
}

ScenarioPlayer::CmdResult ScenarioPlayer::executeChoice(const cJSON* cmd)
{
    _choiceLabels.clear();
    _choiceTargets.clear();
    _choiceEnabled.clear();
    _choicePrompt = interpolate(getString(cmd, "prompt"));

    const cJSON* options = cJSON_GetObjectItemCaseSensitive(cmd, "options");
    if (!cJSON_IsArray(options)) {
        ESP_LOGE(TAG, "choice has no options array");
        return CmdResult::Next;
    }

    const cJSON* opt = nullptr;
    cJSON_ArrayForEach(opt, options) {
        const cJSON* cond = cJSON_GetObjectItemCaseSensitive(opt, "cond");
        const bool available = evaluateCondition(cond);

        // 条件を満たさない選択肢は既定で隠す。
        // hide_if_false: false なら残して、呼び出し側が灰色で出す。
        if (!available && getBool(opt, "hide_if_false", true)) {
            continue;
        }

        _choiceLabels.push_back(interpolate(getString(opt, "label")));
        _choiceTargets.push_back(getString(opt, "next"));
        _choiceEnabled.push_back(available);
    }

    if (_choiceLabels.empty()) {
        // 全部が条件で消えた。ここで止まると先へ進めなくなるので、
        // 警告を出してシーンの next へ流す。
        ESP_LOGW(TAG, "choice in scene '%s' has no available option. Skipping",
                 _sceneId.c_str());
        return CmdResult::Next;
    }

    ESP_LOGI(TAG, "choice: %u option(s)", static_cast<unsigned>(_choiceLabels.size()));
    return CmdResult::NextAndWaitChoice;
}

void ScenarioPlayer::selectChoice(size_t index)
{
    if (_state != State::WaitingChoice) {
        return;
    }
    if (index >= _choiceTargets.size()) {
        ESP_LOGE(TAG, "selectChoice(%u) is out of range", static_cast<unsigned>(index));
        return;
    }
    if (!_choiceEnabled[index]) {
        // 条件を満たしていない選択肢。押されても進めない。
        ESP_LOGW(TAG, "Choice %u is disabled", static_cast<unsigned>(index));
        return;
    }

    const std::string target = _choiceTargets[index];
    ESP_LOGI(TAG, "Choice %u ('%s') -> scene '%s'",
             static_cast<unsigned>(index), _choiceLabels[index].c_str(), target.c_str());

    _choiceLabels.clear();
    _choiceTargets.clear();
    _choiceEnabled.clear();
    _choicePrompt.clear();

    // **ボタンと問いかけを消す。**
    //
    // 描いたのは main.cpp だが、その下に何があったかを知っているのはこちら。
    // 以前は呼び出し側が画面全体を黒く塗っていたが、それでは背景も立ち絵も
    // 消えてしまい、飛び先のシーンが `bg` を書き直していないと
    // 真っ黒な画面に本文だけが乗ることになっていた。
    //
    // **バッチは開けたまま次のシーンへ渡す。**
    // 復元と飛び先の描画がひとつの更新にまとまり、
    // 「黒くなってから描き直す」ちらつきが出ない。
    beginBatch();
    restoreStageAndText();
    markRefresh();

    if (!gotoScene(target)) {
        ESP_LOGE(TAG, "Choice leads to undefined scene '%s'", target.c_str());
        _state = State::Finished;
        flushScreen();
        return;
    }

    run();
}

void ScenarioPlayer::tickTyping()
{
    if (_state != State::Typing || !_typingWriter) {
        return;
    }

    const int64_t nowMs = esp_timer_get_time() / 1000;
    if (nowMs - _lastTypedMs < _typingSpeedMs) {
        return;
    }
    _lastTypedMs = nowMs;

    ++_typedChars;

    const bool done = (_typedChars >= _typingPageChars);
    const size_t limit = done ? 0 : _typedChars;   // 0 は「制限なし」

    // 1文字増やすたびに下地から敷き直す。
    // 前の文字を消さずに重ねると、送りが進むほど滲んでいく。
    //
    // 敷き直しと描画で2回出さないよう、ここも溜めてから出す。
    beginBatch();
    fillTextBoxBackground(_typingBoxName, _typingWriter);

    const TypoWrite::DrawResult result =
        _typingWriter->drawTextPaged(_typingBody, _pageOffset, limit);

    if (!done) {
        // 途中は最速モードで出す。遅さが致命的になるため画質は諦める。
        markRefresh(lgfx::v1::epd_mode_t::epd_fastest);
        flushScreen();
        return;
    }

    // 出し切った。最速モードのままだと薄いので、本来のモードで描き直して定着させる。
    _display->setEpdMode(lgfx::v1::epd_mode_t::epd_quality);
    markRefresh();
    flushScreen();

    _typingWriter = nullptr;
    _typingBody.clear();
    _typingBoxName.clear();

    ESP_LOGI(TAG, "text: typing done (offset %u -> %u, hasMore=%d)",
             static_cast<unsigned>(_pageOffset),
             static_cast<unsigned>(result.nextOffset),
             static_cast<int>(result.hasMore));

    if (result.hasMore) {
        // まだページが残っている。タップで次のページへ。
        _pageOffset = result.nextOffset;
        _state = State::WaitingTap;
        return;
    }

    _pageOffset = 0;

    if (_typingWaitAfter) {
        // 次のコマンドへ進めてからタップ待ちにする。
        // run() を通さずここで位置を進めるのは、
        // このコマンドが StayAndType で返って位置が据え置かれているため。
        if (Frame* frame = currentFrame()) {
            ++frame->index;
        }
        _state = State::WaitingTap;
        return;
    }

    if (Frame* frame = currentFrame()) {
        ++frame->index;
    }
    run();
}

void ScenarioPlayer::tickWait()
{
    if (_state != State::Waiting) {
        return;
    }
    if (esp_timer_get_time() / 1000 < _waitUntilMs) {
        return;
    }
    run();
}

void ScenarioPlayer::onTap()
{
    // `wait` の途中。skippable なら待たずに進む。
    if (_state == State::Waiting) {
        if (_waitSkippable) {
            ESP_LOGI(TAG, "wait skipped by tap");
            run();
        }
        return;
    }

    // `end` の message を表示中。タップで本当に終わる。
    if (_state == State::WaitingTap && _endPending) {
        _endPending = false;
        _state = State::Finished;
        return;
    }

    // 文字送りの途中でタップされたら、待たずに全文を出す。
    // これが無いと、長い台詞で遅さが致命的になる。
    if (_state == State::Typing) {
        ESP_LOGI(TAG, "text: typing skipped by tap");
        _typedChars = _typingPageChars;
        _lastTypedMs = 0;   // 次の tickTyping() で即座に完了させる
        tickTyping();
        return;
    }

    if (_state != State::WaitingTap) {
        return;
    }
    run();
}

void ScenarioPlayer::onTransitionFinished()
{
    if (_state != State::WaitingTransition) {
        return;
    }
    run();
}
