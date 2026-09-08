#include "tailer.h"

#include <cstdio>
#include "esp_timer.h"
#include <cstring>

Tailer::Tailer(const TailerConfig &config)
{
    this->config = config;

    this->applyTrackingWindowConfig();
    this->tracker.Configure(this->stationaryTrackingConfig);
    this->btTracker.Configure(this->stationaryTrackingConfig);
}

void Tailer::applyTrackingWindowConfig()
{
    this->stationaryTrackingConfig.recentMinutes = this->config.stationaryRecentMinutes;
    this->stationaryTrackingConfig.mediumMinutes = this->config.stationaryMediumMinutes;
    this->stationaryTrackingConfig.oldMinutes = this->config.stationaryOldMinutes;
    this->stationaryTrackingConfig.oldestMinutes = this->config.stationaryOldestMinutes;
    this->stationaryTrackingConfig.maxEntries = 4096;

    this->movingTrackingConfig.recentMinutes = this->config.movingRecentMinutes;
    this->movingTrackingConfig.mediumMinutes = this->config.movingMediumMinutes;
    this->movingTrackingConfig.oldMinutes = this->config.movingOldMinutes;
    this->movingTrackingConfig.oldestMinutes = this->config.movingOldestMinutes;
    this->movingTrackingConfig.maxEntries = 4096;
}

void Tailer::updateTrackerMode(unsigned long long nowUs, bool hasMovementObservation, bool observedMovingMode)
{
    if (!hasMovementObservation)
        return;

    if (observedMovingMode == this->trackerMovingMode)
    {
        this->movingObservedSinceUs = 0;
        this->stationaryObservedSinceUs = 0;
        return;
    }

    if (observedMovingMode)
    {
        if (this->movingObservedSinceUs == 0)
            this->movingObservedSinceUs = nowUs;

        this->stationaryObservedSinceUs = 0;

        unsigned long long holdUs = (unsigned long long)this->config.movingModeEngageSeconds * 1000000ULL;
        if (holdUs == 0 || (nowUs >= this->movingObservedSinceUs && (nowUs - this->movingObservedSinceUs) >= holdUs))
        {
            this->trackerMovingMode = true;
            this->movingObservedSinceUs = 0;

            this->tracker.Configure(this->movingTrackingConfig);
            this->btTracker.Configure(this->movingTrackingConfig);
            printf("[TAILER][MODE] switched to moving windows\n");
        }

        return;
    }

    if (this->stationaryObservedSinceUs == 0)
        this->stationaryObservedSinceUs = nowUs;

    this->movingObservedSinceUs = 0;

    unsigned long long holdUs = (unsigned long long)this->config.stationaryModeEngageSeconds * 1000000ULL;
    if (holdUs == 0 || (nowUs >= this->stationaryObservedSinceUs && (nowUs - this->stationaryObservedSinceUs) >= holdUs))
    {
        this->trackerMovingMode = false;
        this->stationaryObservedSinceUs = 0;

        this->tracker.Configure(this->stationaryTrackingConfig);
        this->btTracker.Configure(this->stationaryTrackingConfig);
        printf("[TAILER][MODE] switched to stationary windows\n");
    }
}

Tailer::~Tailer()
{
    this->Stop();
}

bool Tailer::Start()
{
    if (!this->wifi.Start())
        return false;

    if (this->config.channelCount == 0)
        this->config.channelCount = 1;

    if (this->config.channelCount > 14)
        this->config.channelCount = 14;

    if (this->config.persistenceAlertThreshold < 0.0f)
        this->config.persistenceAlertThreshold = 0.0f;

    if (this->config.persistenceAlertThreshold > 1.0f)
        this->config.persistenceAlertThreshold = 1.0f;

    if (this->config.dedupMaxEntries == 0)
        this->config.dedupMaxEntries = 1;

    if (this->config.movementSpeedThresholdKph < 0.0f)
        this->config.movementSpeedThresholdKph = 0.0f;

    this->currentChannelIndex = 0;
    Byte firstChannel = this->config.channels[this->currentChannelIndex];

    if (!this->wifi.SetChannel(firstChannel))
        return false;

    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};

    if (!this->wifi.SetPromiscuousFilter(filter))
        return false;

    this->frameCount = 0;
    this->managementFrameCount = 0;
    this->probeRequestCount = 0;
    this->ignoredProbeCount = 0;
    this->baselineIgnoredProbeCount = 0;
    this->dedupedProbeCount = 0;
    this->persistentAlertCount = 0;
    this->lastProbeMac[0] = '\0';
    this->lastProbeSsid[0] = '\0';
    this->lastProbeRssi = 0;
    this->lastProbeScore = 0.0f;
    this->lastProbeTemporalScore = 0.0f;
    this->lastProbeSpatialScore = 0.0f;
    this->lastProbeDistanceMeters = 0.0f;
    this->lastTemporalWeight = 1.0f;
    this->lastSpatialWeight = 0.0f;
    this->lastMovingMode = false;
    this->lastProbeMatchCount = 0;
    this->lastSeenRecent = false;
    this->lastSeenMedium = false;
    this->lastSeenOld = false;
    this->lastSeenOldest = false;
    this->hasLocationFix = false;
    this->hasLastProbe = false;
    this->lastProbeTimestampUs = 0;
    this->lastCombinedScore = 0.0f;
    this->btAdvertisementCount = 0;
    this->btIgnoredCount = 0;
    this->btBaselineIgnoredCount = 0;
    this->btDedupedCount = 0;
    this->btPersistentAlertCount = 0;
    this->lastBtMac[0] = '\0';
    this->lastBtName[0] = '\0';
    this->lastBtRssi = 0;
    this->lastBtTimestampUs = 0;
    this->lastBtScore = 0.0f;
    this->lastBtMatchCount = 0;
    this->lastBtSeenRecent = false;
    this->lastBtSeenMedium = false;
    this->lastBtSeenOld = false;
    this->lastBtSeenOldest = false;
    this->hasLastBtAdvertisement = false;

    this->lastHopUs = (unsigned long long)esp_timer_get_time();
    this->lastReportUs = this->lastHopUs;
    this->baselineUntilUs = 0;
    this->baselineActive = false;
    this->alertLevel = TailerAlertLevel::Normal;
    this->alertLatched = false;
    this->latchedMinimumAlertLevel = TailerAlertLevel::Normal;
    this->alertPendingResetAfterBaseline = false;
    this->trackerMovingMode = false;
    this->movingObservedSinceUs = 0;
    this->stationaryObservedSinceUs = 0;
    this->tracker.Configure(this->stationaryTrackingConfig);
    this->btTracker.Configure(this->stationaryTrackingConfig);

    // Keep ignore list empty in initial baseline; we'll load rules later.
    this->ignore.Clear();
    this->tracker.Clear();
    this->recentProbes.clear();
    this->btIgnore.Clear();
    this->btTracker.Clear();
    this->recentBtAdvertisements.clear();

    if (!this->wifi.StartPromiscuous(&Tailer::onFrame, this))
        return false;

    if (this->config.bluetoothEnabled)
    {
        if (this->bluetooth.Start())
        {
            if (this->bluetooth.StartScan(&Tailer::onBluetoothAdvertisement, this, 0))
            {
                printf("[TAILER][BT] BLE scan started\n");
            }
            else
            {
                printf("[TAILER][BT][WARN] Failed to start BLE scan\n");
            }
        }
        else
        {
            printf("[TAILER][BT][WARN] Failed to initialize Bluetooth\n");
        }
    }
    else
    {
        printf("[TAILER][BT] disabled by config\n");
    }

    printf("[TAILER][INIT] Promiscuous capture active on channel %u (baselineButtonCapture=%us, moveThreshold=%.1fkph)\n",
           firstChannel,
           this->config.baselineCaptureSeconds,
           this->config.movementSpeedThresholdKph);

    return true;
}

void Tailer::Tick()
{
    if (!this->wifi.Started())
        return;

    unsigned long long nowUs = (unsigned long long)esp_timer_get_time();

    if (this->baselineActive && nowUs >= this->baselineUntilUs)
    {
        this->baselineActive = false;
        this->alertPendingResetAfterBaseline = true;
        printf("[TAILER][BASELINE] capture completed\n");
    }

    const unsigned long long hopIntervalUs = (unsigned long long)this->config.hopIntervalMs * 1000ULL;
    if (this->config.channelCount > 1 && hopIntervalUs > 0 && nowUs - this->lastHopUs >= hopIntervalUs)
    {
        this->currentChannelIndex = (Byte)((this->currentChannelIndex + 1) % this->config.channelCount);
        Byte nextChannel = this->config.channels[this->currentChannelIndex];
        this->wifi.SetChannel(nextChannel);
        this->lastHopUs = nowUs;
    }

    const unsigned long long reportIntervalUs = (unsigned long long)this->config.reportIntervalMs * 1000ULL;
    if (reportIntervalUs > 0 && nowUs - this->lastReportUs >= reportIntervalUs)
    {
        Byte channel = this->config.channels[this->currentChannelIndex];

        if (this->hasLastProbe)
        {
             printf("[TAILER][WIFI] ch=%u frames=%u mgmt=%u probes=%u ignored=%u baselineIgnored=%u deduped=%u last=%s ssid=%s rssi=%d\n",
                   channel,
                   this->frameCount,
                   this->managementFrameCount,
                   this->probeRequestCount,
                   this->ignoredProbeCount,
                 this->baselineIgnoredProbeCount,
                 this->dedupedProbeCount,
                   this->lastProbeMac,
                   this->lastProbeSsid,
                   this->lastProbeRssi);
        }
        else
        {
                                                 printf("[TAILER][WIFI] ch=%u frames=%u mgmt=%u probes=%u ignored=%u baselineIgnored=%u deduped=%u\n",
                   channel,
                   this->frameCount,
                   this->managementFrameCount,
                   this->probeRequestCount,
                 this->ignoredProbeCount,
                                 this->baselineIgnoredProbeCount,
                 this->dedupedProbeCount);
        }

                                 printf("[TAILER][SCORE] trackerEntries=%u persistenceAlerts=%u score=%.2f temporal=%.2f spatial=%.2f dist=%.1fm mode=%s w=%.2f/%.2f matches=%u windows=%c%c%c%c threshold=%.2f\n",
               this->tracker.Count(),
               this->persistentAlertCount,
                         this->lastProbeScore,
                         this->lastProbeTemporalScore,
                         this->lastProbeSpatialScore,
                         this->lastProbeDistanceMeters,
                         this->lastMovingMode ? "moving" : "stationary",
                         this->lastTemporalWeight,
                         this->lastSpatialWeight,
             this->lastProbeMatchCount,
             this->lastSeenRecent ? 'R' : '-',
             this->lastSeenMedium ? 'M' : '-',
             this->lastSeenOld ? 'O' : '-',
             this->lastSeenOldest ? 'X' : '-',
             this->config.persistenceAlertThreshold);

        if (this->config.bluetoothEnabled)
        {
            if (this->lastBtTimestampUs > 0)
            {
                printf("[TAILER][BT] adv=%u ignored=%u baselineIgnored=%u deduped=%u alerts=%u score=%.2f matches=%u windows=%c%c%c%c last=%s name=%s rssi=%d\n",
                       this->btAdvertisementCount,
                       this->btIgnoredCount,
                       this->btBaselineIgnoredCount,
                       this->btDedupedCount,
                       this->btPersistentAlertCount,
                       this->lastBtScore,
                       this->lastBtMatchCount,
                       this->lastBtSeenRecent ? 'R' : '-',
                       this->lastBtSeenMedium ? 'M' : '-',
                       this->lastBtSeenOld ? 'O' : '-',
                       this->lastBtSeenOldest ? 'X' : '-',
                       this->lastBtMac,
                       this->lastBtName[0] != '\0' ? this->lastBtName : "<hidden>",
                       this->lastBtRssi);
            }
            else
            {
                printf("[TAILER][BT] adv=%u ignored=%u baselineIgnored=%u deduped=%u alerts=%u\n",
                       this->btAdvertisementCount,
                       this->btIgnoredCount,
                       this->btBaselineIgnoredCount,
                       this->btDedupedCount,
                       this->btPersistentAlertCount);
            }
        }

        this->lastReportUs = nowUs;
    }

    this->updateAlertLevel(nowUs);
}

void Tailer::Stop()
{
    if (this->config.bluetoothEnabled)
    {
        this->bluetooth.StopScan();
        this->bluetooth.Stop();
    }
    this->wifi.StopPromiscuous();
    this->wifi.Stop();
}

void Tailer::UpdateLocation(const Location &location)
{
    if (this->locationLock.Take(10, true))
    {
        this->latestLocation = location;
        this->hasLocationFix = location.fix;
        this->locationLock.Release();
    }
}

float Tailer::CurrentScore() const
{
    return this->lastCombinedScore;
}

bool Tailer::HasProbe() const
{
    return this->hasLastProbe || this->hasLastBtAdvertisement;
}

unsigned int Tailer::SecondsSinceLastProbe() const
{
    if ((!this->hasLastProbe || this->lastProbeTimestampUs == 0) &&
        (!this->hasLastBtAdvertisement || this->lastBtTimestampUs == 0))
        return 0xFFFFFFFFu;

    unsigned long long nowUs = (unsigned long long)esp_timer_get_time();
    unsigned long long oldestUs = nowUs;

    if (this->hasLastProbe && this->lastProbeTimestampUs > 0 && this->lastProbeTimestampUs < oldestUs)
        oldestUs = this->lastProbeTimestampUs;

    if (this->hasLastBtAdvertisement && this->lastBtTimestampUs > 0 && this->lastBtTimestampUs < oldestUs)
        oldestUs = this->lastBtTimestampUs;

    if (nowUs <= oldestUs)
        return 0;

    return (unsigned int)((nowUs - oldestUs) / 1000000ULL);
}

TailerAlertLevel Tailer::AlertLevel() const
{
    return this->alertLevel;
}

void Tailer::StartBaseline(unsigned int seconds)
{
    if (seconds == 0)
        seconds = this->config.baselineCaptureSeconds;

    if (seconds == 0)
        seconds = 60;

    unsigned long long nowUs = (unsigned long long)esp_timer_get_time();
    this->ignore.Clear();
    this->tracker.Clear();
    this->recentProbes.clear();
    this->btIgnore.Clear();
    this->btTracker.Clear();
    this->recentBtAdvertisements.clear();

    this->baselineIgnoredProbeCount = 0;
    this->btBaselineIgnoredCount = 0;
    this->baselineUntilUs = nowUs + (unsigned long long)seconds * 1000000ULL;
    this->baselineActive = true;
    this->alertLevel = TailerAlertLevel::Normal;
    this->alertLatched = false;
    this->latchedMinimumAlertLevel = TailerAlertLevel::Normal;
    this->alertPendingResetAfterBaseline = false;
    this->trackerMovingMode = false;
    this->movingObservedSinceUs = 0;
    this->stationaryObservedSinceUs = 0;
    this->tracker.Configure(this->stationaryTrackingConfig);
    this->btTracker.Configure(this->stationaryTrackingConfig);
    this->hasLastProbe = false;
    this->lastProbeTimestampUs = 0;
    this->lastProbeScore = 0.0f;
    this->hasLastBtAdvertisement = false;
    this->lastBtTimestampUs = 0;
    this->lastBtScore = 0.0f;
    this->lastCombinedScore = 0.0f;

    printf("[TAILER][BASELINE] capture started (%us)\n", seconds);
}

void Tailer::ClearLearnedBackground()
{
    this->ignore.Clear();
    this->tracker.Clear();
    this->recentProbes.clear();
    this->btIgnore.Clear();
    this->btTracker.Clear();
    this->recentBtAdvertisements.clear();
    this->baselineActive = false;
    this->baselineUntilUs = 0;
    this->baselineIgnoredProbeCount = 0;
    this->btBaselineIgnoredCount = 0;
    this->alertLevel = TailerAlertLevel::Normal;
    this->alertLatched = false;
    this->latchedMinimumAlertLevel = TailerAlertLevel::Normal;
    this->alertPendingResetAfterBaseline = false;
    this->trackerMovingMode = false;
    this->movingObservedSinceUs = 0;
    this->stationaryObservedSinceUs = 0;
    this->tracker.Configure(this->stationaryTrackingConfig);
    this->btTracker.Configure(this->stationaryTrackingConfig);
    this->hasLastProbe = false;
    this->lastProbeTimestampUs = 0;
    this->lastProbeScore = 0.0f;
    this->hasLastBtAdvertisement = false;
    this->lastBtTimestampUs = 0;
    this->lastBtScore = 0.0f;
    this->lastCombinedScore = 0.0f;

    printf("[TAILER][BASELINE] learned baseline cleared\n");
}

bool Tailer::BaselineActive() const
{
    return this->baselineActive;
}

unsigned int Tailer::BaselineSecondsLeft() const
{
    if (!this->baselineActive)
        return 0;

    unsigned long long nowUs = (unsigned long long)esp_timer_get_time();
    if (nowUs >= this->baselineUntilUs)
        return 0;

    unsigned long long leftUs = this->baselineUntilUs - nowUs;
    return (unsigned int)((leftUs + 999999ULL) / 1000000ULL);
}

void Tailer::pruneDedupCache(unsigned long long nowUs)
{
    unsigned long long dedupWindowUs = (unsigned long long)this->config.dedupWindowMs * 1000ULL;
    if (dedupWindowUs == 0)
    {
        this->recentProbes.clear();
        return;
    }

    while (!this->recentProbes.empty())
    {
        const DedupEntry &entry = this->recentProbes.front();
        if (nowUs < entry.timestampUs)
            break;

        if (nowUs - entry.timestampUs <= dedupWindowUs)
            break;

        this->recentProbes.pop_front();
    }

    while (this->recentProbes.size() > this->config.dedupMaxEntries)
        this->recentProbes.pop_front();
}

bool Tailer::isDuplicateProbe(const WifiProbeObservation &current, unsigned long long nowUs)
{
    if (this->config.dedupWindowMs == 0)
        return false;

    this->pruneDedupCache(nowUs);

    for (size_t i = 0; i < this->recentProbes.size(); i++)
    {
        const DedupEntry &entry = this->recentProbes[i];
        bool sameMac = entry.mac[0] != '\0' && current.mac[0] != '\0' && strcmp(entry.mac, current.mac) == 0;
        bool sameSsid = entry.ssid[0] != '\0' && current.ssid[0] != '\0' && strcmp(entry.ssid, current.ssid) == 0;

        if (sameMac || sameSsid)
            return true;
    }

    return false;
}

void Tailer::rememberProbe(const WifiProbeObservation &current, unsigned long long nowUs)
{
    DedupEntry entry;
    strncpy(entry.mac, current.mac, sizeof(entry.mac) - 1);
    entry.mac[sizeof(entry.mac) - 1] = '\0';

    strncpy(entry.ssid, current.ssid, sizeof(entry.ssid) - 1);
    entry.ssid[sizeof(entry.ssid) - 1] = '\0';

    entry.timestampUs = nowUs;

    this->recentProbes.push_back(entry);
    this->pruneDedupCache(nowUs);
}

void Tailer::pruneBtDedupCache(unsigned long long nowUs)
{
    unsigned long long dedupWindowUs = (unsigned long long)this->config.dedupWindowMs * 1000ULL;
    if (dedupWindowUs == 0)
    {
        this->recentBtAdvertisements.clear();
        return;
    }

    while (!this->recentBtAdvertisements.empty())
    {
        const DedupEntry &entry = this->recentBtAdvertisements.front();
        if (nowUs < entry.timestampUs)
            break;

        if (nowUs - entry.timestampUs <= dedupWindowUs)
            break;

        this->recentBtAdvertisements.pop_front();
    }

    while (this->recentBtAdvertisements.size() > this->config.dedupMaxEntries)
        this->recentBtAdvertisements.pop_front();
}

bool Tailer::isDuplicateBtAdvertisement(const BluetoothAdvertisement &advertisement, unsigned long long nowUs)
{
    if (this->config.dedupWindowMs == 0)
        return false;

    this->pruneBtDedupCache(nowUs);

    for (size_t i = 0; i < this->recentBtAdvertisements.size(); i++)
    {
        const DedupEntry &entry = this->recentBtAdvertisements[i];
        bool sameMac = entry.mac[0] != '\0' && advertisement.mac[0] != '\0' && strcmp(entry.mac, advertisement.mac) == 0;
        bool sameName = entry.ssid[0] != '\0' && advertisement.name[0] != '\0' && strcmp(entry.ssid, advertisement.name) == 0;

        if (sameMac || sameName)
            return true;
    }

    return false;
}

void Tailer::rememberBtAdvertisement(const BluetoothAdvertisement &advertisement, unsigned long long nowUs)
{
    DedupEntry entry;
    strncpy(entry.mac, advertisement.mac, sizeof(entry.mac) - 1);
    entry.mac[sizeof(entry.mac) - 1] = '\0';

    strncpy(entry.ssid, advertisement.name, sizeof(entry.ssid) - 1);
    entry.ssid[sizeof(entry.ssid) - 1] = '\0';

    entry.timestampUs = nowUs;

    this->recentBtAdvertisements.push_back(entry);
    this->pruneBtDedupCache(nowUs);
}

void Tailer::onFrame(const Byte *payload, unsigned short length, const WifiFrameMetadata &metadata, void *context)
{
    (void)payload;
    (void)length;

    if (context == nullptr)
        return;

    Tailer *tailer = (Tailer *)context;
    tailer->handleFrame(payload, length, metadata);
}

void Tailer::onBluetoothAdvertisement(const BluetoothAdvertisement &advertisement, void *context)
{
    if (context == nullptr)
        return;

    Tailer *tailer = (Tailer *)context;
    tailer->handleBluetoothAdvertisement(advertisement);
}

void Tailer::handleBluetoothAdvertisement(const BluetoothAdvertisement &advertisement)
{
    this->btAdvertisementCount++;

    bool hasMovementObservation = false;
    bool movingNow = false;
    if (this->locationLock.Take(0, false))
    {
        if (this->hasLocationFix)
        {
            hasMovementObservation = true;
            movingNow = this->latestLocation.speed >= this->config.movementSpeedThresholdKph;
        }

        this->locationLock.Release();
    }

    this->updateTrackerMode(advertisement.timestampUs, hasMovementObservation, movingNow);

    if (this->baselineActive)
    {
        this->btIgnore.AddMac(advertisement.mac);
        this->btIgnore.AddSsid(advertisement.name);
        this->btIgnoredCount++;
        this->btBaselineIgnoredCount++;
        return;
    }

    bool ignored = this->btIgnore.IsIgnoredMac(advertisement.mac) || this->btIgnore.IsIgnoredSsid(advertisement.name);
    if (ignored)
    {
        this->btIgnoredCount++;
        return;
    }

    if (this->isDuplicateBtAdvertisement(advertisement, advertisement.timestampUs))
    {
        this->btDedupedCount++;
        return;
    }

    this->rememberBtAdvertisement(advertisement, advertisement.timestampUs);

    WifiProbeObservation current;
    strncpy(current.mac, advertisement.mac, sizeof(current.mac) - 1);
    current.mac[sizeof(current.mac) - 1] = '\0';

    strncpy(current.ssid, advertisement.name, sizeof(current.ssid) - 1);
    current.ssid[sizeof(current.ssid) - 1] = '\0';
    current.timestampUs = advertisement.timestampUs;

    WifiProbeHistory history = this->btTracker.Analyze(current, advertisement.timestampUs);
    this->btTracker.Add(current, advertisement.timestampUs);

    this->lastBtScore = history.combinedScore;
    this->lastBtMatchCount = history.matchCount;
    this->lastBtSeenRecent = history.seenRecent;
    this->lastBtSeenMedium = history.seenMedium;
    this->lastBtSeenOld = history.seenOld;
    this->lastBtSeenOldest = history.seenOldest;

    if (history.combinedScore >= this->config.persistenceAlertThreshold)
    {
        this->btPersistentAlertCount++;
        printf("[TAILER][BT][ALERT] persistence mac=%s name=%s score=%.2f matches=%u\n",
               current.mac,
               current.ssid[0] != '\0' ? current.ssid : "<hidden>",
               history.combinedScore,
               history.matchCount);
    }

    strncpy(this->lastBtMac, advertisement.mac, sizeof(this->lastBtMac) - 1);
    this->lastBtMac[sizeof(this->lastBtMac) - 1] = '\0';

    strncpy(this->lastBtName, advertisement.name, sizeof(this->lastBtName) - 1);
    this->lastBtName[sizeof(this->lastBtName) - 1] = '\0';

    this->lastBtRssi = advertisement.rssi;
    this->lastBtTimestampUs = advertisement.timestampUs;
    this->hasLastBtAdvertisement = true;
}

void Tailer::handleFrame(const Byte *payload, unsigned short length, const WifiFrameMetadata &metadata)
{
    this->frameCount++;

    if (metadata.type == WIFI_PKT_MGMT)
        this->managementFrameCount++;

    WifiProbeRequest probe;
    if (!WifiFrameParser::TryParseProbeRequest(payload, length, metadata, &probe))
        return;

    if (this->baselineActive)
    {
        this->ignore.AddMac(probe.source);
        this->ignore.AddSsid(probe.ssid);
        this->ignoredProbeCount++;
        this->baselineIgnoredProbeCount++;
        return;
    }

    bool ignored = this->ignore.IsIgnoredMac(probe.source) || this->ignore.IsIgnoredSsid(probe.ssid);
    if (ignored)
    {
        this->ignoredProbeCount++;
        return;
    }

    this->probeRequestCount++;

    WifiProbeObservation current;
    WifiFrameParser::MacToString(probe.source, current.mac);
    strncpy(current.ssid, probe.ssid, sizeof(current.ssid) - 1);
    current.ssid[sizeof(current.ssid) - 1] = '\0';
    current.timestampUs = probe.timestampUs;

    bool hasMovementObservation = false;

    if (this->locationLock.Take(0, false))
    {
        if (this->hasLocationFix)
        {
            hasMovementObservation = true;
            current.hasLocation = true;
            current.latitude = this->latestLocation.coordinate.X;
            current.longitude = this->latestLocation.coordinate.Y;
            current.speedKph = this->latestLocation.speed;
            current.deviceMoving = this->latestLocation.speed >= this->config.movementSpeedThresholdKph;

            if (current.deviceMoving)
            {
                current.temporalWeight = this->config.movingTemporalWeight;
                current.spatialWeight = this->config.movingSpatialWeight;
            }
            else
            {
                current.temporalWeight = this->config.stationaryTemporalWeight;
                current.spatialWeight = this->config.stationarySpatialWeight;
            }
        }

        this->locationLock.Release();
    }

    this->updateTrackerMode(probe.timestampUs, hasMovementObservation, current.deviceMoving);

    if (this->isDuplicateProbe(current, probe.timestampUs))
    {
        this->dedupedProbeCount++;
        return;
    }

    this->rememberProbe(current, probe.timestampUs);

    WifiProbeHistory history = this->tracker.Analyze(current, probe.timestampUs);
    this->tracker.Add(current, probe.timestampUs);

    this->lastProbeScore = history.combinedScore;
    this->lastProbeTemporalScore = history.persistenceScore;
    this->lastProbeSpatialScore = history.spatialScore;
    this->lastProbeDistanceMeters = history.maxDistanceMeters;
    this->lastTemporalWeight = history.temporalWeight;
    this->lastSpatialWeight = history.spatialWeight;
    this->lastMovingMode = history.movingMode;
    this->lastProbeMatchCount = history.matchCount;
    this->lastSeenRecent = history.seenRecent;
    this->lastSeenMedium = history.seenMedium;
    this->lastSeenOld = history.seenOld;
    this->lastSeenOldest = history.seenOldest;

    if (history.combinedScore >= this->config.persistenceAlertThreshold)
    {
        this->persistentAlertCount++;
        printf("[TAILER][ALERT] persistence mac=%s ssid=%s score=%.2f temporal=%.2f spatial=%.2f dist=%.1fm matches=%u\n",
               current.mac,
               current.ssid[0] != '\0' ? current.ssid : "<hidden>",
               history.combinedScore,
               history.persistenceScore,
               history.spatialScore,
               history.maxDistanceMeters,
               history.matchCount);
    }

    WifiFrameParser::MacToString(probe.source, this->lastProbeMac);
    strncpy(this->lastProbeSsid, probe.ssid, sizeof(this->lastProbeSsid) - 1);
    this->lastProbeSsid[sizeof(this->lastProbeSsid) - 1] = '\0';
    this->lastProbeRssi = probe.rssi;
    this->hasLastProbe = true;
    this->lastProbeTimestampUs = (unsigned long long)esp_timer_get_time();
}

void Tailer::updateAlertLevel(unsigned long long nowUs)
{
    if (this->baselineActive)
    {
        this->alertLevel = TailerAlertLevel::Normal;
        this->alertLatched = false;
        this->latchedMinimumAlertLevel = TailerAlertLevel::Normal;
        this->alertPendingResetAfterBaseline = true;
        return;
    }

    if (this->alertPendingResetAfterBaseline)
    {
        this->alertLevel = TailerAlertLevel::Normal;
        this->alertLatched = false;
        this->latchedMinimumAlertLevel = TailerAlertLevel::Normal;
        this->alertPendingResetAfterBaseline = false;
    }

    bool wifiScoreFresh = false;
    if (this->hasLastProbe && this->lastProbeTimestampUs > 0 && nowUs >= this->lastProbeTimestampUs)
    {
        unsigned long long ageUs = nowUs - this->lastProbeTimestampUs;
        unsigned long long freshnessUs = (unsigned long long)this->config.scoreFreshnessSeconds * 1000000ULL;
        wifiScoreFresh = ageUs <= freshnessUs;
    }

    bool btScoreFresh = false;
    if (this->config.bluetoothEnabled && this->hasLastBtAdvertisement && this->lastBtTimestampUs > 0 && nowUs >= this->lastBtTimestampUs)
    {
        unsigned long long ageUs = nowUs - this->lastBtTimestampUs;
        unsigned long long freshnessUs = (unsigned long long)this->config.scoreFreshnessSeconds * 1000000ULL;
        btScoreFresh = ageUs <= freshnessUs;
    }

    float wifiScore = wifiScoreFresh ? this->lastProbeScore : 0.0f;
    float btScore = btScoreFresh ? this->lastBtScore : 0.0f;
    float score = wifiScore > btScore ? wifiScore : btScore;

    if (!this->trackerMovingMode)
    {
        score *= this->config.stationarySensitivityMultiplier;
        if (score > 1.0f)
            score = 1.0f;
    }

    this->lastCombinedScore = score;

    switch (this->alertLevel)
    {
    case TailerAlertLevel::Normal:
        if (score >= this->config.lowEnterScore)
            this->alertLevel = TailerAlertLevel::Low;
        break;
    case TailerAlertLevel::Low:
        if (score >= this->config.mediumEnterScore)
            this->alertLevel = TailerAlertLevel::Medium;
        else if (score < this->config.lowExitScore)
            this->alertLevel = TailerAlertLevel::Normal;
        break;
    case TailerAlertLevel::Medium:
        if (score >= this->config.highEnterScore)
            this->alertLevel = TailerAlertLevel::High;
        else if (score < this->config.mediumExitScore)
            this->alertLevel = TailerAlertLevel::Low;
        break;
    case TailerAlertLevel::High:
        if (score < this->config.highExitScore)
            this->alertLevel = TailerAlertLevel::Medium;
        break;
    }

    if (this->config.latchAlertAfterDetection)
    {
        if (this->alertLevel != TailerAlertLevel::Normal)
            this->alertLatched = true;

        if (this->alertLatched)
        {
            if (this->alertLevel > this->latchedMinimumAlertLevel)
                this->latchedMinimumAlertLevel = this->alertLevel;

            if (this->config.latchHighestAlertLevel)
            {
                if (this->alertLevel < this->latchedMinimumAlertLevel)
                    this->alertLevel = this->latchedMinimumAlertLevel;
            }
            else
            {
                if (this->alertLevel == TailerAlertLevel::Normal)
                    this->alertLevel = TailerAlertLevel::Low;
            }
        }
    }
}