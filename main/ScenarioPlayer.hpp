// main/ScenarioPlayer.hpp - シナリオのシーンとコマンドを実行する
#pragma once

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <M5GFX.h>

#include "ScenarioLoader.hpp"
#include "SimpleTransition.hpp"
#include "TextSystem.hpp"
#include "TypoWrite.hpp"

/**
 * @brief シナリオを1コマンドずつ実行する
 *
 * 仕様は `SCENARIO_SPEC.md` を参照。
 *
 * ## 対応しているコマンド
 *
 * `text` / `bg` / `chara` / `image` / `choice` / `set` / `if` / `random` /
 * `jump` / `call` / `return` / `end` / `clear` / `wait` / `beep` / `refresh` /
 * `save` / `load` / `checkpoint` / `suspend` と、シーンの `next`。
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
        Typing,             //!< 文字送りの途中
        Waiting,            //!< `wait` の途中（`skippable` ならタップで飛ばせる）
        Finished,           //!< 終了（`end` に到達、または進めなくなった）
    };

    /**
     * @brief 表示中の立ち絵
     *
     * 立ち絵は「出しっぱなし」になるもので、`bg` のような一度きりの描画と違う。
     * シーンをまたいでも保持し、`visible: false` か `clear` で消える。
     */
    struct CharaState {
        std::string id;

        /// 単一画像方式のときの表情。レイヤー方式では使わない
        std::string expression;

        /**
         * @brief レイヤー方式のときの「レイヤー名 → 差分名」
         *
         * `assets.characters.<id>.layers` がある立ち絵だけが使う。
         * 空なら単一画像方式。
         *
         * `chara` で書かれなかったレイヤーはここの値が残るので、
         * 「目だけ変える」が書ける。
         */
        std::map<std::string, std::string> layers;

        int x = 0;
        int y = 0;
        float scale = 1.0f;
        bool visible = true;
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

    /**
     * @brief セーブから再開する
     *
     * `start()` と同じ初期化をしたうえで、保存された位置・変数・画面を復元する。
     * メニューの「続きから」が使う。
     *
     * @param slot 読むスロット。`0` はオートセーブ枠
     * @return 再開できたか。**セーブが読めなければ false を返し、
     *         その場合は `start()` で最初から始めること**
     */
    bool resumeFrom(int slot);

    /// タップされた（`WaitingTap` のときだけ意味を持つ）
    void onTap();

    /// 画面遷移が終わった（`WaitingTransition` のときだけ意味を持つ）
    void onTransitionFinished();

    /**
     * @brief 文字送りを進める（`Typing` の間、呼び出し側が毎周期呼ぶ）
     *
     * 前回から `speed` ミリ秒たっていれば1文字増やして描き直す。
     * まだなら何もしない。
     *
     * @note 電子ペーパーは1文字ごとに全面走査が要るため、
     *       最速の `epd_fastest` でも1文字あたり約117ms かかる。
     *       文字送り中はそのモードへ落とし、終わったら元のモードで描き直す。
     */
    void tickTyping();

    /**
     * @brief `wait` の経過を見る（`Waiting` の間、呼び出し側が毎周期呼ぶ）
     *
     * 指定時間が過ぎたら次のコマンドへ進む。
     * 以前は `vTaskDelay()` で止めていたが、それだとタップを拾えず
     * `skippable` が実現できなかった。
     */
    void tickWait();

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
    bool isTyping() const { return _state == State::Typing; }
    bool isWaiting() const { return _state == State::Waiting; }
    bool isFinished() const { return _state == State::Finished; }

    /// 変数の現在値を覗く（デバッグ・セーブ用）
    const std::map<std::string, Value>& variables() const { return _variables; }

    /// `end` コマンドが指定したエンディング識別子（未到達なら空）
    const std::string& endingId() const { return _endingId; }

    // ========================================
    // 本文の履歴（バックログ）
    // ========================================

    /**
     * @brief 直近に読んだ本文（新しいものが後ろ）
     *
     * **保存はしない。** 電源を切れば消える。
     * 既読スキップのような「周回をまたぐ記録」とは別物で、
     * 「さっき何を読んだか」を見返すためだけのもの。
     */
    const std::vector<std::string>& history() const { return _history; }

    /// 履歴に残す件数
    static constexpr size_t MAX_HISTORY = 30;

    /**
     * @brief 舞台と直近の本文を描き直す
     *
     * バックログを閉じたときなど、画面を別のもので上書きした後に呼ぶ。
     * `renderStage()` だけだと本文が消えたままになる。
     */
    void redrawCurrentScreen();

    /**
     * @brief 電池切れが近いので今の位置を残す
     *
     * オートセーブ枠（slot 0）へ書き、周回をまたぐ変数も書き出す。
     * 電源が落ちてから悔やんでも遅いので、**再生中に一度だけ**呼ぶ。
     *
     * @return 書けたか。USB MSC 中や SD 無しでは書けない
     */
    bool emergencySave();

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
        StayAndType,             //!< 同じコマンドのまま文字送りを続ける
        NextAndWaitTime,         //!< 次へ進んでから時間待ち（`wait`）
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

    /**
     * @brief `call` で退避した戻り先
     *
     * シーンごと移るので、シーンIDと実行位置の一式をまとめて積む。
     */
    struct CallSite {
        std::string sceneId;
        std::vector<Frame> frames;
        size_t pageOffset;
    };

    // 待ちに入るまでコマンドを進める
    void run();

    CmdResult executeCommand(const cJSON* cmd);
    CmdResult executeText(const cJSON* cmd);
    CmdResult executeBackground(const cJSON* cmd);
    CmdResult executeChoice(const cJSON* cmd);
    CmdResult executeSet(const cJSON* cmd);
    CmdResult executeIf(const cJSON* cmd);
    CmdResult executeChara(const cJSON* cmd);
    CmdResult executeSave(const cJSON* cmd);
    CmdResult executeLoad(const cJSON* cmd);
    CmdResult executeRandom(const cJSON* cmd);
    CmdResult executeCall(const cJSON* cmd);
    CmdResult executeReturn(const cJSON* cmd);
    CmdResult executeImage(const cJSON* cmd);
    CmdResult executeSuspend(const cJSON* cmd);
    CmdResult executeCheckpoint(const cJSON* cmd);

    /**
     * @brief セーブファイルのパスを組む
     * @param slot 1〜99。0 はオートセーブ枠（`auto.json`）
     */
    std::string savePath(int slot) const;

    /**
     * @brief 今の状態を1つの JSON オブジェクトにまとめる
     *
     * 実行位置・変数・背景・立ち絵・前面絵。要するに再開に要るもの全部。
     * `slot` だけは状態の一部ではないので含めない（`writeStateObject()` が足す）。
     *
     * セーブと `checkpoint` はこの1箇所を共有する。
     * 別々に組むと、項目を足したときに片方だけ直す事故が起きる。
     *
     * @return 呼び出し側が `cJSON_Delete()` する。失敗したら nullptr
     */
    cJSON* buildStateObject() const;

    /**
     * @brief 状態オブジェクトをセーブファイルへ書く
     * @param root `buildStateObject()` の戻り値。**成否によらずここで解放する**
     * @param slot 書き込み先。`0` はオートセーブ枠
     */
    bool writeStateObject(cJSON* root, int slot);

    /**
     * @brief 状態を指定スロットへ書き出す
     *
     * `save` コマンドと `suspend` の両方から使う。
     *
     * **`checkpoint` が控えてあればそちらを書く。**
     * 控えが無いときだけ実行中の状態を書く。
     *
     * **スロット番号は必ず引数で渡すこと。** 以前は `suspend` が
     * 自分の解決した番号を使わず、コマンドをそのまま `executeSave()` へ
     * 渡していた。両者で `slot` の既定値が違った（`suspend` は 0、
     * `save` は 1）ため、`slot` を省いたシナリオでは
     * **保存先と栞の指す先がずれて再開できなかった**。
     */
    bool saveToSlot(int slot);

    /// 控えてある状態を捨てる（二重解放を避けるため必ずこれを通す）
    void releaseCheckpoint();

    /**
     * @brief 指定スロットから状態を復元する
     * @return 復元できたか。位置が変わったかどうかではない
     */
    bool loadFromSlot(int slot);

    /**
     * @brief 再生開始前の状態を整える（`run()` はしない）
     *
     * 変数の初期化、テキストボックスの生成、開始シーンへの移動まで。
     *
     * `run()` を含めないのは、**再開時に冒頭が一瞬表示されるのを防ぐ**ため。
     * `resumeFrom()` はここまで済ませてからセーブを重ね、その後で走らせる。
     */
    bool prepare();

    /**
     * @brief 背景と立ち絵を重ねて描き直す
     *
     * 立ち絵は背景の上に乗るので、片方だけを描くと破綻する。
     *   ・立ち絵だけ描く → 前の立ち絵が消えずに重なる
     *   ・背景だけ描く   → 立ち絵が消える
     * どちらの変更でもこの1つを通し、**必ず背景から描き直す**。
     *
     * @param target 描画先。トランジション時はキャンバス、通常は画面
     */
    void renderStage(lgfx::LovyanGFX* target);

    // 表示中の立ち絵を id で探す。無ければ nullptr
    CharaState* findChara(const std::string& id);

    // シーンを切り替える。見つからなければ false
    bool gotoScene(const std::string& sceneId);

    // 本文の向きに応じた描画器を返す
    TypoWrite* writerFor(const cJSON* cmd) const;

    /**
     * @brief シナリオの `textboxes` から名前付きボックスを作る
     *
     * `start()` で1回だけ行う。`TypoWrite` の生成は重いので、
     * 再生中は作り直さず使い回す。
     */
    void buildTextBoxes();

    /**
     * @brief `textboxes` の `padding` を読む
     *
     * 数値なら四辺まとめて、オブジェクトなら辺ごと（省略した辺は 0）。
     * 2通り受けるのは、枠のない箱では1つの数値で済ませたいため。
     */
    static TextBoxPadding parsePadding(const cJSON* node);

    /**
     * @brief テキストボックスの下地を敷く
     *
     * 背景画像があれば矩形に敷き、無ければ色で塗る。
     * 従来の `clearArea(TFT_BLACK)` を置き換えるもの。
     *
     * @param boxName 名前付きボックスの名前。既定ボックスなら空
     * @param writer  対象の描画器（位置と大きさを引くのに使う）
     */
    void fillTextBoxBackground(const std::string& boxName, TypoWrite* writer);

    // ---- 変数 ----

    // `variables` の初期値からストアを作る
    void initVariables();

    // ---- 永続変数 ----
    //
    // `variables` で `{ "value": ..., "persistent": true }` と書いたものは、
    // ニューゲームでもリセットせず周回をまたいで残す。
    // エンディングの回収記録などを、専用の記法を増やさずに書けるようにするため。

    /// `scenarios/<id>/saves/persistent.json`
    std::string persistentPath() const;

    /// 前回までの値を読んで上から被せる（`initVariables()` の最後で呼ぶ）
    void loadPersistent();

    /**
     * @brief 永続変数を書き出す
     *
     * **`set` のたびには呼ばない。** SD への書き込みが増えすぎる。
     * `end` と `suspend` のときだけ書く。
     */
    bool savePersistent();

    // cJSON の値から Value を作る
    static Value valueFromJson(const cJSON* item);

    /**
     * @brief 本文中の `{変数名}` を現在の値に置き換える
     *
     * ```
     * "body": "こんにちは、{name}さん。好感度は {affection} です。"
     * ```
     *
     * - `{{` と `}}` はそれぞれ `{` `}` そのものになる（記号を出したいとき用）
     * - 宣言されていない変数は**置き換えずそのまま残し**、警告を出す。
     *   空文字にすると書き間違いに気づけないため
     */
    std::string interpolate(const std::string& text) const;

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

    // ---- 文字送り ----
    //
    // 実行中の text コマンドを覚えておき、1文字ずつ増やして描き直す。
    std::string _typingBody;      //!< 加工済みの本文（話者名を含む）
    TypoWrite* _typingWriter = nullptr;
    std::string _typingBoxName;   //!< 送っている最中のボックス名（下地の再描画に要る）
    size_t _typedChars = 0;       //!< 今何文字目まで出しているか
    size_t _typingPageChars = 0;  //!< このページに入る文字数
    int _typingSpeedMs = 0;       //!< 1文字あたりの間隔
    int64_t _lastTypedMs = 0;
    bool _typingWaitAfter = true; //!< 出し切った後にタップを待つか

    // ---- wait ----
    int64_t _waitUntilMs = 0;
    bool _waitSkippable = true;

    // ---- end ----
    // `message` が指定されていた場合、表示してタップを待ってから終わる。
    // 即座に終わるとメニューへ戻ってしまい、メッセージが読めない。
    bool _endPending = false;

    std::string _endingId;

    // 変数の現在値
    std::map<std::string, Value> _variables;

    // そのうち周回をまたいで残すもの
    std::set<std::string> _persistentNames;

    // ---- 本文の履歴 ----
    std::vector<std::string> _history;

    // 直近に描いた本文。バックログを閉じたときに描き直すために持つ。
    // ページ送りの途中なら、そのページの先頭位置も要る。
    std::string _lastBody;
    std::string _lastBoxName;
    size_t _lastPageOffset = 0;

    // 表示中の立ち絵。追加順に重なる（後のものが手前）
    std::vector<CharaState> _charas;

    /**
     * @brief レイヤー合成の結果を控えておく箱
     *
     * レイヤー方式の立ち絵は、描き直すたびにレイヤーの数だけ PNG を読む。
     * 表情を変えるだけでも背景から全部やり直すので、部位を細かく割るほど遅い。
     *
     * **「背景のその部分 → 各レイヤー」を合成した1枚**を持っておき、
     * 組み合わせが変わるまで貼るだけにする。
     *
     * 背景を焼き込むのは、透過を元のアルファ合成のまま保つため。
     * 色キーで抜くと、16階調しかない画面では縁に色が滲む。
     *
     * **そのぶん、他の立ち絵と重なる立ち絵には使えない。**
     * 背景ごと貼るので、下にいる立ち絵を消してしまう。
     * 重なりを見つけたらキャッシュを使わず直接描く。
     */
    struct CharaCache {
        /**
         * @brief 背景のその部分だけを写した1枚
         *
         * **背景と位置が変わったときだけ**作り直す。
         * 実測で背景 PNG の読み込みは 900ms あり、描画全体の 86% を占めていた。
         * 表情を変えるたびにこれを読み直しては、控えを持つ意味が無い。
         */
        M5Canvas* bgSlice = nullptr;

        /// 背景 + レイヤーを重ねた1枚。実際に画面へ貼るのはこちら
        M5Canvas* composite = nullptr;

        /// bgSlice の内容を表す文字列（背景・位置・倍率）
        std::string bgKey;

        /// composite の内容を表す文字列（bgKey + レイヤーの組み合わせ）
        std::string key;

        /// 確保してある大きさ。倍率が変わったら作り直す
        int w = 0;
        int h = 0;

        /// 使えないと分かった理由を1度だけ出すための印
        bool warned = false;
    };

    /// 立ち絵の id ごとの合成結果
    std::map<std::string, CharaCache> _charaCache;

    /// 背景のその部分を表す文字列（背景名・位置・倍率）
    std::string charaBgKey(const CharaState& c) const;

    /// 合成結果を表す文字列（背景 + レイヤーの組み合わせ）
    std::string charaCacheKey(const CharaState& c) const;

    /**
     * @brief 合成結果を用意して返す
     *
     * 背景が変わっていなければ**背景は読み直さない**。
     * 控えてある1枚を写してからレイヤーだけ重ねる。
     *
     * @return 使えなければ nullptr（呼び出し側は直接描くこと）
     */
    M5Canvas* ensureCharaComposite(const CharaState& c, const cJSON* layerDefs);

    /// 立ち絵の外接矩形。`assets.characters.<id>.size` から引く
    bool charaBounds(const CharaState& c, int& w, int& h) const;

    /// 他の表示中の立ち絵と重なっているか（重なっていたらキャッシュを使わない）
    bool charaOverlaps(const CharaState& c) const;

    /// 合成結果を捨てる（シナリオを閉じるとき）
    void clearCharaCache();

    /**
     * @brief 控えてある合成結果を貼る
     *
     * 使えない場合（`size` の宣言が無い・他の立ち絵と重なる・
     * 描画先がキャンバス）は false を返すので、呼び出し側は直接描くこと。
     */
    bool drawCharaCached(lgfx::LovyanGFX* target, const CharaState& c,
                         const cJSON* layerDefs);

    /**
     * @brief 前面の一枚絵（イベントCG）
     *
     * 立ち絵より手前に1枚だけ出せる。
     * 立ち絵と違って差分を持たず、`assets.backgrounds` の画像をそのまま使う。
     */
    struct ForegroundState {
        std::string image;
        int x = 0;
        int y = 0;
        float scale = 1.0f;
        bool visible = false;
    };
    ForegroundState _foreground;

    // `call` の戻り先。入れ子にできる
    std::vector<CallSite> _callStack;

    /**
     * @brief `checkpoint` が控えた状態（無ければ nullptr）
     *
     * `save` / `suspend` は、これがあれば実行中の状態ではなく**こちらを書く**。
     *
     * 中断は普通「選択肢 → 中断メッセージ → 電源断」と進むので、
     * `suspend` が置かれる場所と、読者が戻りたい場所は最初から食い違う。
     * どこの状態を残すかは作者にしか決められない。
     *
     * **次の `checkpoint` まで効き続ける。** シーンをまたいでも消えない。
     * `{"type":"checkpoint","clear":true}` で捨てられる。
     *
     * 中身は `buildStateObject()` が作るものと同じ形。
     * 専用の構造体を作らないのは、項目を足したときに
     * セーブ側と二重に直す羽目になるため。
     */
    cJSON* _checkpoint = nullptr;

    /// `call` を積める深さの上限。手書きの JSON が無限再帰しても止まるように
    static constexpr size_t MAX_CALL_DEPTH = 16;

    /**
     * @brief `suspend` で電源を切る前に表示の定着を待つ時間
     *
     * **短くすると画面上部に横線が入る。**
     * 走査が終わりきる前に電源が落ちると、中途半端な行が残る。
     * `SystemMenu::SHUTDOWN_SETTLE_MS` と同じ理由・同じ値。
     */
    static constexpr int SUSPEND_SETTLE_MS = 3000;

    // 現在の背景。立ち絵を描き直すときに背景から描き直すため、
    // 論理名と位置を覚えておく
    std::string _currentBackground;
    int _backgroundX = 0;
    int _backgroundY = 0;
    float _backgroundScale = 1.0f;

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
