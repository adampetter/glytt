#include <cstdio>

#include "api/common/time.h"
#include "api/io/i2c.h"
#include "api/io/uart.h"
#include "api/motion/lis3dh.h"
#include "api/navigation/pa1010d.h"
#include "api/system/loop.h"
#include "api/transmission/e22900t30.h"
#include "sensor.h"

#include "esp_mac.h"

static unsigned short PseudonymousDeviceIdFromEfuse()
{
    Byte mac[6] = {0};
    if (esp_efuse_mac_get_default(mac) != ESP_OK)
        return 0xBEEF;

    unsigned short id = 0x5A5A;
    for (int i = 0; i < 6; i++)
        id = (unsigned short)((id << 5) ^ (id >> 1) ^ mac[i]);

    return id;
}

static void WriteLed(GpioNum pin, bool on, bool activeLow)
{
    if (pin == GPIO_NONE)
        return;

    Gpio::Write(pin, activeLow ? !on : on);
}

static void RunLedSelfTest(GpioNum greenLedPin, GpioNum redLedPin, bool ledActiveLow)
{
    Gpio::Mode(greenLedPin, GpioMode::GpioMode_Output);
    Gpio::Mode(redLedPin, GpioMode::GpioMode_Output);

    WriteLed(greenLedPin, false, ledActiveLow);
    WriteLed(redLedPin, false, ledActiveLow);

    printf("[SENSOR][LED] self-test start\n");

    WriteLed(greenLedPin, true, ledActiveLow);
    delay(500);
    WriteLed(greenLedPin, false, ledActiveLow);

    WriteLed(redLedPin, true, ledActiveLow);
    delay(500);
    WriteLed(redLedPin, false, ledActiveLow);

    printf("[SENSOR][LED] self-test done\n");
}

class SensorLoop : public Loop
{
private:
    Sensor &sensor;

public:
    SensorLoop(Sensor &sensor, Byte rate = 0)
        : Loop(rate), sensor(sensor)
    {
    }

    void Execute(const FrameTime &time) override
    {
        (void)time;
        this->sensor.Tick();
    }
};

extern "C" void app_main(void)
{
    const GpioNum greenLedPin = (GpioNum)11;
    const GpioNum yellowLedPin = GPIO_NONE;
    const GpioNum redLedPin = (GpioNum)9;
    const GpioNum buzzerPin = (GpioNum)12;
    const GpioNum lis3dhInterruptPin = (GpioNum)3;
    const bool ledActiveLow = false;

    // Assumed I2C pins from previous board setup. Adjust if your wiring differs.
    const GpioNum i2cSdaPin = (GpioNum)14;
    const GpioNum i2cSclPin = (GpioNum)13;

    // Radio pin assumptions from prior setup + your new AUX/M1 values.
    const GpioNum radioAuxPin = (GpioNum)1;
    const GpioNum radioM1Pin = (GpioNum)2;
    const GpioNum radioM0Pin = (GpioNum)15;
    const GpioNum radioTxPin = (GpioNum)6;
    const GpioNum radioRxPin = (GpioNum)5;

    printf("[SENSOR][HW] LEDs g=%d y=%d r=%d buzzer=%d lisInt=%d\n",
           greenLedPin,
           yellowLedPin,
           redLedPin,
           buzzerPin,
           lis3dhInterruptPin);
    printf("[SENSOR][HW] I2C sda=%d scl=%d (100kHz for long cable)\n", i2cSdaPin, i2cSclPin);
    printf("[SENSOR][HW] RADIO aux=%d m1=%d m0=%d tx=%d rx=%d\n",
           radioAuxPin,
           radioM1Pin,
           radioM0Pin,
           radioTxPin,
           radioRxPin);

    RunLedSelfTest(greenLedPin, redLedPin, ledActiveLow);

    I2c *sensorI2c = new I2c({.port = I2cPort::I2cPort_0,
                              .sda = i2cSdaPin,
                              .scl = i2cSclPin,
                              .mode = I2cMode::I2cMode_Master,
                              .frequency = I2cFrequency::I2cFrequency_01M,
                              .internalPullup = true});

    Byte lis3dhAddress = 0x18;
    if (!sensorI2c->Exists(lis3dhAddress) && sensorI2c->Exists(0x19))
        lis3dhAddress = 0x19;

    if (!sensorI2c->Exists(lis3dhAddress))
    {
        printf("[SENSOR][ERROR] LIS3DH not detected on I2C (0x18/0x19)\n");
        while (true)
            delay(1000);
    }

    Lis3dhConfig lis3dhConfig = {
        .i2c = sensorI2c,
        .address = lis3dhAddress,
        .mode = Lis3dhMode::Lis3dhMode_ByPass,
        .sampleRate = Lis3dhSampleRate::Lis3dhSampleRate_200,
        .axis = Lis3dhAxis::Lis3dhAxis_All,
        .interrupt = lis3dhInterruptPin,
        .interruptDuration = 1,
        .interruptThreshold = Lis3dhThreshold::Lis3dhThreshold_1,
        .wakeupDuration = 1,
        .wakeupThreshold = Lis3dhThreshold::Lis3dhThreshold_1,
    };

    Lis3dh *lis3dh = new Lis3dh(&lis3dhConfig);

    PA1010D *gps = nullptr;
    if (sensorI2c->Exists(PA1010D_I2C_DEFAULT_ADDRESS))
    {
        gps = new PA1010D({.i2c = sensorI2c,
                           .address = PA1010D_I2C_DEFAULT_ADDRESS,
                           .refreshRateSeconds = 1});
        printf("[SENSOR][GPS] PA1010D detected\n");
    }
    else
        printf("[SENSOR][GPS] not detected, continuing without GPS\n");

    auto makeRadio = [&](UartPort uartPort, GpioNum txPin, GpioNum rxPin, GpioNum auxPin, GpioNum m0Pin, GpioNum m1Pin, UartBaudrate baudRate) {
        const bool canConfigureModule = m0Pin != GPIO_NONE && m1Pin != GPIO_NONE;

        return new E22900T30({
            .startupInNormalModeOnly = !canConfigureModule,
            .serial = {
                .uart = new Uart({
                    .port = uartPort,
                    .baudRate = baudRate,
                    .tx = txPin,
                    .rx = rxPin,
                }),
                .baudRate = baudRate,
                .parity = E22900T30SerialParity::Parity_8N1},
            .interupt = {.aux = auxPin, .expander = NULL},
            .mode = {.m0 = m0Pin, .m1 = m1Pin, .expander = NULL},
            .network = {
                .id = 0,
                .address = 0,
                .airDataRate = E22900T30AirDataRate::Rate_300,
                .subPacketSize = E22900T30SubPacketSize::Size_32b,
                .power = E22900T30Power::Power_30dBm,
                .channel = 18,
                .rssi = E22900T30RSSI::RSSI_Disabled,
                .ambientNoise = E22900T30RSSIAmbientNoise::AmbientNoise_Disable,
                .txmode = E22900T30TransmissionMode::TransmissionMode_Transparent,
                .reply = E22900T30Reply::Reply_Disabled,
                .lbt = E22900T30LBT::LBT_Disabled},
            .security = {.encryptByteH = 0, .encryptByteL = 0},
            .wor = {.mode = E22900T30WakeOnRadio::WOR_Receiver,
                    .interval = E22900T30WakeOnRadioInterval::WOR_Interval_2000ms}});
    };

    struct RadioAttempt
    {
        UartPort port;
        GpioNum tx;
        GpioNum rx;
        GpioNum aux;
        GpioNum m0;
        GpioNum m1;
        UartBaudrate baud;
        const char *label;
    };

    const RadioAttempt attempts[] = {
        // Fastest path to a working link on this hardware.
        {UartPort::UartPort_1, radioTxPin, radioRxPin, GPIO_NONE, GPIO_NONE, GPIO_NONE, UartBaudrate::UartBaudrate_9600, "uart1 tx6/rx5 + no-aux + no-mode-pins + 9600"},
        {UartPort::UartPort_1, radioRxPin, radioTxPin, GPIO_NONE, GPIO_NONE, GPIO_NONE, UartBaudrate::UartBaudrate_9600, "uart1 tx5/rx6 + no-aux + no-mode-pins + 9600"},
        {UartPort::UartPort_1, (GpioNum)43, (GpioNum)44, GPIO_NONE, GPIO_NONE, GPIO_NONE, UartBaudrate::UartBaudrate_9600, "uart1 tx43/rx44 + no-aux + no-mode-pins + 9600"},
        {UartPort::UartPort_1, (GpioNum)44, (GpioNum)43, GPIO_NONE, GPIO_NONE, GPIO_NONE, UartBaudrate::UartBaudrate_9600, "uart1 tx44/rx43 + no-aux + no-mode-pins + 9600"},

        {UartPort::UartPort_1, radioTxPin, radioRxPin, radioAuxPin, radioM0Pin, radioM1Pin, UartBaudrate::UartBaudrate_9600, "uart1 tx6/rx5 + aux + m0/m1 + 9600"},
        {UartPort::UartPort_1, radioRxPin, radioTxPin, radioAuxPin, radioM0Pin, radioM1Pin, UartBaudrate::UartBaudrate_9600, "uart1 tx5/rx6 + aux + m0/m1 + 9600"},
        {UartPort::UartPort_1, (GpioNum)43, (GpioNum)44, radioAuxPin, radioM0Pin, radioM1Pin, UartBaudrate::UartBaudrate_9600, "uart1 tx43/rx44 + aux + m0/m1 + 9600"},
        {UartPort::UartPort_1, (GpioNum)44, (GpioNum)43, radioAuxPin, radioM0Pin, radioM1Pin, UartBaudrate::UartBaudrate_9600, "uart1 tx44/rx43 + aux + m0/m1 + 9600"},
        {UartPort::UartPort_2, (GpioNum)43, (GpioNum)44, radioAuxPin, radioM0Pin, radioM1Pin, UartBaudrate::UartBaudrate_9600, "uart2 tx43/rx44 + aux + m0/m1 + 9600"},
        {UartPort::UartPort_2, (GpioNum)44, (GpioNum)43, radioAuxPin, radioM0Pin, radioM1Pin, UartBaudrate::UartBaudrate_9600, "uart2 tx44/rx43 + aux + m0/m1 + 9600"},
        {UartPort::UartPort_1, radioTxPin, radioRxPin, GPIO_NONE, radioM0Pin, radioM1Pin, UartBaudrate::UartBaudrate_9600, "uart1 tx6/rx5 + no-aux + m0/m1 + 9600"},
        {UartPort::UartPort_1, radioRxPin, radioTxPin, GPIO_NONE, radioM0Pin, radioM1Pin, UartBaudrate::UartBaudrate_9600, "uart1 tx5/rx6 + no-aux + m0/m1 + 9600"},
        {UartPort::UartPort_1, (GpioNum)43, (GpioNum)44, GPIO_NONE, radioM0Pin, radioM1Pin, UartBaudrate::UartBaudrate_9600, "uart1 tx43/rx44 + no-aux + m0/m1 + 9600"},
        {UartPort::UartPort_1, (GpioNum)44, (GpioNum)43, GPIO_NONE, radioM0Pin, radioM1Pin, UartBaudrate::UartBaudrate_9600, "uart1 tx44/rx43 + no-aux + m0/m1 + 9600"},
        {UartPort::UartPort_1, radioTxPin, radioRxPin, GPIO_NONE, radioM1Pin, radioM0Pin, UartBaudrate::UartBaudrate_9600, "uart1 tx6/rx5 + no-aux + m1/m0 + 9600"},
        {UartPort::UartPort_1, radioRxPin, radioTxPin, GPIO_NONE, radioM1Pin, radioM0Pin, UartBaudrate::UartBaudrate_9600, "uart1 tx5/rx6 + no-aux + m1/m0 + 9600"},
        {UartPort::UartPort_1, (GpioNum)43, (GpioNum)44, GPIO_NONE, radioM1Pin, radioM0Pin, UartBaudrate::UartBaudrate_9600, "uart1 tx43/rx44 + no-aux + m1/m0 + 9600"},
        {UartPort::UartPort_1, (GpioNum)44, (GpioNum)43, GPIO_NONE, radioM1Pin, radioM0Pin, UartBaudrate::UartBaudrate_9600, "uart1 tx44/rx43 + no-aux + m1/m0 + 9600"},
        {UartPort::UartPort_1, radioTxPin, radioRxPin, GPIO_NONE, radioM0Pin, radioM1Pin, UartBaudrate::UartBaudrate_115200, "uart1 tx6/rx5 + no-aux + m0/m1 + 115200"},
        {UartPort::UartPort_1, radioRxPin, radioTxPin, GPIO_NONE, radioM0Pin, radioM1Pin, UartBaudrate::UartBaudrate_115200, "uart1 tx5/rx6 + no-aux + m0/m1 + 115200"},
    };

    E22900T30 *radio = nullptr;
    for (unsigned int i = 0; i < (sizeof(attempts) / sizeof(attempts[0])); i++)
    {
        const RadioAttempt &attempt = attempts[i];
          printf("[SENSOR][RADIO] init attempt %u/%u: %s (port=%d tx=%d rx=%d aux=%d m0=%d m1=%d)\n",
               (unsigned int)(i + 1),
               (unsigned int)(sizeof(attempts) / sizeof(attempts[0])),
               attempt.label,
               (int)attempt.port,
               attempt.tx,
               attempt.rx,
               attempt.aux,
               attempt.m0,
               attempt.m1);

           E22900T30 *candidate = makeRadio(attempt.port, attempt.tx, attempt.rx, attempt.aux, attempt.m0, attempt.m1, attempt.baud);
        if (candidate->Ready())
        {
            radio = candidate;
            printf("[SENSOR][RADIO] ready with %s\n", attempt.label);

            if (attempt.m0 == GPIO_NONE || attempt.m1 == GPIO_NONE)
                printf("[SENSOR][RADIO][WARN] running normal-mode-only fallback; TX power may not be forced to 30 dBm\n");

            break;
        }

        delete candidate;
    }

    if (radio == nullptr)
    {
        printf("[SENSOR][RADIO][WARN] module not ready in any UART/AUX attempt; check M0/M1/UART wiring and power\n");
        while (true)
            delay(1000);
    }

    SensorConfig config;
    config.pins.ledGreen = greenLedPin;
    config.pins.ledYellow = yellowLedPin;
    config.pins.ledRed = redLedPin;
    config.pins.buzzer = buzzerPin;
    config.pins.motionInterrupt = lis3dhInterruptPin;

    config.sensors.accelerometer = lis3dh;
    config.sensors.gps = gps;

    config.radio.transceiver = radio;
    config.radio.packetLength = 64;
    config.radio.heartbeatIntervalMs = 300000;
    config.radio.airDataRate = E22900T30AirDataRate::Rate_300;

    config.motion.deadband = 28;
    config.motion.minDeltaSquared = 280;
    config.motion.requiredEvents = 1;
    config.motion.eventWindowMs = 3000;
    config.motion.startupGraceMs = 5000;

    config.alarm.intervalMs = 5000;
    config.alarm.timeoutMs = 60000;
    config.alarm.cooldownMs = 12000;
    config.alarm.warningWindowMs = 10000;
    config.alarm.minSecondDetectionDelayMs = 1000;

    config.power.deepSleepPreferred = false;
    config.power.monitorSleepMs = 10;

    config.indicators.buzzerEnabled = true;
    config.indicators.ledActiveLow = ledActiveLow;
    config.indicators.greenBlinkIntervalMs = 1500;
    config.indicators.greenBlinkOnMs = 700;
    config.indicators.redAlarmBlinkIntervalMs = 500;
    config.privacy.pseudonymousDeviceId = PseudonymousDeviceIdFromEfuse();

    Sensor sensor(config);

    if (!sensor.Start())
    {
        printf("[SENSOR][ERROR] failed to start\n");
        while (true)
            delay(1000);
    }

    SensorLoop sensorLoop(sensor, 0);
    sensorLoop.Run();
}
