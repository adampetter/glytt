#pragma once

#include "api/common/types.h"
#include "esp_wifi.h"

struct WifiFrameMetadata
{
	signed char rssi = 0;
	Byte channel = 0;
	unsigned short length = 0;
	unsigned long long timestampUs = 0;
	wifi_promiscuous_pkt_type_t type = WIFI_PKT_MISC;
};

typedef void (*WifiFrameCallback)(const Byte *payload, unsigned short length, const WifiFrameMetadata &metadata, void *context);

class Wifi
{
private:
	static Wifi *instance;

	bool started = false;
	bool promiscuous = false;
	bool staCreated = false;

	WifiFrameCallback callback = nullptr;
	void *callbackContext = nullptr;

	wifi_promiscuous_filter_t filter = {
		.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA};

	static void onPromiscuous(void *buffer, wifi_promiscuous_pkt_type_t type);
	void handlePromiscuous(void *buffer, wifi_promiscuous_pkt_type_t type);

public:
	Wifi();
	~Wifi();

	bool Start();
	bool Stop();

	bool SetChannel(Byte primaryChannel, wifi_second_chan_t secondChannel = WIFI_SECOND_CHAN_NONE);
	bool SetPromiscuousFilter(wifi_promiscuous_filter_t filter);
	bool StartPromiscuous(WifiFrameCallback callback, void *context = nullptr);
	bool StopPromiscuous();

	bool Started() const;
	bool Promiscuous() const;
};
