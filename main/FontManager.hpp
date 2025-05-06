// main/FontManager.hpp
#ifndef _FONT_MANAGER_HPP_
#define _FONT_MANAGER_HPP_

#include <M5GFX.h>
#include <map>
#include <string>

// vlwフォントを管理するクラス
class FontManager {
private:
    static constexpr const char* TAG = "FONT_MGR";
    
    // ロード済みフォントを管理する
    std::map<std::string, const lgfx::IFont*> _loadedFonts;
    
    // デフォルトフォント
    const lgfx::IFont* _defaultFont;
    
public:
    FontManager();
    ~FontManager();
    
    // vlwフォントを登録する
    bool registerFont(const char* name, const uint8_t* fontData, size_t dataSize);
    
    // 登録済みフォントを取得する
    const lgfx::IFont* getFont(const char* name);
    
    // デフォルトフォントを設定する
    void setDefaultFont(const lgfx::IFont* font);
    const lgfx::IFont* getDefaultFont() { return _defaultFont; }
    
    // 登録済みフォントをリストアップする
    void listFonts();
};

// グローバルインスタンス
extern FontManager fontManager;

#endif // _FONT_MANAGER_HPP_