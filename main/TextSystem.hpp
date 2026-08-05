// main/TextSystem.hpp - テキスト描画系の初期化と保持
#pragma once

#include <map>
#include <string>

#include <M5GFX.h>

#include "TypoWrite.hpp"
#include "VLWFontParser.hpp"

/**
 * @brief テキストボックスの余白（外枠と本文のあいだ）
 *
 * `TextSystem` の入れ子にしないのは、既定引数
 * （`defineBox(..., const TextBoxPadding& = TextBoxPadding{})`）から
 * 使えないため。入れ子のクラスの既定メンバ初期化子は、
 * 外側のクラスが完成するまで参照できない。
 */
struct TextBoxPadding {
    int top = 0;
    int right = 0;
    int bottom = 0;
    int left = 0;
};

/**
 * @brief 縦書きの字間の既定値
 *
 * 縦書きの送りは全角なら 1em ちょうど（16px フォントなら 16px）で、
 * そのままだと字面どうしの隙間が 1px しか空かず詰まって見える。
 * この値を足して読みやすい間隔にする。
 *
 * 既定のボックスと `textboxes` の両方で同じ値を使うこと。
 * 片方だけ違うと、同じ画面の中で行の詰まり方が変わる。
 *
 * 横書きは送りがプロポーショナルで元から隙間があるため 0。
 */
static constexpr int DEFAULT_VERTICAL_CHAR_SPACING = 2;

/**
 * @brief VLW フォントと TypoWrite をまとめて用意する
 *
 * フォント解析と描画器の生成は重い。
 *   ・`VLWFontParser::init()` … 約138KB の確保 + 4414グリフの解析
 *   ・`TypoWrite` の構築 … マッピングテーブル構築 + スプライト確保
 * 起動時に1回だけ済ませ、以降は使い回す。
 *
 * ## 描画器は2つ
 *
 * 縦書きと横書きで領域も設定も違うため、別々に持つ。
 * シナリオの `text` コマンドは `direction` に応じてどちらかを選ぶ。
 *
 * 位置と大きさは**画面の向きで変わる**（`layoutDefaultBoxes()`）。
 *
 * | 描画器 | 縦長 540x960 | 横長 960x540 |
 * |---|---|---|
 * | `vertical()` | (400, 0) 130 x 700 | (820, 20) 130 x 500 |
 * | `horizontal()` | (10, 420) 380 x 180 | (20, 360) 780 x 160 |
 *
 * @note グローバル実体 `textSystem` を1つ用意してある（`SD` や `buzzer` と同じ流儀）。
 */
class TextSystem {
public:
    TextSystem() = default;
    ~TextSystem();

    TextSystem(const TextSystem&) = delete;
    TextSystem& operator=(const TextSystem&) = delete;

    /**
     * @brief フォントを解析し、描画器を生成する（起動時に1回）
     * @return 成功したか。失敗時も表示系以外は動くので、呼び出し側は続行してよい
     */
    bool begin(M5GFX* display);

    bool isReady() const { return _ready; }

    TypoWrite* vertical() { return _vertical; }
    TypoWrite* horizontal() { return _horizontal; }
    VLWFontParser* parser() { return &_parser; }

    /// ボタンのラベルなど、TypoWrite を通さず直接描く場合に使うフォントデータ
    const uint8_t* fontData() const;

    // ========================================
    // シナリオ独自のフォント
    // ========================================

    /**
     * @brief SD の VLW を読み込んで本文フォントを差し替える
     *
     * シナリオが `meta.font` で指定したときに使う。
     * **シナリオを読み込んだ直後、テキストボックスを組む前に呼ぶこと。**
     * 後から呼ぶと、既に作ったボックスのメトリクスが古いままになる。
     *
     * フォントは**丸ごと PSRAM へ読む**。SD から流し読みにすると、
     * 1文字描くたびに SD を占有して画像が描けなくなる
     * （`SDCardWrapper` は同時に1ファイルしか開けない）。
     * そのぶん 1MB 前後を食うので、シナリオ本文に使えるメモリは減る。
     *
     * @param path `/sdcard` からのフルパス
     * @return 差し替えられたか。**失敗時は内蔵フォントのまま**再生を続けられる
     */
    bool loadScenarioFont(const std::string& path);

    /**
     * @brief 内蔵フォントへ戻し、シナリオのフォントを解放する
     *
     * シナリオを閉じるときに呼ぶ。呼ばないと 1MB 前後を抱えたままになる。
     */
    void useBuiltinFont();

    /// いま使っているフォントの名前（ログ・情報表示用）
    const std::string& fontName() const { return _fontName; }

    /// 既定を含む全ての描画器のルビ解釈をまとめて切り替える
    void setRubyEnabled(bool enabled);

    // ========================================
    // 名前付きテキストボックス
    // ========================================
    //
    // シナリオが `textboxes` で定義したものを保持する。
    // 既定の2つ（vertical / horizontal）とは別枠で、
    // シナリオを閉じるときに破棄する。

    /**
     * @brief 名前付きのテキストボックスを1つ用意する
     *
     * 既に同じ名前があれば設定を上書きする。
     *
     * @param name         `text` の `box` から指名する名前
     * @param x,y,w,h      **外枠**の位置と大きさ。下地はこの範囲いっぱいに敷かれる
     * @param vertical     縦書きか
     * @param fontSize     本文の倍率。**ビットマップフォントなので
     *                     2.0 倍は粗く、1.0 未満は読めなくなる**
     * @param lineSpacing  行間（縦書きでは列の間隔）
     * @param charSpacing  字間
     * @param align        揃え
     * @param padding      外枠と本文のあいだの余白。枠付きの背景画像を使うとき、
     *                     本文が枠に食い込まないようにするためのもの
     * @return 作れたか
     */
    bool defineBox(const std::string& name, int x, int y, int w, int h,
                   bool vertical, float fontSize,
                   int lineSpacing, int charSpacing, TextAlignment align,
                   const TextBoxPadding& padding = TextBoxPadding{});

    /**
     * @brief 既定の2つの描画器を、今の画面の向きに合わせて置き直す
     *
     * 画面を回転すると縦長 540x960 と横長 960x540 が入れ替わる。
     * 縦長前提の座標（縦書きの帯は高さ 700）は横長では画面に収まらないため、
     * **`setRotation()` の直後に必ず呼ぶこと。**
     *
     * シナリオが `textboxes` で定義したボックスは対象外。
     * そちらの座標は作者が向きを決めたうえで書くもの。
     */
    void layoutDefaultBoxes();

    /**
     * @brief 名前でテキストボックスを引く
     * @return 見つからなければ nullptr
     */
    TypoWrite* box(const std::string& name);

    /// シナリオが定義したボックスを全て捨てる（シナリオを閉じるとき）
    void clearBoxes();

private:
    // TypoWrite を1つ作って共通設定を入れる
    TypoWrite* createWriter();

    /**
     * @brief パーサと全描画器を指定のフォントデータへ向け直す
     *
     * 既定の2つに加え、シナリオが定義したボックスにも配る。
     * 配り忘れると、そのボックスだけ前のフォントのメトリクスで組まれる。
     */
    bool applyFont(const uint8_t* data, size_t size, const std::string& name);

    // 今使っているフォント。内蔵なら _scenarioFont は nullptr
    const uint8_t* _activeFont = nullptr;
    size_t _activeFontSize = 0;
    std::string _fontName;

    // SD から読んだシナリオ独自のフォント（PSRAM）。内蔵のときは nullptr
    uint8_t* _scenarioFont = nullptr;

    bool _ready = false;
    bool _rubyEnabled = false;
    M5GFX* _display = nullptr;
    VLWFontParser _parser;
    TypoWrite* _vertical = nullptr;
    TypoWrite* _horizontal = nullptr;

    // シナリオが定義したボックス。名前で引く
    std::map<std::string, TypoWrite*> _boxes;
};

/// グローバル実体
extern TextSystem textSystem;
