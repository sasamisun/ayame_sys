// SDcard.hpp
#ifndef _SDCARD_HPP_
#define _SDCARD_HPP_

#include <M5GFX.h>
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"

// DataWrapperを継承した独自クラス
class SDCardWrapper : public lgfx::v1::DataWrapper {
private:
    FILE* _file;
    bool _initialized;
    sdmmc_card_t* _card;
    
    // 初期化時のSPI設定用パラメータ
    struct SDConfig {
        int pin_miso;
        int pin_mosi;
        int pin_sck;
        int pin_cs;
        int max_files;
        bool format_if_failed;
    };
    
    SDConfig _config;
    
public:
    SDCardWrapper();
    ~SDCardWrapper();
    
    // 初期化メソッド（デフォルト設定）
    bool init();
    
    // カスタム設定での初期化メソッド
    bool init(int pin_miso, int pin_mosi, int pin_sck, int pin_cs, 
             int max_files = 5, bool format_if_failed = false);
    
    // DataWrapperの抽象メソッドを実装
    int read(uint8_t *buf, uint32_t len) override;
    int read(uint8_t *buf, uint32_t maximum_len, uint32_t required_len) override;
    void skip(int32_t offset) override;
    bool seek(uint32_t position) override;
    bool seek(uint32_t position, int origin);
    void close(void) override;
    int32_t tell(void) override;
    
    bool open(const char* path) override; // DataWrapperのoverride
    
    // その他の便利なメソッド
    bool exists(const char* path);
    bool mkdir(const char* path);
    bool remove(const char* path);
    uint32_t size(const char* path);
    
    // 状態チェック
    bool isInitialized() { return _initialized; }
};

// グローバルインスタンス
extern SDCardWrapper SD;

#endif // _SDCARD_HPP_