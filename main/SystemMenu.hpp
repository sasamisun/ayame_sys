// main/SystemMenu.hpp - シナリオ選択と本体機能のメニュー
#pragma once

#include <string>
#include <vector>

#include <M5GFX.h>

#include "Button.hpp"
#include "TouchHandler.hpp"

/**
 * @brief 起動時に出るメニュー
 *
 * SD の `scenarios/` を列挙して選ばせる。USB MSC の切り替えもここが持つ。
 *
 * ## 一覧では scenario.json を読まない
 *
 * 各シナリオのタイトルを出すには全フォルダの JSON を解析することになり、
 * シナリオが増えるほどメニューの表示が重くなる。
 * 一覧は**フォルダ名とサムネイル**で見せ、`meta.title` は選ばれてから読む。
 *
 * ## ボタンはこの画面が所有する
 *
 * `ButtonManager` を自前で持ち、`enter()` で作って `leave()` で捨てる。
 * 全画面で1つの `ButtonManager` を共有すると、画面を移るたびに
 * 隠す・戻すの操作が要り、隠し忘れが事故になる
 * （再生中にメニューのボタンが押せてしまう、など）。
 */
class SystemMenu {
public:
    SystemMenu() = default;
    ~SystemMenu();

    SystemMenu(const SystemMenu&) = delete;
    SystemMenu& operator=(const SystemMenu&) = delete;

    void begin(M5GFX* display, TouchHandler* touch);

    /**
     * @brief メニューを表示する
     *
     * SD からシナリオ一覧を読み直してからボタンを並べる。
     * USB MSC で PC からシナリオが追加された場合に拾えるよう、毎回読み直す。
     */
    void enter();

    /// ボタンを片付ける（画面を離れるとき）
    void leave();

    /**
     * @brief 入力を処理する
     * @param hasTouchEvent 呼び出し側が `TouchHandler::update()` で得た結果
     */
    void update(bool hasTouchEvent);

    /// シナリオが選ばれたか
    bool hasSelection() const { return !_selectedId.empty(); }

    /**
     * @brief 選ばれたのが「続きから」か
     *
     * true なら、呼び出し側は読み込んだ後に中断位置を復元する。
     * `selectedScenarioId()` と併せて使う。
     */
    bool selectionIsResume() const { return _selectionIsResume; }

    /// 選ばれたシナリオのフォルダ名
    const std::string& selectedScenarioId() const { return _selectedId; }

    /// 選択を消費したことを伝える
    void clearSelection() { _selectedId.clear(); _selectionIsResume = false; }

private:
    /// メニューの中で今どこを見せているか
    enum class View {
        List,    //!< シナリオ一覧
        About,   //!< 情報表示
    };

    // SD からシナリオのフォルダ名を集める
    void scanScenarios();

    // 一覧とボタンを描く
    void draw();

    // 情報画面を描く
    void drawAbout();

    // 下段の本体機能ボタンを並べる（一覧・情報の両方で使う）
    void buildFooterButtons();

    // ボタンだけ捨てる（一覧は保持）
    void destroyButtons();

    // 電源OFFの確認状態を解除して表示を戻す
    void cancelPowerOffConfirm();

    /// 一覧の総ページ数（最低1）
    int pageCount() const;

    /**
     * @brief ページを移る
     * @param delta +1 で次、-1 で前
     * @return 実際に移ったか（端にいれば false）
     */
    bool changePage(int delta);

    // 電子ペーパーに終了を示す画面を残してから電源を切る
    void shutdown();

    static void onScenarioTapped(Button* btn);
    static void onResumeTapped(Button* btn);

    /// 「続きから」を出せる状態か（続きがあり、そのシナリオが実在する）
    bool canResume() const;
    static void onUsbMscTapped(Button* btn);
    static void onRefreshTapped(Button* btn);
    static void onPowerOffTapped(Button* btn);
    static void onSoundTapped(Button* btn);
    static void onAboutTapped(Button* btn);

    M5GFX* _display = nullptr;
    TouchHandler* _touch = nullptr;
    ButtonManager* _buttons = nullptr;

    std::vector<std::string> _scenarioIds;
    // 一覧に出す表示名。_scenarioIds と同じ並び。
    // meta.title が取れればそれ、駄目ならフォルダ名が入る。
    std::vector<std::string> _scenarioTitles;
    std::vector<Button*> _scenarioButtons;

    // 一覧の現在ページ（0 始まり）。上下スワイプで移る
    int _page = 0;

    Button* _usbMscButton = nullptr;
    Button* _refreshButton = nullptr;
    Button* _powerOffButton = nullptr;
    Button* _aboutButton = nullptr;
    Button* _soundButton = nullptr;

    View _view = View::List;

    std::string _selectedId;
    bool _selectionIsResume = false;

    Button* _resumeButton = nullptr;

    // 電源OFFは押し間違いが致命的なので、2回押しで実行する。
    // 1回目で確認状態に入り、放置すると勝手に戻る。
    bool _powerOffArmed = false;
    int64_t _powerOffArmedAtMs = 0;

    /// 確認状態を保つ時間。過ぎたら自動で解除する
    static constexpr int64_t POWER_OFF_CONFIRM_TIMEOUT_MS = 5000;

    /**
     * @brief 電源を切る前に表示の定着を待つ時間
     *
     * **短くすると画面上部に横線が入る。**
     * 300ms では走査が終わりきらず、中途半端な行が残ることがあった。
     * 電源断は頻繁な操作ではないので、確実さを優先して長めに取ってある。
     */
    static constexpr int SHUTDOWN_SETTLE_MS = 3000;

    /// 下段に並べるボタンの数。アイコン幅 x この数 が画面幅に一致する
    static constexpr int FOOTER_BUTTON_COUNT = 5;

    // 静的コールバックから実体へ戻るための参照。
    // Button のコールバックが素の関数ポインタなので、
    // インスタンスを渡す手段がこれしかない。
    // メニューは1つしか作らない前提。
    static SystemMenu* s_instance;
};
