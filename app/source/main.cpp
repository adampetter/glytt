#include <cstdio>
#include "api/motion/lis3dh.h"
#include "api/navigation/pa1010d.h"
#include "api/transmission/e22900t30s.h"
#include "api/common/time.h"

// Main entry point
extern "C" void app_main(void)
{
    delay(1000);

    I2c *i2c = new I2c({.port = I2cPort::I2cPort_0,
                        .sda = (GpioNum)8,
                        .scl = (GpioNum)9,
                        .mode = I2cMode::I2cMode_Master,
                        .frequency = I2cFrequency::I2cFrequency_04M,
                        .internalPullup = true});

    i2c->Scan();

    Gpio::Mode((GpioNum)14, GpioMode::GpioMode_Output);
    Gpio::Write((GpioNum)14, false);

    delay(1000);

    const PA1010DConfig gpsConfig = {
        .i2c = i2c,
        .address = PA1010D_I2C_DEFAULT_ADDRESS,
        .refreshRateSeconds = 15};

    PA1010D *gps = new PA1010D(gpsConfig);

    delay(500);

    const E22900T30SConfig transceiverConfig = {
        .serial = {
            .uart = new Uart({
                .port = UartPort::UartPort_1,
                .baudRate = UartBaudrate::UartBaudrate_9600,
                .tx = (GpioNum)6,
                .rx = (GpioNum)5,
            }),
            .baudRate = UartBaudrate::UartBaudrate_9600,
            .parity = E22900T30SSerialParity::Parity_8N1},
        .interupt = {.aux = (GpioNum)4, .expander = NULL},
        .mode = {.m0 = (GpioNum)15, .m1 = (GpioNum)7, .expander = NULL},
        .network = {.id = 0, .address = 0, .airDataRate = E22900T30SAirDataRate::Rate_2400, .subPacketSize = E22900T30SSubPacketSize::Size_240b, .power = E22900T30SPower::Power_30dBm, .ambientNoise = E22900T30SRSSIAmbientNoise::AmbientNoise_Disable}};

    E22900T30S *transceiver = new E22900T30S(transceiverConfig);

    delay(500);

    transceiver->Send((Byte *)"Hello, World!", 13);
    printf("Message sent: Hello, World!\n");

    delay(500);

    const Lis3dhConfig config = {
        .i2c = i2c,
        .address = 0x18,
        .mode = Lis3dhMode::Lis3dhMode_ByPass,
        .sampleRate = Lis3dhSampleRate::Lis3dhSampleRate_200,
        .axis = Lis3dhAxis::Lis3dhAxis_All,
        .interrupt = GPIO_NONE,
        .interruptDuration = 1,
        .interruptThreshold = Lis3dhThreshold::Lis3dhThreshold_3,
        .wakeupDuration = 0,
        .wakeupThreshold = Lis3dhThreshold::Lis3dhThreshold_1};

    Lis3dh *lis3dh = new Lis3dh(&config);

    constexpr short kAxisDeadband = 48;
    constexpr long long kMovementThresholdSquared = 1024;
    constexpr int kLoopDelayMs = 10;
    constexpr int kLedHoldMs = 120;

    Acceleration acceleration, previousAcceleration;
    int ledHoldRemainingMs = 0;
    int elapsedTime = 0;

    while (true)
    {
        lis3dh->Read(&acceleration);
        Acceleration delta = acceleration - previousAcceleration;
        previousAcceleration = acceleration;

        delta = Acceleration::Deadband(delta, kAxisDeadband);

        const long long movementSquared = delta.LengthSquared();

        if (movementSquared >= kMovementThresholdSquared)
        {
            ledHoldRemainingMs = kLedHoldMs;
        }

        if (ledHoldRemainingMs > 0)
        {
            Gpio::Write((GpioNum)14, true);
            ledHoldRemainingMs -= kLoopDelayMs;
        }
        else
        {
            Gpio::Write((GpioNum)14, false);
        }

        // printf("Delta: X=%d, Y=%d, Z=%d, MovementSq=%lld\n", delta.X, delta.Y, delta.Z, movementSquared);

        if (elapsedTime >= 3000)
        {
            elapsedTime = 0;

            Location location;
            gps->Read(&location);

            printf("Location: Fix=%d, Satellites=%d, Lat=%.6f, Lon=%.6f, Alt=%.2f, Speed=%.2f kph\n",
                   location.fix,
                   location.satellites,
                   location.coordinate.X,
                   location.coordinate.Y,
                   location.altitude,
                   location.speed);
        }

        delay(kLoopDelayMs);
        elapsedTime += kLoopDelayMs;
    }

    delay(3000);
    printf("Exiting...\n");
}