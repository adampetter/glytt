#pragma once

#include "api/common/types.h"

struct BluetoothScanParams
{
	unsigned short interval = 0x50;
	unsigned short window = 0x30;
	bool active = true;
	bool allowDuplicates = false;
};

struct BluetoothAdvertisement
{
	char mac[18] = {0};
	char name[32] = {0};
	signed char rssi = 0;
	Byte addressType = 0;
	Byte eventType = 0;
	unsigned short payloadLength = 0;
	unsigned long long timestampUs = 0;
};

typedef void (*BluetoothAdvertisementCallback)(const BluetoothAdvertisement &advertisement, void *context);

class Bluetooth
{
private:
	static Bluetooth *instance;

	bool started = false;
	bool scanning = false;
	BluetoothScanParams scanParams = {};

	BluetoothAdvertisementCallback callback = nullptr;
	void *callbackContext = nullptr;

public:
	Bluetooth();
	~Bluetooth();

	static Bluetooth *Instance();

	bool Start();
	bool Stop();

	bool SetScanParams(const BluetoothScanParams &params);
	bool StartScan(BluetoothAdvertisementCallback callback, void *context = nullptr, unsigned int durationSeconds = 0);
	bool StopScan();

	bool Started() const;
	bool Scanning() const;

	// Internal bridge methods for platform callbacks.
	void HandleAdvertisement(const BluetoothAdvertisement &advertisement);
	void HandleScanStopped();
};
