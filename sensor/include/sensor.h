#pragma once

#include "api/common/types.h"
#include "api/motion/acceleration.h"
#include "api/motion/lis3dh.h"
#include "api/navigation/location.h"
#include "api/navigation/pa1010d.h"
#include "api/io/buzzer.h"
#include "api/io/led.h"
#include "api/transmission/e22900t30.h"
#include "api/transmission/package.h"
#include "api/transmission/transceiver.h"

enum class SensorState
{
    Boot = 0,
    ProbePeripherals = 1,
    ArmAndSleep = 2,
    WakeValidateMotion = 3,
    WarningActive = 4,
    AlarmActive = 5,
    AlarmCooldown = 6,
    Fault = 7
};

struct SensorConfig
{
    struct Pins
    {
        GpioNum ledGreen = GPIO_NONE;
        GpioNum ledYellow = GPIO_NONE;
        GpioNum ledRed = GPIO_NONE;
        GpioNum buzzer = GPIO_NONE;
        GpioNum motionInterrupt = GPIO_NONE;
    } pins;

    struct Radio
    {
        Transceiver *transceiver = nullptr;
        unsigned short packetLength = 64;
        unsigned int receiveTimeoutMs = 3;
        unsigned int heartbeatIntervalMs = 15000;
        unsigned int statusReceiveWindowMs = 50;
        E22900T30AirDataRate airDataRate = E22900T30AirDataRate::Rate_300;
    } radio;

    struct Sensors
    {
        Lis3dh *accelerometer = nullptr;
        PA1010D *gps = nullptr;
    } sensors;

    struct Motion
    {
        unsigned short deadband = 48;
        unsigned int minDeltaSquared = 1024;
        unsigned short requiredEvents = 3;
        unsigned int eventWindowMs = 2500;
        unsigned int startupGraceMs = 4000;
    } motion;

    struct Alarm
    {
        unsigned int warningWindowMs = 10000;
        unsigned int minSecondDetectionDelayMs = 1000;
        unsigned int warningIntervalMs = 5000;
        unsigned int intervalMs = 1500;
        unsigned int timeoutMs = 60000;
        unsigned int cooldownMs = 12000;
    } alarm;

    struct Power
    {
        bool deepSleepPreferred = true;
        unsigned int monitorSleepMs = 250;
    } power;

    struct Indicators
    {
        bool buzzerEnabled = true;
        bool ledActiveLow = true;
        unsigned int greenBlinkIntervalMs = 1500;
        unsigned int greenBlinkOnMs = 700;
        unsigned int redAlarmBlinkIntervalMs = 400;
    } indicators;

    struct Privacy
    {
        unsigned short pseudonymousDeviceId = 0;
    } privacy;
};

struct SensorStats
{
    unsigned int statusTx = 0;
    unsigned int statusTxErrors = 0;
    unsigned int warningTx = 0;
    unsigned int warningTxErrors = 0;
    unsigned int alarmTx = 0;
    unsigned int alarmTxErrors = 0;
    unsigned int ackRx = 0;
    unsigned int statusRx = 0;
    unsigned int rxInvalid = 0;
    unsigned int rxTimeouts = 0;
    unsigned int wakeEvents = 0;
    unsigned int wakeRejected = 0;
    unsigned int wakeConfirmed = 0;
    unsigned int gpsReads = 0;
    unsigned int gpsFixes = 0;
};

class Sensor
{
private:
    SensorConfig config;
    SensorState state = SensorState::Boot;
    SensorStats stats;
    bool gpsAvailable = false;
    unsigned short sequence = 0;
    unsigned short motionEventsInWindow = 0;
    unsigned long lastStatusAtMs = 0;
    unsigned long lastAlarmAtMs = 0;
    unsigned long alarmStartedAtMs = 0;
    unsigned long warningStartedAtMs = 0;
    unsigned long warningBlinkUntilAtMs = 0;
    unsigned long cooldownStartedAtMs = 0;
    unsigned long firstMotionEventAtMs = 0;
    unsigned long lastAmplitudeOnlyHitAtMs = 0;
    unsigned short amplitudeOnlyHits = 0;
    unsigned int lastMotionDeltaSquared = 0;
    unsigned short warningConfidenceScore = 0;
    unsigned short warningConfirmedEvents = 0;
    unsigned short warningInterruptEvents = 0;
    unsigned short warningStrongIrqEvents = 0;
    unsigned int warningPeakDeltaSquared = 0;
    unsigned short preWarningIrqHits = 0;
    unsigned long preWarningFirstAtMs = 0;
    unsigned long lastTelemetryAtMs = 0;
    bool warningHasInterruptEvidence = false;
    bool lastMotionEventHadInterrupt = false;
    unsigned long startedAtMs = 0;
    Led greenLed;
    Led redLed;
    Buzzer buzzer;
    Acceleration previousAcceleration;
    Location lastLocation;
    Byte txBuffer[TRANSCEIVER_PACKAGE_SIZE] = {0};
    Byte rxBuffer[TRANSCEIVER_PACKAGE_SIZE] = {0};
    Byte payloadBuffer[TRANSCEIVER_PACKAGE_SIZE] = {0};

    void updateOutputs(unsigned long nowMs);
    void applySleepInMonitor();
    bool detectMotionEvent();
    void pollIncomingFrames();
    bool sendWarningFrame(unsigned long nowMs);
    bool sendAlarmFrame(unsigned long nowMs);
    bool sendStatusFrame(unsigned long nowMs);
    bool transmitFrame(PackageType type, const char *payloadText);
    static const char *stateName(SensorState state);

public:
    Sensor(const SensorConfig &config);

    bool Start();
    void Tick();
    void Stop();

    SensorState State() const;
    SensorStats Stats() const;
    bool BuzzerEnabled() const;
    void SetBuzzerEnabled(bool enabled);
    void ToggleBuzzer();
};
