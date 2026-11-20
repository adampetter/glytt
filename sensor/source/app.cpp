#include <cstdio>

#include "api/common/time.h"
#include "api/io/gpio.h"
#include "api/io/i2c.h"
#include "api/io/uart.h"
#include "api/motion/lis3dh.h"
#include "api/navigation/pa1010d.h"
#include "api/transmission/e22900t30.h"
#include "sensor.h"
#include "config.h"

namespace
{
    constexpr UartPort RadioUartPort = SENSOR_RADIO_UART_PORT;
    constexpr GpioNum RadioTxPin = (GpioNum)SENSOR_RADIO_TX_PIN;
    constexpr GpioNum RadioRxPin = (GpioNum)SENSOR_RADIO_RX_PIN;
    constexpr GpioNum RadioAuxPin = (GpioNum)SENSOR_RADIO_AUX_PIN;
    constexpr GpioNum RadioModeM0Pin = (GpioNum)SENSOR_RADIO_M0_PIN;
    constexpr GpioNum RadioModeM1Pin = (GpioNum)SENSOR_RADIO_M1_PIN;
    constexpr UartBaudrate RadioBaudrate = SENSOR_RADIO_BAUDRATE;
    constexpr bool RadioStartupNormalOnly = SENSOR_RADIO_STARTUP_NORMAL_ONLY == 1;
}

extern "C" void app_main(void)
{
#if SENSOR_MODE != SENSOR_MODE_ALARM_SEND && SENSOR_MODE != SENSOR_MODE_RECEIVE_ALARM && SENSOR_MODE != SENSOR_MODE_DISABLED
#error "Invalid SENSOR_MODE value"
#endif

    const GpioNum i2cSdaPin = (GpioNum)14;
    const GpioNum i2cSclPin = (GpioNum)13;
    const GpioNum lis3dhInterruptPin = (GpioNum)46;
    const GpioNum greenLedPin = GPIO_NONE;
#if SENSOR_LED
    const GpioNum redLedPin = (GpioNum)9;
    const GpioNum statusLedPin = (GpioNum)10;
#else
    const GpioNum redLedPin = GPIO_NONE;
    const GpioNum statusLedPin = GPIO_NONE;
#endif
    const GpioNum radioAuxPin = RadioAuxPin;
    const GpioNum radioModeM0Pin = RadioModeM0Pin;
    const GpioNum radioModeM1Pin = RadioModeM1Pin;

    if (SENSOR_MODE == SENSOR_MODE_DISABLED)
    {
        printf("[SENSOR][MODE] Disabled (default safe mode). Use tools/set-sensor-role.ps1 to set sender/receiver.\n");
        return;
    }

    if (!RadioStartupNormalOnly && (!Gpio::IsValid(radioModeM0Pin) || !Gpio::IsValid(radioModeM1Pin)))
    {
        printf("[SENSOR][RADIO][ERROR] Invalid mode pin mapping for this board. Pick two valid output GPIOs for M0/M1.\n");
        return;
    }

    if (RadioStartupNormalOnly && SENSOR_REQUIRE_RADIO_CONFIG_READY == 1)
    {
        printf("[SENSOR][RADIO][ERROR] SENSOR_RADIO_STARTUP_NORMAL_ONLY=1 conflicts with SENSOR_REQUIRE_RADIO_CONFIG_READY=1\n");
        return;
    }

    if (!RadioStartupNormalOnly)
    {
        // Enforce deterministic startup mode for E22: normal mode (M0=0, M1=0).
        Gpio::Mode(radioModeM0Pin, GpioMode::GpioMode_Input_Output);
        Gpio::Mode(radioModeM1Pin, GpioMode::GpioMode_Input_Output);
        Gpio::Write(radioModeM0Pin, false);
        Gpio::Write(radioModeM1Pin, false);
        delay(20);
    }

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

    I2c *sensorI2c = new I2c({.port = I2cPort::I2cPort_0,
                               .sda = i2cSdaPin,
                               .scl = i2cSclPin,
                               .mode = I2cMode::I2cMode_Master,
                               .frequency = I2cFrequency::I2cFrequency_01M,
                               .internalPullup = true});

    Byte lis3dhAddress = 0x18;
    if (!sensorI2c->Exists(lis3dhAddress) && sensorI2c->Exists(0x19))
        lis3dhAddress = 0x19;

    Lis3dh *lis3dh = nullptr;
    if (sensorI2c->Exists(lis3dhAddress))
    {
        Lis3dhConfig lis3dhConfig = {
            .i2c = sensorI2c,
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
        printf("[SENSOR][ACC] LIS3DH ready at 0x%02X irq(thr=%u dur=%u cfg=0x%02X pol=%s latched=%s hpf=%s range=0x%02X) wake(thr=%u dur=%u)\n",
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
        printf("[SENSOR][ACC][WARN] LIS3DH not found on 0x18/0x19\n");

    PA1010D *gps = nullptr;
    if (sensorI2c->Exists(PA1010D_I2C_DEFAULT_ADDRESS))
    {
        gps = new PA1010D({.i2c = sensorI2c,
                           .address = PA1010D_I2C_DEFAULT_ADDRESS,
                           .refreshRateSeconds = 1});
        printf("[SENSOR][GPS] PA1010D ready at 0x%02X\n", PA1010D_I2C_DEFAULT_ADDRESS);
    }
    else
    {
        printf("[SENSOR][GPS][WARN] PA1010D not found at 0x%02X\n", PA1010D_I2C_DEFAULT_ADDRESS);
    }

    E22900T30 *radio = nullptr;
    const bool startInReceiveAlarm = SENSOR_MODE == SENSOR_MODE_RECEIVE_ALARM;
    const bool requireRadioConfigReady = SENSOR_REQUIRE_RADIO_CONFIG_READY == 1;
    const E22900T30WakeOnRadio worMode = startInReceiveAlarm
                                             ? E22900T30WakeOnRadio::WOR_Receiver
                                             : E22900T30WakeOnRadio::WOR_Transmitter;
    const E22900T30Config transceiverConfig = {
        .startupInNormalModeOnly = RadioStartupNormalOnly,
        .serial = {
            .uart = new Uart({
                .port = RadioUartPort,
                .baudRate = RadioBaudrate,
                .tx = RadioTxPin,
                .rx = RadioRxPin,
            }),
            .baudRate = RadioBaudrate,
            .parity = E22900T30SerialParity::Parity_8N1},
        .interupt = {.aux = radioAuxPin, .expander = NULL},
        .mode = {.m0 = radioModeM0Pin, .m1 = radioModeM1Pin, .expander = NULL},
        .network = {
            .id = 0,
            .address = 0,
            .airDataRate = E22900T30AirDataRate::Rate_9600,
            .subPacketSize = E22900T30SubPacketSize::Size_240b,
            .power = E22900T30Power::Power_21dBm,
            .channel = 18,
            .rssi = E22900T30RSSI::RSSI_Disabled,
            .ambientNoise = E22900T30RSSIAmbientNoise::AmbientNoise_Disable,
            .txmode = E22900T30TransmissionMode::TransmissionMode_Transparent,
            .reply = E22900T30Reply::Reply_Disabled,
            .lbt = E22900T30LBT::LBT_Disabled},
        .security = {.encryptByteH = 0, .encryptByteL = 0},
            .wor = {.mode = worMode,
                .interval = SENSOR_RADIO_WOR_INTERVAL}};

            printf("[SENSOR][RADIO] init: uart=%u tx=%d rx=%d aux=%d m0=%d m1=%d baud=%u normalOnly=%u worMode=%u worInterval=%u\n",
           (unsigned int)RadioUartPort,
           (int)RadioTxPin,
           (int)RadioRxPin,
           (int)radioAuxPin,
           (int)radioModeM0Pin,
           (int)radioModeM1Pin,
           (unsigned int)RadioBaudrate,
               RadioStartupNormalOnly ? 1U : 0U,
               (unsigned int)worMode,
               (unsigned int)SENSOR_RADIO_WOR_INTERVAL);

    E22900T30 *candidate = new E22900T30(transceiverConfig);
    const bool acceptsNormalOnly = !requireRadioConfigReady && RadioStartupNormalOnly;
    if (candidate->Ready() && (!RadioStartupNormalOnly || acceptsNormalOnly))
    {
        radio = candidate;
        printf("[SENSOR][RADIO] selected primary profile\n");
    }
    else
    {
        printf("[SENSOR][RADIO] init failed for primary profile\n");
        delete candidate;
    }

    if (!radio)
    {
        printf("[SENSOR][WARN] no radio init attempt succeeded\n");
        return;
    }

    SensorConfig config;
    config.rate = 30;                 // 60 Hz loop for tighter motion tracking.
    config.ledGreen = greenLedPin;
    config.ledStatus = statusLedPin;
    config.ledRed = redLedPin;
    config.ledActiveLow = false;
    config.buzzer = (GpioNum)12;
    config.buzzerActiveLow = false;
    config.printTelemetry = startInReceiveAlarm;
    config.telemetryIntervalMs = 0;   // Realtime telemetry (print each loop tick).
    config.detectionIrqThreshold = 1;
    config.detectionWindowMs = 500;
    config.detectionPulseMs = 500;
    config.alarmDetectionCount = 3;
    config.alarmDetectionWindowMs = 10000;
    config.alarmDurationMs = 60000;
    config.alarmBlinkIntervalMs = 125; // 4 Hz active blink
    config.armedBlinkIntervalMs = startInReceiveAlarm ? 1000 : SENSOR_ALARMSEND_STATUS_LED_BLINK_INTERVAL_MS;
    config.armedBlinkOnMs = startInReceiveAlarm ? 500 : SENSOR_ALARMSEND_STATUS_LED_BLINK_ON_MS;
    config.noDetectionSleepMs = 10000;
    config.statusRadioIntervalMs = SENSOR_STATUS_HEARTBEAT_MS;
    config.startInReceiveAlarm = startInReceiveAlarm;
    config.sleep.enabled = !config.startInReceiveAlarm && (SENSOR_DEBUG == 0);
    config.sleep.wakePin = lis3dhInterruptPin;
    config.sleep.wakeOnLow = interruptActiveLow;
    config.sleep.mode = SleepMode::Light;
    config.radioPacketLength = TRANSCEIVER_PACKAGE_SIZE;
    config.accelerometer = lis3dh;
    config.gps = gps;
    config.transceiver = radio;

    printf("[SENSOR][CFG] mode=%s statusIntervalMs=%u sleepEnabled=%u radioPacketLength=%u receiveTimeoutMs=%u\n",
           config.startInReceiveAlarm ? "ReceiveAlarm" : "AlarmSend",
           config.statusRadioIntervalMs,
           config.sleep.enabled ? 1U : 0U,
           (unsigned int)config.radioPacketLength,
           config.receiveTimeoutMs);

    if (config.startInReceiveAlarm)
        printf("[SENSOR] mode=ReceiveAlarm (forced)\n");
    else
        printf("[SENSOR] mode=AlarmSend (forced)\n");

    Sensor sensor(config);

    if (!sensor.Start())
    {
        printf("[SENSOR][ERROR] failed to start\n");
        return;
    }

    sensor.Run();
}
