#include "sensor.h"
#include "config.h"
#include "api/common/time.h"
#include "api/transmission/package.h"

#include <cstdio>
#include <cstring>

#include "driver/gpio.h"
#include "esp_rom_sys.h"

static const unsigned int GreenMonitorBlinkIntervalMs = 30000;
static const unsigned int GreenMonitorBlinkOnMs = 100;
static const unsigned short AlarmPacketLength = 240;
static const unsigned short AlarmPayloadMax = AlarmPacketLength - Package::HeaderSize;
static Byte AlarmPacketBuffer[AlarmPacketLength] = {0};
static char AlarmPayloadBuffer[AlarmPayloadMax + 1] = {0};

Sensor::Sensor(const SensorConfig &config)
    : Loop(config.rate), config(config), sleep(config.sleep)
{
}

bool Sensor::Start()
{
    this->state = SensorState::Running;
    this->mode = SensorMode::Monitor;
    this->startedAtMs = millis();
    this->lastTelemetryAtMs = this->startedAtMs;
    this->lastDetectionAtMs = this->startedAtMs;
    this->alarmStartedAtMs = 0;
    this->alarmLastToggleAtMs = 0;
    this->alarmOutputOn = false;
    this->alarmPackageSequence = 0;
    this->warnedMissingAccelerometer = false;
    this->lastAcceleration = Acceleration::Empty;
    this->sampleCount = 0;
    this->lastInterrupt = false;
    this->interruptCount = 0;
    this->irqEventHead = 0;
    this->irqEventCount = 0;
    this->detectionWindowIrqDelta = 0;
    this->detectionEventHead = 0;
    this->detectionEventCount = 0;
    this->detectionWindowLatched = false;
    this->redPulseUntilAtMs = 0;
    this->greenBlinkStartedAtMs = this->startedAtMs;
    this->greenBlinkUntilAtMs = 0;

    if (this->config.ledGreen != GPIO_NONE)
    {
        this->greenLed.Begin(this->config.ledGreen, this->config.ledActiveLow);
        this->greenLed.SetOff();
    }

    if (this->config.ledRed != GPIO_NONE)
    {
        this->redLed.Begin(this->config.ledRed, this->config.ledActiveLow);
        this->redLed.SetOff();
    }

#if BUZZER
    if (this->config.buzzer != GPIO_NONE)
    {
        this->buzzer.Begin(this->config.buzzer, this->config.buzzerActiveLow);
        this->buzzer.SetEnabled(true);
        this->buzzer.SetOff();
        this->buzzer.Update(this->startedAtMs);
    }
#endif

    if (this->config.noDetectionSleepMs == 0)
        this->config.noDetectionSleepMs = 10000;

    if (this->config.detectionWindowMs == 0)
        this->config.detectionWindowMs = 2000;

    if (this->config.detectionIrqThreshold == 0)
        this->config.detectionIrqThreshold = 3;

    if (this->config.detectionPulseMs == 0)
        this->config.detectionPulseMs = 500;

    if (this->config.alarmDetectionCount == 0)
        this->config.alarmDetectionCount = 2;

    if (this->config.alarmDetectionWindowMs == 0)
        this->config.alarmDetectionWindowMs = 5000;

    if (this->config.alarmDurationMs == 0)
        this->config.alarmDurationMs = 60000;

    if (this->config.alarmBlinkIntervalMs == 0)
        this->config.alarmBlinkIntervalMs = 500;

    if (!this->sleep.Ready())
    {
        this->state = SensorState::Fault;
        printf("[SENSOR][ERROR] invalid sleep wake pin: %d\n", this->config.sleep.wakePin);
        return false;
    }

    printf("[SENSOR] started target=%ums\n", this->Target());

    if (this->config.sleep.enabled)
    {
         printf("[SENSOR][SLEEP] wakePin=%d wakeLevel=%s noDetectionSleepMs=%ums wakeCause=%d\n",
               this->config.sleep.wakePin,
               this->config.sleep.wakeOnLow ? "low" : "high",
             this->config.noDetectionSleepMs,
                             (int)this->sleep.WakeupCause());
    }

        printf("[SENSOR][DETECT] threshold=%u windowMs=%u pulseMs=%u\n",
            this->config.detectionIrqThreshold,
            this->config.detectionWindowMs,
            this->config.detectionPulseMs);

    return true;
}

void Sensor::Execute(const FrameTime &time)
{
    (void)time;

    if (this->state != SensorState::Running)
        return;

    if (this->pendingWakeLog)
    {
        this->pendingWakeLog = false;
        if (this->pendingWakeFromConfiguredSource)
        {
            printf("[SENSOR][SLEEP] wake detected from configured source, detection timer reset\n");
                esp_rom_printf("[SENSOR][SLEEP][ROM] wake configured source\n");
        }
        else
        {
            printf("[SENSOR][SLEEP] wake detected from other source (cause=%d), detection timer reset\n",
                   (int)this->pendingWakeCause);
                esp_rom_printf("[SENSOR][SLEEP][ROM] wake other source cause=%d\n", (int)this->pendingWakeCause);
        }
            fflush(stdout);
    }

    unsigned long nowMs = millis();

    if (this->mode == SensorMode::Alarm)
    {
        this->UpdateAlarm(nowMs);
        return;
    }

    if (this->mode == SensorMode::Monitor && this->config.ledGreen != GPIO_NONE)
    {
        if (this->greenBlinkUntilAtMs != 0 && nowMs >= this->greenBlinkUntilAtMs)
        {
            this->greenBlinkUntilAtMs = 0;
            this->greenLed.SetOff();
        }

        if ((nowMs - this->greenBlinkStartedAtMs) >= GreenMonitorBlinkIntervalMs)
        {
            this->greenBlinkStartedAtMs = nowMs;
            this->greenBlinkUntilAtMs = nowMs + GreenMonitorBlinkOnMs;
            this->greenLed.SetSolidOn();
        }
    }

    if (this->config.accelerometer != nullptr)
    {
        this->lastInterrupt = this->config.accelerometer->Interrupting(true);
        if (this->lastInterrupt)
        {
            this->interruptCount++;

            if (this->irqEventCount < Sensor::MaxIrqEventsInWindow)
            {
                unsigned int writeIndex = (this->irqEventHead + this->irqEventCount) % Sensor::MaxIrqEventsInWindow;
                this->irqEventTimesMs[writeIndex] = nowMs;
                this->irqEventCount++;
            }
            else
            {
                this->irqEventTimesMs[this->irqEventHead] = nowMs;
                this->irqEventHead = (this->irqEventHead + 1) % Sensor::MaxIrqEventsInWindow;
            }
        }

        while (this->irqEventCount > 0)
        {
            unsigned long oldestAtMs = this->irqEventTimesMs[this->irqEventHead];
            if ((nowMs - oldestAtMs) <= this->config.detectionWindowMs)
                break;

            this->irqEventHead = (this->irqEventHead + 1) % Sensor::MaxIrqEventsInWindow;
            this->irqEventCount--;
        }

        this->detectionWindowIrqDelta = this->irqEventCount;

        if (this->detectionWindowIrqDelta >= this->config.detectionIrqThreshold)
        {
            if (!this->detectionWindowLatched)
            {
                this->detectionWindowLatched = true;
                this->lastDetectionAtMs = nowMs;

                if (this->config.ledRed != GPIO_NONE)
                {
                    this->redLed.SetSolidOn();
                    this->redPulseUntilAtMs = nowMs + this->config.detectionPulseMs;
                }

#if BUZZER
                this->buzzer.SetSolidOn();
                this->buzzer.Update(nowMs);
#endif

                this->PushDetectionEvent(nowMs);
                if (this->detectionEventCount >= this->config.alarmDetectionCount)
                {
                    this->EnterAlarm(nowMs);
                    return;
                }

                printf("[SENSOR][DETECT] detection confirmed (irqDelta=%u/%u in %ums)\n",
                       this->detectionWindowIrqDelta,
                       this->config.detectionIrqThreshold,
                       this->config.detectionWindowMs);
            }
        }
        else
        {
            this->detectionWindowLatched = false;
        }

        this->config.accelerometer->Read(&this->lastAcceleration);
        this->sampleCount++;
    }
    else if (!this->warnedMissingAccelerometer)
    {
        this->warnedMissingAccelerometer = true;
        printf("[SENSOR][WARN] no accelerometer configured\n");
    }

    if (this->config.ledRed != GPIO_NONE && this->redPulseUntilAtMs != 0 && nowMs >= this->redPulseUntilAtMs)
    {
        this->redPulseUntilAtMs = 0;
        this->redLed.SetOff();

#if BUZZER
        this->buzzer.SetOff();
        this->buzzer.Update(nowMs);
#endif
    }

#if BUZZER
    this->buzzer.Update(nowMs);
#endif

    if (this->config.sleep.enabled && (nowMs - this->lastDetectionAtMs) >= this->config.noDetectionSleepMs)
    {
        this->mode = SensorMode::Sleep;
        printf("[SENSOR][SLEEP] no detection for %ums, entering sleep\n", this->config.noDetectionSleepMs);
        esp_rom_printf("[SENSOR][SLEEP][ROM] entering sleep\n");
        fflush(stdout);
        this->Sleep();
        this->mode = SensorMode::Monitor;
        fflush(stdout);
        return;
    }

    bool shouldPrintTelemetry = this->config.printTelemetry &&
                                (this->config.telemetryIntervalMs == 0 ||
                                 (nowMs - this->lastTelemetryAtMs) >= this->config.telemetryIntervalMs);

    if (shouldPrintTelemetry)
    {
        this->lastTelemetryAtMs = nowMs;

        if (this->config.accelerometer != nullptr)
        {
                         printf("[SENSOR] uptime=%lums samples=%lu irq=%d irqCount=%lu winDelta=%u acc(x=%d y=%d z=%d |len|=%.2f)\n",
                   nowMs - this->startedAtMs,
                   this->sampleCount,
                 this->lastInterrupt ? 1 : 0,
                 this->interruptCount,
                 this->detectionWindowIrqDelta,
                   this->lastAcceleration.X,
                   this->lastAcceleration.Y,
                   this->lastAcceleration.Z,
                   this->lastAcceleration.Length());
        }
        else
        {
                        printf("[SENSOR] uptime=%lums samples=%lu acc=unavailable\n",
                   nowMs - this->startedAtMs,
                   this->sampleCount);
        }
    }
}

void Sensor::Stop()
{
    this->state = SensorState::Stopped;
    this->mode = SensorMode::Monitor;
    this->Terminate();

#if BUZZER
    this->buzzer.SetOff();
    this->buzzer.Update(millis());
#endif

    if (this->config.ledGreen != GPIO_NONE)
        this->greenLed.SetOff();

    if (this->config.ledRed != GPIO_NONE)
        this->redLed.SetOff();

    printf("[SENSOR] stopped\n");
}

SensorState Sensor::State() const
{
    return this->state;
}

void Sensor::Sleep()
{
    this->mode = SensorMode::Sleep;

    if (this->config.ledGreen != GPIO_NONE)
    {
        this->greenLed.SetOff();
        this->greenBlinkUntilAtMs = 0;
    }

#if BUZZER
    this->buzzer.SetOff();
    this->buzzer.Update(millis());
#endif

    if (this->config.accelerometer != nullptr)
    {
        (void)this->config.accelerometer->Interrupting(true);
    }

    gpio_num_t wakePin = (gpio_num_t)this->config.sleep.wakePin;
    int activeLevel = this->config.sleep.wakeOnLow ? 0 : 1;
    int level = gpio_get_level(wakePin);
    if (level == activeLevel)
    {
        this->lastDetectionAtMs = millis();
        printf("[SENSOR][SLEEP] wake pin already active on GPIO%d, delaying sleep\n", this->config.sleep.wakePin);
        fflush(stdout);
        this->mode = SensorMode::Monitor;
        return;
    }

    if (this->config.ledGreen != GPIO_NONE)
        this->greenLed.SetOff();

    printf("[SENSOR][SLEEP] sleeping on GPIO%d %s\n",
           this->config.sleep.wakePin,
           this->config.sleep.wakeOnLow ? "LOW" : "HIGH");
    fflush(stdout);

    bool enteredSleep = false;
    esp_err_t sleepResult = ESP_ERR_NOT_SUPPORTED;

#if DEBUG
    printf("[SENSOR][SLEEP] developer bypass active, skipping Sleep::Begin\n");
    esp_rom_printf("[SENSOR][SLEEP][ROM] developer bypass active\n");
#else
    enteredSleep = this->sleep.Begin();
    sleepResult = this->sleep.LastResult();
#endif

    if (!enteredSleep)
    {
        if (this->config.sleep.mode == SleepMode::Deep)
        {
            this->mode = SensorMode::Monitor;
            return;
        }

        if (sleepResult == ESP_ERR_SLEEP_REJECT ||
            sleepResult == ESP_ERR_SLEEP_TOO_SHORT_SLEEP_DURATION ||
            sleepResult == ESP_ERR_NOT_SUPPORTED)
        {
            unsigned long nowMs = millis();
            this->startedAtMs = nowMs;
            this->lastTelemetryAtMs = nowMs;
            this->lastDetectionAtMs = nowMs;
            this->irqEventHead = 0;
            this->irqEventCount = 0;
            this->detectionWindowIrqDelta = 0;
            this->detectionWindowLatched = false;

            if (sleepResult == ESP_ERR_NOT_SUPPORTED)
            {
                printf("[SENSOR][SLEEP] light sleep bypassed by build config (err=%d), retry window reset\n", (int)sleepResult);
                esp_rom_printf("[SENSOR][SLEEP][ROM] light sleep bypassed by build config err=%d\n", (int)sleepResult);
            }
            else
            {
                printf("[SENSOR][SLEEP] light sleep rejected (err=%d), retry window reset\n", (int)sleepResult);
                esp_rom_printf("[SENSOR][SLEEP][ROM] light sleep rejected err=%d\n", (int)sleepResult);
            }
            fflush(stdout);
            this->mode = SensorMode::Monitor;
            return;
        }

        this->state = SensorState::Fault;
        printf("[SENSOR][ERROR] failed to enter sleep (err=%d)\n", (int)sleepResult);
            esp_rom_printf("[SENSOR][ERROR][ROM] failed to enter sleep err=%d\n", (int)sleepResult);
            fflush(stdout);
        this->Terminate();
        this->mode = SensorMode::Monitor;
        return;
    }

    unsigned long wakeAtMs = millis();
    bool wokeFromConfiguredSource = this->sleep.WokeFromConfiguredSource();
    esp_sleep_wakeup_cause_t wakeCause = this->sleep.WakeupCause();
    fflush(stdout);

    this->startedAtMs = wakeAtMs;
    this->lastTelemetryAtMs = wakeAtMs;
    this->lastDetectionAtMs = wakeAtMs;
    this->greenBlinkStartedAtMs = wakeAtMs;
    this->greenBlinkUntilAtMs = 0;
    this->irqEventHead = 0;
    this->irqEventCount = 0;
    this->detectionWindowIrqDelta = 0;
    this->detectionWindowLatched = false;
    this->redPulseUntilAtMs = 0;
    if (this->config.ledRed != GPIO_NONE)
        this->redLed.SetOff();

    if (this->config.ledGreen != GPIO_NONE)
        this->greenLed.SetOff();

    this->pendingWakeFromConfiguredSource = wokeFromConfiguredSource;
    this->pendingWakeCause = wakeCause;
    this->pendingWakeLog = true;
    this->mode = SensorMode::Monitor;
}

void Sensor::PushDetectionEvent(unsigned long nowMs)
{
    if (this->config.alarmDetectionWindowMs == 0)
        this->config.alarmDetectionWindowMs = 5000;

    while (this->detectionEventCount > 0)
    {
        unsigned long oldestAtMs = this->detectionEventTimesMs[this->detectionEventHead];
        if ((nowMs - oldestAtMs) <= this->config.alarmDetectionWindowMs)
            break;

        this->detectionEventHead = (this->detectionEventHead + 1) % Sensor::MaxDetectionEventsInWindow;
        this->detectionEventCount--;
    }

    if (this->detectionEventCount < Sensor::MaxDetectionEventsInWindow)
    {
        unsigned int writeIndex = (this->detectionEventHead + this->detectionEventCount) % Sensor::MaxDetectionEventsInWindow;
        this->detectionEventTimesMs[writeIndex] = nowMs;
        this->detectionEventCount++;
    }
    else
    {
        this->detectionEventTimesMs[this->detectionEventHead] = nowMs;
        this->detectionEventHead = (this->detectionEventHead + 1) % Sensor::MaxDetectionEventsInWindow;
    }
}

void Sensor::EnterAlarm(unsigned long nowMs)
{
    this->mode = SensorMode::Alarm;
    this->alarmStartedAtMs = nowMs;
    this->alarmLastToggleAtMs = nowMs;
    this->alarmOutputOn = true;
    this->redPulseUntilAtMs = 0;
    this->greenBlinkUntilAtMs = 0;

    if (this->config.ledGreen != GPIO_NONE)
        this->greenLed.SetOff();

    if (this->config.ledRed != GPIO_NONE)
        this->redLed.SetSolidOn();

#if BUZZER
    this->buzzer.SetSolidOn();
    this->buzzer.Update(nowMs);
#endif

    printf("[SENSOR][ALARM] entered (detections=%u within %ums)\n",
           this->detectionEventCount,
           this->config.alarmDetectionWindowMs);

    Location location;
    if (this->config.gps != nullptr)
        this->config.gps->Read(&location);

    memset(AlarmPayloadBuffer, 0, sizeof(AlarmPayloadBuffer));
    int payloadChars = snprintf(AlarmPayloadBuffer,
                               sizeof(AlarmPayloadBuffer),
                               "alarm|fix=%u|sat=%u|lat=%.6f|lon=%.6f|alt=%.1f|spd=%.1f|crs=%.1f|utc=%04u%02u%02u%02u%02u%02u",
                               location.fix ? 1U : 0U,
                               (unsigned int)location.satellites,
                               location.coordinate.Y,
                               location.coordinate.X,
                               location.altitude,
                               location.speed,
                               location.course,
                               (unsigned int)location.datetime.Year(),
                               (unsigned int)location.datetime.Month(),
                               (unsigned int)location.datetime.Day(),
                               (unsigned int)location.datetime.Hour(),
                               (unsigned int)location.datetime.Minute(),
                               (unsigned int)location.datetime.Second());

    if (payloadChars < 0)
    {
        printf("[SENSOR][ALARM][PKG][ERROR] failed to format payload\n");
        return;
    }

    unsigned short payloadLength = (unsigned short)payloadChars;
    if (payloadLength > AlarmPayloadMax)
        payloadLength = AlarmPayloadMax;

    memset(AlarmPacketBuffer, 0, sizeof(AlarmPacketBuffer));
    PackageHeader header;
    header.type = PackageType::Alarm;
    header.sequence = this->alarmPackageSequence++;
    header.flags = location.fix ? 1 : 0;
    header.meta = (unsigned short)location.satellites;

    unsigned short written = 0;
    if (!Package::Encode(header,
                         (const Byte *)AlarmPayloadBuffer,
                         payloadLength,
                         AlarmPacketBuffer,
                         AlarmPacketLength,
                         &written))
    {
        printf("[SENSOR][ALARM][PKG][ERROR] encode failed payloadLen=%u\n", payloadLength);
        return;
    }

    printf("[SENSOR][ALARM][PKG] type=%u seq=%u bytes=%u payload=\"%.*s\"\n",
           (unsigned int)header.type,
           (unsigned int)header.sequence,
           (unsigned int)written,
           (int)payloadLength,
           AlarmPayloadBuffer);
}

void Sensor::UpdateAlarm(unsigned long nowMs)
{
    if ((nowMs - this->alarmStartedAtMs) >= this->config.alarmDurationMs)
    {
        if (this->config.ledRed != GPIO_NONE)
            this->redLed.SetOff();

#if BUZZER
        this->buzzer.SetOff();
        this->buzzer.Update(nowMs);
#endif

        this->mode = SensorMode::Monitor;
        this->lastDetectionAtMs = nowMs;
        this->detectionEventHead = 0;
        this->detectionEventCount = 0;
        this->detectionWindowLatched = false;
        printf("[SENSOR][ALARM] finished, back to monitor\n");
        return;
    }

    if ((nowMs - this->alarmLastToggleAtMs) >= this->config.alarmBlinkIntervalMs)
    {
        this->alarmLastToggleAtMs = nowMs;
        this->alarmOutputOn = !this->alarmOutputOn;

        if (this->config.ledRed != GPIO_NONE)
        {
            if (this->alarmOutputOn)
                this->redLed.SetSolidOn();
            else
                this->redLed.SetOff();
        }

#if BUZZER
        if (this->alarmOutputOn)
            this->buzzer.SetSolidOn();
        else
            this->buzzer.SetOff();

        this->buzzer.Update(nowMs);
#endif
    }
}
