#include <cstdio>
#include <cstring>
#include <string>

#include "api/common/time.h"
#include "api/transmission/mock_transceiver.h"
#include "api/io/uart.h"
#include "api/transmission/e22900t30.h"
#include "cli.h"
#include "talkie.h"

struct TalkieAppCaptureContext
{
    unsigned short counter = 0;
};

static unsigned short CaptureFrame(Byte *payload,
                                   unsigned short payloadLength,
                                   Byte *codec,
                                   unsigned short *sampleRate,
                                   void *context)
{
    if (payload == nullptr || codec == nullptr || sampleRate == nullptr || context == nullptr)
        return 0;

    TalkieAppCaptureContext *capture = (TalkieAppCaptureContext *)context;
    capture->counter++;

    // Placeholder payload for transport validation. Replace with codec output from microphone pipeline.
    int written = snprintf((char *)payload,
                           payloadLength,
                           "voice:%u",
                           capture->counter);

    if (written <= 0)
        return 0;

    if (written > payloadLength)
        written = payloadLength;

    *codec = 255; // App-local debug codec id until real codec is connected.
    *sampleRate = 8000;
    return (unsigned short)written;
}

static void PlaybackFrame(const Byte *payload,
                          unsigned short payloadLength,
                          Byte codec,
                          unsigned short sampleRate,
                          unsigned short sequence,
                          void *context)
{
    (void)context;

    char buffer[64] = {0};
    unsigned short copyLength = payloadLength;

    if (copyLength >= sizeof(buffer))
        copyLength = (unsigned short)(sizeof(buffer) - 1);

    if (copyLength > 0)
        memcpy(buffer, payload, copyLength);

    printf("[TALKIE][RX] seq=%u codec=%u rate=%u payload=%s\n",
           sequence,
           codec,
           sampleRate,
           buffer);
}

extern "C" void app_main(void)
{
    constexpr bool useMockRadio = true;

    Transceiver *transceiver = nullptr;

    MockTransceiver mockRadio({
        .latencyMs = 25,
        .maxQueue = 64,
        .dropEveryNPacket = 0,
    });

    const E22900T30Config transceiverConfig = {
        .serial = {
            .uart = new Uart({
                .port = UartPort::UartPort_1,
                .baudRate = UartBaudrate::UartBaudrate_115200,
                .tx = (GpioNum)6,
                .rx = (GpioNum)5,
            }),
            .baudRate = UartBaudrate::UartBaudrate_115200,
            .parity = E22900T30SerialParity::Parity_8N1},
        .interupt = {.aux = (GpioNum)4, .expander = NULL},
        .mode = {.m0 = (GpioNum)15, .m1 = (GpioNum)7, .expander = NULL},
        .network = {
            .id = 0,
            .address = 0,
            .airDataRate = E22900T30AirDataRate::Rate_9600,
            .subPacketSize = E22900T30SubPacketSize::Size_64b,
            .power = E22900T30Power::Power_30dBm,
            .channel = 18,
            .rssi = E22900T30RSSI::RSSI_Disabled,
            .ambientNoise = E22900T30RSSIAmbientNoise::AmbientNoise_Disable,
            .txmode = E22900T30TransmissionMode::TransmissionMode_Transparent,
            .reply = E22900T30Reply::Reply_Disabled,
            .lbt = E22900T30LBT::LBT_Disabled},
        .security = {.encryptByteH = 0, .encryptByteL = 0},
        .wor = {.mode = E22900T30WakeOnRadio::WOR_Receiver,
                .interval = E22900T30WakeOnRadioInterval::WOR_Interval_2000ms}};

    E22900T30 *e22900 = nullptr;
    if (useMockRadio)
    {
        transceiver = &mockRadio;
        printf("[APP][RADIO] using mock transceiver loopback\n");
    }
    else
    {
        e22900 = new E22900T30(transceiverConfig);
        transceiver = e22900;
        printf("[APP][RADIO] using E22900T30 hardware transceiver\n");
    }

    TalkieAppCaptureContext captureContext;

    TalkieConfig talkieConfig;
    talkieConfig.radio.transceiver = transceiver;
    talkieConfig.radio.frameIntervalMs = 20;
    talkieConfig.radio.packetLength = TRANSCEIVER_PACKAGE_SIZE;
    talkieConfig.radio.receiveTimeoutMs = 3;
    talkieConfig.input.pttPin = GPIO_NONE;
    talkieConfig.input.pttActiveLow = true;
    talkieConfig.input.pttDebounceMs = 25;
    talkieConfig.telemetry.statusIntervalMs = 1000;
    talkieConfig.audio.jitterBufferTargetFrames = 3;
    talkieConfig.audio.jitterBufferMaxFrames = 8;
    talkieConfig.audio.minCapturePayloadBytes = 12;
    talkieConfig.audio.rxPriorityHoldMs = 120;
    talkieConfig.audio.duplex = TalkieDuplexMode::HalfDuplexRxPriority;
    talkieConfig.ble.enabled = true;
    talkieConfig.ble.scanEnabled = true;
    talkieConfig.ble.scanDurationSeconds = 0;
    talkieConfig.ble.scan.allowDuplicates = false;
    talkieConfig.capture = &CaptureFrame;
    talkieConfig.captureContext = &captureContext;
    talkieConfig.playback = &PlaybackFrame;
    talkieConfig.playbackContext = nullptr;

    Talkie talkie(talkieConfig);

    if (!talkie.Start())
        printf("[TALKIE][ERROR] failed to start\n");

    TalkieCLI cli(TalkieCliConfig{});
    cli.Register("ping", "Health check command", [](const std::string &args) {
        (void)args;
        return std::string("pong");
    });

    cli.Register("talkie:state", "Show current talkie state", [&talkie](const std::string &args) {
        (void)args;
        if (talkie.State() == TalkieState::Transmit)
            return std::string("transmit");

        if (talkie.State() == TalkieState::Transceive)
            return std::string("transceive");

        return std::string("receive");
    });

    cli.Register("talkie:stats", "Show talkie transport statistics", [&talkie](const std::string &args) {
        (void)args;
        TalkieStats stats = talkie.Stats();

        char response[200] = {0};
        snprintf(response,
                 sizeof(response),
                 "tx=%u drop=%u txRxHold=%u txMinCap=%u txErr=%u rx=%u play=%u rxInv=%u rxTimeout=%u gap=%u ooo=%u dup=%u qDrop=%u lastRxSeq=%s",
                 stats.txFrames,
                 stats.txDroppedFrames,
                 stats.txSuppressedByRx,
                 stats.txSuppressedByMinCap,
                 stats.txErrors,
                 stats.rxFrames,
                 stats.rxPlaybackFrames,
                 stats.rxInvalidFrames,
                 stats.rxTimeouts,
                 stats.rxSequenceGaps,
                 stats.rxOutOfOrder,
                 stats.rxDuplicate,
                 stats.rxBufferDrops,
                 stats.hasLastRxSequence ? "set" : "none");

        return std::string(response);
    });

    cli.Register("talkie:reset", "Reset talkie transport statistics", [&talkie](const std::string &args) {
        (void)args;
        talkie.ResetStats();
        return std::string("ok");
    });

    cli.Register("talkie:bt", "Show bluetooth status and last advertisement", [&talkie](const std::string &args) {
        (void)args;
        TalkieStats stats = talkie.Stats();

        char response[240] = {0};
        snprintf(response,
                 sizeof(response),
                 "bt=%s scan=%s adv=%u lastMac=%s lastName=%s lastRssi=%d",
                 stats.bluetoothStarted ? "on" : "off",
                 stats.bluetoothScanning ? "on" : "off",
                 stats.bluetoothAdvertisements,
                 stats.lastBluetoothMac[0] == '\0' ? "-" : stats.lastBluetoothMac,
                 stats.lastBluetoothName[0] == '\0' ? "-" : stats.lastBluetoothName,
                 stats.lastBluetoothRssi);

        return std::string(response);
    });

    std::string startupHelp = cli.Dispatch("help");
    printf("[CLI][HELP] %s\n", startupHelp.c_str());

    while (true)
    {
        talkie.Tick();
        delay(1);
    }
}
