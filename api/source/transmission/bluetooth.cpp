#include "api/transmission/bluetooth.h"

#include <cstdio>
#include <cstring>

#if __has_include("esp_gap_ble_api.h")
#define GLYTT_BLE_AVAILABLE 1
#include "esp_gap_ble_api.h"
#else
#define GLYTT_BLE_AVAILABLE 0
#endif

#if GLYTT_BLE_AVAILABLE
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#endif

Bluetooth *Bluetooth::instance = nullptr;

#if GLYTT_BLE_AVAILABLE
static void onGapEventBridge(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    Bluetooth *bluetooth = Bluetooth::Instance();
    if (bluetooth == nullptr || param == nullptr)
        return;

    if (event == ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT)
    {
        bluetooth->HandleScanStopped();
        return;
    }

    if (event != ESP_GAP_BLE_SCAN_RESULT_EVT)
        return;

    const esp_ble_gap_cb_param_t::ble_scan_result_evt_param &scanResult = param->scan_rst;
    if (scanResult.search_evt != ESP_GAP_SEARCH_INQ_RES_EVT)
        return;

    BluetoothAdvertisement advertisement;
    snprintf(advertisement.mac,
             sizeof(advertisement.mac),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             scanResult.bda[0],
             scanResult.bda[1],
             scanResult.bda[2],
             scanResult.bda[3],
             scanResult.bda[4],
             scanResult.bda[5]);

    advertisement.rssi = scanResult.rssi;
    advertisement.addressType = (Byte)scanResult.ble_addr_type;
    advertisement.eventType = (Byte)scanResult.ble_evt_type;
    advertisement.payloadLength = (unsigned short)(scanResult.adv_data_len + scanResult.scan_rsp_len);
    advertisement.timestampUs = (unsigned long long)esp_timer_get_time();

    unsigned char nameLength = 0;
    unsigned char *nameData = esp_ble_resolve_adv_data(const_cast<unsigned char *>(scanResult.ble_adv),
                                                       ESP_BLE_AD_TYPE_NAME_CMPL,
                                                       &nameLength);

    if (nameData != nullptr && nameLength > 0)
    {
        size_t copyLength = nameLength;
        if (copyLength > sizeof(advertisement.name) - 1)
            copyLength = sizeof(advertisement.name) - 1;

        memcpy(advertisement.name, nameData, copyLength);
        advertisement.name[copyLength] = '\0';
    }

    bluetooth->HandleAdvertisement(advertisement);
}
#endif

Bluetooth::Bluetooth()
{
    if (instance == nullptr)
        instance = this;
}

Bluetooth::~Bluetooth()
{
    this->StopScan();
    this->Stop();

    if (instance == this)
        instance = nullptr;
}

Bluetooth *Bluetooth::Instance()
{
    return instance;
}

bool Bluetooth::Start()
{
    if (this->started)
        return true;

#if !GLYTT_BLE_AVAILABLE
    printf("[BT][ERROR] BLE headers unavailable. Enable Bluetooth stack in sdkconfig.\n");
    return false;
#else
#if !defined(CONFIG_BT_BLUEDROID_ENABLED) || !CONFIG_BT_BLUEDROID_ENABLED
    printf("[BT][ERROR] This BLE scanner backend requires CONFIG_BT_BLUEDROID_ENABLED=y.\n");
    return false;
#endif

    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        if (nvs_flash_erase() != ESP_OK)
        {
            printf("[BT][ERROR] nvs_flash_erase failed\n");
            return false;
        }

        result = nvs_flash_init();
    }

    if (result != ESP_OK && result != ESP_ERR_NVS_NOT_INITIALIZED)
    {
        printf("[BT][ERROR] nvs_flash_init failed err=0x%x\n", result);
        return false;
    }

    (void)esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

    esp_bt_controller_status_t status = esp_bt_controller_get_status();
    if (status == ESP_BT_CONTROLLER_STATUS_IDLE)
    {
        esp_bt_controller_config_t btConfig = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        esp_err_t err = esp_bt_controller_init(&btConfig);
        if (err != ESP_OK)
        {
            printf("[BT][ERROR] esp_bt_controller_init failed err=0x%x\n", err);
            return false;
        }
    }

    status = esp_bt_controller_get_status();
    if (status == ESP_BT_CONTROLLER_STATUS_INITED)
    {
        esp_err_t err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
        if (err != ESP_OK)
        {
            printf("[BT][ERROR] esp_bt_controller_enable failed err=0x%x\n", err);
            return false;
        }
    }

    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED)
    {
        esp_err_t err = esp_bluedroid_init();
        if (err != ESP_OK)
        {
            printf("[BT][ERROR] esp_bluedroid_init failed err=0x%x\n", err);
            return false;
        }
    }

    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED)
    {
        esp_err_t err = esp_bluedroid_enable();
        if (err != ESP_OK)
        {
            printf("[BT][ERROR] esp_bluedroid_enable failed err=0x%x\n", err);
            return false;
        }
    }

    // Keep runtime identity ephemeral if discoverable roles are enabled later.
    char randomName[32] = {0};
    unsigned int suffix = esp_random() & 0xFFFFFFu;
    snprintf(randomName, sizeof(randomName), "GLYTT-%06X", suffix);
    if (esp_bt_dev_set_device_name(randomName) == ESP_OK)
        printf("[BT][INIT] deviceName=%s\n", randomName);
    else
        printf("[BT][WARN] failed to set randomized device name\n");

    esp_err_t callbackErr = esp_ble_gap_register_callback(&onGapEventBridge);
    if (callbackErr != ESP_OK)
    {
        printf("[BT][ERROR] esp_ble_gap_register_callback failed err=0x%x\n", callbackErr);
        return false;
    }

    printf("[BT][INIT] Bluetooth BLE backend initialized\n");

    this->started = true;
    return true;
#endif
}

bool Bluetooth::Stop()
{
    if (!this->started)
        return true;

#if !GLYTT_BLE_AVAILABLE
    this->started = false;
    this->scanning = false;
    this->callback = nullptr;
    this->callbackContext = nullptr;
    return true;
#else
    bool success = true;

    if (this->scanning)
        success = this->StopScan() && success;

    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED)
    {
        if (esp_bluedroid_disable() != ESP_OK)
            success = false;
    }

    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED)
    {
        if (esp_bluedroid_deinit() != ESP_OK)
            success = false;
    }

    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED)
    {
        if (esp_bt_controller_disable() != ESP_OK)
            success = false;
    }

    if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_IDLE)
    {
        if (esp_bt_controller_deinit() != ESP_OK)
            success = false;
    }

    this->started = false;
    this->scanning = false;
    this->callback = nullptr;
    this->callbackContext = nullptr;

    return success;
#endif
}

bool Bluetooth::SetScanParams(const BluetoothScanParams &params)
{
    this->scanParams = params;

#if !GLYTT_BLE_AVAILABLE
    return false;
#else
    if (!this->started)
        return true;

    esp_ble_scan_params_t nativeParams = {
        .scan_type = params.active ? BLE_SCAN_TYPE_ACTIVE : BLE_SCAN_TYPE_PASSIVE,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval = params.interval,
        .scan_window = params.window,
        .scan_duplicate = params.allowDuplicates ? BLE_SCAN_DUPLICATE_DISABLE : BLE_SCAN_DUPLICATE_ENABLE};

    return esp_ble_gap_set_scan_params(&nativeParams) == ESP_OK;
#endif
}

bool Bluetooth::StartScan(BluetoothAdvertisementCallback callback, void *context, unsigned int durationSeconds)
{
#if !GLYTT_BLE_AVAILABLE
    (void)callback;
    (void)context;
    (void)durationSeconds;
    return false;
#else
    if (!this->started && !this->Start())
        return false;

    this->callback = callback;
    this->callbackContext = context;

    if (!this->SetScanParams(this->scanParams))
    {
        printf("[BT][ERROR] SetScanParams failed\n");
        return false;
    }

    esp_err_t startErr = esp_ble_gap_start_scanning(durationSeconds);
    if (startErr != ESP_OK)
    {
        printf("[BT][ERROR] esp_ble_gap_start_scanning failed err=0x%x\n", startErr);
        return false;
    }

    this->scanning = true;
    printf("[BT][SCAN] started duration=%us\n", durationSeconds);
    return true;
#endif
}

bool Bluetooth::StopScan()
{
#if !GLYTT_BLE_AVAILABLE
    return false;
#else
    if (!this->scanning)
        return true;

    bool success = esp_ble_gap_stop_scanning() == ESP_OK;
    this->scanning = false;

    return success;
#endif
}

bool Bluetooth::Started() const
{
    return this->started;
}

bool Bluetooth::Scanning() const
{
    return this->scanning;
}

void Bluetooth::HandleAdvertisement(const BluetoothAdvertisement &advertisement)
{
    if (this->callback != nullptr)
        this->callback(advertisement, this->callbackContext);
}

void Bluetooth::HandleScanStopped()
{
    this->scanning = false;
}
