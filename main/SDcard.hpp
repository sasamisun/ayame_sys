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

    /**
     * @brief マウントポイントを補ってフルパスを構築する
     *
     * path が既に mount_point で始まっていればそのまま複製し、そうでなければ
     * "<mount_point>/<path>" を組み立てる。snprintf を使うため出力は常にNUL終端される。
     *
     * @param path    入力パス（相対・絶対どちらでもよい）
     * @param out     出力バッファ
     * @param outSize 出力バッファのサイズ
     * @return 成功時true。切り詰めが発生した場合や引数が不正な場合はfalse
     */
    bool buildFullPath(const char* path, char* out, size_t outSize) const;

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

    /**
     * @brief ファイル全体を PSRAM に読み込む
     *
     * `size()` → `open()` → `read()` → `close()` をまとめたもの。
     * 末尾に NUL を足すので、テキストならそのまま C 文字列として使える。
     *
     * ## なぜ全部読むのか
     *
     * 本クラスは `FILE*` を1本しか持たないため、**同時に開けるファイルは1つ**。
     * シナリオJSONを開いたままだと画像を描画できない。
     * JSONは最初に全部メモリへ載せて閉じてしまい、以降のSDアクセスを
     * 画像とセーブに明け渡す。
     *
     * ## 確保先
     *
     * PSRAM を明示指定する。`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384` により
     * 16KB 未満は内部RAMへ行ってしまうため、小さいファイルでも
     * 内部RAM（起動時312KB）を削らないようにしている。
     *
     * @param path    読み込むファイル（`/sdcard` 起点の相対パスでよい）
     * @param outLen  [out] 読み込んだバイト数（NUL は含まない）。不要なら nullptr
     * @return 確保したバッファ。**呼び出し側が free() すること**。失敗時は nullptr
     *
     * @note USB MSC が有効な間は必ず失敗する（全ファイル操作が禁止されるため）。
     */
    char* readFileToBuffer(const char* path, size_t* outLen = nullptr);

    /**
     * @brief バッファの内容をファイルへ書き出す（既存は上書き）
     *
     * 本クラスは長らく読み込み専用（`open()` が `"rb"` 固定）で、
     * 書き込みの手段が `mkdir` / `remove` しか無かった。
     * 設定やセーブデータを残すために追加したもの。
     *
     * @param path 書き出し先（`/sdcard` 起点の相対パスでよい）
     * @param data 書き出す内容
     * @param len  バイト数
     * @return 成功したか
     *
     * @note 親ディレクトリは**作らない**。無ければ失敗する。
     *       必要なら呼び出し側で `mkdir()` すること。
     * @note USB MSC が有効な間は必ず失敗する（全ファイル操作が禁止されるため）。
     *       PC 側と同時に書くと内容が壊れるため、これは意図した制限。
     */
    bool writeFileFromBuffer(const char* path, const void* data, size_t len);

    /**
     * @brief ファイルの先頭だけを読む
     *
     * ファイル全体は要らず、冒頭の情報だけ欲しい場合に使う。
     * メニューが各シナリオのタイトルを引くのが主な用途で、
     * `readFileToBuffer()` で全文を読むと**シナリオが増えるほど表示が重くなる**。
     *
     * @param path   読み込むファイル
     * @param out    書き込み先。末尾に NUL を置くので `maxLen` は
     *               「NUL を含む」大きさで渡すこと
     * @param maxLen `out` の大きさ
     * @return 読み込めたバイト数（NUL を含まない）。失敗時は 0
     *
     * @note ファイルが `maxLen` より小さければその分だけ読む。
     * @note **途中で切れた内容が返る**ので、JSON として解析してはいけない。
     *       文字列として走査する用途に限る。
     * @note USB MSC が有効な間は必ず失敗する。
     */
    size_t readFilePrefix(const char* path, char* out, size_t maxLen);
    
    // operator overload for Arduino compatibility
    operator bool() { return _initialized; }
};

// グローバルインスタンス
extern SDCardWrapper SD;

#endif // _SDCARD_HPP_