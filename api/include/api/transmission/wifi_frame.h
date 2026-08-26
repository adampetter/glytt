#pragma once

#include "api/common/types.h"
#include "api/transmission/wifi.h"

struct WifiProbeRequest
{
    Byte source[6] = {0};
    Byte bssid[6] = {0};
    char ssid[33] = {0};

    signed char rssi = 0;
    Byte channel = 0;
    unsigned long long timestampUs = 0;
};

class WifiFrameParser
{
public:
    static bool TryParseProbeRequest(const Byte *payload, unsigned short length, const WifiFrameMetadata &metadata, WifiProbeRequest *output);
    static void MacToString(const Byte mac[6], char output[18]);
};