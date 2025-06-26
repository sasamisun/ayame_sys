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
      _glyphTable(nullptr), _glyphTableSize(0) {
    
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
    debugPrintFontInfo();
    
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
    
    ESP_LOGI(TAG, "Glyph table built successfully with %lu entries", _glyphTableSize);
    return true;
}

void VLWFontParser::calculateFontMetrics() {
    ESP_LOGI(TAG, "Calculating font metrics...");
    
    // 最大サイズの初期化
    _fontMetrics.maxCharWidth = 0;
    _fontMetrics.maxCharHeight = 0;
    
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
}

const VLWFontParser::GlyphInfo* VLWFontParser::findGlyph(uint16_t unicode) const {
    if (!_glyphTable) {
        return nullptr;
    }
    
    // 線形検索（小さなフォントの場合は十分高速）
    // 大きなフォントの場合はバイナリサーチを検討
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
    const GlyphInfo* glyph = findGlyph(unicode);
    return glyph ? glyph->height + glyph->topExtent : _fontMetrics.fontHeight;
}

uint32_t VLWFontParser::getCharSetWidth(uint16_t unicode) const {
    const GlyphInfo* glyph = findGlyph(unicode);
    return glyph ? glyph->setWidth : _fontMetrics.fontWidth;
}

bool VLWFontParser::hasChar(uint16_t unicode) const {
    return findGlyph(unicode) != nullptr;
}

uint32_t VLWFontParser::calculateTextWidth(const char* text) const {
    if (!text || !_initialized) {
        return 0;
    }
    
    uint32_t totalWidth = 0;
    size_t textLen = strlen(text);
    size_t i = 0;
    
    while (i < textLen) {
        uint16_t unicode = 0;
        
        // UTF-8をUnicodeに変換
        if ((text[i] & 0x80) == 0) {
            // 1バイト文字
            unicode = text[i];
            i++;
        } else if ((text[i] & 0xE0) == 0xC0) {
            // 2バイト文字
            if (i + 1 < textLen) {
                unicode = ((text[i] & 0x1F) << 6) | (text[i + 1] & 0x3F);
                i += 2;
            } else {
                break;
            }
        } else if ((text[i] & 0xF0) == 0xE0) {
            // 3バイト文字
            if (i + 2 < textLen) {
                unicode = ((text[i] & 0x0F) << 12) | 
                         ((text[i + 1] & 0x3F) << 6) | 
                         (text[i + 2] & 0x3F);
                i += 3;
            } else {
                break;
            }
        } else {
            // 4バイト文字はスキップ
            i += 4;
            continue;
        }
        
        totalWidth += getCharSetWidth(unicode);
    }
    
    return totalWidth;
}

size_t VLWFontParser::utf8ToUnicode(const char* utf8Text, uint16_t* unicodeArray, size_t maxLength) const {
    if (!utf8Text || !unicodeArray || maxLength == 0) {
        return 0;
    }
    
    size_t textLen = strlen(utf8Text);
    size_t i = 0;
    size_t outIndex = 0;
    
    while (i < textLen && outIndex < maxLength) {
        uint16_t unicode = 0;
        
        // UTF-8をUnicodeに変換
        if ((utf8Text[i] & 0x80) == 0) {
            // 1バイト文字
            unicode = utf8Text[i];
            i++;
        } else if ((utf8Text[i] & 0xE0) == 0xC0) {
            // 2バイト文字
            if (i + 1 < textLen) {
                unicode = ((utf8Text[i] & 0x1F) << 6) | (utf8Text[i + 1] & 0x3F);
                i += 2;
            } else {
                break; // 不完全な文字
            }
        } else if ((utf8Text[i] & 0xF0) == 0xE0) {
            // 3バイト文字
            if (i + 2 < textLen) {
                unicode = ((utf8Text[i] & 0x0F) << 12) | 
                         ((utf8Text[i + 1] & 0x3F) << 6) | 
                         (utf8Text[i + 2] & 0x3F);
                i += 3;
            } else {
                break; // 不完全な文字
            }
        } else {
            // 4バイト文字は16ビットに収まらないのでスキップ
            i += 4;
            continue;
        }
        
        unicodeArray[outIndex++] = unicode;
    }
    
    return outIndex;
}

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