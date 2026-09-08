#pragma once

#include "api/common/types.h"

#include <string>
#include <vector>

class WifiIgnoreFilter
{
private:
    std::vector<std::string> macList;
    std::vector<std::string> ssidList;

public:
    void AddMac(const char *mac);
    void AddMac(const Byte mac[6]);
    void AddSsid(const char *ssid);

    bool IsIgnoredMac(const Byte mac[6]) const;
    bool IsIgnoredMac(const char *mac) const;
    bool IsIgnoredSsid(const char *ssid) const;

    void Clear();
};