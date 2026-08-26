#include "api/transmission/wifi_ignore.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

static std::string NormalizeMacString(const char *mac)
{
    if (mac == nullptr)
        return "";

    std::string value(mac);

    for (size_t i = 0; i < value.size(); i++)
    {
        if (value[i] == '-')
            value[i] = ':';
        else
            value[i] = (char)toupper((unsigned char)value[i]);
    }

    return value;
}

static std::string FormatMac(const Byte mac[6])
{
    if (mac == nullptr)
        return "";

    char buffer[18] = {0};
    snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return std::string(buffer);
}

void WifiIgnoreFilter::AddMac(const char *mac)
{
    std::string value = NormalizeMacString(mac);
    if (value.empty())
        return;

    if (std::find(this->macList.begin(), this->macList.end(), value) == this->macList.end())
        this->macList.push_back(value);
}

void WifiIgnoreFilter::AddMac(const Byte mac[6])
{
    std::string value = FormatMac(mac);
    if (value.empty())
        return;

    if (std::find(this->macList.begin(), this->macList.end(), value) == this->macList.end())
        this->macList.push_back(value);
}

void WifiIgnoreFilter::AddSsid(const char *ssid)
{
    if (ssid == nullptr || ssid[0] == '\0')
        return;

    std::string value(ssid);
    if (std::find(this->ssidList.begin(), this->ssidList.end(), value) == this->ssidList.end())
        this->ssidList.push_back(value);
}

bool WifiIgnoreFilter::IsIgnoredMac(const Byte mac[6]) const
{
    std::string value = FormatMac(mac);
    if (value.empty())
        return false;

    return std::find(this->macList.begin(), this->macList.end(), value) != this->macList.end();
}

bool WifiIgnoreFilter::IsIgnoredSsid(const char *ssid) const
{
    if (ssid == nullptr || ssid[0] == '\0')
        return false;

    std::string value(ssid);
    return std::find(this->ssidList.begin(), this->ssidList.end(), value) != this->ssidList.end();
}

void WifiIgnoreFilter::Clear()
{
    this->macList.clear();
    this->ssidList.clear();
}
