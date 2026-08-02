// main/ScenarioPlayer.cpp - シナリオのシーンとコマンドを実行する

#include "ScenarioPlayer.hpp"

#include "Buzzer.hpp"
#include "SDcard.hpp"
#include "esp_log.h"
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

    const cJSON* vars = _loader->variablesNode();
    if (!cJSON_IsObject(vars)) {
        return;
    }

    const cJSON* item = nullptr;
    cJSON_ArrayForEach(item, vars) {
        if (!item->string) {
            continue;
        }
        _variables[item->string] = valueFromJson(item);
    }

    ESP_LOGI(TAG, "Variables initialized: %u", static_cast<unsigned>(_variables.size()));
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

bool ScenarioPlayer::start()
{
    if (!_display || !_loader || !_loader->isLoaded()) {
        ESP_LOGE(TAG, "Not ready (display=%p loader=%p loaded=%d)",
                 _display, _loader, _loader ? _loader->isLoaded() : 0);
        _state = State::Finished;
        return false;
    }

    _endingId.clear();
    _pageOffset = 0;
    _frames.clear();
    initVariables();

    if (!gotoScene(_loader->startSceneId())) {
        ESP_LOGE(TAG, "Start scene '%s' not found", _loader->startSceneId().c_str());
        _state = State::Finished;
        return false;
    }

    ESP_LOGI(TAG, "Playing '%s' from scene '%s'",
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
    // 優先順位: コマンドの direction > meta.text_direction > 縦書き
    std::string dir = getString(cmd, "direction");
    if (dir.empty() && _loader) {
        dir = _loader->defaultTextDirection();
    }

    return (dir == "HORIZONTAL") ? _horizontal : _vertical;
}

void ScenarioPlayer::run()
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
        return CmdResult::Next;
    }
    if (type == "wait") {
        const int ms = getInt(cmd, "ms", 0);
        if (ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(ms));
        }
        return CmdResult::Next;
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
        if (getBool(cmd, "clear_ghost", false)) {
            SimpleTransition::clearGhosting(_display);
        } else {
            SimpleTransition::refreshScreen(_display);
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

    std::string body = getString(cmd, "body");

    // 話者名は本文の先頭に添える。
    // 専用の名前欄はまだ無いので、まずはこの形で読めるようにしておく。
    //
    // ページ送りのオフセットは「加工後の本文」を基準にしているため、
    // 2ページ目以降も同じ加工をしないと位置がずれる。
    // ここで分岐させず常に付けるのはそのため。
    const std::string speaker = getString(cmd, "speaker");
    if (!speaker.empty()) {
        body = "【" + speaker + "】\n" + body;
    }

    // 背景を透過にしてあるので、前ページの消去は呼び出し側の責任。
    // ここでは単色で潰す（背景画像の上に出す場合は将来 bg の再描画に差し替える）。
    writer->clearArea(TFT_BLACK);

    const TypoWrite::DrawResult result = writer->drawTextPaged(body, _pageOffset);

    ESP_LOGI(TAG, "text: offset %u -> %u, hasMore=%d",
             static_cast<unsigned>(_pageOffset),
             static_cast<unsigned>(result.nextOffset),
             static_cast<int>(result.hasMore));

    SimpleTransition::refreshScreen(_display);

    if (result.hasMore) {
        // 続きがある。同じコマンドのまま次のページを待つ。
        _pageOffset = result.nextOffset;
        return CmdResult::StayAndWaitTap;
    }

    _pageOffset = 0;
    return getBool(cmd, "wait", true) ? CmdResult::NextAndWaitTap : CmdResult::Next;
}

ScenarioPlayer::CmdResult ScenarioPlayer::executeBackground(const cJSON* cmd)
{
    const std::string image = getString(cmd, "image");

    std::string path;
    if (!_loader->resolveBackgroundPath(image.c_str(), path)) {
        ESP_LOGE(TAG, "Background '%s' is not defined in assets", image.c_str());
        return CmdResult::Next;
    }

    const int x = getInt(cmd, "x", 0);
    const int y = getInt(cmd, "y", 0);

    const std::string transitionName = getString(cmd, "transition", "NONE");
    bool known = false;
    const SimpleTransitionType type = parseTransition(transitionName, known);
    if (!known) {
        ESP_LOGW(TAG, "Unknown transition '%s'. Drawing without effect",
                 transitionName.c_str());
    }

    // 演出なし、またはトランジションが使えない場合は直接描く
    if (!known || type == SimpleTransitionType::NONE || !_transition) {
        if (!_display->drawPngFile(&SD, path.c_str(), x, y)) {
            ESP_LOGE(TAG, "Failed to draw %s", path.c_str());
        }
        SimpleTransition::refreshScreen(_display);
        return CmdResult::Next;
    }

    // 演出つき。キャンバスに描いてから遷移させる。
    M5Canvas* canvas = _transition->getMainCanvas();
    if (!canvas) {
        ESP_LOGE(TAG, "Transition canvas unavailable. Drawing directly");
        _display->drawPngFile(&SD, path.c_str(), x, y);
        SimpleTransition::refreshScreen(_display);
        return CmdResult::Next;
    }

    canvas->fillSprite(TFT_BLACK);
    if (!canvas->drawPngFile(&SD, path.c_str(), x, y)) {
        ESP_LOGE(TAG, "Failed to draw %s to canvas", path.c_str());
    }

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

ScenarioPlayer::CmdResult ScenarioPlayer::executeChoice(const cJSON* cmd)
{
    _choiceLabels.clear();
    _choiceTargets.clear();
    _choiceEnabled.clear();
    _choicePrompt = getString(cmd, "prompt");

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

        _choiceLabels.push_back(getString(opt, "label"));
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

    if (!gotoScene(target)) {
        ESP_LOGE(TAG, "Choice leads to undefined scene '%s'", target.c_str());
        _state = State::Finished;
        return;
    }

    run();
}

void ScenarioPlayer::onTap()
{
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
