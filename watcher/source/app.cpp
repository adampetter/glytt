#include <cstdio>

#include "api/io/i2c.h"
#include "api/io/uart.h"
#include "api/motion/lis3dh.h"
#include "api/navigation/pa1010d.h"
#include "api/transmission/e22900t30.h"
#include "watcher.h"

extern "C" void app_main(void)
{
    const GpioNum i2cSdaPin = (GpioNum)14;
    const GpioNum i2cSclPin = (GpioNum)13;
    const GpioNum lis3dhInterruptPin = (GpioNum)46;
    const GpioNum blueLedPin = (GpioNum)10;

    // IRQ sensitivity tuning:
    // Lower threshold + lower duration => more sensitive.
    // Higher threshold + higher duration => less sensitive.
    const Lis3dhThreshold irqThreshold = Lis3dhThreshold::Lis3dhThreshold_0;
    const Byte irqDuration = 1;
    const Lis3dhThreshold wakeThreshold = Lis3dhThreshold::Lis3dhThreshold_0;
    const Byte wakeDuration = 0;
    const Lis3dhRange range = Lis3dhRange::Lis3dhRange_2G;
    const bool interruptActiveLow = true;
    const bool interruptLatched = true;
    const bool highPassOnInterrupt = true;
    // INT1_CFG: high-event only on X/Y/Z (0x2A) to reduce background noise.
    const Byte interruptConfig = 0x2A;

    I2c *watcherI2c = new I2c({.port = I2cPort::I2cPort_0,
                               .sda = i2cSdaPin,
                               .scl = i2cSclPin,
                               .mode = I2cMode::I2cMode_Master,
                               .frequency = I2cFrequency::I2cFrequency_01M,
                               .internalPullup = true});

    Byte lis3dhAddress = 0x18;
    if (!watcherI2c->Exists(lis3dhAddress) && watcherI2c->Exists(0x19))
        lis3dhAddress = 0x19;

    Lis3dh *lis3dh = nullptr;
    if (watcherI2c->Exists(lis3dhAddress))
    {
        Lis3dhConfig lis3dhConfig = {
            .i2c = watcherI2c,
            .address = lis3dhAddress,
            .mode = Lis3dhMode::Lis3dhMode_ByPass,
            .sampleRate = Lis3dhSampleRate::Lis3dhSampleRate_400,
            .axis = Lis3dhAxis::Lis3dhAxis_All,
            .range = range,
            .highResolution = true,
            .blockDataUpdate = true,
            .interrupt = lis3dhInterruptPin,
            .interruptActiveLow = interruptActiveLow,
            .interruptLatched = interruptLatched,
            .highPassOnInterrupt = highPassOnInterrupt,
            .interruptConfig = interruptConfig,
            .interruptDuration = irqDuration,
            .interruptThreshold = irqThreshold,
            .wakeupDuration = wakeDuration,
            .wakeupThreshold = wakeThreshold,
        };

        lis3dh = new Lis3dh(&lis3dhConfig);
        printf("[WATCHER][ACC] LIS3DH ready at 0x%02X irq(thr=%u dur=%u cfg=0x%02X pol=%s latched=%s hpf=%s range=0x%02X) wake(thr=%u dur=%u)\n",
               lis3dhAddress,
               (unsigned int)irqThreshold,
               (unsigned int)irqDuration,
               (unsigned int)interruptConfig,
               interruptActiveLow ? "low" : "high",
               interruptLatched ? "on" : "off",
               highPassOnInterrupt ? "on" : "off",
               (unsigned int)range,
               (unsigned int)wakeThreshold,
               (unsigned int)wakeDuration);
    }
    else
        printf("[WATCHER][ACC][WARN] LIS3DH not found on 0x18/0x19\n");

    PA1010D *gps = nullptr;
    if (watcherI2c->Exists(PA1010D_I2C_DEFAULT_ADDRESS))
    {
        gps = new PA1010D({.i2c = watcherI2c,
                           .address = PA1010D_I2C_DEFAULT_ADDRESS,
                           .refreshRateSeconds = 1});
        printf("[WATCHER][GPS] PA1010D ready at 0x%02X\n", PA1010D_I2C_DEFAULT_ADDRESS);
    }
    else
    {
        printf("[WATCHER][GPS][WARN] PA1010D not found at 0x%02X\n", PA1010D_I2C_DEFAULT_ADDRESS);
    }

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
        printf("[WATCHER][RADIO] init attempt %u/%u: %s\n",
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
            printf("[WATCHER][RADIO] selected: %s\n", attempt.label);
            break;
        }

        delete candidate;
    }

    if (!radio)
    {
        printf("[WATCHER][WARN] no radio init attempt succeeded\n");
        return;
    }

    WatcherConfig config;
    config.rate = 30;                 // 60 Hz loop for tighter motion tracking.
    config.ledGreen = GPIO_NONE;
    config.ledBlue = blueLedPin;
    config.ledRed = GPIO_NONE;
    config.ledActiveLow = false;
    config.buzzer = (GpioNum)12;
    config.buzzerActiveLow = false;
    config.printTelemetry = true;
    config.telemetryIntervalMs = 0;   // Realtime telemetry (print each loop tick).
    config.detectionIrqThreshold = 1;
    config.detectionWindowMs = 500;
    config.detectionPulseMs = 500;
    config.alarmDetectionCount = 3;
    config.alarmDetectionWindowMs = 10000;
    config.alarmDurationMs = 60000;
    config.alarmBlinkIntervalMs = 125; // 4 Hz active blink
    config.armedBlinkIntervalMs = 1000; // 1 Hz ready blink
    config.armedBlinkOnMs = 500;
    config.noDetectionSleepMs = 10000;
    config.startInReceiveAlarm = (lis3dh == nullptr);
    config.sleep.enabled = !config.startInReceiveAlarm;
    config.sleep.wakePin = lis3dhInterruptPin;
    config.sleep.wakeOnLow = interruptActiveLow;
    config.sleep.mode = SleepMode::Light;
    config.radioPacketLength = TRANSCEIVER_PACKAGE_SIZE;
    config.accelerometer = lis3dh;
    config.gps = gps;
    config.transceiver = radio;

    if (config.startInReceiveAlarm)
        printf("[WATCHER] mode=ReceiveAlarm\n");
    else
        printf("[WATCHER] mode=AlarmSend\n");

    Watcher watcher(config);

    if (!watcher.Start())
    {
        printf("[WATCHER][ERROR] failed to start\n");
        return;
    }

    watcher.Run();
}
