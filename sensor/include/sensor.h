#pragma once

#include "api/common/time.h"
#include "api/common/types.h"
#include "api/io/buzzer.h"
#include "api/io/led.h"
#include "api/motion/lis3dh.h"
#include "api/system/loop.h"
#include "api/system/sleep.h"

enum class SensorState
{
    Boot = 0,
    Running = 1,
    Fault = 2,
    Stopped = 3
};

struct SensorConfig
{
    Byte rate = 30;
    GpioNum ledGreen = GPIO_NONE;
    GpioNum ledRed = GPIO_NONE;
    bool ledActiveLow = false;
    GpioNum buzzer = GPIO_NONE;
    bool buzzerActiveLow = false;
    bool printTelemetry = true;
    unsigned int telemetryIntervalMs = 0; // 0 = print every loop tick.
    SleepConfig sleep = {.enabled = true,
                         .wakePin = (GpioNum)18,
                         .wakeOnLow = true,
                         .mode = SleepMode::Light};
    unsigned int noDetectionSleepMs = 10000;
    unsigned int detectionWindowMs = 2000;
    unsigned int detectionIrqThreshold = 3;
    unsigned int detectionPulseMs = 500;
    Lis3dh *accelerometer = nullptr;
};

class Sensor : public Loop
{
private:
    SensorConfig config;
    SensorState state = SensorState::Boot;
    unsigned long startedAtMs = 0;
    unsigned long lastTelemetryAtMs = 0;
    unsigned long lastDetectionAtMs = 0;
    Buzzer buzzer;
    Led greenLed;
    Led redLed;
    bool warnedMissingAccelerometer = false;
    Acceleration lastAcceleration = Acceleration::Empty;
    unsigned long sampleCount = 0;
    bool lastInterrupt = false;
    unsigned long interruptCount = 0;
    static const unsigned int MaxIrqEventsInWindow = 64;
    unsigned long irqEventTimesMs[MaxIrqEventsInWindow] = {0};
    unsigned int irqEventHead = 0;
    unsigned int irqEventCount = 0;
    unsigned int detectionWindowIrqDelta = 0;
    bool detectionWindowLatched = false;
    unsigned long redPulseUntilAtMs = 0;
    unsigned long greenWakePulseUntilAtMs = 0;
    Sleep sleep;
    bool pendingWakeLog = false;
    bool pendingWakeFromConfiguredSource = false;
    esp_sleep_wakeup_cause_t pendingWakeCause = ESP_SLEEP_WAKEUP_UNDEFINED;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wchanges-meaning"
#endif
    void Sleep();
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

public:
    Sensor(const SensorConfig &config);

    bool Start();
    void Execute(const FrameTime &time) override;
    void Stop();

    SensorState State() const;
};
