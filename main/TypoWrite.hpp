// main/TypoWrite.hpp - 固定値微調整版ヘッダー
#ifndef _TYPO_WRITE_HPP_
#define _TYPO_WRITE_HPP_

#include <M5GFX.h>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include "VLWFontParser.hpp"

// ========================================
// 列挙型定義
// ========================================

// テキスト方向の列挙型
enum class TextDirection {
    HORIZONTAL,  // 横書き（左から右）
    VERTICAL     // 縦書き（上から下）
};

// テキスト揃えの列挙型
enum class TextAlignment {
    LEFT,    // 左揃え（縦書きの場合は上揃え）
    CENTER,  // 中央揃え
    RIGHT    // 右揃え（縦書きの場合は下揃え）
};

// 文字カテゴリの列挙型
enum class CharCategory {
    NORMAL,         // 通常文字
    BRACKET,        // 括弧類
    HORIZONTAL_BAR, // 横棒・長音記号類
    PUNCTUATION,    // 句読点類
    SMALL_CHAR,     // 小文字（ひらがな・カタカナ）
    OTHER_SPECIAL   // その他の特殊文字
};

// ========================================
// 構造体定義
// ========================================

// 小文字の描画設定構造体
// 小文字（捨て仮名）の縦組み設定
//
// 縦組みでは小文字を em の右上へ寄せる。
// フォントの小文字グリフは横書き用に設計されており大文字と同じベースラインに
// 揃う（下寄せ）ため、縦組みではこの変位が必要になる。
struct SmallCharSettings {
    // 縮小率。**フォントに小文字の専用グリフが無い場合の代用時のみ**使う。
    // 専用グリフがある場合は縮小せずそのまま描く（0.7〜0.8推奨）
    float scale;

    // X方向オフセット（emボックス幅に対する比率、正で右へ）
    // 専用グリフ・代用のどちらにも適用される
    float offsetX;

    // Y方向オフセット（emボックス高に対する比率、負で上へ）
    // 専用グリフ・代用のどちらにも適用される
    float offsetY;

    // デフォルト設定
    //
    // オフセットは shippori_16（emW=18 / emH=24）で
    //   X: 18 * 0.15 = +2px（右へ）
    //   Y: 24 * -0.10 = -2px（上へ）
    // となる。寄せ具合はフォントの字形設計に依存するので、
    // 実機で見て調整すること。
    static SmallCharSettings getDefault() {
        return {
            0.75f,   // 代用時は75%に縮小
            0.15f,   // 右へ em幅の15%
            -0.10f   // 上へ em高の10%
        };
    }
};

// 文字メトリクス構造体（統一管理用）
struct CharMetrics {
    int32_t width;      // 文字幅（ピクセル）
    int32_t height;     // 文字高さ（ピクセル）
    int32_t setWidth;   // 文字送り幅（ピクセル）
    int32_t baseline;   // ベースライン位置
};

// 文字種別の微調整設定構造体（固定値版）
struct CharTypeAdjustment {
    float widthScale;     // 幅の調整倍率
    float heightScale;    // 高さの調整倍率
    int spacingOffset;    // 字間オフセット（ピクセル）
    int verticalOffset;   // 縦位置オフセット（ピクセル）
    int horizontalOffset; // 横位置オフセット（ピクセル）
};

// ========================================
// TypoWriteクラス定義
// ========================================
class TypoWrite {
private:
    // ========== 基本設定 ==========
    M5GFX* _display;                     // 描画先のディスプレイ（デフォルト）
    lgfx::LGFX_Sprite* _drawTarget;      // 描画先スプライト（外部から設定可能）
    lgfx::LGFX_Sprite* _charSprite;      // 文字描画用再利用スプライト（最適化用）
    
    // ========== 描画設定 ==========
    TextDirection _direction;            // テキスト方向
    TextAlignment _alignment;            // テキスト揃え
    int _x, _y;                         // 描画開始位置
    int _width, _height;                // 描画領域のサイズ
    uint16_t _color;                    // テキスト色
    uint16_t _bgColor;                  // 背景色
    bool _transparentBg;                // 背景透明フラグ
    bool _wrap;                         // 折り返しフラグ
    
    // ========== フォント設定 ==========
    float _fontSize;                    // フォントサイズ
    const lgfx::IFont* _font;          // フォント
    const uint8_t* _vlwFont;           // VLWフォントデータ
    bool _isCustomFont;                // カスタムフォントフラグ
    VLWFontParser* _vlwParser;         // VLWパーサー（外部から設定）
    bool _useVLWParser;                // VLWパーサー使用フラグ
    
    // ========== スペーシング設定 ==========
    int _lineSpacing;                  // 行間
    int _charSpacing;                  // 文字間
    
    // ========== 描画状態 ==========
    int _currentX, _currentY;          // 現在の描画位置
    
    // ========== 小文字処理設定 ==========
    bool _enableSmallCharHandling;     // 小文字処理有効フラグ
    SmallCharSettings _smallCharSettings; // 小文字描画設定
    
    // ========== 固定値微調整機能 ==========
    bool _showBorder;                  // 枠線表示フラグ
    uint16_t _borderColor;             // 枠線色
    bool _enableCharAdjustment;        // 文字調整有効フラグ（固定値適用の ON/OFF）
    std::unordered_map<CharCategory, CharTypeAdjustment> _charAdjustments; // 固定値調整テーブル
    
    // ========== マッピングテーブル ==========
    std::unordered_map<uint16_t, uint16_t> _smallToLargeMap;   // 小文字→大文字マッピング
    std::unordered_map<uint16_t, uint16_t> _verticalGlyphMap;  // 縦書き用グリフマッピング
    
    // ========== パフォーマンス最適化 ==========
    // メトリクスキャッシュの上限件数。
    // 上限に達したら全消去して入れ直す（getCharMetrics() 参照）。
    static constexpr size_t METRICS_CACHE_LIMIT = 256;

    std::unordered_map<uint16_t, CharMetrics> _metricsCache;   // メトリクスキャッシュ

public:
    // ========================================
    // コンストラクタ・デストラクタ
    // ========================================
    TypoWrite(M5GFX *display);
    ~TypoWrite();
    
    // ========================================
    // 基本設定メソッド
    // ========================================
    
    // 描画先の設定
    void setDrawTarget(lgfx::LGFX_Sprite *sprite);
    
    // VLWパーサーの設定
    void setVLWParser(VLWFontParser* parser);
    
    // テキスト方向の設定
    void setDirection(TextDirection direction);
    
    // テキスト揃えの設定
    void setAlignment(TextAlignment alignment);
    
    // 描画位置の設定
    void setPosition(int x, int y);
    
    // 描画領域の設定
    void setArea(int width, int height);
    
    // テキスト色の設定
    void setColor(uint16_t color);
    
    // 背景色の設定
    // TFT_TRANSPARENT を渡した場合は背景透明モードを自動的に有効化する
    void setBackgroundColor(uint16_t bgColor);

    // 背景透明モードの明示設定
    // 有効時は描画領域の塗りつぶしと文字背景の塗りつぶしを行わない
    void setTransparentBackground(bool transparent);

    // 背景透明モードの取得
    bool isTransparentBackground() const { return _transparentBg; }

    // 折り返しの設定
    void setWrap(bool wrap);


    
    // ========================================
    // 固定値微調整機能設定（簡易版）
    // ========================================
    
    // 枠線表示の切り替え（デバッグ用）
    // 色は省略可。既定値はコンストラクタが使う
    // TypoWriteConstants::Border::DEFAULT_COLOR と同じ TFT_RED。
    //
    // 以前はコメントが「色は固定値 TFT_RED を使用」（実際は引数で変更可能）、
    // 既定引数が TFT_WHITE、コンストラクタの初期値が TFT_RED と三者が食い違っていた。
    // 定数は .cpp 側にあり既定引数から参照できないため、値を直接そろえている。
    void setBorderDisplay(bool show, uint16_t color = TFT_RED);
    
    // 文字調整機能の簡易切り替え（固定値の適用 ON/OFF）
    void setCharacterAdjustment(bool enable);

    /**
     * @brief 小文字の代用処理を有効にするか設定する
     *
     * 有効（既定）でも、フォントが小文字の専用グリフを収録していれば
     * 代用は行われない（`needsSmallCharSubstitution()` が自動判定する）。
     * 専用グリフを持つフォントで強制的に代用させたい場合以外、通常は変更不要。
     */
    void setSmallCharHandling(bool enable);

    /** @brief 小文字の代用処理が有効か */
    bool isSmallCharHandlingEnabled() const { return _enableSmallCharHandling; }

    // 固定値による文字種別調整の取得
    CharTypeAdjustment getCharAdjustment(uint16_t unicode_char);
    // ========================================
    // フォント設定メソッド
    // ========================================
    
    // フォントサイズの設定
    void setFontSize(float size);
    
    // フォントの設定
    void setFont(const lgfx::IFont *font);
    
    // カスタムフォントの読み込み
    bool loadFontFromArray(const uint8_t *fontArray);
    
    // ========================================
    // スペーシング設定メソッド
    // ========================================
    
    // 行間の設定
    void setLineSpacing(int spacing);
    
    // 文字間の設定
    void setCharSpacing(int spacing);
    
    // 注: setColumnSpacing() は廃止した。縦組みでは「行」＝「列」なので、
    //     列の間隔は setLineSpacing() で指定する。
    
    
    // ========================================
    // メインテキスト描画メソッド
    // ========================================
    
    // テキスト描画（固定値調整適用版）
    void drawText(const std::string &text);
    
    // 中央揃えでテキスト描画
    void drawTextCentered(const std::string &text);
    
    // ========================================
    // サイズ計算メソッド
    // ========================================
    
    // テキスト幅の取得
    int getTextWidth(const std::string &text);
    
    // テキスト高さの取得
    int getTextHeight(const std::string &text);
    
    // ========================================
    // 描画領域管理メソッド
    // ========================================
    
    // 描画領域のクリア
    void clearArea(uint16_t color = TFT_BLACK);
    
    // ========================================
    // デバッグメソッド
    // ========================================
    
    // 小文字マッピングテーブルのデバッグ表示
    void debugPrintSmallCharMap() const;
    
    // 文字列内の小文字分析
    void debugAnalyzeSmallChars(const std::string& text);
    
    // 固定値調整設定のデバッグ表示
    // 注: debugShowFixedAdjustments() は debugShowCharAdjustments() と
    //     内容が重複していたため削除した。


    void debugShowCharAdjustments();

private:
    // ========================================
    // 内部初期化メソッド
    // ========================================
    
    void initializeAllTables();

    // ========================================
    // 内部描画メソッド
    // ========================================
    
    // 横書きテキスト描画（固定値調整適用版）
    void drawHorizontalTextEnhanced(const std::string &text);
    
    // 縦書きテキスト描画（固定値調整適用版）
    void drawVerticalTextEnhanced(const std::string &text);
    
    // 枠線描画
    void drawAreaBorder();
    
    // 文字描画（スケール調整対応）
    void drawEnhancedCharacter(uint16_t unicode_char, int x, int y,
                              float widthScale = 1.0f, float heightScale = 1.0f);
    
    // 回転付き文字描画（スケール調整対応）
    void drawEnhancedCharacterWithRotation(uint16_t unicode_char, int x, int y,
                                          float widthScale, float heightScale,
                                          float rotation);
    
    // スケール調整文字描画
    void drawScaledCharacter(uint16_t unicode_char, int x, int y,
                            float widthScale, float heightScale);
    
    // スケール調整＋回転文字描画
    void drawScaledCharacterWithRotation(uint16_t unicode_char, int x, int y,
                                        float widthScale, float heightScale,
                                        float rotation);
    
    // 直接描画（既存）
    void drawDirectCharacter(uint16_t unicode_char, int x, int y);

    /**
     * @brief 全文字共通の描画枠（emボックス）のサイズを求める
     *
     * フォントとフォントサイズが同じなら全文字で同一の値になる。
     * 最大グリフが収まるサイズを返す。
     *
     * @param emW 幅の出力先
     * @param emH 高さの出力先
     */
    void getEmBoxSize(int &emW, int &emH);

    /**
     * @brief 文字描画用スプライトを指定サイズで用意する
     *
     * 既に同じサイズなら何もしない。サイズが異なる場合のみ作り直す。
     * emボックスは全文字共通なので、実際に作り直されるのは
     * フォントやフォントサイズを変更したときだけになる。
     *
     * @return 用意できた場合true
     */
    bool ensureCharSprite(int w, int h);

    /**
     * @brief 描画先にフォント・サイズ・色をまとめて適用する
     *
     * 文字列の描画を始める前に1回だけ呼ぶ。
     * 特に loadFont() は VLW ヘッダの解析を伴うため、
     * 1文字ごとに呼ぶと文字数に比例したコストになる。
     */
    void applyTextStyle(lgfx::LovyanGFX *target);

    /**
     * @brief applyTextStyle() で読み込んだフォントを解放する
     *
     * 文字列の描画を終えた後に1回だけ呼ぶ。
     * カスタムフォントを使っていない場合は何もしない。
     */
    void releaseTextStyle(lgfx::LovyanGFX *target);
    
    
    // ========================================
    // 内部計算・判定メソッド
    // ========================================
    
    // テキストサイズの計算
    void calculateTextSize(const std::string &text, int &width, int &height);
    
    // 文字メトリクスの取得
    CharMetrics getCharMetrics(uint16_t unicode_char);
    
    // 注: getFixedCharAdjustment() の宣言がここにあったが、定義が存在せず
    //     呼び出せばリンクエラーになる状態だった。
    //     同じ役割は public の getCharAdjustment() が担っているため宣言を削除した。

    // 文字カテゴリの判定
    CharCategory getCharCategory(uint16_t unicode_char);
    
    // 小文字判定
    bool isSmallChar(uint16_t unicode_char) const;

    /**
     * @brief 小文字を「大文字の縮小」で代用する必要があるか
     *
     * 小文字（ぁぃぅゃゅょっ等）の専用グリフを持たないフォント向けの代用処理を
     * 行うべきかどうかを判定する。
     *
     * フォントが専用グリフを収録している場合は代用しない。
     * 専用グリフはサイズも em ボックス内の位置も適切に設計されているため、
     * 大文字を縮小した代用品より確実に良い結果になる。
     * （代用すると縮小によってインク下端が上がり、次の文字との間隔が広がる）
     *
     * @return 代用が必要な場合true
     */
    bool needsSmallCharSubstitution(uint16_t unicode_char) const;
    
    // 小文字の対応する大文字を取得
    uint16_t getCorrespondingLargeChar(uint16_t small_char) const;
    
    // 縦書き時の回転判定
    bool shouldRotateInVertical(uint16_t unicode_char);
    
    // 縦書き用グリフへの変換
    uint16_t convertToVerticalGlyph(uint16_t unicode_char);
    
    // ========================================
    // ヘルパーメソッド
    // ========================================
    
    // UTF-8からUnicodeへの変換
    std::vector<uint16_t> utf8ToUnicode(const std::string &utf8_string);
    
    // UnicodeからUTF-8への変換
    std::string unicodeToUtf8(uint16_t unicode_char);
    
    // 行の高さを取得
    int getLineHeight();
    
    // 最大文字幅を取得（縦書き用）
    int getMaxCharWidth();
};

#endif // _TYPO_WRITE_HPP_