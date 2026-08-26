#pragma once

#include "api/common/types.h"

#include <deque>

struct WifiProbeObservation
{
    char mac[18] = {0};
    char ssid[33] = {0};
    unsigned long long timestampUs = 0;
};

struct WifiTrackingConfig
{
    unsigned int recentMinutes = 5;
    unsigned int mediumMinutes = 10;
    unsigned int oldMinutes = 15;
    unsigned int oldestMinutes = 20;
    unsigned int maxEntries = 4096;
};

struct WifiProbeHistory
{
    bool seenRecent = false;
    bool seenMedium = false;
    bool seenOld = false;
    bool seenOldest = false;

    unsigned int matchCount = 0;
    float persistenceScore = 0.0f;
};

class WifiProbeTracker
{
private:
    WifiTrackingConfig config;
    std::deque<WifiProbeObservation> observations;

    unsigned long long minutesToUs(unsigned int minutes) const;
    void prune(unsigned long long nowUs);
    bool matches(const WifiProbeObservation &existing, const WifiProbeObservation &current) const;

public:
    WifiProbeTracker() = default;
    explicit WifiProbeTracker(const WifiTrackingConfig &config);

    void Configure(const WifiTrackingConfig &config);
    void Clear();

    WifiProbeHistory Analyze(const WifiProbeObservation &current, unsigned long long nowUs);
    void Add(const WifiProbeObservation &observation, unsigned long long nowUs);

    unsigned int Count() const;
};