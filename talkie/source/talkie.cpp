#include "talkie.h"

#include "api/common/time.h"
#include "api/io/gpio.h"
#include <cstdio>
#include <cstring>

Talkie::Talkie(const TalkieConfig &config)
{
    this->config = config;

    this->normalizeLegacyConfig();

    if (this->config.radio.packetLength > sizeof(this->txBuffer))
        this->config.radio.packetLength = sizeof(this->txBuffer);

    if (this->config.radio.packetLength <= Package::HeaderSize)
        this->config.radio.packetLength = (unsigned short)sizeof(this->txBuffer);

    if (this->config.radio.frameIntervalMs == 0)
        this->config.radio.frameIntervalMs = 20;

    if (this->config.radio.receiveTimeoutMs == 0)
        this->config.radio.receiveTimeoutMs = 1;

    if (this->config.input.pttDebounceMs == 0)
        this->config.input.pttDebounceMs = 25;

    if (this->config.telemetry.statusIntervalMs == 0)
        this->config.telemetry.statusIntervalMs = 1000;

    if (this->config.audio.jitterBufferMaxFrames == 0)
        this->config.audio.jitterBufferMaxFrames = 8;

    if (this->config.audio.jitterBufferTargetFrames == 0)
        this->config.audio.jitterBufferTargetFrames = 1;

    if (this->config.audio.jitterBufferTargetFrames > this->config.audio.jitterBufferMaxFrames)
        this->config.audio.jitterBufferTargetFrames = this->config.audio.jitterBufferMaxFrames;
}

Talkie::~Talkie()
{
    this->Stop();
}

bool Talkie::Start()
{
    if (this->config.radio.transceiver == nullptr)
        return false;

    if (this->config.input.pttPin != GPIO_NONE)
    {
        Gpio::Mode(this->config.input.pttPin, GpioMode::GpioMode_Input);
        Gpio::Pull(this->config.input.pttPin, GpioPull::GpioPull_Up);
    }

    this->state = TalkieState::Receive;
    this->stats = TalkieStats{};
    this->sequence = 0;
    this->lastSendAtMs = millis();
    this->lastStatusAtMs = this->lastSendAtMs;
    this->lastPlaybackAtMs = this->lastSendAtMs;
    this->lastRxFrameAtMs = this->lastSendAtMs;
    this->lastPttEdgeAtMs = this->lastSendAtMs;
    this->pttStableState = this->pttPressed();
    this->hasExpectedSequence = false;
    this->playbackBuffer.clear();

        printf("[TALKIE][INIT] started packet=%u frame=%ums bt=%s scan=%s duplex=%s\n",
           this->config.radio.packetLength,
           this->config.radio.frameIntervalMs,
           this->config.ble.enabled ? "on" : "off",
            this->config.ble.scanEnabled ? "on" : "off",
            this->config.audio.duplex == TalkieDuplexMode::HalfDuplexRxPriority ? "half-rx-prio" : "full-exp");

    if (this->config.ble.enabled)
    {
        if (this->bluetooth.Start())
        {
            this->stats.bluetoothStarted = true;
            this->bluetooth.SetScanParams(this->config.ble.scan);

            if (this->config.ble.scanEnabled)
            {
                if (this->bluetooth.StartScan(&Talkie::onBluetoothAdvertisement, this, this->config.ble.scanDurationSeconds))
                    this->stats.bluetoothScanning = true;
                else
                    printf("[TALKIE][BT][WARN] scan start failed\n");
            }
        }
        else
            printf("[TALKIE][BT][WARN] bluetooth start failed\n");
    }

    if (this->config.audio.adapter != nullptr)
    {
        if (!this->config.audio.adapter->Start())
            printf("[TALKIE][AUDIO][WARN] audio adapter start failed\n");
        else
            printf("[TALKIE][AUDIO] adapter started\n");
    }

    return true;
}

void Talkie::Tick()
{
    unsigned long nowMs = millis();
    bool usePtt = this->config.input.pttPin != GPIO_NONE;
    bool pressed = usePtt ? this->pttPressed() : true;

    if (usePtt && pressed != this->pttStableState)
    {
        if ((nowMs - this->lastPttEdgeAtMs) >= this->config.input.pttDebounceMs)
        {
            this->pttStableState = pressed;
            this->lastPttEdgeAtMs = nowMs;
        }
    }
    else if (usePtt)
        this->lastPttEdgeAtMs = nowMs;

    if (usePtt)
    {
        if (this->pttStableState && this->state != TalkieState::Transmit)
            this->setState(TalkieState::Transmit);
        else if (!this->pttStableState && this->state != TalkieState::Receive)
            this->setState(TalkieState::Receive);

        if (this->state == TalkieState::Transmit)
            this->transmitTick(nowMs);
        else
        {
            this->receiveTick(nowMs);
            this->playbackTick(nowMs);
        }
    }
    else
    {
        if (this->state != TalkieState::Transceive)
            this->setState(TalkieState::Transceive);

        this->receiveTick(nowMs);
        this->playbackTick(nowMs);
        this->transmitTick(nowMs);
    }

    if ((nowMs - this->lastStatusAtMs) >= this->config.telemetry.statusIntervalMs)
    {
        this->lastStatusAtMs = nowMs;
        this->stats.bluetoothStarted = this->bluetooth.Started();
        this->stats.bluetoothScanning = this->bluetooth.Scanning();

        const char *stateName = "rx";
        if (this->state == TalkieState::Transmit)
            stateName = "tx";
        else if (this->state == TalkieState::Transceive)
            stateName = "trx";

        printf("[TALKIE][STAT] state=%s tx=%u drop=%u txRxHold=%u txMinCap=%u txErr=%u rx=%u play=%u rxInv=%u rxTimeout=%u gap=%u ooo=%u dup=%u q=%u bt=%s scan=%s btAdv=%u\n",
               stateName,
               this->stats.txFrames,
               this->stats.txDroppedFrames,
               this->stats.txSuppressedByRx,
               this->stats.txSuppressedByMinCap,
               this->stats.txErrors,
               this->stats.rxFrames,
             this->stats.rxPlaybackFrames,
               this->stats.rxInvalidFrames,
               this->stats.rxTimeouts,
             this->stats.rxSequenceGaps,
             this->stats.rxOutOfOrder,
             this->stats.rxDuplicate,
             (unsigned int)this->playbackBuffer.size(),
               this->stats.bluetoothStarted ? "on" : "off",
               this->stats.bluetoothScanning ? "on" : "off",
               this->stats.bluetoothAdvertisements);
    }
}

void Talkie::Stop()
{
    if (this->config.audio.adapter != nullptr)
        this->config.audio.adapter->Stop();

    if (this->bluetooth.Scanning())
        this->bluetooth.StopScan();

    if (this->bluetooth.Started())
        this->bluetooth.Stop();

    this->stats.bluetoothStarted = false;
    this->stats.bluetoothScanning = false;
}

TalkieState Talkie::State() const
{
    return this->state;
}

unsigned short Talkie::Sequence() const
{
    return this->sequence;
}

TalkieStats Talkie::Stats() const
{
    return this->stats;
}

const TalkieConfig &Talkie::Config() const
{
    return this->config;
}

void Talkie::ResetStats()
{
    this->stats = TalkieStats{};
    this->hasExpectedSequence = false;
}

bool Talkie::pttPressed() const
{
    if (this->config.input.pttPin == GPIO_NONE)
        return false;

    bool pinState = Gpio::Read(this->config.input.pttPin);
    return this->config.input.pttActiveLow ? !pinState : pinState;
}

void Talkie::setState(TalkieState state)
{
    this->state = state;

    if (state == TalkieState::Transmit)
        printf("[TALKIE][PTT] TX\n");
    else
        printf("[TALKIE][PTT] RX\n");
}

void Talkie::transmitTick(unsigned long nowMs)
{
    if ((nowMs - this->lastSendAtMs) < this->config.radio.frameIntervalMs)
        return;

    if (!this->canTransmit(nowMs))
    {
        this->stats.txSuppressedByRx++;
        this->lastSendAtMs = nowMs;
        return;
    }

    unsigned short maxPayloadLength = Package::MaxPayloadLength(this->config.radio.packetLength);
    if (maxPayloadLength > sizeof(this->payloadBuffer))
        maxPayloadLength = sizeof(this->payloadBuffer);

    Byte codec = 0;
    unsigned short sampleRate = 8000;
    unsigned short payloadLength = 0;

    if (this->config.audio.adapter != nullptr)
    {
        payloadLength = this->config.audio.adapter->Capture(this->payloadBuffer,
                                                            maxPayloadLength,
                                                            &codec,
                                                            &sampleRate);
    }
    else if (this->config.capture != nullptr)
    {
        payloadLength = this->config.capture(this->payloadBuffer,
                                             maxPayloadLength,
                                             &codec,
                                             &sampleRate,
                                             this->config.captureContext);
    }

    if (payloadLength == 0)
    {
        this->stats.txDroppedFrames++;
        this->lastSendAtMs = nowMs;
        return;
    }

    if (!this->capturePassesGate(this->payloadBuffer, payloadLength, codec, sampleRate))
    {
        this->stats.txSuppressedByMinCap++;
        this->lastSendAtMs = nowMs;
        return;
    }

    PackageHeader header;
    header.type = PackageType::Voice;
    header.sequence = this->sequence++;
    header.flags = codec;
    header.meta = sampleRate;

    memset(this->txBuffer, 0, this->config.radio.packetLength);

    unsigned short written = 0;
    if (!Package::Encode(header,
                         this->payloadBuffer,
                         payloadLength,
                         this->txBuffer,
                         this->config.radio.packetLength,
                         &written))
    {
        this->stats.txErrors++;
        this->lastSendAtMs = nowMs;
        return;
    }

    if (!this->config.radio.transceiver->Send(this->txBuffer, this->config.radio.packetLength))
    {
        printf("[TALKIE][TX][WARN] send failed\n");
        this->stats.txErrors++;
    }
    else
        this->stats.txFrames++;

    this->lastSendAtMs = nowMs;
}

void Talkie::receiveTick(unsigned long nowMs)
{
    int count = this->config.radio.transceiver->Receive(this->rxBuffer,
                                                        this->config.radio.packetLength,
                                                        this->config.radio.receiveTimeoutMs);

    if (count != this->config.radio.packetLength)
    {
        if (count <= 0)
            this->stats.rxTimeouts++;

        return;
    }

    PackageHeader header;
    unsigned short payloadLength = 0;

    if (!Package::Decode(this->rxBuffer,
                         this->config.radio.packetLength,
                         &header,
                         this->payloadBuffer,
                         sizeof(this->payloadBuffer),
                         &payloadLength))
    {
        this->stats.rxInvalidFrames++;
        return;
    }

    if (header.type != PackageType::Voice)
    {
        this->stats.rxInvalidFrames++;
        return;
    }

    this->stats.rxFrames++;
    this->stats.lastRxSequence = header.sequence;
    this->stats.hasLastRxSequence = true;
    this->lastRxFrameAtMs = nowMs;

    this->updateSequenceStats(header.sequence);

    if (this->playbackBuffer.size() >= this->config.audio.jitterBufferMaxFrames)
    {
        this->playbackBuffer.pop_front();
        this->stats.rxBufferDrops++;
    }

    PlaybackFrame frame;
    frame.codec = header.flags;
    frame.sampleRate = header.meta;
    frame.sequence = header.sequence;
    frame.payloadLength = payloadLength;

    if (payloadLength > 0)
        memcpy(frame.payload, this->payloadBuffer, payloadLength);

    this->playbackBuffer.push_back(frame);
}

void Talkie::playbackTick(unsigned long nowMs)
{
    if (this->playbackBuffer.empty())
        return;

    if ((nowMs - this->lastPlaybackAtMs) < this->config.radio.frameIntervalMs)
        return;

    if (this->playbackBuffer.size() < this->config.audio.jitterBufferTargetFrames)
        return;

    PlaybackFrame frame = this->playbackBuffer.front();
    this->playbackBuffer.pop_front();

    bool played = false;
    if (this->config.audio.adapter != nullptr)
        played = this->config.audio.adapter->Playback(frame.payload,
                                                      frame.payloadLength,
                                                      frame.codec,
                                                      frame.sampleRate,
                                                      frame.sequence);
    else if (this->config.playback != nullptr)
    {
        this->config.playback(frame.payload,
                              frame.payloadLength,
                              frame.codec,
                              frame.sampleRate,
                              frame.sequence,
                              this->config.playbackContext);
        played = true;
    }

    if (played)
        this->stats.rxPlaybackFrames++;

    this->lastPlaybackAtMs = nowMs;
}

void Talkie::updateSequenceStats(unsigned short sequence)
{
    if (!this->hasExpectedSequence)
    {
        this->expectedSequence = (unsigned short)(sequence + 1);
        this->hasExpectedSequence = true;
        return;
    }

    if (sequence == this->expectedSequence)
    {
        this->expectedSequence = (unsigned short)(sequence + 1);
        return;
    }

    if (sequence > this->expectedSequence)
    {
        this->stats.rxSequenceGaps += (unsigned int)(sequence - this->expectedSequence);
        this->expectedSequence = (unsigned short)(sequence + 1);
        return;
    }

    if ((unsigned short)(this->expectedSequence - sequence) == 1)
        this->stats.rxDuplicate++;
    else
        this->stats.rxOutOfOrder++;
}

bool Talkie::canTransmit(unsigned long nowMs) const
{
    if (this->config.audio.duplex == TalkieDuplexMode::FullDuplexExperimental)
        return true;

    if (!this->playbackBuffer.empty())
        return false;

    if ((nowMs - this->lastRxFrameAtMs) < this->config.audio.rxPriorityHoldMs)
        return false;

    return true;
}

bool Talkie::capturePassesGate(const Byte *payload, unsigned short payloadLength, Byte codec, unsigned short sampleRate) const
{
    if (payload == nullptr)
        return false;

    if (payloadLength < this->config.audio.minCapturePayloadBytes)
        return false;

    if (this->config.captureGate != nullptr)
        return this->config.captureGate(payload, payloadLength, codec, sampleRate, this->config.captureGateContext);

    return true;
}

void Talkie::onBluetoothAdvertisement(const BluetoothAdvertisement &advertisement, void *context)
{
    if (context == nullptr)
        return;

    ((Talkie *)context)->handleBluetoothAdvertisement(advertisement);
}

void Talkie::handleBluetoothAdvertisement(const BluetoothAdvertisement &advertisement)
{
    this->stats.bluetoothAdvertisements++;
    this->stats.lastBluetoothRssi = advertisement.rssi;

    strncpy(this->stats.lastBluetoothName, advertisement.name, sizeof(this->stats.lastBluetoothName) - 1);
    this->stats.lastBluetoothName[sizeof(this->stats.lastBluetoothName) - 1] = '\0';

    strncpy(this->stats.lastBluetoothMac, advertisement.mac, sizeof(this->stats.lastBluetoothMac) - 1);
    this->stats.lastBluetoothMac[sizeof(this->stats.lastBluetoothMac) - 1] = '\0';
}

void Talkie::normalizeLegacyConfig()
{
    if (this->config.radio.transceiver == nullptr)
        this->config.radio.transceiver = this->config.transceiver;

    if (this->config.radio.packetLength == 240 && this->config.radioPacketLength != 240)
        this->config.radio.packetLength = this->config.radioPacketLength;

    if (this->config.radio.frameIntervalMs == 20 && this->config.frameIntervalMs != 20)
        this->config.radio.frameIntervalMs = this->config.frameIntervalMs;

    if (this->config.radio.receiveTimeoutMs == 3 && this->config.receiveTimeoutMs != 3)
        this->config.radio.receiveTimeoutMs = this->config.receiveTimeoutMs;

    if (this->config.input.pttPin == GPIO_NONE && this->config.pttPin != GPIO_NONE)
        this->config.input.pttPin = this->config.pttPin;

    if (this->config.input.pttActiveLow == true && this->config.pttActiveLow == false)
        this->config.input.pttActiveLow = this->config.pttActiveLow;

    if (this->config.input.pttDebounceMs == 25 && this->config.pttDebounceMs != 25)
        this->config.input.pttDebounceMs = this->config.pttDebounceMs;

    if (this->config.telemetry.statusIntervalMs == 1000 && this->config.statusIntervalMs != 1000)
        this->config.telemetry.statusIntervalMs = this->config.statusIntervalMs;
}
