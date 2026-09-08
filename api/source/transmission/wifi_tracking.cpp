#include "api/transmission/wifi_tracking.h"

#include <cmath>
#include <cstring>

namespace
{
    double toRadians(double degrees)
    {
        return degrees * (3.14159265358979323846 / 180.0);
    }

    float haversineMeters(double lat1, double lon1, double lat2, double lon2)
    {
        const double dLat = toRadians(lat2 - lat1);
        const double dLon = toRadians(lon2 - lon1);

        const double a = sin(dLat * 0.5) * sin(dLat * 0.5) +
                         cos(toRadians(lat1)) * cos(toRadians(lat2)) *
                             sin(dLon * 0.5) * sin(dLon * 0.5);

        const double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
        return (float)(6371000.0 * c);
    }
}

WifiProbeTracker::WifiProbeTracker(const WifiTrackingConfig &config)
{
    this->Configure(config);
}

void WifiProbeTracker::Configure(const WifiTrackingConfig &config)
{
    this->config = config;

    if (this->config.recentMinutes == 0)
        this->config.recentMinutes = 1;

    if (this->config.mediumMinutes < this->config.recentMinutes)
        this->config.mediumMinutes = this->config.recentMinutes;

    if (this->config.oldMinutes < this->config.mediumMinutes)
        this->config.oldMinutes = this->config.mediumMinutes;

    if (this->config.oldestMinutes < this->config.oldMinutes)
        this->config.oldestMinutes = this->config.oldMinutes;

    if (this->config.maxEntries == 0)
        this->config.maxEntries = 256;
}

void WifiProbeTracker::Clear()
{
    this->observations.clear();
}

unsigned long long WifiProbeTracker::minutesToUs(unsigned int minutes) const
{
    return (unsigned long long)minutes * 60ULL * 1000000ULL;
}

void WifiProbeTracker::prune(unsigned long long nowUs)
{
    unsigned long long oldestWindowUs = this->minutesToUs(this->config.oldestMinutes);

    while (!this->observations.empty())
    {
        const WifiProbeObservation &entry = this->observations.front();
        if (nowUs < entry.timestampUs)
            break;

        unsigned long long ageUs = nowUs - entry.timestampUs;
        if (ageUs <= oldestWindowUs)
            break;

        this->observations.pop_front();
    }

    while (this->observations.size() > this->config.maxEntries)
        this->observations.pop_front();
}

bool WifiProbeTracker::matches(const WifiProbeObservation &existing, const WifiProbeObservation &current) const
{
    bool macMatch = existing.mac[0] != '\0' && current.mac[0] != '\0' && strcmp(existing.mac, current.mac) == 0;
    bool ssidMatch = existing.ssid[0] != '\0' && current.ssid[0] != '\0' && strcmp(existing.ssid, current.ssid) == 0;

    return macMatch || ssidMatch;
}

WifiProbeHistory WifiProbeTracker::Analyze(const WifiProbeObservation &current, unsigned long long nowUs)
{
    this->prune(nowUs);

    WifiProbeHistory history;

    unsigned long long recentUs = this->minutesToUs(this->config.recentMinutes);
    unsigned long long mediumUs = this->minutesToUs(this->config.mediumMinutes);
    unsigned long long oldUs = this->minutesToUs(this->config.oldMinutes);
    unsigned long long oldestUs = this->minutesToUs(this->config.oldestMinutes);
    float maxDistanceMeters = 0.0f;

    for (size_t i = 0; i < this->observations.size(); i++)
    {
        const WifiProbeObservation &entry = this->observations[i];
        if (!this->matches(entry, current))
            continue;

        if (nowUs < entry.timestampUs)
            continue;

        unsigned long long ageUs = nowUs - entry.timestampUs;
        if (ageUs > oldestUs)
            continue;

        history.matchCount++;

        if (ageUs <= recentUs)
            history.seenRecent = true;
        else if (ageUs <= mediumUs)
            history.seenMedium = true;
        else if (ageUs <= oldUs)
            history.seenOld = true;
        else
            history.seenOldest = true;

        if (current.hasLocation && entry.hasLocation)
        {
            float distance = haversineMeters(current.latitude, current.longitude, entry.latitude, entry.longitude);
            if (distance > maxDistanceMeters)
                maxDistanceMeters = distance;
        }
    }

    if (history.seenRecent)
        history.persistenceScore += 0.25f;

    if (history.seenMedium)
        history.persistenceScore += 0.25f;

    if (history.seenOld)
        history.persistenceScore += 0.25f;

    if (history.seenOldest)
        history.persistenceScore += 0.25f;

    if (history.matchCount >= 8)
        history.persistenceScore += 0.40f;
    else if (history.matchCount >= 4)
        history.persistenceScore += 0.30f;
    else if (history.matchCount >= 2)
        history.persistenceScore += 0.20f;
    else if (history.matchCount >= 1)
        history.persistenceScore += 0.20f;

    if (history.persistenceScore > 1.0f)
        history.persistenceScore = 1.0f;

    history.maxDistanceMeters = maxDistanceMeters;

    if (maxDistanceMeters >= 100.0f)
        history.spatialScore = 1.0f;
    else if (maxDistanceMeters >= 50.0f)
        history.spatialScore = 0.75f;
    else if (maxDistanceMeters >= 20.0f)
        history.spatialScore = 0.50f;
    else if (maxDistanceMeters >= 5.0f)
        history.spatialScore = 0.25f;
    else
        history.spatialScore = 0.0f;

    if (current.hasLocation)
    {
        float temporalWeight = current.temporalWeight;
        float spatialWeight = current.spatialWeight;

        if (temporalWeight < 0.0f)
            temporalWeight = 0.0f;
        if (temporalWeight > 1.0f)
            temporalWeight = 1.0f;

        if (spatialWeight < 0.0f)
            spatialWeight = 0.0f;
        if (spatialWeight > 1.0f)
            spatialWeight = 1.0f;

        float sum = temporalWeight + spatialWeight;
        if (sum <= 0.0f)
        {
            temporalWeight = 1.0f;
            spatialWeight = 0.0f;
        }
        else
        {
            temporalWeight /= sum;
            spatialWeight /= sum;
        }

        history.temporalWeight = temporalWeight;
        history.spatialWeight = spatialWeight;
        history.movingMode = current.deviceMoving;

        history.combinedScore = history.persistenceScore * history.temporalWeight +
                                history.spatialScore * history.spatialWeight;
    }
    else
    {
        history.temporalWeight = 1.0f;
        history.spatialWeight = 0.0f;
        history.movingMode = false;
        history.combinedScore = history.persistenceScore;
    }

    if (history.combinedScore > 1.0f)
        history.combinedScore = 1.0f;

    return history;
}

void WifiProbeTracker::Add(const WifiProbeObservation &observation, unsigned long long nowUs)
{
    this->prune(nowUs);
    this->observations.push_back(observation);

    if (this->observations.size() > this->config.maxEntries)
        this->observations.pop_front();
}

unsigned int WifiProbeTracker::Count() const
{
    return (unsigned int)this->observations.size();
}
