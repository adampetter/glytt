#include "api/transmission/wifi.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "nvs_flash.h"

Wifi *Wifi::instance = nullptr;

Wifi::Wifi()
{
    if (instance == nullptr)
        instance = this;
}

Wifi::~Wifi()
{
    this->StopPromiscuous();
    this->Stop();

    if (instance == this)
        instance = nullptr;
}

bool Wifi::Start()
{
    if (this->started)
        return true;

    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        if (nvs_flash_erase() != ESP_OK)
            return false;

        result = nvs_flash_init();
    }

    if (result != ESP_OK && result != ESP_ERR_NVS_NOT_INITIALIZED)
        return false;

    result = esp_netif_init();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE)
        return false;

    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE)
        return false;

    esp_netif_t *sta = esp_netif_create_default_wifi_sta();
    this->staCreated = sta != nullptr;

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&config) != ESP_OK)
        return false;

    if (esp_wifi_set_mode(WIFI_MODE_NULL) != ESP_OK)
        return false;

    if (esp_wifi_start() != ESP_OK)
        return false;

    this->started = true;
    return true;
}

bool Wifi::Stop()
{
    if (!this->started)
        return true;

    if (this->promiscuous)
        this->StopPromiscuous();

    bool success = true;

    if (esp_wifi_stop() != ESP_OK)
        success = false;

    if (esp_wifi_deinit() != ESP_OK)
        success = false;

    this->started = false;
    this->staCreated = false;
    return success;
}

bool Wifi::SetChannel(Byte primaryChannel, wifi_second_chan_t secondChannel)
{
    if (!this->started)
        return false;

    if (primaryChannel < 1 || primaryChannel > 14)
        return false;

    return esp_wifi_set_channel(primaryChannel, secondChannel) == ESP_OK;
}

bool Wifi::SetPromiscuousFilter(wifi_promiscuous_filter_t filter)
{
    this->filter = filter;

    if (!this->started)
        return true;

    return esp_wifi_set_promiscuous_filter(&this->filter) == ESP_OK;
}

bool Wifi::StartPromiscuous(WifiFrameCallback callback, void *context)
{
    if (!this->started && !this->Start())
        return false;

    this->callback = callback;
    this->callbackContext = context;

    if (esp_wifi_set_promiscuous_filter(&this->filter) != ESP_OK)
        return false;

    esp_wifi_set_promiscuous_rx_cb(&Wifi::onPromiscuous);

    if (esp_wifi_set_promiscuous(true) != ESP_OK)
        return false;

    this->promiscuous = true;
    return true;
}

bool Wifi::StopPromiscuous()
{
    if (!this->promiscuous)
        return true;

    bool success = esp_wifi_set_promiscuous(false) == ESP_OK;

    this->promiscuous = false;
    this->callback = nullptr;
    this->callbackContext = nullptr;

    return success;
}

bool Wifi::Started() const
{
    return this->started;
}

bool Wifi::Promiscuous() const
{
    return this->promiscuous;
}

void Wifi::onPromiscuous(void *buffer, wifi_promiscuous_pkt_type_t type)
{
    if (instance != nullptr)
        instance->handlePromiscuous(buffer, type);
}

void Wifi::handlePromiscuous(void *buffer, wifi_promiscuous_pkt_type_t type)
{
    if (this->callback == nullptr || buffer == nullptr)
        return;

    const wifi_promiscuous_pkt_t *packet = (const wifi_promiscuous_pkt_t *)buffer;

    WifiFrameMetadata metadata;
    metadata.rssi = packet->rx_ctrl.rssi;
    metadata.channel = (Byte)packet->rx_ctrl.channel;
    metadata.length = (unsigned short)packet->rx_ctrl.sig_len;
    metadata.timestampUs = (unsigned long long)esp_timer_get_time();
    metadata.type = type;

    this->callback((const Byte *)packet->payload, metadata.length, metadata, this->callbackContext);
}
