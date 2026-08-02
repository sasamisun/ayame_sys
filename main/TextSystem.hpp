// main/TextSystem.hpp - テキスト描画系の初期化と保持
#pragma once

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

    /// 両方の描画器のルビ解釈をまとめて切り替える
    void setRubyEnabled(bool enabled);

private:
    bool _ready = false;
    VLWFontParser _parser;
    TypoWrite* _vertical = nullptr;
    TypoWrite* _horizontal = nullptr;
};

/// グローバル実体
extern TextSystem textSystem;
