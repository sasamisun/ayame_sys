// main/TextSystem.hpp - テキスト描画系の初期化と保持
#pragma once

#include <map>
#include <string>

#include <M5GFX.h>

#include "TypoWrite.hpp"
#include "VLWFontParser.hpp"

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
 * | 描画器 | 位置 | 領域 |
 * |---|---|---|
 * | `vertical()` | (400, 0) | 130 x 700 |
 * | `horizontal()` | (10, 420) | 380 x 180 |
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
     * @param x,y,w,h      位置と大きさ
     * @param vertical     縦書きか
     * @param fontSize     本文の倍率。**ビットマップフォントなので
     *                     2.0 倍は粗く、1.0 未満は読めなくなる**
     * @param lineSpacing  行間（縦書きでは列の間隔）
     * @param charSpacing  字間
     * @param align        揃え
     * @return 作れたか
     */
    bool defineBox(const std::string& name, int x, int y, int w, int h,
                   bool vertical, float fontSize,
                   int lineSpacing, int charSpacing, TextAlignment align);

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
