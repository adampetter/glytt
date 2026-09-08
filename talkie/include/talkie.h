#pragma once

#include "api/common/types.h"
#include "api/transmission/audio_adapter.h"
#include "api/transmission/bluetooth.h"
#include "api/transmission/package.h"
#include "api/transmission/transceiver.h"

#include <deque>

enum class TalkieState
{
    Receive = 0,
    Transmit = 1,
    Transceive = 2
};

enum class TalkieDuplexMode
{
    HalfDuplexRxPriority = 0,
    FullDuplexExperimental = 1
};

typedef unsigned short (*TalkieCaptureCallback)(Byte *payload,
                                                unsigned short payloadLength,
                                                Byte *codec,
                                                unsigned short *sampleRate,
                                                void *context);

typedef void (*TalkiePlaybackCallback)(const Byte *payload,
                                       unsigned short payloadLength,
                                       Byte codec,
                                       unsigned short sampleRate,
                                       unsigned short sequence,
                                       void *context);

typedef bool (*TalkieCaptureGateCallback)(const Byte *payload,
                                          unsigned short payloadLength,
                                          Byte codec,
                                          unsigned short sampleRate,
                                          void *context);

struct TalkieConfig
{
    struct Radio
    {
        Transceiver *transceiver = nullptr;
        unsigned short packetLength = 240;
        unsigned int frameIntervalMs = 20;
        unsigned int receiveTimeoutMs = 3;
    } radio;

    struct Input
    {
        GpioNum pttPin = GPIO_NONE;
        bool pttActiveLow = true;
        unsigned int pttDebounceMs = 25;
    } input;

    struct Telemetry
    {
        unsigned int statusIntervalMs = 1000;
    } telemetry;

    struct Audio
    {
        AudioAdapter *adapter = nullptr;
        unsigned int jitterBufferTargetFrames = 3;
        unsigned int jitterBufferMaxFrames = 8;
        unsigned short minCapturePayloadBytes = 12;
        unsigned int rxPriorityHoldMs = 120;
        TalkieDuplexMode duplex = TalkieDuplexMode::HalfDuplexRxPriority;
    } audio;

    struct Ble
    {
        bool enabled = false;
        bool scanEnabled = false;
        unsigned int scanDurationSeconds = 0;
        BluetoothScanParams scan = {};
    } ble;

    // Legacy compatibility fields. New code should use radio/input/telemetry groups.
    Transceiver *transceiver = nullptr;

    GpioNum pttPin = GPIO_NONE;
    bool pttActiveLow = true;

    unsigned short radioPacketLength = 240;
    unsigned int frameIntervalMs = 20;
    unsigned int receiveTimeoutMs = 3;
    unsigned int pttDebounceMs = 25;
    unsigned int statusIntervalMs = 1000;

    TalkieCaptureCallback capture = nullptr;
    void *captureContext = nullptr;

    TalkiePlaybackCallback playback = nullptr;
    void *playbackContext = nullptr;

    TalkieCaptureGateCallback captureGate = nullptr;
    void *captureGateContext = nullptr;
};

struct TalkieStats
{
    unsigned int txFrames = 0;
    unsigned int txDroppedFrames = 0;
    unsigned int txSuppressedByRx = 0;
    unsigned int txSuppressedByMinCap = 0;
    unsigned int txErrors = 0;
    unsigned int rxFrames = 0;
    unsigned int rxPlaybackFrames = 0;
    unsigned int rxInvalidFrames = 0;
    unsigned int rxTimeouts = 0;
    unsigned int rxBufferDrops = 0;
    unsigned int rxSequenceGaps = 0;
    unsigned int rxOutOfOrder = 0;
    unsigned int rxDuplicate = 0;
    unsigned short lastRxSequence = 0;
    bool hasLastRxSequence = false;
    bool bluetoothStarted = false;
    bool bluetoothScanning = false;
    unsigned int bluetoothAdvertisements = 0;
    signed char lastBluetoothRssi = 0;
    char lastBluetoothName[32] = {0};
    char lastBluetoothMac[18] = {0};
};

class Talkie
{
private:
    struct PlaybackFrame
    {
        Byte codec = 0;
        unsigned short sampleRate = 8000;
        unsigned short sequence = 0;
        unsigned short payloadLength = 0;
        Byte payload[230] = {0};
    };

    TalkieConfig config;
    TalkieState state = TalkieState::Receive;
    TalkieStats stats;
    Bluetooth bluetooth;

    unsigned short sequence = 0;
    unsigned long lastSendAtMs = 0;
    unsigned long lastStatusAtMs = 0;
    unsigned long lastPlaybackAtMs = 0;
    unsigned long lastRxFrameAtMs = 0;
    unsigned long lastPttEdgeAtMs = 0;
    bool pttStableState = false;
    unsigned short expectedSequence = 0;
    bool hasExpectedSequence = false;

    std::deque<PlaybackFrame> playbackBuffer;

    Byte txBuffer[240] = {0};
    Byte rxBuffer[240] = {0};
    Byte payloadBuffer[230] = {0};

    bool pttPressed() const;
    void setState(TalkieState state);
    void transmitTick(unsigned long nowMs);
    void receiveTick(unsigned long nowMs);
    void playbackTick(unsigned long nowMs);
    void updateSequenceStats(unsigned short sequence);
    bool canTransmit(unsigned long nowMs) const;
    bool capturePassesGate(const Byte *payload, unsigned short payloadLength, Byte codec, unsigned short sampleRate) const;
    static void onBluetoothAdvertisement(const BluetoothAdvertisement &advertisement, void *context);
    void handleBluetoothAdvertisement(const BluetoothAdvertisement &advertisement);
    void normalizeLegacyConfig();

public:
    Talkie(const TalkieConfig &config);
    ~Talkie();

    bool Start();
    void Tick();
    void Stop();

    TalkieState State() const;
    unsigned short Sequence() const;
    TalkieStats Stats() const;
    const TalkieConfig &Config() const;
    void ResetStats();
};
