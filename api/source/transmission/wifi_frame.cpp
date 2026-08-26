#include "api/transmission/wifi_frame.h"

#include <cstdio>
#include <cstring>

bool WifiFrameParser::TryParseProbeRequest(const Byte *payload, unsigned short length, const WifiFrameMetadata &metadata, WifiProbeRequest *output)
{
    if (payload == nullptr || output == nullptr)
        return false;

    // 802.11 management header is 24 bytes.
    if (length < 24)
        return false;

    Byte frameControl = payload[0];
    Byte frameType = (Byte)((frameControl >> 2) & 0x03);
    Byte frameSubType = (Byte)((frameControl >> 4) & 0x0F);

    // Type 0 = management, subtype 4 = probe request.
    if (frameType != 0 || frameSubType != 4)
        return false;

    memcpy(output->source, payload + 10, 6);
    memcpy(output->bssid, payload + 16, 6);

    output->ssid[0] = '\0';

    // Information elements start after the fixed 24-byte management header.
    unsigned short index = 24;
    while ((unsigned short)(index + 2) <= length)
    {
        Byte elementId = payload[index];
        Byte elementLength = payload[index + 1];
        index = (unsigned short)(index + 2);

        if ((unsigned short)(index + elementLength) > length)
            break;

        if (elementId == 0)
        {
            unsigned short copyLength = elementLength > 32 ? 32 : elementLength;
            if (copyLength > 0)
                memcpy(output->ssid, payload + index, copyLength);

            output->ssid[copyLength] = '\0';
            break;
        }

        index = (unsigned short)(index + elementLength);
    }

    output->rssi = metadata.rssi;
    output->channel = metadata.channel;
    output->timestampUs = metadata.timestampUs;

    return true;
}

void WifiFrameParser::MacToString(const Byte mac[6], char output[18])
{
    if (output == nullptr)
        return;

    if (mac == nullptr)
    {
        output[0] = '\0';
        return;
    }

    snprintf(output, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
