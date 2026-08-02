// main/ScenarioPlayer.hpp - シナリオのシーンとコマンドを実行する
#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include <M5GFX.h>

#include "ScenarioLoader.hpp"
#include "SimpleTransition.hpp"
#include "TypoWrite.hpp"

/**
 * @brief シナリオを1コマンドずつ実行する
 *
 * 仕様は `SCENARIO_SPEC.md` を参照。
 *
 * ## 対応しているコマンド
 *
 * `text` / `bg` / `choice` / `set` / `if` / `jump` / `end` /
 * `clear` / `wait` / `beep` / `refresh` と、シーンの `next`。
 * 未対応のコマンドは警告を出して読み飛ばす（前方互換のため）。
 *
 * ## 進み方
 *
 * `start()` を呼ぶと、待ちが必要になるまでコマンドを進める。
 * 待ちは3種類ある。
 *
 * | 待ち | 解除の仕方 |
 * |---|---|
 * | タップ待ち（`text` の後） | `onTap()` |
 * | トランジション待ち（`bg` の演出中） | `onTransitionFinished()` |
 * | 選択待ち（`choice`） | `selectChoice()` |
 *
 * 呼び出し側は `loop()` でこの3つを見て、該当したら対応するメソッドを呼ぶ。
 *
 * ## 選択肢の UI は持たない
 *
 * `choice` に来ると `choiceLabels()` に選択肢が並ぶだけで、
 * **ボタンの描画と入力は呼び出し側の仕事**。
 * プレイヤーが `ButtonManager` を握ると、メニュー側とレイアウトの持ち方が
 * 二重になるため、UI からは切り離してある。
 *
 * ## 本文のページ送り
 *
 * `text` の本文が本文ボックスに収まらない場合、**1つの `text` コマンドが
 * 複数ページに分かれる**。タップのたびに次のページを出し、
 * 出し切ってから次のコマンドへ進む。
 */
class ScenarioPlayer {
public:
    enum class State {
        Idle,               //!< 未開始
        WaitingTap,         //!< タップ待ち
        WaitingTransition,  //!< 画面遷移の完了待ち
        WaitingChoice,      //!< 選択肢の入力待ち
        Finished,           //!< 終了（`end` に到達、または進めなくなった）
    };

    /**
     * @brief シナリオ変数
     *
     * 型は `variables` の初期値で決まり、以後変わらない。
     */
    struct Value {
        enum class Type { Bool, Number, String };

        Type type = Type::Bool;
        bool boolValue = false;
        double numberValue = 0.0;
        std::string stringValue;

        std::string toString() const;
    };

    /**
     * @brief 依存を渡す（`start()` の前に1回）
     *
     * @param display    描画先
     * @param loader     読み込み済みのシナリオ
     * @param vertical   縦書き用の描画器
     * @param horizontal 横書き用の描画器
     * @param transition 画面遷移。nullptr なら `transition` 指定を無視して即時描画する
     */
    void begin(M5GFX* display,
               ScenarioLoader* loader,
               TypoWrite* vertical,
               TypoWrite* horizontal,
               SimpleTransition* transition);

    /**
     * @brief `start` のシーンから再生を始める
     * @return 開始できたか
     */
    bool start();

    /// タップされた（`WaitingTap` のときだけ意味を持つ）
    void onTap();

    /// 画面遷移が終わった（`WaitingTransition` のときだけ意味を持つ）
    void onTransitionFinished();

    /**
     * @brief 選択肢が選ばれた（`WaitingChoice` のときだけ意味を持つ）
     * @param index `choiceLabels()` 上の添字
     */
    void selectChoice(size_t index);

    /**
     * @brief 表示すべき選択肢の文言
     *
     * `WaitingChoice` のときだけ中身がある。
     * 条件を満たさない選択肢は既に取り除かれている。
     */
    const std::vector<std::string>& choiceLabels() const { return _choiceLabels; }

    /**
     * @brief 各選択肢が選べる状態か
     *
     * `choiceLabels()` と同じ並び。`hide_if_false: false` の選択肢は
     * 条件を満たさなくても一覧に残るので、呼び出し側はこれを見て
     * 灰色表示にするなどして選べないことを示す。
     */
    const std::vector<bool>& choiceEnabled() const { return _choiceEnabled; }

    /// 選択肢の上に出す問いかけ（`choice.prompt`）。無ければ空
    const std::string& choicePrompt() const { return _choicePrompt; }

    State state() const { return _state; }
    bool isWaitingTap() const { return _state == State::WaitingTap; }
    bool isWaitingTransition() const { return _state == State::WaitingTransition; }
    bool isWaitingChoice() const { return _state == State::WaitingChoice; }
    bool isFinished() const { return _state == State::Finished; }

    /// 変数の現在値を覗く（デバッグ・セーブ用）
    const std::map<std::string, Value>& variables() const { return _variables; }

    /// `end` コマンドが指定したエンディング識別子（未到達なら空）
    const std::string& endingId() const { return _endingId; }

private:
    /// 1コマンド実行した結果、次に何をするか
    ///
    /// 待ちの種類まで結果に持たせている。
    /// 状態の設定を run() の1箇所に集約し、
    /// 各コマンドが _state を直接触らないようにするため。
    enum class CmdResult {
        Next,                    //!< 次のコマンドへ
        StayAndWaitTap,          //!< 同じコマンドのままタップ待ち（本文の続きがある）
        NextAndWaitTap,          //!< 次へ進んでからタップ待ち
        NextAndWaitTransition,   //!< 次へ進んでから画面遷移の完了待ち
        NextAndWaitChoice,       //!< 次へ進んでから選択待ち
        Pushed,                  //!< 入れ子の配列に入った（`if`）。位置は設定済み
        Jumped,                  //!< シーンが変わった（位置は gotoScene が設定済み）
        Finished,                //!< 再生終了
    };

    /**
     * @brief 実行位置
     *
     * `if` の `then` / `else` は入れ子の配列なので、
     * 「今どの配列のどこを見ているか」を積み重ねて持つ必要がある。
     * 添字1個では入れ子から戻れない。
     */
    struct Frame {
        const cJSON* commands;   //!< 見ている配列
        int index;               //!< その中の位置
    };

    // 待ちに入るまでコマンドを進める
    void run();

    CmdResult executeCommand(const cJSON* cmd);
    CmdResult executeText(const cJSON* cmd);
    CmdResult executeBackground(const cJSON* cmd);
    CmdResult executeChoice(const cJSON* cmd);
    CmdResult executeSet(const cJSON* cmd);
    CmdResult executeIf(const cJSON* cmd);

    // シーンを切り替える。見つからなければ false
    bool gotoScene(const std::string& sceneId);

    // 本文の向きに応じた描画器を返す
    TypoWrite* writerFor(const cJSON* cmd) const;

    // ---- 変数 ----

    // `variables` の初期値からストアを作る
    void initVariables();

    // cJSON の値から Value を作る
    static Value valueFromJson(const cJSON* item);

    // ---- 条件式 ----

    /**
     * @brief 条件式を評価する
     *
     * `{var, op, value}` と `all` / `any` / `not` を再帰で辿る。
     *
     * @param cond 条件。nullptr なら true（条件なし＝常に成立）
     */
    bool evaluateCondition(const cJSON* cond) const;

    // 単一条件 `{var, op, value}` の判定
    bool evaluateComparison(const cJSON* cond) const;

    // ---- 実行位置 ----

    // 今見ているフレーム。空なら nullptr
    Frame* currentFrame();

    // 現在位置のコマンドを取り出す。無ければ nullptr
    const cJSON* currentCommand();

    M5GFX* _display = nullptr;
    ScenarioLoader* _loader = nullptr;
    TypoWrite* _vertical = nullptr;
    TypoWrite* _horizontal = nullptr;
    SimpleTransition* _transition = nullptr;

    State _state = State::Idle;

    std::string _sceneId;
    const cJSON* _scene = nullptr;

    // 実行位置。底がシーンの commands で、`if` に入るたびに積まれる
    std::vector<Frame> _frames;

    // 実行中の text コマンドの、次に描くページの開始バイトオフセット
    size_t _pageOffset = 0;

    std::string _endingId;

    // 変数の現在値
    std::map<std::string, Value> _variables;

    // choice で待っている間の選択肢。
    // ラベルは呼び出し側が描き、遷移先はこちらが持つ。
    std::vector<std::string> _choiceLabels;
    std::vector<std::string> _choiceTargets;
    std::vector<bool> _choiceEnabled;
    std::string _choicePrompt;

    // 1回の run() で処理するコマンド数の上限。
    // 手書きの JSON は jump が輪になっていることがあり、
    // 待ちの入らない循環に落ちると戻ってこなくなる。
    static constexpr int MAX_STEPS_PER_RUN = 1000;
};
