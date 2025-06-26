// main/VLWFontParser.hpp
#ifndef _VLW_FONT_PARSER_HPP_
#define _VLW_FONT_PARSER_HPP_

#include <stdint.h>
#include <stddef.h>

/**
 * @brief VLWフォントの文字メトリクス情報
 */
struct VLWCharMetrics {
    uint16_t unicode;       // Unicode文字コード
    uint32_t width;         // 文字の幅（ピクセル）
    uint32_t height;        // 文字の高さ（ピクセル）
    uint32_t setWidth;      // 送り幅（次の文字までの距離）
    int32_t topExtent;      // 上部拡張（ベースラインからの距離）
    int32_t leftExtent;     // 左側拡張（グリフ原点からの距離）
    uint32_t dataOffset;    // ビットマップデータのオフセット
    bool exists;            // この文字がフォントに存在するか
};

/**
 * @brief VLWフォントの全体メトリクス情報
 */
struct VLWFontMetrics {
    uint32_t glyphCount;        // グリフ数
    uint32_t version;           // バージョン（通常は11）
    uint32_t fontSize;          // フォントサイズ（ポイント単位）
    uint32_t padding;           // パディング（通常は0）
    int32_t ascent;             // アセント（ベースラインから上部までの距離）
    int32_t descent;            // ディセント（ベースラインから下部までの距離、負の値）
    uint32_t maxCharWidth;      // 最大文字幅（全グリフ中の最大値）
    uint32_t maxCharHeight;     // 最大文字高（全グリフ中の最大値）
    uint32_t fontHeight;        // フォント高（アセント + |ディセント|）
    uint32_t fontWidth;         // フォント幅（代表的な文字幅）
    bool isValid;               // フォントデータが有効か
};

/**
 * @brief VLWフォント解析クラス
 * 
 * VLW（Vector Letterform Workshop）フォント形式のバイナリデータを解析し、
 * フォントメトリクス情報を取得するためのクラスです。
 * Processing環境で生成されるVLWフォーマットに対応しています。
 */
class VLWFontParser {
private:
    const uint8_t* _fontData;       // フォントバイナリデータへのポインタ
    size_t _dataSize;               // データサイズ
    VLWFontMetrics _fontMetrics;    // フォント全体のメトリクス
    bool _initialized;              // 初期化済みフラグ
    
    // 内部データ構造（VLW仕様に基づく）
    struct GlyphInfo {
        uint16_t unicode;       // Unicodeコードポイント
        uint32_t height;        // 高さ（ピクセル）
        uint32_t width;         // 幅（ピクセル）
        uint32_t setWidth;      // 送り幅（次の文字までの距離）
        int32_t topExtent;      // 上部拡張
        int32_t leftExtent;     // 左側拡張
        uint32_t padding;       // パディング（通常は0）
        uint32_t dataOffset;    // ビットマップデータのオフセット位置
    };
    
    GlyphInfo* _glyphTable;         // グリフテーブル
    uint32_t _glyphTableSize;       // グリフテーブルサイズ
    
    // 内部メソッド
    /**
     * @brief VLWヘッダーを解析する（24バイト）
     * @return 解析成功時はtrue
     */
    bool parseHeader();
    
    /**
     * @brief グリフテーブルを構築する
     * @return 構築成功時はtrue
     */
    bool buildGlyphTable();
    
    /**
     * @brief フォント全体のメトリクスを計算する
     */
    void calculateFontMetrics();
    
    /**
     * @brief バイナリデータから32bit整数を読み取る（ビッグエンディアン）
     * @param offset データオフセット
     * @return 読み取った値
     */
    uint32_t readUint32BE(size_t offset) const;
    
    /**
     * @brief バイナリデータから32bit符号付き整数を読み取る（ビッグエンディアン）
     * @param offset データオフセット
     * @return 読み取った値
     */
    int32_t readInt32BE(size_t offset) const;
    
    /**
     * @brief 指定されたUnicode文字のグリフ情報を検索する
     * @param unicode Unicode文字コード
     * @return グリフ情報へのポインタ（見つからない場合はnullptr）
     */
    const GlyphInfo* findGlyph(uint16_t unicode) const;
    
    /**
     * @brief メモリを解放する
     */
    void cleanup();

public:
    /**
     * @brief コンストラクタ
     */
    VLWFontParser();
    
    /**
     * @brief デストラクタ
     */
    ~VLWFontParser();
    
    /**
     * @brief VLWフォントデータを初期化する
     * @param fontData フォントバイナリデータ
     * @param dataSize データサイズ
     * @return 初期化成功時はtrue
     */
    bool init(const uint8_t* fontData, size_t dataSize);
    
    /**
     * @brief フォント全体のメトリクス情報を取得する
     * @return フォントメトリクス構造体
     */
    const VLWFontMetrics& getFontMetrics() const { return _fontMetrics; }
    
    /**
     * @brief 指定された文字のメトリクス情報を取得する
     * @param unicode Unicode文字コード
     * @return 文字メトリクス構造体
     */
    VLWCharMetrics getCharMetrics(uint16_t unicode) const;
    
    /**
     * @brief フォントの基本高さを取得する（アセント + |ディセント|）
     * @return フォント高さ（ピクセル）
     */
    uint32_t getFontHeight() const { return _fontMetrics.fontHeight; }
    
    /**
     * @brief フォントの基本幅を取得する（代表的な文字幅）
     * @return フォント幅（ピクセル）
     */
    uint32_t getFontWidth() const { return _fontMetrics.fontWidth; }
    
    /**
     * @brief フォントサイズを取得する（ポイント単位）
     * @return フォントサイズ
     */
    uint32_t getFontSize() const { return _fontMetrics.fontSize; }
    
    /**
     * @brief アセント値を取得する
     * @return アセント（ピクセル）
     */
    int32_t getAscent() const { return _fontMetrics.ascent; }
    
    /**
     * @brief ディセント値を取得する
     * @return ディセント（ピクセル、通常は負の値）
     */
    int32_t getDescent() const { return _fontMetrics.descent; }
    
    /**
     * @brief 最大文字幅を取得する
     * @return 最大文字幅（ピクセル）
     */
    uint32_t getMaxCharWidth() const { return _fontMetrics.maxCharWidth; }
    
    /**
     * @brief 最大文字高を取得する
     * @return 最大文字高（ピクセル）
     */
    uint32_t getMaxCharHeight() const { return _fontMetrics.maxCharHeight; }
    
    /**
     * @brief 指定された文字の幅を取得する
     * @param unicode Unicode文字コード
     * @return 文字幅（ピクセル、存在しない場合は0）
     */
    uint32_t getCharWidth(uint16_t unicode) const;
    
    /**
     * @brief 指定された文字の高さを取得する
     * @param unicode Unicode文字コード
     * @return 文字高さ（ピクセル、存在しない場合は0）
     */
    uint32_t getCharHeight(uint16_t unicode) const;
    
    /**
     * @brief 指定された文字の送り幅を取得する
     * @param unicode Unicode文字コード
     * @return 送り幅（ピクセル、存在しない場合はfontWidthを返す）
     */
    uint32_t getCharSetWidth(uint16_t unicode) const;
    
    /**
     * @brief 指定された文字がフォントに存在するかチェックする
     * @param unicode Unicode文字コード
     * @return 存在する場合はtrue
     */
    bool hasChar(uint16_t unicode) const;
    
    /**
     * @brief フォントが正常に初期化されているかチェックする
     * @return 初期化済みの場合はtrue
     */
    bool isInitialized() const { return _initialized && _fontMetrics.isValid; }
    
    /**
     * @brief 文字列の描画幅を計算する
     * @param text UTF-8文字列
     * @return 描画幅（ピクセル）
     */
    uint32_t calculateTextWidth(const char* text) const;
    
    /**
     * @brief UTF-8文字列をUnicode配列に変換する
     * @param utf8Text UTF-8文字列
     * @param unicodeArray 出力先Unicode配列
     * @param maxLength 配列の最大長
     * @return 変換された文字数
     */
    size_t utf8ToUnicode(const char* utf8Text, uint16_t* unicodeArray, size_t maxLength) const;
    
    /**
     * @brief デバッグ用：フォント情報をログに出力する
     */
    void debugPrintFontInfo() const;
};

#endif // _VLW_FONT_PARSER_HPP_