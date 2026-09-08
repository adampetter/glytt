#pragma once

#include "api/transmission/wifi_frame.h"
#include "api/transmission/wifi_ignore.h"
#include "api/transmission/wifi_tracking.h"
#include "api/transmission/wifi.h"
#include "api/transmission/bluetooth.h"
#include "api/navigation/location.h"
#include "api/system/thread.h"

#include <deque>

enum class TailerAlertLevel
{
    Normal = 0,
    Low = 1,
    Medium = 2,
    High = 3,
};

struct TailerConfig
{
    bool bluetoothEnabled = true;
    Byte channels[14] = {1, 6, 11};
    Byte channelCount = 3;
    unsigned int hopIntervalMs = 350;
    unsigned int reportIntervalMs = 1000;
    float persistenceAlertThreshold = 0.50f;
    unsigned int dedupWindowMs = 1200;
    unsigned int dedupMaxEntries = 256;
    unsigned int baselineCaptureSeconds = 60;
    float movementSpeedThresholdKph = 2.5f;
    float movingTemporalWeight = 0.60f;
    float movingSpatialWeight = 0.40f;
    float stationaryTemporalWeight = 0.90f;
    float stationarySpatialWeight = 0.10f;
    float stationarySensitivityMultiplier = 2.4f;

    // Stilla: 0-2, 2-5, 5-10, 10-15 min
    unsigned int stationaryRecentMinutes = 2;
    unsigned int stationaryMediumMinutes = 5;
    unsigned int stationaryOldMinutes = 10;
    unsigned int stationaryOldestMinutes = 15;

    // Rörelse: 0-4, 4-8, 8-12, 12-16 min
    unsigned int movingRecentMinutes = 4;
    unsigned int movingMediumMinutes = 8;
    unsigned int movingOldMinutes = 12;
    unsigned int movingOldestMinutes = 16;

    // Mode hysteresis before switching tracker windows.
    unsigned int stationaryModeEngageSeconds = 60;
    unsigned int movingModeEngageSeconds = 15;

    unsigned int scoreFreshnessSeconds = 6;
    float lowEnterScore = 0.52f;
    float lowExitScore = 0.25f;
    float mediumEnterScore = 0.72f;
    float mediumExitScore = 0.48f;
    float highEnterScore = 0.87f;
    float highExitScore = 0.70f;
    bool latchAlertAfterDetection = true;
    bool latchHighestAlertLevel = true;
};

class Tailer
{
private:
    struct DedupEntry
    {
        char mac[18] = {0};
        char ssid[33] = {0};
        unsigned long long timestampUs = 0;
    };

    TailerConfig config;
    Wifi wifi;
    WifiIgnoreFilter ignore;
    WifiProbeTracker tracker;
    WifiIgnoreFilter btIgnore;
    WifiProbeTracker btTracker;
    std::deque<DedupEntry> recentProbes;
    std::deque<DedupEntry> recentBtAdvertisements;
    Mutex locationLock;
    Location latestLocation;
    bool hasLocationFix = false;

    Byte currentChannelIndex = 0;

    unsigned int frameCount = 0;
    unsigned int managementFrameCount = 0;
    unsigned int probeRequestCount = 0;
    unsigned int ignoredProbeCount = 0;
    unsigned int baselineIgnoredProbeCount = 0;
    unsigned int dedupedProbeCount = 0;
    unsigned int persistentAlertCount = 0;

    char lastProbeMac[18] = {0};
    char lastProbeSsid[33] = {0};
    signed char lastProbeRssi = 0;
    float lastProbeScore = 0.0f;
    float lastProbeTemporalScore = 0.0f;
    float lastProbeSpatialScore = 0.0f;
    float lastProbeDistanceMeters = 0.0f;
    float lastTemporalWeight = 1.0f;
    float lastSpatialWeight = 0.0f;
    bool lastMovingMode = false;
    unsigned int lastProbeMatchCount = 0;
    bool lastSeenRecent = false;
    bool lastSeenMedium = false;
    bool lastSeenOld = false;
    bool lastSeenOldest = false;
    bool hasLastProbe = false;
    unsigned long long lastProbeTimestampUs = 0;
    float lastCombinedScore = 0.0f;

    Bluetooth bluetooth;
    unsigned int btAdvertisementCount = 0;
    unsigned int btIgnoredCount = 0;
    unsigned int btBaselineIgnoredCount = 0;
    unsigned int btDedupedCount = 0;
    unsigned int btPersistentAlertCount = 0;
    char lastBtMac[18] = {0};
    char lastBtName[32] = {0};
    signed char lastBtRssi = 0;
    unsigned long long lastBtTimestampUs = 0;
    float lastBtScore = 0.0f;
    unsigned int lastBtMatchCount = 0;
    bool lastBtSeenRecent = false;
    bool lastBtSeenMedium = false;
    bool lastBtSeenOld = false;
    bool lastBtSeenOldest = false;
    bool hasLastBtAdvertisement = false;

    unsigned long long lastHopUs = 0;
    unsigned long long lastReportUs = 0;
    unsigned long long baselineUntilUs = 0;
    bool baselineActive = false;
    TailerAlertLevel alertLevel = TailerAlertLevel::Normal;
    bool alertPendingResetAfterBaseline = false;
    bool alertLatched = false;
    TailerAlertLevel latchedMinimumAlertLevel = TailerAlertLevel::Normal;

    WifiTrackingConfig stationaryTrackingConfig = {};
    WifiTrackingConfig movingTrackingConfig = {};
    bool trackerMovingMode = false;
    unsigned long long movingObservedSinceUs = 0;
    unsigned long long stationaryObservedSinceUs = 0;

    void applyTrackingWindowConfig();
    void updateTrackerMode(unsigned long long nowUs, bool hasMovementObservation, bool observedMovingMode);

    static void onFrame(const Byte *payload, unsigned short length, const WifiFrameMetadata &metadata, void *context);
    void handleFrame(const Byte *payload, unsigned short length, const WifiFrameMetadata &metadata);
    void pruneDedupCache(unsigned long long nowUs);
    bool isDuplicateProbe(const WifiProbeObservation &current, unsigned long long nowUs);
    void rememberProbe(const WifiProbeObservation &current, unsigned long long nowUs);
    void pruneBtDedupCache(unsigned long long nowUs);
    bool isDuplicateBtAdvertisement(const BluetoothAdvertisement &advertisement, unsigned long long nowUs);
    void rememberBtAdvertisement(const BluetoothAdvertisement &advertisement, unsigned long long nowUs);
    void updateAlertLevel(unsigned long long nowUs);
    static void onBluetoothAdvertisement(const BluetoothAdvertisement &advertisement, void *context);
    void handleBluetoothAdvertisement(const BluetoothAdvertisement &advertisement);

public:
    Tailer(const TailerConfig &config);
    ~Tailer();

    bool Start();
    void Tick();
    void Stop();
    void UpdateLocation(const Location &location);
    float CurrentScore() const;
    bool HasProbe() const;
    unsigned int SecondsSinceLastProbe() const;
    TailerAlertLevel AlertLevel() const;
    void StartBaseline(unsigned int seconds);
    void ClearLearnedBackground();
    bool BaselineActive() const;
    unsigned int BaselineSecondsLeft() const;
};