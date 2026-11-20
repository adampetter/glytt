#include <cstdio>

#include "api/io/uart.h"
#include "api/transmission/e22900t30.h"
#include "receiver.h"

extern "C" void app_main(void)
{
    const GpioNum greenLedPin = (GpioNum)11;
    const GpioNum redLedPin = (GpioNum)9;
    const GpioNum blueLedPin = (GpioNum)10;
    const GpioNum buzzerPin = (GpioNum)12;

    struct RadioAttempt
    {
        UartPort port;
        GpioNum tx;
        GpioNum rx;
        GpioNum aux;
        GpioNum m0;
        GpioNum m1;
        UartBaudrate baud;
        bool startupInNormalModeOnly;
        const char *label;
    };

    const RadioAttempt attempts[] = {
        {UartPort::UartPort_1, (GpioNum)6, (GpioNum)5, (GpioNum)1, (GpioNum)42, (GpioNum)2, UartBaudrate::UartBaudrate_9600, false, "uart1 tx6/rx5 aux1 m0=42 m1=2 9600 cfg"},
        {UartPort::UartPort_1, (GpioNum)6, (GpioNum)5, (GpioNum)1, (GpioNum)42, (GpioNum)2, UartBaudrate::UartBaudrate_115200, false, "uart1 tx6/rx5 aux1 m0=42 m1=2 115200 cfg"},
        {UartPort::UartPort_1, (GpioNum)5, (GpioNum)6, (GpioNum)1, (GpioNum)42, (GpioNum)2, UartBaudrate::UartBaudrate_9600, false, "uart1 tx5/rx6 aux1 m0=42 m1=2 9600 cfg"},
        {UartPort::UartPort_1, (GpioNum)6, (GpioNum)5, GPIO_NONE, GPIO_NONE, GPIO_NONE, UartBaudrate::UartBaudrate_9600, true, "uart1 tx6/rx5 normal-only 9600"},
        {UartPort::UartPort_1, (GpioNum)5, (GpioNum)6, GPIO_NONE, GPIO_NONE, GPIO_NONE, UartBaudrate::UartBaudrate_9600, true, "uart1 tx5/rx6 normal-only 9600"},
        {UartPort::UartPort_1, (GpioNum)43, (GpioNum)44, GPIO_NONE, GPIO_NONE, GPIO_NONE, UartBaudrate::UartBaudrate_9600, true, "uart1 tx43/rx44 normal-only 9600"},
    };

    E22900T30 *radio = nullptr;
    for (unsigned int i = 0; i < (sizeof(attempts) / sizeof(attempts[0])); i++)
    {
        const RadioAttempt &attempt = attempts[i];
        printf("[RECEIVER][RADIO] init attempt %u/%u: %s\n",
               (unsigned int)(i + 1),
               (unsigned int)(sizeof(attempts) / sizeof(attempts[0])),
               attempt.label);

        const E22900T30Config transceiverConfig = {
            .startupInNormalModeOnly = attempt.startupInNormalModeOnly,
            .serial = {
                .uart = new Uart({
                    .port = attempt.port,
                    .baudRate = attempt.baud,
                    .tx = attempt.tx,
                    .rx = attempt.rx,
                }),
                .baudRate = attempt.baud,
                .parity = E22900T30SerialParity::Parity_8N1},
            .interupt = {.aux = attempt.aux, .expander = NULL},
            .mode = {.m0 = attempt.m0, .m1 = attempt.m1, .expander = NULL},
            .network = {
                .id = 0,
                .address = 0,
                .airDataRate = E22900T30AirDataRate::Rate_9600,
                .subPacketSize = E22900T30SubPacketSize::Size_240b,
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

        E22900T30 *candidate = new E22900T30(transceiverConfig);
        if (candidate->Ready() || attempt.startupInNormalModeOnly)
        {
            radio = candidate;
            printf("[RECEIVER][RADIO] selected: %s\n", attempt.label);
            break;
        }

        delete candidate;
    }

    if (!radio)
    {
        printf("[RECEIVER][WARN] no radio init attempt succeeded\n");
        return;
    }

    ReceiverConfig config;
    config.rate = 30;
    config.ledGreen = greenLedPin;
    config.ledBlue = blueLedPin;
    config.ledRed = redLedPin;
    config.ledActiveLow = false;
    config.buzzer = buzzerPin;
    config.buzzerActiveLow = false;
    config.alarmDurationMs = 60000;
    config.alarmBlinkIntervalMs = 500;
    config.powerBlinkIntervalMs = 30000;
    config.blinkOnMs = 100;
    config.statusBlueOnMs = 2000;
    config.packetLength = TRANSCEIVER_PACKAGE_SIZE;
    config.receiveTimeoutMs = 500;
    config.transceiver = radio;

    Receiver receiver(config);

    if (!receiver.Start())
    {
        printf("[RECEIVER][ERROR] failed to start\n");
        return;
    }

    receiver.Run();
}
