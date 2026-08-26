#pragma once

#include "api/transmission/wifi_frame.h"
#include "api/transmission/wifi_ignore.h"
#include "api/transmission/wifi_tracking.h"
#include "api/transmission/wifi.h"

#include <deque>

struct AppTailerConfig
{
    Byte channels[14] = {1, 6, 11};
    Byte channelCount = 3;
    unsigned int hopIntervalMs = 350;
    unsigned int reportIntervalMs = 1000;
    float persistenceAlertThreshold = 0.50f;
    unsigned int dedupWindowMs = 1200;
    unsigned int dedupMaxEntries = 256;
};

class AppTailer
{
private:
    struct DedupEntry
    {
        char mac[18] = {0};
        char ssid[33] = {0};
        unsigned long long timestampUs = 0;
    };

    AppTailerConfig config;
    Wifi wifi;
    WifiIgnoreFilter ignore;
    WifiProbeTracker tracker;
    std::deque<DedupEntry> recentProbes;

    Byte currentChannelIndex = 0;

    unsigned int frameCount = 0;
    unsigned int managementFrameCount = 0;
    unsigned int probeRequestCount = 0;
    unsigned int ignoredProbeCount = 0;
    unsigned int dedupedProbeCount = 0;
    unsigned int persistentAlertCount = 0;

    char lastProbeMac[18] = {0};
    char lastProbeSsid[33] = {0};
    signed char lastProbeRssi = 0;
    float lastProbeScore = 0.0f;
    unsigned int lastProbeMatchCount = 0;
    bool lastSeenRecent = false;
    bool lastSeenMedium = false;
    bool lastSeenOld = false;
    bool lastSeenOldest = false;
    bool hasLastProbe = false;

    unsigned long long lastHopUs = 0;
    unsigned long long lastReportUs = 0;

    static void onFrame(const Byte *payload, unsigned short length, const WifiFrameMetadata &metadata, void *context);
    void handleFrame(const Byte *payload, unsigned short length, const WifiFrameMetadata &metadata);
    void pruneDedupCache(unsigned long long nowUs);
    bool isDuplicateProbe(const WifiProbeObservation &current, unsigned long long nowUs);
    void rememberProbe(const WifiProbeObservation &current, unsigned long long nowUs);

public:
    AppTailer(const AppTailerConfig &config);
    ~AppTailer();

    bool Start();
    void Tick();
    void Stop();
};