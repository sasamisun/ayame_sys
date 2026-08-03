// main/SDcard.cpp
#include "SDcard.hpp"
#include <stdio.h>
#include <sys/stat.h>
#include "tinyusb.h"
#include "tusb_msc_storage.h"
#include <dirent.h>
#include "esp_heap_caps.h"   // heap_caps_malloc（readFileToBuffer で PSRAM を明示指定）

// ログタグ
const char* SDCardWrapper::TAG = "SD_CARD";

SDCardWrapper SD; // グローバルインスタンス

// 各イベント用のコールバック関数を定義
static void onMscMountChanged(tinyusb_msc_event_t *event)
{
    // マウント状態が変更された時のイベント処理
    ESP_LOGI("SD_CARD", "MSC Mount state changed: mounted = %d", event->mount_changed_data.is_mounted);
}

// メイン処理
SDCardWrapper::SDCardWrapper()
{
    _file = nullptr;
    _initialized = false;
    _card = nullptr;
    _usbMscEnabled = false;
    need_transaction = true; // DataWrapperのメンバ変数

    // デフォルト設定 - M5Paper S3のSPIピン
    _config.pin_miso = GPIO_NUM_40; // SPI MISO ピン
    _config.pin_mosi = GPIO_NUM_38; // SPI MOSI ピン
    _config.pin_sck = GPIO_NUM_39;  // SPI SCK ピン
    _config.pin_cs = GPIO_NUM_47;   // SPI CS ピン
    _config.max_files = 5;
    _config.format_if_failed = false;
    _config.mount_point = "/sdcard";
}

SDCardWrapper::~SDCardWrapper()
{
    // USB MSCを無効化
    if (_usbMscEnabled) {
        disableUSBMSC();
    }

    close();
    // SDカードがマウントされていれば、アンマウント
    if (_initialized && _card != nullptr)
    {
        esp_vfs_fat_sdcard_unmount(_config.mount_point, _card);
        _initialized = false;
        _card = nullptr;
    }
}

bool SDCardWrapper::init()
{
    // デフォルト設定で初期化
    return init(_config.pin_miso, _config.pin_mosi, _config.pin_sck, _config.pin_cs,
                _config.max_files, _config.format_if_failed);
}

bool SDCardWrapper::init(int pin_miso, int pin_mosi, int pin_sck, int pin_cs,
                         int max_files, bool format_if_failed)
{
    // すでに初期化されていれば何もしない
    if (_initialized)
        return true;
        
    // USB MSCが有効になっている場合は無効化する
    if (_usbMscEnabled) {
        disableUSBMSC();
    }

    // 設定を保存
    _config.pin_miso = pin_miso;
    _config.pin_mosi = pin_mosi;
    _config.pin_sck = pin_sck;
    _config.pin_cs = pin_cs;
    _config.max_files = max_files;
    _config.format_if_failed = format_if_failed;

    // SPIバス設定
    spi_bus_config_t bus_cfg = {};

    // 必要な値だけ設定
    bus_cfg.mosi_io_num = pin_mosi;
    bus_cfg.miso_io_num = pin_miso;
    bus_cfg.sclk_io_num = pin_sck;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 4000;

    ESP_LOGI(TAG, "Initializing SPI bus for SD card. MISO: %d, MOSI: %d, SCK: %d, CS: %d", 
             pin_miso, pin_mosi, pin_sck, pin_cs);

    // SPIバス初期化（ここでMISO、MOSI、SCKピンを設定）
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI bus initialization failed: %s", esp_err_to_name(ret));
        return false;
    }

    // SPI用のSDMMCホスト設定
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    // SPI用のスロット設定（ここではCSピンのみ設定）
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = (gpio_num_t)pin_cs;
    slot_config.host_id = SPI2_HOST;
    // MISO、MOSI、SCKはSPIバス初期化で設定済み

    // マウント設定
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = format_if_failed,
        .max_files = max_files,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false,
        .use_one_fat = false};

    // SDカードのマウント（SPIモード）
    ESP_LOGI(TAG, "Mounting SD card via SPI...");
    ret = esp_vfs_fat_sdspi_mount(_config.mount_point, &host, &slot_config, &mount_config, &_card);

    // 初期化成功したかどうか
    _initialized = (ret == ESP_OK);
    
    if (_initialized) {
        ESP_LOGI(TAG, "SD card initialized successfully. Card info:");
        ESP_LOGI(TAG, "Name: %s", _card->cid.name);
        ESP_LOGI(TAG, "Capacity: %lluMB", ((uint64_t)_card->csd.capacity * _card->csd.sector_size) / (1024 * 1024));
        ESP_LOGI(TAG, "Sector size: %d bytes", _card->csd.sector_size);
    } else {
        ESP_LOGE(TAG, "Failed to initialize SD card: %s", esp_err_to_name(ret));

        // マウントに失敗したらSPIバスを解放して、初期化前の状態に戻す。
        // 解放しないと次回 init() の spi_bus_initialize() が
        // ESP_ERR_INVALID_STATE を返すため、カード挿抜によるリトライが
        // 永久に失敗してしまう。
        _card = nullptr;
        esp_err_t free_ret = spi_bus_free(SPI2_HOST);
        if (free_ret != ESP_OK) {
            ESP_LOGW(TAG, "spi_bus_free() failed: %s", esp_err_to_name(free_ret));
        }
    }

    return _initialized;
}



bool SDCardWrapper::buildFullPath(const char *path, char *out, size_t outSize) const
{
    if (!path || !out || outSize == 0) {
        ESP_LOGE(TAG, "buildFullPath: invalid argument");
        return false;
    }

    int written;
    if (strncmp(path, _config.mount_point, strlen(_config.mount_point)) != 0) {
        written = snprintf(out, outSize, "%s/%s", _config.mount_point, path);
    } else {
        written = snprintf(out, outSize, "%s", path);
    }

    // snprintf は必ずNUL終端するが、戻り値が outSize 以上なら切り詰められている。
    // 従来この処理は strncpy で書かれており、切り詰め時にNUL終端されず
    // 後続の stat()/fopen() がバッファ外を読む可能性があった。
    if (written < 0 || static_cast<size_t>(written) >= outSize) {
        ESP_LOGE(TAG, "Path too long (needs %d bytes, buffer is %u): %s",
                 written, static_cast<unsigned>(outSize), path);
        return false;
    }

    return true;
}

bool SDCardWrapper::open(const char *path)
{
    // USB MSCが有効な場合はファイルアクセスできない
    if (_usbMscEnabled) {
        ESP_LOGE(TAG, "Cannot open file while USB MSC is enabled");
        return false;
    }

    close();

    // 初期化されていなければ初期化を試みる
    if (!_initialized)
    {
        if (!init())
        {
            return false;
        }
    }

    // 完全なパスを構築（/sdcardプレフィックスが無い場合は追加）
    char full_path[256];
    if (!buildFullPath(path, full_path, sizeof(full_path)))
    {
        return false;
    }

    _file = fopen(full_path, "rb");

    if (_file) {
        ESP_LOGI(TAG, "Opened file: %s", path);
    } else {
        ESP_LOGE(TAG, "Failed to open file: %s", path);
    }

    return (_file != nullptr);
}

void SDCardWrapper::close(void)
{
    if (_file)
    {
        fclose(_file);
        _file = nullptr;
        ESP_LOGI(TAG, "File closed");
    }
}

int SDCardWrapper::read(uint8_t *buf, uint32_t len)
{
    if (!_file)
        return 0;

    if (parent && fp_pre_read)
        fp_pre_read(parent); // DataWrapperのメンバ関数を使用

    int result = fread(buf, 1, len, _file);

    if (parent && fp_post_read)
        fp_post_read(parent); // DataWrapperのメンバ関数を使用

    return result;
}

int SDCardWrapper::read(uint8_t *buf, uint32_t maximum_len, uint32_t required_len)
{
    // DataWrapperのread()オーバーロードを実装
    (void)required_len; // 今回は使わないがオーバーライドは必要
    return read(buf, maximum_len);
}

void SDCardWrapper::skip(int32_t offset)
{
    if (!_file)
        return;
    fseek(_file, offset, SEEK_CUR);
}

bool SDCardWrapper::seek(uint32_t position)
{
    if (!_file)
        return false;
    return 0 == fseek(_file, position, SEEK_SET);
}

bool SDCardWrapper::seek(uint32_t position, int origin)
{
    if (!_file)
        return false;
    return 0 == fseek(_file, position, origin);
}

int32_t SDCardWrapper::tell(void)
{
    if (!_file)
        return 0;
    return ftell(_file);
}

bool SDCardWrapper::exists(const char *path)
{
    // USB MSCが有効な場合はファイルアクセスできない
    if (_usbMscEnabled) {
        ESP_LOGE(TAG, "Cannot check file existence while USB MSC is enabled");
        return false;
    }

    if (!_initialized)
        return false;

    char full_path[256];
    if (!buildFullPath(path, full_path, sizeof(full_path)))
        return false;

    struct stat st;
    bool exists = (stat(full_path, &st) == 0);
    // 探索ループから呼ばれるとログが溢れるため ESP_LOGD にする
    ESP_LOGD(TAG, "File %s %s", full_path, exists ? "exists" : "does not exist");
    return exists;
}

bool SDCardWrapper::mkdir(const char *path)
{
    // USB MSCが有効な場合はファイルアクセスできない
    if (_usbMscEnabled) {
        ESP_LOGE(TAG, "Cannot create directory while USB MSC is enabled");
        return false;
    }

    if (!_initialized)
        return false;

    char full_path[256];
    if (!buildFullPath(path, full_path, sizeof(full_path)))
        return false;

    bool result = (::mkdir(full_path, 0755) == 0);
    if (result) {
        ESP_LOGI(TAG, "Directory created: %s", full_path);
    } else {
        ESP_LOGE(TAG, "Failed to create directory: %s", full_path);
    }
    return result;
}

bool SDCardWrapper::remove(const char *path)
{
    // USB MSCが有効な場合はファイルアクセスできない
    if (_usbMscEnabled) {
        ESP_LOGE(TAG, "Cannot remove file while USB MSC is enabled");
        return false;
    }

    if (!_initialized)
        return false;

    char full_path[256];
    if (!buildFullPath(path, full_path, sizeof(full_path)))
        return false;

    bool result = (::remove(full_path) == 0);
    if (result) {
        ESP_LOGI(TAG, "File removed: %s", full_path);
    } else {
        ESP_LOGE(TAG, "Failed to remove file: %s", full_path);
    }
    return result;
}

uint32_t SDCardWrapper::size(const char *path)
{
    // USB MSCが有効な場合はファイルアクセスできない
    if (_usbMscEnabled) {
        ESP_LOGE(TAG, "Cannot get file size while USB MSC is enabled");
        return 0;
    }

    if (!_initialized)
        return 0;

    char full_path[256];
    if (!buildFullPath(path, full_path, sizeof(full_path)))
        return 0;

    struct stat st;
    if (stat(full_path, &st) == 0)
    {
        ESP_LOGI(TAG, "File size: %lu bytes", (uint32_t)st.st_size);
        return st.st_size;
    }
    ESP_LOGE(TAG, "Failed to get file size: %s", full_path);
    return 0;
}

bool SDCardWrapper::initMSC()
{
    if (!_initialized) {
        ESP_LOGE(TAG, "SD card must be initialized before initializing USB MSC");
        return false;
    }

    // USBデバイス設定 - すべてのフィールドを初期化
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,          // デフォルトのデバイス記述子を使用
        .string_descriptor = NULL,          // デフォルトの文字列記述子を使用
        .string_descriptor_count = 0,
        .external_phy = false,              // 内部PHYを使用
        .configuration_descriptor = NULL,   // デフォルトのコンフィグ記述子を使用
        .self_powered = false,              // バスパワー
        .vbus_monitor_io = -1               // VBUSモニターなし
    };

    // TinyUSBスタックの初期化
    ESP_LOGI(TAG, "Initializing TinyUSB for MSC");
    esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize TinyUSB: %s", esp_err_to_name(ret));
        return false;
    }

    // MSC SDカード設定 - すべてのフィールドを初期化
    const tinyusb_msc_sdmmc_config_t config_sdmmc = {
        .card = _card,                      // SDカードハンドル
        .callback_mount_changed = onMscMountChanged,  // マウント変更コールバック
        .callback_premount_changed = NULL,  // プレマウント変更コールバック（使用しない）
        .mount_config = {
            .format_if_mount_failed = false,
            .max_files = _config.max_files,
            .allocation_unit_size = 16 * 1024,
            .disk_status_check_enable = false,
            .use_one_fat = false
        }
    };

    // MSC設定 - 新しいAPIを使用
    ret = tinyusb_msc_storage_init_sdmmc(&config_sdmmc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MSC storage: %s", esp_err_to_name(ret));
        return false;
    }
    
    ESP_LOGI(TAG, "USB MSC initialized successfully");
    return true;
}

bool SDCardWrapper::enableUSBMSC()
{
    if (!_initialized) {
        ESP_LOGE(TAG, "SD card must be initialized before enabling USB MSC");
        return false;
    }
    
    // 既に有効化されていれば何もしない
    if (_usbMscEnabled) {
        ESP_LOGI(TAG, "USB MSC already enabled");
        return true;
    }
    
    // ファイルがオープンされている場合はクローズする
    if (_file) {
        ESP_LOGI(TAG, "Closing open file before enabling USB MSC");
        close();
    }
    
    // USB MSCを初期化
    if (!initMSC()) {
        ESP_LOGE(TAG, "Failed to initialize USB MSC");
        return false;
    }
    
    // ここで以前は tud_init(TUD_OPT_RHPORT) を呼んでいたが削除した。
    // initMSC() 内の tinyusb_driver_install() が
    //   ・tusb_init()     （CONFIG_TINYUSB_INIT_IN_DEFAULT_TASK が未設定のため）
    //   ・tusb_run_task() （CONFIG_TINYUSB_NO_DEFAULT_TASK が未設定のため）
    // を既に実行しており、TinyUSBスタックの二重初期化になっていた。

    // アプリケーションからのSDカードアクセスを無効化するためアンマウント
    ESP_LOGI(TAG, "Unmounting SD card from application to allow USB host access");
    esp_err_t ret = tinyusb_msc_storage_unmount();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount storage: %s", esp_err_to_name(ret));

        // ここまでに確保したものを対称に巻き戻す（setupの逆順）。
        // 巻き戻さないと次回の enableUSBMSC() で
        // tinyusb_driver_install() が ESP_ERR_INVALID_STATE になる。
        tinyusb_msc_storage_deinit();
        esp_err_t un_ret = tinyusb_driver_uninstall();
        if (un_ret != ESP_OK) {
            ESP_LOGW(TAG, "tinyusb_driver_uninstall() failed: %s", esp_err_to_name(un_ret));
        }
        return false;
    }

    _usbMscEnabled = true;
    ESP_LOGI(TAG, "USB MSC enabled successfully");
    return true;
}

bool SDCardWrapper::disableUSBMSC()
{
    // 有効化されていなければ何もしない
    if (!_usbMscEnabled) {
        ESP_LOGI(TAG, "USB MSC already disabled");
        return true;
    }
    
    // SDカードをアプリケーションに戻す。
    // tinyusb_msc_storage_deinit() は FATFS をアンマウントせずハンドルを解放するだけなので、
    // ここで先に再マウントしておけば、以降もアプリから /sdcard を読める。
    ESP_LOGI(TAG, "Mounting SD card for application access");
    esp_err_t ret = tinyusb_msc_storage_mount(_config.mount_point);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount storage: %s", esp_err_to_name(ret));
        // 続行する（下でTinyUSB側は確実に解放する）
    }

    // MSCストレージとTinyUSBドライバをセットアップの逆順で解放する。
    // 以前は tud_disconnect() のみで tinyusb_driver_uninstall() を呼んでいなかったため、
    // 再度 enableUSBMSC() すると tinyusb_driver_install() が
    // ESP_ERR_INVALID_STATE を返し、有効化→無効化→有効化のトグルが
    // 2周目で必ず失敗していた。
    ESP_LOGI(TAG, "Deinitializing MSC storage and uninstalling TinyUSB driver");
    tinyusb_msc_storage_deinit();
    esp_err_t un_ret = tinyusb_driver_uninstall();
    if (un_ret != ESP_OK) {
        ESP_LOGW(TAG, "tinyusb_driver_uninstall() failed: %s", esp_err_to_name(un_ret));
    }

    _usbMscEnabled = false;
    ESP_LOGI(TAG, "USB MSC disabled successfully");
    return true;
}

bool SDCardWrapper::isUSBMSCConnected()
{
    if (!_usbMscEnabled) {
        return false;
    }
    
    // USBホストとの接続状態をチェック
    bool connected = tinyusb_msc_storage_in_use_by_usb_host();
    ESP_LOGD(TAG, "USB MSC connection status: %s", connected ? "Connected" : "Disconnected");
    return connected;
}

// 補足: onMscRead / onMscWrite / onMscIsReady / onMscGetBlockCount /
// onMscGetBlockSize の5関数をここに実装していたが、いずれも登録・参照されておらず
// -Wunused-function の警告源になっていたため削除した。
// セクタI/Oは tinyusb_msc_storage_init_sdmmc() が内部で処理するため不要。
// 独自のセクタI/Oフックが必要になった場合は tinyusb_msc_storage の
// API ドキュメントを確認のうえ、登録処理と対で追加すること。

DirInfo* SDCardWrapper::listDir(const char* path)
{
    // USB MSCが有効な場合はファイルアクセスできない
    if (_usbMscEnabled) {
        ESP_LOGE(TAG, "Cannot list directory while USB MSC is enabled");
        return nullptr;
    }

    if (!_initialized) {
        ESP_LOGE(TAG, "SD card not initialized");
        return nullptr;
    }

    // 完全なパスを構築（/sdcardプレフィックスが無い場合は追加）
    char full_path[256];
    if (!buildFullPath(path, full_path, sizeof(full_path))) {
        return nullptr;
    }

    // 1パス目: エントリ数をカウントする（配列サイズを決めるため）
    DIR* dir = opendir(full_path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", full_path);
        return nullptr;
    }

    size_t file_count = 0;
    struct dirent* entry;
    while (readdir(dir) != nullptr) {
        file_count++;
    }
    closedir(dir);

    // 2パス目: 先頭から読み直す。
    // 以前はこの opendir() の戻り値を検査しておらず、失敗すると
    // readdir(nullptr) を呼んでしまう状態だった。
    dir = opendir(full_path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to reopen directory: %s", full_path);
        return nullptr;
    }

    // DirInfo構造体を確保
    DirInfo* dirInfo = (DirInfo*)malloc(sizeof(DirInfo));
    if (!dirInfo) {
        ESP_LOGE(TAG, "Failed to allocate memory for DirInfo");
        closedir(dir);
        return nullptr;
    }

    // ファイル情報の配列を確保（空ディレクトリでは malloc(0) を避ける）
    if (file_count > 0) {
        dirInfo->files = (FileInfo*)malloc(file_count * sizeof(FileInfo));
        if (!dirInfo->files) {
            ESP_LOGE(TAG, "Failed to allocate memory for FileInfo array");
            free(dirInfo);
            closedir(dir);
            return nullptr;
        }
    } else {
        dirInfo->files = nullptr;
    }

    // パスを保存（snprintf なので必ずNUL終端される）
    snprintf(dirInfo->path, sizeof(dirInfo->path), "%s", path);
    dirInfo->count = 0;   // 実際に読めた件数で後から確定する

    // ファイル情報を取得
    size_t index = 0;
    char entry_path[512];
    struct stat st;

    while (index < file_count && (entry = readdir(dir)) != nullptr) {
        // ファイル名をコピー（snprintf なので必ずNUL終端される）
        snprintf(dirInfo->files[index].name, sizeof(dirInfo->files[index].name),
                 "%s", entry->d_name);

        // ファイルの詳細情報を取得
        snprintf(entry_path, sizeof(entry_path), "%s/%s", full_path, entry->d_name);
        if (stat(entry_path, &st) == 0) {
            dirInfo->files[index].isDirectory = S_ISDIR(st.st_mode);
            dirInfo->files[index].size = st.st_size;
            dirInfo->files[index].lastModified = st.st_mtime;
        } else {
            // stat取得失敗時はデフォルト値を設定
            dirInfo->files[index].isDirectory = false;
            dirInfo->files[index].size = 0;
            dirInfo->files[index].lastModified = 0;
        }
        
        index++;
    }

    closedir(dir);

    // 実際に読み取れた件数で確定する。
    // 1パス目と2パス目の間にエントリが減っていた場合、以前は count に
    // 1パス目の値を入れていたため、末尾要素が未初期化メモリのまま
    // 呼び出し側へ渡されていた。
    dirInfo->count = index;
    if (index != file_count) {
        ESP_LOGW(TAG, "Directory changed during listing: counted %u, read %u",
                 static_cast<unsigned>(file_count), static_cast<unsigned>(index));
    }

    ESP_LOGI(TAG, "Directory listing completed: %s, %u entries found",
             path, static_cast<unsigned>(dirInfo->count));
    return dirInfo;
}

char* SDCardWrapper::readFileToBuffer(const char* path, size_t* outLen)
{
    if (outLen) {
        *outLen = 0;
    }

    // USB MSC 中は size() も open() も失敗するので、
    // 中途半端に進まないよう先に弾いて理由を明確に出す
    if (_usbMscEnabled) {
        ESP_LOGE(TAG, "Cannot read file while USB MSC is enabled: %s", path);
        return nullptr;
    }

    const uint32_t fileSize = size(path);
    if (fileSize == 0) {
        ESP_LOGE(TAG, "File is empty or not found: %s", path);
        return nullptr;
    }

    // PSRAM を明示指定する。既定の malloc では
    // CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384 により
    // 16KB 未満が内部RAMへ行き、内部RAMを削ってしまう。
    char* buffer = static_cast<char*>(
        heap_caps_malloc(fileSize + 1, MALLOC_CAP_SPIRAM));
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate %lu bytes in PSRAM for %s",
                 static_cast<unsigned long>(fileSize + 1), path);
        return nullptr;
    }

    if (!open(path)) {
        ESP_LOGE(TAG, "Failed to open: %s", path);
        free(buffer);
        return nullptr;
    }

    // 短く返ることがあるので、要求量に届くまで繰り返す
    size_t total = 0;
    while (total < fileSize) {
        const int got = read(reinterpret_cast<uint8_t*>(buffer) + total,
                             fileSize - total);
        if (got <= 0) {
            break;
        }
        total += static_cast<size_t>(got);
    }
    close();

    if (total != fileSize) {
        ESP_LOGE(TAG, "Short read on %s: %u of %lu bytes",
                 path, static_cast<unsigned>(total),
                 static_cast<unsigned long>(fileSize));
        free(buffer);
        return nullptr;
    }

    buffer[total] = '\0';   // テキストとしてそのまま扱えるように
    if (outLen) {
        *outLen = total;
    }

    ESP_LOGI(TAG, "Read %u bytes from %s", static_cast<unsigned>(total), path);
    return buffer;
}

size_t SDCardWrapper::readFilePrefix(const char* path, char* out, size_t maxLen)
{
    if (!out || maxLen == 0) {
        return 0;
    }
    out[0] = '\0';

    if (_usbMscEnabled) {
        ESP_LOGD(TAG, "Cannot read while USB MSC is enabled: %s", path);
        return 0;
    }
    if (!open(path)) {
        return 0;
    }

    // 末尾の NUL のぶんを残す
    const size_t want = maxLen - 1;
    size_t total = 0;

    while (total < want) {
        const int got = read(reinterpret_cast<uint8_t*>(out) + total, want - total);
        if (got <= 0) {
            break;   // ファイルが短ければここで終わる
        }
        total += static_cast<size_t>(got);
    }
    close();

    out[total] = '\0';
    return total;
}

bool SDCardWrapper::writeFileFromBuffer(const char* path, const void* data, size_t len)
{
    // USB MSC 中は PC 側がストレージを握っている。
    // ここで書くと双方の書き込みが衝突してファイルシステムが壊れる。
    if (_usbMscEnabled) {
        ESP_LOGE(TAG, "Cannot write while USB MSC is enabled: %s", path);
        return false;
    }

    if (!_initialized) {
        ESP_LOGE(TAG, "SD card is not initialized");
        return false;
    }
    if (!path || !data) {
        return false;
    }

    char full_path[256];
    if (!buildFullPath(path, full_path, sizeof(full_path))) {
        return false;
    }

    // 読み込み用の _file とは別に開く。
    // この関数の中で開いて閉じるので、_file の「同時に1つ」制約には触れない。
    FILE* out = fopen(full_path, "wb");
    if (!out) {
        ESP_LOGE(TAG, "Failed to open for writing: %s", full_path);
        return false;
    }

    const size_t written = fwrite(data, 1, len, out);
    const int closeResult = fclose(out);

    if (written != len) {
        ESP_LOGE(TAG, "Short write on %s: %u of %u bytes",
                 full_path, static_cast<unsigned>(written), static_cast<unsigned>(len));
        return false;
    }
    if (closeResult != 0) {
        // fclose の失敗はフラッシュに失敗した可能性がある。
        // 書けたように見えて中身が落ちていないことがあるので、成功にしない。
        ESP_LOGE(TAG, "fclose failed on %s", full_path);
        return false;
    }

    ESP_LOGI(TAG, "Wrote %u bytes to %s", static_cast<unsigned>(len), path);
    return true;
}

void SDCardWrapper::freeDirInfo(DirInfo* dirInfo)
{
    if (dirInfo) {
        if (dirInfo->files) {
            free(dirInfo->files);
        }
        free(dirInfo);
    }
}