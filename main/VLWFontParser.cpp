// main/VLWFontParser.cpp (完全修正版)
#include "VLWFontParser.hpp"
#include "esp_log.h"
#include <cstring>
#include <cstdlib>
#include <algorithm>

// ログタグ
static const char* TAG = "VLWParser";

// VLWフォーマットの定数（仕様書に基づく）
static const uint32_t VLW_FILE_HEADER_SIZE = 24;    // ファイルヘッダーサイズ（6 × 4バイト）
static const uint32_t VLW_GLYPH_HEADER_SIZE = 28;   // グリフヘッダーサイズ（7 × 4バイト）
static const uint32_t VLW_EXPECTED_VERSION = 11;    // 期待されるバージョン

VLWFontParser::VLWFontParser()
    : _fontData(nullptr), _dataSize(0), _initialized(false),
      _glyphTable(nullptr), _glyphTableSize(0), _glyphTableSorted(false) {
    
    // フォントメトリクスを初期化
    memset(&_fontMetrics, 0, sizeof(_fontMetrics));
    _fontMetrics.isValid = false;
}

VLWFontParser::~VLWFontParser() {
    cleanup();
}

void VLWFontParser::cleanup() {
    if (_glyphTable) {
        free(_glyphTable);
        _glyphTable = nullptr;
    }
    _glyphTableSize = 0;
    _glyphTableSorted = false;
    _initialized = false;
    _fontMetrics.isValid = false;
}

uint32_t VLWFontParser::readUint32BE(size_t offset) const {
    if (offset + 4 > _dataSize) {
        ESP_LOGE(TAG, "Reading uint32 beyond data boundary at offset %lu", (unsigned long)offset);
        return 0;
    }
    
    // ビッグエンディアンで読み取り（VLW仕様）
    return (_fontData[offset] << 24) |
           (_fontData[offset + 1] << 16) |
           (_fontData[offset + 2] << 8) |
           _fontData[offset + 3];
}

int32_t VLWFontParser::readInt32BE(size_t offset) const {
    // 符号付き整数として解釈
    return static_cast<int32_t>(readUint32BE(offset));
}

bool VLWFontParser::init(const uint8_t* fontData, size_t dataSize) {
    if (!fontData || dataSize < VLW_FILE_HEADER_SIZE) {
        ESP_LOGE(TAG, "Invalid font data or size too small (minimum %lu bytes required)", 
                 (unsigned long)VLW_FILE_HEADER_SIZE);
        return false;
    }
    
    // 既存データをクリーンアップ
    cleanup();
    
    _fontData = fontData;
    _dataSize = dataSize;
    
    // ヘッダーを解析
    if (!parseHeader()) {
        ESP_LOGE(TAG, "Failed to parse VLW header");
        cleanup();
        return false;
    }
    
    // グリフテーブルを構築
    if (!buildGlyphTable()) {
        ESP_LOGE(TAG, "Failed to build glyph table");
        cleanup();
        return false;
    }
    
    // フォント全体のメトリクスを計算
    calculateFontMetrics();
    
    _initialized = true;
    _fontMetrics.isValid = true;
    
    ESP_LOGI(TAG, "VLW font initialized successfully");

    // ここで debugPrintFontInfo() を無条件に呼んでいたが、
    // 呼び出し側（textDisplayDemo()）も明示的に呼んでいるため
    // 起動ログに同じ内容が2回出力されていた。
    // デバッグ出力は呼び出し側の判断に任せる。

    return true;
}

bool VLWFontParser::parseHeader() {
    ESP_LOGI(TAG, "Parsing VLW file header...");
    
    size_t offset = 0;
    
    // VLW仕様に基づくヘッダー解析
    _fontMetrics.glyphCount = readUint32BE(offset);
    offset += 4;
    
    _fontMetrics.version = readUint32BE(offset);
    offset += 4;
    
    _fontMetrics.fontSize = readUint32BE(offset);
    offset += 4;
    
    _fontMetrics.padding = readUint32BE(offset);
    offset += 4;
    
    _fontMetrics.ascent = readInt32BE(offset);
    offset += 4;
    
    _fontMetrics.descent = readInt32BE(offset);
    offset += 4;
    
    // 基本的な妥当性チェック
    if (_fontMetrics.glyphCount == 0 || _fontMetrics.glyphCount > 65536) {
        ESP_LOGE(TAG, "Invalid glyph count: %lu", _fontMetrics.glyphCount);
        return false;
    }
    
    if (_fontMetrics.version != VLW_EXPECTED_VERSION) {
        ESP_LOGW(TAG, "Unexpected version: %lu (expected %lu)", 
                 _fontMetrics.version, (unsigned long)VLW_EXPECTED_VERSION);
        // 警告のみで続行
    }
    
    if (_fontMetrics.fontSize == 0) {
        ESP_LOGW(TAG, "Font size is 0, using default value 16");
        _fontMetrics.fontSize = 16;
    }
    
    // 必要なファイルサイズをチェック
    size_t requiredSize = VLW_FILE_HEADER_SIZE + (_fontMetrics.glyphCount * VLW_GLYPH_HEADER_SIZE);
    if (_dataSize < requiredSize) {
        ESP_LOGE(TAG, "File size too small: %lu bytes (required at least %lu bytes)", 
                 (unsigned long)_dataSize, (unsigned long)requiredSize);
        return false;
    }
    
    ESP_LOGI(TAG, "Header parsed successfully:");
    ESP_LOGI(TAG, "  Glyph count: %lu", _fontMetrics.glyphCount);
    ESP_LOGI(TAG, "  Version: %lu", _fontMetrics.version);
    ESP_LOGI(TAG, "  Font size: %lu pt", _fontMetrics.fontSize);
    ESP_LOGI(TAG, "  Ascent: %ld", (long)_fontMetrics.ascent);
    ESP_LOGI(TAG, "  Descent: %ld", (long)_fontMetrics.descent);
    
    return true;
}

bool VLWFontParser::buildGlyphTable() {
    ESP_LOGI(TAG, "Building glyph table...");
    
    if (_fontMetrics.glyphCount == 0) {
        ESP_LOGE(TAG, "Cannot build glyph table: glyph count is 0");
        return false;
    }
    
    // グリフテーブルメモリを確保
    _glyphTableSize = _fontMetrics.glyphCount;
    _glyphTable = static_cast<GlyphInfo*>(malloc(_glyphTableSize * sizeof(GlyphInfo)));
    
    if (!_glyphTable) {
        ESP_LOGE(TAG, "Failed to allocate memory for glyph table (%lu bytes)",
                 (unsigned long)(_glyphTableSize * sizeof(GlyphInfo)));
        return false;
    }
    
    // グリフヘッダーの解析（VLW仕様：すべてのヘッダーが連続して配置）
    size_t headerOffset = VLW_FILE_HEADER_SIZE;
    size_t bitmapOffset = VLW_FILE_HEADER_SIZE + (_fontMetrics.glyphCount * VLW_GLYPH_HEADER_SIZE);
    
    for (uint32_t i = 0; i < _fontMetrics.glyphCount; i++) {
        if (headerOffset + VLW_GLYPH_HEADER_SIZE > _dataSize) {
            ESP_LOGE(TAG, "Glyph header %lu extends beyond file boundary", i);
            return false;
        }
        
        GlyphInfo& glyph = _glyphTable[i];
        
        // VLW仕様に基づくグリフヘッダー解析
        glyph.unicode = static_cast<uint16_t>(readUint32BE(headerOffset));
        headerOffset += 4;
        
        glyph.height = readUint32BE(headerOffset);
        headerOffset += 4;
        
        glyph.width = readUint32BE(headerOffset);
        headerOffset += 4;
        
        glyph.setWidth = readUint32BE(headerOffset);
        headerOffset += 4;
        
        glyph.topExtent = readInt32BE(headerOffset);
        headerOffset += 4;
        
        glyph.leftExtent = readInt32BE(headerOffset);
        headerOffset += 4;
        
        glyph.padding = readUint32BE(headerOffset);
        headerOffset += 4;
        
        // ビットマップデータのオフセットを計算
        glyph.dataOffset = bitmapOffset;
        
        // 次のビットマップデータの位置を計算（幅 × 高さ バイト）
        uint32_t bitmapSize = glyph.width * glyph.height;
        bitmapOffset += bitmapSize;
        
        // ファイルサイズの妥当性チェック
        if (bitmapOffset > _dataSize) {
            ESP_LOGE(TAG, "Glyph %lu bitmap data extends beyond file boundary", i);
            return false;
        }
        
        ESP_LOGD(TAG, "Glyph %lu: U+%04X, size=%lux%lu, setWidth=%lu, topExtent=%ld, leftExtent=%ld",
                 i, glyph.unicode, glyph.width, glyph.height, glyph.setWidth,
                 (long)glyph.topExtent, (long)glyph.leftExtent);
    }

    // グリフが unicode 昇順に並んでいるかを判定する。
    // VLWは通常昇順なので二分探索（O(log N)）が使えるが、
    // 保証されているわけではないため実データで確認し、
    // 崩れている場合は findGlyph() を線形検索にフォールバックさせる。
    _glyphTableSorted = true;
    for (uint32_t i = 1; i < _glyphTableSize; i++) {
        if (_glyphTable[i - 1].unicode >= _glyphTable[i].unicode) {
            _glyphTableSorted = false;
            ESP_LOGW(TAG, "Glyph table is not sorted by unicode "
                          "(index %lu: U+%04X >= U+%04X). Falling back to linear search.",
                     i, _glyphTable[i - 1].unicode, _glyphTable[i].unicode);
            break;
        }
    }

    ESP_LOGI(TAG, "Glyph table built successfully with %lu entries (lookup: %s)",
             _glyphTableSize, _glyphTableSorted ? "binary search" : "linear search");
    return true;
}

void VLWFontParser::calculateFontMetrics() {
    ESP_LOGI(TAG, "Calculating font metrics...");
    
    // 最大サイズの初期化
    _fontMetrics.maxCharWidth = 0;
    _fontMetrics.maxCharHeight = 0;

    // 実効アセントはヘッダ値から始めて、グリフの topExtent で押し上げる。
    // M5GFX の VLWfont::loadFont() と同じ規則にすること。
    // ここがずれると、縦書きで回転した半角文字の位置補正が狂う。
    _fontMetrics.maxAscent = _fontMetrics.ascent;
    
    // 全角スペース（U+3000）を代表的な文字として探す
    uint32_t representativeWidth = _fontMetrics.fontSize; // デフォルト値
    
    // 全グリフをスキャンして最大値と代表値を計算
    for (uint32_t i = 0; i < _glyphTableSize; i++) {
        const GlyphInfo& glyph = _glyphTable[i];
        
        // 最大サイズを更新
        if (glyph.width > _fontMetrics.maxCharWidth) {
            _fontMetrics.maxCharWidth = glyph.width;
        }
        if (glyph.height > _fontMetrics.maxCharHeight) {
            _fontMetrics.maxCharHeight = glyph.height;
        }

        // U+3000 を除くのは M5GFX に合わせるため。
        // 全角スペースは字面が無く、topExtent が実態を表さない。
        if (glyph.unicode != 0x3000 &&
            static_cast<int32_t>(glyph.topExtent) > _fontMetrics.maxAscent) {
            _fontMetrics.maxAscent = static_cast<int32_t>(glyph.topExtent);
        }
        
        // 代表的な文字の幅を取得
        if (glyph.unicode == 0x3000) { // 全角スペース
            representativeWidth = glyph.setWidth;
            ESP_LOGI(TAG, "Found ideographic space (U+3000), width: %lu", representativeWidth);
        } else if (glyph.unicode == 0x0020 && representativeWidth == _fontMetrics.fontSize) {
            // 全角スペースが見つからない場合は半角スペースを使用
            representativeWidth = glyph.setWidth * 2; // 全角換算
            ESP_LOGI(TAG, "Using half-width space (U+0020) x2, width: %lu", representativeWidth);
        }
    }
    
    // フォント高を計算（アセント + |ディセント|）
    _fontMetrics.fontHeight = _fontMetrics.ascent + abs(_fontMetrics.descent);
    
    // フォント幅を設定（代表的な文字幅）
    _fontMetrics.fontWidth = representativeWidth;
    
    ESP_LOGI(TAG, "Font metrics calculated:");
    ESP_LOGI(TAG, "  Font height: %lu (ascent=%ld + |descent|=%ld)",
             _fontMetrics.fontHeight, (long)_fontMetrics.ascent, (long)abs(_fontMetrics.descent));
    ESP_LOGI(TAG, "  Font width: %lu", _fontMetrics.fontWidth);
    ESP_LOGI(TAG, "  Max char size: %lux%lu", _fontMetrics.maxCharWidth, _fontMetrics.maxCharHeight);
    ESP_LOGI(TAG, "  Max ascent: %ld (header ascent %ld)",
             (long)_fontMetrics.maxAscent, (long)_fontMetrics.ascent);
}

const VLWFontParser::GlyphInfo* VLWFontParser::findGlyph(uint16_t unicode) const {
    if (!_glyphTable || _glyphTableSize == 0) {
        return nullptr;
    }

    if (_glyphTableSorted) {
        // 二分探索 O(log N)
        // 日本語フォント（約4400グリフ）では最大13回程度の比較で済む。
        // 従来は線形検索で、TypoWrite が1文字あたり
        // getCharWidth/getCharHeight/getCharSetWidth と3回呼ぶため
        // 1文字ごとに最悪 3 × 4400 回の比較が発生していた。
        uint32_t low = 0;
        uint32_t high = _glyphTableSize - 1;
        while (low <= high) {
            uint32_t mid = low + (high - low) / 2;
            const uint16_t code = _glyphTable[mid].unicode;
            if (code == unicode) {
                return &_glyphTable[mid];
            }
            if (code < unicode) {
                low = mid + 1;
            } else {
                if (mid == 0) {
                    break;   // low が unsigned なのでアンダーフローを防ぐ
                }
                high = mid - 1;
            }
        }
        return nullptr;
    }

    // 昇順でない場合のフォールバック（線形検索 O(N)）
    for (uint32_t i = 0; i < _glyphTableSize; i++) {
        if (_glyphTable[i].unicode == unicode) {
            return &_glyphTable[i];
        }
    }

    return nullptr;
}

VLWCharMetrics VLWFontParser::getCharMetrics(uint16_t unicode) const {
    VLWCharMetrics metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.unicode = unicode;
    
    const GlyphInfo* glyph = findGlyph(unicode);
    if (glyph) {
        metrics.width = glyph->width;
        metrics.height = glyph->height;
        metrics.setWidth = glyph->setWidth;
        metrics.topExtent = glyph->topExtent;
        metrics.leftExtent = glyph->leftExtent;
        metrics.dataOffset = glyph->dataOffset;
        metrics.exists = true;
    } else {
        // 文字が見つからない場合はデフォルト値を使用
        metrics.width = _fontMetrics.fontWidth;
        metrics.height = _fontMetrics.fontHeight;
        metrics.setWidth = _fontMetrics.fontWidth;
        metrics.topExtent = _fontMetrics.ascent;
        metrics.leftExtent = 0;
        metrics.dataOffset = 0;
        metrics.exists = false;
        
        ESP_LOGD(TAG, "Character U+%04X not found in font", unicode);
    }
    
    return metrics;
}

uint32_t VLWFontParser::getCharWidth(uint16_t unicode) const {
    const GlyphInfo* glyph = findGlyph(unicode);
    return glyph ? glyph->width : _fontMetrics.fontWidth;
}

uint32_t VLWFontParser::getCharHeight(uint16_t unicode) const {
    // 以前は height + topExtent を返していたが、height はビットマップ高、
    // topExtent はベースラインからの距離であり、両者の和には幾何的な意味がない。
    // getCharMetrics().height（= glyph->height）と定義が食い違っていたため、
    // ビットマップ高に統一した。見つからない場合のフォールバックも
    // getCharMetrics() と同じ _fontMetrics.fontHeight に揃えてある。
    const GlyphInfo* glyph = findGlyph(unicode);
    return glyph ? glyph->height : _fontMetrics.fontHeight;
}

uint32_t VLWFontParser::getCharSetWidth(uint16_t unicode) const {
    const GlyphInfo* glyph = findGlyph(unicode);
    return glyph ? glyph->setWidth : _fontMetrics.fontWidth;
}

bool VLWFontParser::hasChar(uint16_t unicode) const {
    return findGlyph(unicode) != nullptr;
}

// 補足:
//   calculateTextWidth(const char*) と
//   utf8ToUnicode(const char*, uint16_t*, size_t)
// をここに実装していたが、いずれも呼び出し元が存在しない死蔵コードだったため削除した。
//
// 加えて、UTF-8デコード処理が本ファイル内で2箇所、さらに
// TypoWrite::utf8ToUnicode() にも1箇所と、計3実装に重複していた
// （境界チェックの有無が三者で異なっていた）。
// 現在使われているのは TypoWrite 側の実装のみ。
//
// なお calculateTextWidth() は改行を考慮せず全文字を横方向に合算する実装で、
// 複数行テキストでは意味のない値を返す不具合があった。
// 再導入する場合は共通ユーティリティとして1本化し、改行の扱いを決めること。


void VLWFontParser::debugPrintFontInfo() const {
    if (!_initialized) {
        ESP_LOGW(TAG, "Font not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "=== VLW Font Information ===");
    ESP_LOGI(TAG, "Font size: %lu pt", _fontMetrics.fontSize);
    ESP_LOGI(TAG, "Font dimensions: %lu x %lu pixels", _fontMetrics.fontWidth, _fontMetrics.fontHeight);
    ESP_LOGI(TAG, "Ascent: %ld, Descent: %ld", (long)_fontMetrics.ascent, (long)_fontMetrics.descent);
    ESP_LOGI(TAG, "Max char size: %lu x %lu pixels", _fontMetrics.maxCharWidth, _fontMetrics.maxCharHeight);
    ESP_LOGI(TAG, "Total glyphs: %lu", _fontMetrics.glyphCount);
    ESP_LOGI(TAG, "Version: %lu", _fontMetrics.version);
    
    // いくつかの代表的な文字の情報を表示
    const uint16_t testChars[] = {
        0x0020, // スペース
        0x0041, // 'A'
        0x3042, // あ
        0x3000, // 全角スペース
        0x3001, // 、
        0x3002  // 。
    };
    
    ESP_LOGI(TAG, "Sample character metrics:");
    for (size_t i = 0; i < sizeof(testChars) / sizeof(testChars[0]); i++) {
        if (hasChar(testChars[i])) {
            VLWCharMetrics metrics = getCharMetrics(testChars[i]);
            ESP_LOGI(TAG, "  U+%04X: %lux%lu, setWidth=%lu", 
                     testChars[i], metrics.width, metrics.height, metrics.setWidth);
        }
    }
    ESP_LOGI(TAG, "=============================");
}