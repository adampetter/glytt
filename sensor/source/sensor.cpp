#include "sensor.h"
#include "config.h"
#include "api/common/time.h"

#include <cstdio>

#include "driver/gpio.h"
#include "esp_rom_sys.h"

Sensor::Sensor(const SensorConfig &config)
    : Loop(config.rate), config(config), sleep(config.sleep)
{
}

bool Sensor::Start()
{
    this->state = SensorState::Running;
    this->startedAtMs = millis();
    this->lastTelemetryAtMs = this->startedAtMs;
    this->lastDetectionAtMs = this->startedAtMs;
    this->warnedMissingAccelerometer = false;
    this->lastAcceleration = Acceleration::Empty;
    this->sampleCount = 0;
    this->lastInterrupt = false;
    this->interruptCount = 0;
    this->irqEventHead = 0;
    this->irqEventCount = 0;
    this->detectionWindowIrqDelta = 0;
    this->detectionWindowLatched = false;
    this->redPulseUntilAtMs = 0;
    this->greenWakePulseUntilAtMs = 0;

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

#ifdef BUZZER
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

    if (this->config.ledGreen != GPIO_NONE &&
        this->greenWakePulseUntilAtMs != 0 &&
        nowMs >= this->greenWakePulseUntilAtMs)
    {
        this->greenWakePulseUntilAtMs = 0;
        this->greenLed.SetOff();
    }

#if BUZZER
    this->buzzer.Update(nowMs);
#endif

    if (this->config.sleep.enabled && (nowMs - this->lastDetectionAtMs) >= this->config.noDetectionSleepMs)
    {
        printf("[SENSOR][SLEEP] no detection for %ums, entering sleep\n", this->config.noDetectionSleepMs);
        esp_rom_printf("[SENSOR][SLEEP][ROM] entering sleep\n");
        fflush(stdout);
        this->Sleep();
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
            return;

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
            return;
        }

        this->state = SensorState::Fault;
        printf("[SENSOR][ERROR] failed to enter sleep (err=%d)\n", (int)sleepResult);
            esp_rom_printf("[SENSOR][ERROR][ROM] failed to enter sleep err=%d\n", (int)sleepResult);
            fflush(stdout);
        this->Terminate();
        return;
    }

    unsigned long wakeAtMs = millis();
    bool wokeFromConfiguredSource = this->sleep.WokeFromConfiguredSource();
    esp_sleep_wakeup_cause_t wakeCause = this->sleep.WakeupCause();
    fflush(stdout);

    this->startedAtMs = wakeAtMs;
    this->lastTelemetryAtMs = wakeAtMs;
    this->lastDetectionAtMs = wakeAtMs;
    this->irqEventHead = 0;
    this->irqEventCount = 0;
    this->detectionWindowIrqDelta = 0;
    this->detectionWindowLatched = false;
    this->redPulseUntilAtMs = 0;
    if (this->config.ledRed != GPIO_NONE)
        this->redLed.SetOff();

    if (this->config.ledGreen != GPIO_NONE)
    {
        this->greenLed.SetSolidOn();
        this->greenWakePulseUntilAtMs = wakeAtMs + 150;
    }

    this->pendingWakeFromConfiguredSource = wokeFromConfiguredSource;
    this->pendingWakeCause = wakeCause;
    this->pendingWakeLog = true;
}
