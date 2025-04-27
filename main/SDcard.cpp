// SDcard.cpp
#include "SDcard.hpp"
#include <stdio.h>
#include <sys/stat.h>

SDCardWrapper SD; // グローバルインスタンス

SDCardWrapper::SDCardWrapper()
{
    _file = nullptr;
    _initialized = false;
    _card = nullptr;
    need_transaction = true; // DataWrapperのメンバ変数

    // デフォルト設定
    _config.pin_miso = GPIO_NUM_40; // デフォルトのMISOピン
    _config.pin_mosi = GPIO_NUM_38; // デフォルトのMOSIピン
    _config.pin_sck = GPIO_NUM_39;  // デフォルトのSCKピン
    _config.pin_cs = GPIO_NUM_47;   // デフォルトのCSピン
    _config.max_files = 5;
    _config.format_if_failed = false;
}

SDCardWrapper::~SDCardWrapper()
{
    close();
    // SDカードがマウントされていれば、アンマウント
    if (_initialized)
    {
        esp_vfs_fat_sdcard_unmount("/sdcard", _card);
        _initialized = false;
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

    // SPIバス初期化（ここでMISO、MOSI、SCKピンを設定）
    esp_err_t ret = spi_bus_initialize(SDSPI_DEFAULT_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK)
    {
        return false;
    }

    // SPI用のSDMMCホスト設定
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    // SPI用のスロット設定（ここではCSピンのみ設定）
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = (gpio_num_t)pin_cs;
    // MISO、MOSI、SCKはSPIバス初期化で設定済み！

    // マウント設定
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = format_if_failed,
        .max_files = max_files,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false,
        .use_one_fat = false};

    // SDカードのマウント（SPIモード）
    ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &_card);

    // 初期化成功したかどうか
    _initialized = (ret == ESP_OK);

    return _initialized;
}

bool SDCardWrapper::open(const char *path)
{
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
    if (strncmp(path, "/sdcard/", 8) != 0)
    {
        snprintf(full_path, sizeof(full_path), "/sdcard/%s", path);
        _file = fopen(full_path, "rb");
    }
    else
    {
        _file = fopen(path, "rb");
    }

    return (_file != nullptr);
}

void SDCardWrapper::close(void)
{
    if (_file)
    {
        fclose(_file);
        _file = nullptr;
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
    if (!_initialized)
        return false;

    char full_path[256];
    if (strncmp(path, "/sdcard/", 8) != 0)
    {
        snprintf(full_path, sizeof(full_path), "/sdcard/%s", path);
    }
    else
    {
        strncpy(full_path, path, sizeof(full_path));
    }

    struct stat st;
    return (stat(full_path, &st) == 0);
}

bool SDCardWrapper::mkdir(const char *path)
{
    if (!_initialized)
        return false;

    char full_path[256];
    if (strncmp(path, "/sdcard/", 8) != 0)
    {
        snprintf(full_path, sizeof(full_path), "/sdcard/%s", path);
    }
    else
    {
        strncpy(full_path, path, sizeof(full_path));
    }

    return (::mkdir(full_path, 0755) == 0);
}

bool SDCardWrapper::remove(const char *path)
{
    if (!_initialized)
        return false;

    char full_path[256];
    if (strncmp(path, "/sdcard/", 8) != 0)
    {
        snprintf(full_path, sizeof(full_path), "/sdcard/%s", path);
    }
    else
    {
        strncpy(full_path, path, sizeof(full_path));
    }

    return (::remove(full_path) == 0);
}

uint32_t SDCardWrapper::size(const char *path)
{
    if (!_initialized)
        return 0;

    char full_path[256];
    if (strncmp(path, "/sdcard/", 8) != 0)
    {
        snprintf(full_path, sizeof(full_path), "/sdcard/%s", path);
    }
    else
    {
        strncpy(full_path, path, sizeof(full_path));
    }

    struct stat st;
    if (stat(full_path, &st) == 0)
    {
        return st.st_size;
    }
    return 0;
}