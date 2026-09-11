#include <cstdio>

#include "api/io/i2c.h"
#include "api/motion/lis3dh.h"
#include "sensor.h"

extern "C" void app_main(void)
{
    const GpioNum i2cSdaPin = (GpioNum)14;
    const GpioNum i2cSclPin = (GpioNum)13;
    const GpioNum lis3dhInterruptPin = (GpioNum)46;
    const GpioNum redLedPin = (GpioNum)9;

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

    SensorConfig config;
    config.rate = 30;                 // 60 Hz loop for tighter motion tracking.
    config.ledGreen = (GpioNum)11;    // Optional heartbeat LED.
    config.ledRed = redLedPin;
    config.ledActiveLow = false;
    config.buzzer = (GpioNum)12;
    config.buzzerActiveLow = false;
    config.printTelemetry = true;
    config.telemetryIntervalMs = 0;   // Realtime telemetry (print each loop tick).
    config.detectionIrqThreshold = 1;
    config.detectionWindowMs = 500;
    config.detectionPulseMs = 500;
    config.noDetectionSleepMs = 10000;
    config.sleep.enabled = true;
    config.sleep.wakePin = lis3dhInterruptPin;
    config.sleep.wakeOnLow = interruptActiveLow;
    config.sleep.mode = SleepMode::Light;
    config.accelerometer = lis3dh;

    Sensor sensor(config);

    if (!sensor.Start())
    {
        printf("[SENSOR][ERROR] failed to start\n");
        return;
    }

    sensor.Run();
}
