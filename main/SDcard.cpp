// main/SDcard.cpp
#include "SDcard.hpp"
#include <stdio.h>
#include <sys/stat.h>
#include "tinyusb.h"
#include "tusb_msc_storage.h"
#include <dirent.h>

// ログタグ
const char* SDCardWrapper::TAG = "SD_CARD";

SDCardWrapper SD; // グローバルインスタンス

// MSCコールバック関数のプロトタイプ宣言
static int32_t onMscRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize);
static int32_t onMscWrite(uint32_t lba, uint32_t offset, const void* buffer, uint32_t bufsize);
static bool onMscIsReady(void);
static uint32_t onMscGetBlockCount(void);
static uint16_t onMscGetBlockSize(void);

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
    }

    return _initialized;
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
    if (strncmp(path, _config.mount_point, strlen(_config.mount_point)) != 0)
    {
        snprintf(full_path, sizeof(full_path), "%s/%s", _config.mount_point, path);
        _file = fopen(full_path, "rb");
    }
    else
    {
        _file = fopen(path, "rb");
    }

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
    if (strncmp(path, _config.mount_point, strlen(_config.mount_point)) != 0)
    {
        snprintf(full_path, sizeof(full_path), "%s/%s", _config.mount_point, path);
    }
    else
    {
        strncpy(full_path, path, sizeof(full_path));
    }

    struct stat st;
    bool exists = (stat(full_path, &st) == 0);
    ESP_LOGI(TAG, "File %s %s", full_path, exists ? "exists" : "does not exist");
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
    if (strncmp(path, _config.mount_point, strlen(_config.mount_point)) != 0)
    {
        snprintf(full_path, sizeof(full_path), "%s/%s", _config.mount_point, path);
    }
    else
    {
        strncpy(full_path, path, sizeof(full_path));
    }

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
    if (strncmp(path, _config.mount_point, strlen(_config.mount_point)) != 0)
    {
        snprintf(full_path, sizeof(full_path), "%s/%s", _config.mount_point, path);
    }
    else
    {
        strncpy(full_path, path, sizeof(full_path));
    }

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
    if (strncmp(path, _config.mount_point, strlen(_config.mount_point)) != 0)
    {
        snprintf(full_path, sizeof(full_path), "%s/%s", _config.mount_point, path);
    }
    else
    {
        strncpy(full_path, path, sizeof(full_path));
    }

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
    
    // TinyUSBタスクを開始 - 新APIを使用
    ESP_LOGI(TAG, "Starting TinyUSB task");
    // 最新バージョンではtud_init()関数を使用
    tud_init(TUD_OPT_RHPORT);
    
    // アプリケーションからのSDカードアクセスを無効化するためアンマウント
    ESP_LOGI(TAG, "Unmounting SD card from application to allow USB host access");
    esp_err_t ret = tinyusb_msc_storage_unmount();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount storage: %s", esp_err_to_name(ret));
        // tinyusb_disableの代わりにデバイスを停止
        tud_disconnect();
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
    
    // SDカードをアプリケーションに戻す
    ESP_LOGI(TAG, "Mounting SD card for application access");
    esp_err_t ret = tinyusb_msc_storage_mount(_config.mount_point);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount storage: %s", esp_err_to_name(ret));
        // 続行する
    }
    
    // TinyUSBタスクを停止 - 新APIを使用
    ESP_LOGI(TAG, "Stopping TinyUSB task");
    // tinyusb_disableの代わりにデバイスを停止
    tud_disconnect();
    
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

// MSCコールバック関数の実装
static bool onMscIsReady(void)
{
    return SD.isInitialized() && SD.getCard() != NULL;
}

static uint32_t onMscGetBlockCount(void)
{
    sdmmc_card_t* card = SD.getCard();
    if (card == NULL) {
        return 0;
    }
    return card->csd.capacity;
}

static uint16_t onMscGetBlockSize(void)
{
    sdmmc_card_t* card = SD.getCard();
    if (card == NULL) {
        return 0;
    }
    return card->csd.sector_size;
}

static int32_t onMscRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
    sdmmc_card_t* card = SD.getCard();
    if (card == NULL) {
        ESP_LOGE("SD on Read", "Card not available for read operation");
        return -1;
    }

    // SDカードからの読み込み
    ESP_LOGD("SD on Read", "Reading %lu bytes from SD at LBA %lu", bufsize, lba);
    if (ESP_OK != sdmmc_read_sectors(card, buffer, lba, bufsize / card->csd.sector_size)) {
        ESP_LOGE("SD on Read", "Failed to read from SD card");
        return -1;
    }
    return bufsize;
}

static int32_t onMscWrite(uint32_t lba, uint32_t offset, const void* buffer, uint32_t bufsize)
{
    sdmmc_card_t* card = SD.getCard();
    if (card == NULL) {
        ESP_LOGE("SD on write", "Card not available for write operation");
        return -1;
    }

    // SDカードへの書き込み
    ESP_LOGD("SD on write", "Writing %lu bytes to SD at LBA %lu", bufsize, lba);
    if (ESP_OK != sdmmc_write_sectors(card, buffer, lba, bufsize / card->csd.sector_size)) {
        ESP_LOGE("SD on write", "Failed to write to SD card");
        return -1;
    }
    return bufsize;
}

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
    if (strncmp(path, _config.mount_point, strlen(_config.mount_point)) != 0) {
        snprintf(full_path, sizeof(full_path), "%s/%s", _config.mount_point, path);
    } else {
        strncpy(full_path, path, sizeof(full_path));
    }

    // ディレクトリを開く
    DIR* dir = opendir(full_path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", full_path);
        return nullptr;
    }

    // ファイル数をカウントする（事前に配列サイズを決めるため）
    size_t file_count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        file_count++;
    }
    // ディレクトリを閉じて再度開く（最初から読み直す）
    closedir(dir);
    dir = opendir(full_path);

    // DirInfo構造体を確保
    DirInfo* dirInfo = (DirInfo*)malloc(sizeof(DirInfo));
    if (!dirInfo) {
        ESP_LOGE(TAG, "Failed to allocate memory for DirInfo");
        closedir(dir);
        return nullptr;
    }
    
    // ファイル情報の配列を確保
    dirInfo->files = (FileInfo*)malloc(file_count * sizeof(FileInfo));
    if (!dirInfo->files) {
        ESP_LOGE(TAG, "Failed to allocate memory for FileInfo array");
        free(dirInfo);
        closedir(dir);
        return nullptr;
    }
    
    // パスを保存
    strncpy(dirInfo->path, path, sizeof(dirInfo->path));
    dirInfo->count = file_count;
    
    // ファイル情報を取得
    size_t index = 0;
    char entry_path[512];
    struct stat st;
    
    while ((entry = readdir(dir)) != nullptr && index < file_count) {
        // ファイル名をコピー
        strncpy(dirInfo->files[index].name, entry->d_name, sizeof(dirInfo->files[index].name));
        
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
    ESP_LOGI(TAG, "Directory listing completed: %s, %d files found", path, dirInfo->count);
    return dirInfo;
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