// main/FontManager.cpp
#include "FontManager.hpp"
#include "esp_log.h"
#include <cstring>

FontManager fontManager; // グローバルインスタンス

FontManager::FontManager()
{
    // M5GFXのデフォルトフォントを設定
    _defaultFont = &fonts::lgfxJapanGothic_16;
}

FontManager::~FontManager()
{
    // 動的に割り当てられたフォントは自動的に解放される
}

bool FontManager::registerFont(const char* name, const uint8_t* fontData, size_t dataSize)
{
    if (!name || !fontData || dataSize == 0) {
        ESP_LOGE(TAG, "Invalid parameters for registerFont");
        return false;
    }
    
    // 既に同名のフォントが登録されているか確認
    if (_loadedFonts.find(name) != _loadedFonts.end()) {
        ESP_LOGW(TAG, "Font '%s' is already registered", name);
        return false;
    }
    
    // vlwフォントをロードする
    auto font = lgfx::v1::Font::loadFont(fontData);
    if (!font) {
        ESP_LOGE(TAG, "Failed to load font '%s' (dataSize: %d bytes)", name, dataSize);
        return false;
    }
    
    // フォントを登録
    _loadedFonts[name] = font;
    ESP_LOGI(TAG, "Font '%s' registered successfully", name);
    
    return true;
}

const lgfx::IFont* FontManager::getFont(const char* name)
{
    if (!name) {
        ESP_LOGW(TAG, "Font name is null, returning default font");
        return _defaultFont;
    }
    
    // 登録済みフォントから検索
    auto it = _loadedFonts.find(name);
    if (it != _loadedFonts.end()) {
        return it->second;
    }
    
    ESP_LOGW(TAG, "Font '%s' not found, returning default font", name);
    return _defaultFont;
}

void FontManager::setDefaultFont(const lgfx::IFont* font)
{
    if (font) {
        _defaultFont = font;
        ESP_LOGI(TAG, "Default font updated");
    }
}

void FontManager::listFonts()
{
    ESP_LOGI(TAG, "Registered fonts:");
    for (const auto& pair : _loadedFonts) {
        ESP_LOGI(TAG, "  - %s", pair.first.c_str());
    }
    ESP_LOGI(TAG, "Default font: %s", 
             (_defaultFont == &fonts::lgfxJapanGothic_16) ? "lgfxJapanGothic_16" : "Custom");
}