#include "app_tailer.h"

#include "esp_timer.h"
#include <cstdio>
#include <cstring>

AppTailer::AppTailer(const AppTailerConfig &config)
{
    this->config = config;

    WifiTrackingConfig trackingConfig;
    trackingConfig.recentMinutes = 5;
    trackingConfig.mediumMinutes = 10;
    trackingConfig.oldMinutes = 15;
    trackingConfig.oldestMinutes = 20;
    trackingConfig.maxEntries = 4096;

    this->tracker.Configure(trackingConfig);
}

AppTailer::~AppTailer()
{
    this->Stop();
}

bool AppTailer::Start()
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
    this->dedupedProbeCount = 0;
    this->persistentAlertCount = 0;
    this->lastProbeMac[0] = '\0';
    this->lastProbeSsid[0] = '\0';
    this->lastProbeRssi = 0;
    this->lastProbeScore = 0.0f;
    this->lastProbeMatchCount = 0;
    this->lastSeenRecent = false;
    this->lastSeenMedium = false;
    this->lastSeenOld = false;
    this->lastSeenOldest = false;
    this->hasLastProbe = false;

    this->lastHopUs = (unsigned long long)esp_timer_get_time();
    this->lastReportUs = this->lastHopUs;

    // Keep ignore list empty in initial baseline; we'll load rules later.
    this->ignore.Clear();
    this->tracker.Clear();
    this->recentProbes.clear();

    if (!this->wifi.StartPromiscuous(&AppTailer::onFrame, this))
        return false;

    printf("(AppTailer) Promiscuous capture active on channel %u\n", firstChannel);

    return true;
}

void AppTailer::Tick()
{
    if (!this->wifi.Started())
        return;

    unsigned long long nowUs = (unsigned long long)esp_timer_get_time();

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
             printf("(AppTailer) ch=%u frames=%u mgmt=%u probes=%u ignored=%u deduped=%u last=%s ssid=%s rssi=%d\n",
                   channel,
                   this->frameCount,
                   this->managementFrameCount,
                   this->probeRequestCount,
                   this->ignoredProbeCount,
                 this->dedupedProbeCount,
                   this->lastProbeMac,
                   this->lastProbeSsid,
                   this->lastProbeRssi);
        }
        else
        {
             printf("(AppTailer) ch=%u frames=%u mgmt=%u probes=%u ignored=%u deduped=%u\n",
                   channel,
                   this->frameCount,
                   this->managementFrameCount,
                   this->probeRequestCount,
                 this->ignoredProbeCount,
                 this->dedupedProbeCount);
        }

         printf("(AppTailer) trackerEntries=%u persistenceAlerts=%u lastScore=%.2f matches=%u windows=%c%c%c%c threshold=%.2f\n",
               this->tracker.Count(),
               this->persistentAlertCount,
             this->lastProbeScore,
             this->lastProbeMatchCount,
             this->lastSeenRecent ? 'R' : '-',
             this->lastSeenMedium ? 'M' : '-',
             this->lastSeenOld ? 'O' : '-',
             this->lastSeenOldest ? 'X' : '-',
             this->config.persistenceAlertThreshold);

        this->lastReportUs = nowUs;
    }
}

void AppTailer::Stop()
{
    this->wifi.StopPromiscuous();
    this->wifi.Stop();
}

void AppTailer::pruneDedupCache(unsigned long long nowUs)
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

bool AppTailer::isDuplicateProbe(const WifiProbeObservation &current, unsigned long long nowUs)
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

void AppTailer::rememberProbe(const WifiProbeObservation &current, unsigned long long nowUs)
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

void AppTailer::onFrame(const Byte *payload, unsigned short length, const WifiFrameMetadata &metadata, void *context)
{
    (void)payload;
    (void)length;

    if (context == nullptr)
        return;

    AppTailer *tailer = (AppTailer *)context;
    tailer->handleFrame(payload, length, metadata);
}

void AppTailer::handleFrame(const Byte *payload, unsigned short length, const WifiFrameMetadata &metadata)
{
    this->frameCount++;

    if (metadata.type == WIFI_PKT_MGMT)
        this->managementFrameCount++;

    WifiProbeRequest probe;
    if (!WifiFrameParser::TryParseProbeRequest(payload, length, metadata, &probe))
        return;

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

    if (this->isDuplicateProbe(current, probe.timestampUs))
    {
        this->dedupedProbeCount++;
        return;
    }

    this->rememberProbe(current, probe.timestampUs);

    WifiProbeHistory history = this->tracker.Analyze(current, probe.timestampUs);
    this->tracker.Add(current, probe.timestampUs);

    this->lastProbeScore = history.persistenceScore;
    this->lastProbeMatchCount = history.matchCount;
    this->lastSeenRecent = history.seenRecent;
    this->lastSeenMedium = history.seenMedium;
    this->lastSeenOld = history.seenOld;
    this->lastSeenOldest = history.seenOldest;

    if (history.persistenceScore >= this->config.persistenceAlertThreshold)
    {
        this->persistentAlertCount++;
        printf("(AppTailer) persistence alert mac=%s ssid=%s score=%.2f matches=%u\n",
               current.mac,
               current.ssid[0] != '\0' ? current.ssid : "<hidden>",
               history.persistenceScore,
               history.matchCount);
    }

    WifiFrameParser::MacToString(probe.source, this->lastProbeMac);
    strncpy(this->lastProbeSsid, probe.ssid, sizeof(this->lastProbeSsid) - 1);
    this->lastProbeSsid[sizeof(this->lastProbeSsid) - 1] = '\0';
    this->lastProbeRssi = probe.rssi;
    this->hasLastProbe = true;
}