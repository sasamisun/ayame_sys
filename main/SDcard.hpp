// main/SDcard.hpp
#ifndef _SDCARD_HPP_
#define _SDCARD_HPP_

#include <M5GFX.h>
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"

// ファイルとフォルダを表す構造体 - クラス宣言の前に配置
struct FileInfo {
    char name[256];      // ファイル/フォルダ名
    bool isDirectory;    // ディレクトリかどうか
    uint32_t size;       // ファイルサイズ（バイト）
    time_t lastModified; // 最終更新日時
};

// ディレクトリ内のファイル一覧を表す構造体 - クラス宣言の前に配置
struct DirInfo {
    FileInfo* files;     // ファイル・フォルダ情報の配列
    size_t count;        // ファイル・フォルダの数
    char path[256];      // 現在のパス
};

// DataWrapperを継承した独自クラス
class SDCardWrapper : public lgfx::v1::DataWrapper {
private:
    static const char* TAG;
    FILE* _file;
    bool _initialized;
    sdmmc_card_t* _card;
    bool _usbMscEnabled;  // USB MSC有効フラグ
    
    // 初期化時のSPI設定用パラメータ
    struct SDConfig {
        int pin_miso;
        int pin_mosi;
        int pin_sck;
        int pin_cs;
        int max_files;
        bool format_if_failed;
        const char* mount_point;
    };
    
    SDConfig _config;
    
    // MSC関連の内部関数
    bool initMSC();
    //bool initMSC(const char*, const char*, const char*);
    
public:
    SDCardWrapper();
    ~SDCardWrapper();
    
    // 初期化メソッド（デフォルト設定）
    bool init();
    bool begin() { return init(); }  // Arduino互換の名前を追加
    
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
    
    // USBマスストレージ関連の機能
    /**
     * @brief USB MSCを初期化して有効化する
     * 
     * @param vendor_str ベンダー名
     * @param product_str 製品名
     * @param serial_str シリアル番号
     * @return 初期化が成功したかどうか
     */
    bool enableUSBMSC();
    
    /**
     * @brief USB MSCを無効化する
     * 
     * @return 無効化が成功したかどうか
     */
    bool disableUSBMSC();
    
    /**
     * @brief USB MSCが有効かどうかを取得する
     * 
     * @return 有効ならtrue、そうでなければfalse
     */
    bool isUSBMSCEnabled() const { return _usbMscEnabled; }
    
    /**
     * @brief USB MSCの接続状態をチェックする
     * 
     * @return 接続されていればtrue、そうでなければfalse
     */
    bool isUSBMSCConnected();
    
    // SDカードのハンドラを取得（USB MSC用）
    sdmmc_card_t* getCard() { return _card; }
    
    // 新しく追加したメソッド: ディレクトリ内のファイル一覧を取得
    DirInfo* listDir(const char* path);
    
    // 新しく追加したメソッド: DirInfo構造体のメモリを解放
    void freeDirInfo(DirInfo* dirInfo);
    
    // operator overload for Arduino compatibility
    operator bool() { return _initialized; }
};

// グローバルインスタンス
extern SDCardWrapper SD;

#endif // _SDCARD_HPP_