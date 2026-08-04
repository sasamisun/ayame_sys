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
