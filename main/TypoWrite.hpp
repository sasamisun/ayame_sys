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
    // 本文領域。外枠から余白を引いたもの。
    //
    // 描画・折り返し・揃えの計算はすべてこの4つだけを見る。
    // 余白を足してもレイアウトのコードを触らずに済むよう、
    // 「内部から見える領域＝本文領域」に保ってある。
    int _x, _y;                         // 本文の開始位置
    int _width, _height;                // 本文領域のサイズ

    // 外枠。下地（背景色・背景画像）とクリップの範囲。
    int _boxX, _boxY;
    int _boxWidth, _boxHeight;

    // 外枠と本文領域のあいだの余白
    int _padTop, _padRight, _padBottom, _padLeft;

    // rotatedBandOffset() の結果。フォント／サイズを変えるまで使い回す
    float _rotatedBandOffset = 0.0f;
    bool _rotatedBandOffsetValid = false;

    uint16_t _color;                    // テキスト色
    uint16_t _bgColor;                  // 背景色
    bool _transparentBg;                // 背景透明フラグ
    bool _wrap;                         // 折り返しフラグ
    bool _kinsoku;                      // 行頭・行末の禁則とぶら下げを行うか
    
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

    // ========== ルビ（ふりがな） ==========
    bool _rubyEnabled;                 // ルビ記法を解釈するか
    float _rubyScale;                  // 本文に対するルビの倍率
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
    
    // 描画位置の設定（外枠の左上）
    void setPosition(int x, int y);

    // 描画領域の設定（外枠の大きさ）
    void setArea(int width, int height);

    /**
     * @brief 本文と外枠のあいだの余白
     *
     * 下地（背景色・背景画像）は**外枠いっぱい**に敷かれ、
     * 本文だけがこの分だけ内側に入る。
     * 枠付きの背景画像を使うとき、本文が枠に食い込まないようにするためのもの。
     *
     * 余白が大きすぎて本文が置けなくなったら警告を出し、1px で止める。
     */
    void setPadding(int top, int right, int bottom, int left);

    // 外枠を読み出す。
    // 呼び出し側がボックスの矩形に下地を敷くときに使う。
    int areaX() const { return _boxX; }
    int areaY() const { return _boxY; }
    int areaWidth() const { return _boxWidth; }
    int areaHeight() const { return _boxHeight; }

    // 余白を除いた本文領域
    int textX() const { return _x; }
    int textY() const { return _y; }
    int textWidth() const { return _width; }
    int textHeight() const { return _height; }
    
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

    /**
     * @brief 禁則処理の有無（既定は有効）
     *
     * 行頭に `、。」` が来ないよう前の行へ戻し、
     * 行末が `「（` で終わらないよう次の行へ送る。
     * 句読点は行からはみ出させる（ぶら下げ）。
     *
     * 切ると、幅で機械的に折り返すだけになる。
     */
    void setKinsoku(bool enabled);


    
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
    // ルビ（ふりがな）
    // ========================================

    /**
     * @brief ルビ記法の解釈を有効にする
     *
     * 有効にすると、本文中の記法を読み取ってルビを描く。
     *
     * ```
     * |漢字<かんじ>      半角（入力しやすい）
     * ｜漢字《かんじ》    全角（青空文庫式）
     * ```
     *
     * 縦棒がルビを振る範囲の開始、括弧がルビの囲み。
     * **半角と全角のどちらでも書ける**（混在も可）。
     * シナリオは SD 上のテキストを手で書くため、`｜` や `《》` を出しにくい
     * 環境でも入力できるようにしてある。
     *
     * | 役割 | 受け付ける文字 |
     * |---|---|
     * | 開始 | `\|`(U+007C) / `｜`(U+FF5C) |
     * | 開き | `<`(U+003C) / `《`(U+300A) / `＜`(U+FF1C) |
     * | 閉じ | `>`(U+003E) / `》`(U+300B) / `＞`(U+FF1E) |
     *
     * ルビは**縦書きなら本文の右、横書きなら本文の上**に描かれる。
     *
     * @note 有効にすると**行の高さ（縦書きなら列の幅）がルビ帯のぶん増える**。
     *       ルビの有無にかかわらず全行に帯を確保するため、行間が一定に保たれる。
     *       そのぶん1画面に入る文字数は減る。
     * @note 無効のときは記法を解釈しないので、縦棒や括弧がそのまま表示される。
     * @note 本文中にたまたま `|` が現れ、後ろにルビの括弧が無い場合は、
     *       普通の文字として扱われる（警告ログは出る）。
     *
     * @warning ルビが本文より長い場合、はみ出して隣の文字に重なることがある
     *          （本文側の送りを広げる処理は未実装）。
     *          漢字1文字にかな2文字までなら収まる。
     */
    void setRubyEnabled(bool enabled);
    bool isRubyEnabled() const { return _rubyEnabled; }

    /**
     * @brief 本文に対するルビの倍率を設定する
     * @param scale 既定は 0.5（本文の半分）
     */
    void setRubyScale(float scale);
    float getRubyScale() const { return _rubyScale; }

    // ========================================
    // メインテキスト描画メソッド
    // ========================================

    // テキスト描画（固定値調整適用版）
    //
    // 描画領域に収まらない分は切り捨てられる。
    // 続きを次のページに出したい場合は drawTextPaged() を使う。
    void drawText(const std::string &text);

    /**
     * @brief ページ送り描画の結果
     *
     * @see drawTextPaged()
     */
    struct DrawResult {
        size_t nextOffset;  //!< 次ページの開始バイトオフセット。全部描けたなら text.size()
        bool hasMore;       //!< まだ描き切れていない文字が残っているか
        size_t pageChars;   //!< このページに入る文字数（文字送りの上限に使う）
    };

    /**
     * @brief 指定位置からテキストを描画し、描き切れなかった位置を返す
     *
     * 描画領域に収まる分だけを描き、**次に描くべき位置**を返す。
     * 戻り値の `nextOffset` をそのまま次回の `startOffset` に渡せば続きが出る。
     *
     * ```cpp
     * size_t offset = 0;
     * do {
     *     auto r = writer->drawTextPaged(longText, offset);
     *     offset = r.nextOffset;
     *     // ここでタップ待ち
     * } while (offset < longText.size());
     * ```
     *
     * @param text        本文全体（毎回同じ文字列を渡す。内部で切り出す）
     * @param startOffset 描画を開始するバイトオフセット
     * @param maxChars    このページのうち先頭から何文字を描くか。`0` で全部
     * @return 次ページの開始位置、続きの有無、このページの文字数
     *
     * @note `maxChars` は**描画だけ**を制限する。改行位置も揃えも
     *       ページの全文で計算するため、文字を増やしても既に出ている文字は動かない。
     *       文字送り（`text` の `speed`）はこれを使って実現している。
     *
     * @note `startOffset` は**バイト単位**であり文字数ではない。
     *       UTF-8 の途中を指した場合は、直前の文字境界へ切り下げる。
     * @note `drawText()` は本メソッドを `startOffset = 0` で呼ぶだけの薄い包み。
     *
     * @warning **背景透過時は前ページの消去を呼び出し側で行うこと。**
     *          背景が不透明（`setBackgroundColor()` に実色を指定）なら
     *          本メソッドが描画前に領域を塗りつぶすが、透過モード
     *          （`TFT_TRANSPARENT` 指定 / `setTransparentBackground(true)`）では
     *          塗りつぶさないため、**前ページの文字が残って重なる**。
     *          透過は背景画像の上に文字を重ねるための機能であり、
     *          下に何があるかを知っているのは呼び出し側だけなので、
     *          消去も呼び出し側の責任になる。
     *          単色でよければ `clearArea(color)` が使える。
     */
    DrawResult drawTextPaged(const std::string &text, size_t startOffset = 0,
                             size_t maxChars = 0);

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
    
    /**
     * @brief ルビ付き範囲。本文中の連続した数文字に対して1つ対応する
     */
    struct RubyRun {
        size_t baseStart;             //!< 本文（記法除去後）上の開始添字
        size_t baseEnd;               //!< 終了添字（exclusive）
        std::vector<uint16_t> ruby;   //!< ルビ文字列
    };

    /**
     * @brief ルビ記法を解釈し、本文とルビ範囲に分解する
     *
     * `|漢字<かんじ>`（半角）と `｜漢字《かんじ》`（全角・青空文庫式）の
     * どちらも読み取る。受け付ける文字は setRubyEnabled() の表を参照。
     *
     * @param raw            記法を含んだままの文字列
     * @param rawByteOffsets raw の各文字の先頭バイト位置
     * @param baseChars      [out] 記法を取り除いた本文
     * @param baseByteOffsets [out] baseChars の各文字に対応する「元の文字列上の」
     *                        バイト位置。ルビ範囲の先頭文字には `｜` の位置を入れる
     *                        （その位置から再解釈すればルビが復元できるようにするため）。
     *                        要素数は baseChars + 1 で、末尾は元の文字列全体の長さ
     * @param runs           [out] 検出したルビ範囲
     *
     * @note 対応する `《` や `》` が見つからない不正な記法は、
     *       `｜` を普通の文字として扱って読み進める（本文を失わないため）。
     */
    void parseRubyMarkup(const std::vector<uint16_t> &raw,
                         const std::vector<size_t> &rawByteOffsets,
                         size_t rawTextLength,
                         std::vector<uint16_t> &baseChars,
                         std::vector<size_t> &baseByteOffsets,
                         std::vector<RubyRun> &runs);

    /**
     * @brief 添字 index から始まるルビ範囲を探す
     * @return 見つかればその添字、無ければ runs.size()
     */
    size_t findRubyRunStartingAt(const std::vector<RubyRun> &runs, size_t index) const;

    /**
     * @brief 範囲の送り量を測る（末尾の字間は含めない）
     *
     * 横書きなら幅、縦書きなら高さ。
     * **描画側と同じ式で計算すること。** 食い違うと
     * `getTextWidth()` と実際の描画がずれ、揃えの位置も外れる。
     */
    int measureRange(const std::vector<uint16_t> &chars,
                     size_t from, size_t to) const;

    // ========== 禁則処理 ==========
    //
    // 行の切れ目を決めたあとで、日本語組版として不自然な位置を避ける。
    // **縦書き・横書き・calculateTextSize() の3箇所から同じものを使う。**
    // 別々に書くと、実際の描画と getTextWidth() がずれる。

    /// 行頭に来てはいけない文字（句読点・閉じ括弧・長音・小書き仮名）
    bool isLineStartProhibited(uint16_t c) const;

    /// 行末に来てはいけない文字（開き括弧）
    static bool isLineEndProhibited(uint16_t c);

    /// 行からはみ出させてよい文字（句読点のみ）
    static bool isHangingChar(uint16_t c);

    /**
     * @brief 行の切れ目を禁則に合わせて動かす
     *
     * @param chars     全文字
     * @param lineStart この行の先頭
     * @param lineEnd   折り返しで決まった終端（この位置は含まない）
     * @param runs      ルビ範囲。途中で切らないために見る
     * @param hangCount [out] ぶら下げた文字数（0 か 1）。
     *                  **揃えの計算では幅に入れないこと**
     * @return 調整後の終端
     */
    size_t applyKinsoku(const std::vector<uint16_t> &chars,
                        size_t lineStart, size_t lineEnd,
                        const std::vector<RubyRun> &runs,
                        size_t &hangCount) const;

    // ルビ1文字あたりの送り量（本文の送りに _rubyScale を掛けた値）
    int getRubyAdvance(uint16_t unicode_char);

    // ルビ帯の厚み（縦書きなら幅、横書きなら高さ）
    int getRubyStripSize();

    // 横書き/縦書きの描画本体。
    //
    // ページ送りのため「開始位置を受け取り、描き切れなかった位置を返す」形にしてある。
    // 文字列ではなく変換済みの配列を受け取るのは、
    // ページごとに utf8ToUnicode() をやり直さないため。
    //
    // @param chars     変換済みの全文字列
    // @param start     描画を開始する chars 上の添字
    // @param drawUntil この添字より手前だけを実際に描く（文字送り用）。
    //                  **測定と位置の計算はこの制限を受けない。**
    //                  制限しても既に出ている文字が動かないようにするため
    // @return 描画できなかった最初の文字の添字。全部描けたなら chars.size()

    // 横書きテキスト描画（固定値調整適用版）
    size_t drawHorizontalTextEnhanced(const std::vector<uint16_t> &chars, size_t start,
                                      const std::vector<RubyRun> &runs,
                                      size_t drawUntil);

    // 縦書きテキスト描画（固定値調整適用版）
    size_t drawVerticalTextEnhanced(const std::vector<uint16_t> &chars, size_t start,
                                    const std::vector<RubyRun> &runs,
                                    size_t drawUntil);
    
    // 枠線描画
    void drawAreaBorder();

    /// 外枠と余白から本文領域（`_x` / `_y` / `_width` / `_height`）を出し直す
    void applyPadding();
    
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
     * @brief 縦書きで90度回した半角文字の横位置の補正量
     *
     * 半角の英数記号の字面が em ボックスの縦のどこに乗るかを調べ、
     * その帯の中心を em ボックスの中心へ寄せる差分を返す。
     * 回転すると縦の位置がそのまま横の位置になるため、この値を
     * 横方向のずらし量として使う。
     *
     * **フォントごとに1つの定数。** 文字ごとに中央へ寄せると
     * g と T でベースラインが揃わなくなる。
     *
     * 走査は 94 文字ぶんなので、結果はフォント／サイズが変わるまで持ち回す。
     *
     * @return `_fontSize` を掛けた後の画素数。
     *         呼び出し側は pushRotateZoom に渡す倍率だけを追加で掛けること。
     *         **素の値と混ぜると、倍率を変えたときだけずれる。**
     */
    float rotatedBandOffset();

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

    // UTF-8からUnicodeへの変換（各文字の先頭バイト位置つき）
    //
    // ページ送りは「次に描く位置」をバイトオフセットで返す必要があるため、
    // 文字の添字からバイト位置を引けるようにする。
    // byteOffsets は chars と同じ要素数 + 1 で、末尾には文字列全体の長さが入る
    // （全部描き切ったときの nextOffset に使う）。
    std::vector<uint16_t> utf8ToUnicode(const std::string &utf8_string,
                                        std::vector<size_t> &byteOffsets);
    
    // UnicodeからUTF-8への変換
    std::string unicodeToUtf8(uint16_t unicode_char);
    
    // 行の高さを取得
    int getLineHeight();
    
    // 最大文字幅を取得（縦書き用）
    int getMaxCharWidth();
};

#endif // _TYPO_WRITE_HPP_