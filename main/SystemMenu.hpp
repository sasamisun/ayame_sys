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

    /// 選ばれたシナリオのフォルダ名
    const std::string& selectedScenarioId() const { return _selectedId; }

    /// 選択を消費したことを伝える
    void clearSelection() { _selectedId.clear(); }

private:
    // SD からシナリオのフォルダ名を集める
    void scanScenarios();

    // 一覧とボタンを描く
    void draw();

    // ボタンだけ捨てる（一覧は保持）
    void destroyButtons();

    static void onScenarioTapped(Button* btn);
    static void onUsbMscTapped(Button* btn);
    static void onRefreshTapped(Button* btn);

    M5GFX* _display = nullptr;
    TouchHandler* _touch = nullptr;
    ButtonManager* _buttons = nullptr;

    std::vector<std::string> _scenarioIds;
    std::vector<Button*> _scenarioButtons;

    Button* _usbMscButton = nullptr;
    Button* _refreshButton = nullptr;

    std::string _selectedId;

    // 静的コールバックから実体へ戻るための参照。
    // Button のコールバックが素の関数ポインタなので、
    // インスタンスを渡す手段がこれしかない。
    // メニューは1つしか作らない前提。
    static SystemMenu* s_instance;
};
