#include "sensor.h"
#include "config.h"
#include "api/common/time.h"
#include "api/transmission/package.h"

#include <cstdio>
#include <cstring>

#include "driver/gpio.h"
#include "esp_rom_sys.h"

static const unsigned short AlarmPacketLength = 240;
static const unsigned short AlarmPayloadMax = AlarmPacketLength - Package::HeaderSize;
static Byte AlarmPacketBuffer[AlarmPacketLength] = {0};
static char AlarmPayloadBuffer[AlarmPayloadMax + 1] = {0};
static char StatusPayloadBuffer[AlarmPayloadMax + 1] = {0};

Sensor::Sensor(const SensorConfig &config)
    : Loop(config.rate), config(config), sleep(config.sleep)
{
}

bool Sensor::Start()
{
    this->state = SensorState::Ready;
    this->startedAtMs = millis();
    this->lastTelemetryAtMs = this->startedAtMs;
    this->lastDetectionAtMs = this->startedAtMs;
    this->alarmStartedAtMs = 0;
    this->alarmLastToggleAtMs = 0;
    this->alarmLastRadioAtMs = 0;
    this->statusLastRadioAtMs = 0;
    this->alarmOutputOn = false;
    this->alarmPackageSequence = 0;
    this->lastRxAtMs = this->startedAtMs;
    this->lastHeartbeatAtMs = this->startedAtMs;
    this->rxPackets = 0;
    this->rxInvalid = 0;
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
    this->greenBlinkUntilAtMs = 0;
    this->statusLedBlinkStartedAtMs = this->startedAtMs;
    this->statusLedBlinkUntilAtMs = 0;
    this->modeSignalUntilAtMs = 0;

    if (this->config.ledGreen != GPIO_NONE)
    {
        this->greenLed.Begin(this->config.ledGreen, this->config.ledActiveLow);
        this->greenLed.SetSolidOn();
        this->greenBlinkUntilAtMs = this->startedAtMs + 2000;
    }

    if (this->config.ledStatus != GPIO_NONE)
    {
        this->statusLed.Begin(this->config.ledStatus, this->config.ledActiveLow);
        this->statusLed.SetSolidOn();
        this->modeSignalUntilAtMs = this->startedAtMs + 2000;
    }

    if (this->config.ledRed != GPIO_NONE)
    {
        this->redLed.Begin(this->config.ledRed, this->config.ledActiveLow);
        this->redLed.SetOff();
    }

    this->BeginMode(this->config.startInReceiveAlarm ? SensorMode::ReceiveAlarm : SensorMode::AlarmSend,
                    this->startedAtMs);

#if SENSOR_BUZZER
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
        this->config.alarmDetectionCount = 3;

    if (this->config.alarmDetectionWindowMs == 0)
        this->config.alarmDetectionWindowMs = 10000;

    if (this->config.alarmDurationMs == 0)
        this->config.alarmDurationMs = 60000;

    if (this->config.alarmBlinkIntervalMs == 0)
        this->config.alarmBlinkIntervalMs = 500;

    if (this->config.alarmRadioIntervalMs == 0)
        this->config.alarmRadioIntervalMs = 5000;

    if (this->config.statusRadioIntervalMs == 0)
        this->config.statusRadioIntervalMs = 1200000;

    if (this->config.armedBlinkIntervalMs == 0)
        this->config.armedBlinkIntervalMs = 30000;

    if (this->config.armedBlinkOnMs == 0)
        this->config.armedBlinkOnMs = 100;

    if (!this->config.startInReceiveAlarm && !this->sleep.Ready())
    {
        this->state = SensorState::Fault;
        printf("[SENSOR][ERROR] invalid sleep wake pin: %d\n", this->config.sleep.wakePin);
        return false;
    }

    printf("[SENSOR] started target=%ums\n", this->Target());

    if (!this->config.startInReceiveAlarm && this->config.sleep.enabled)
    {
         printf("[SENSOR][SLEEP] wakePin=%d wakeLevel=%s noDetectionSleepMs=%ums wakeCause=%d\n",
               this->config.sleep.wakePin,
               this->config.sleep.wakeOnLow ? "low" : "high",
             this->config.noDetectionSleepMs,
                             (int)this->sleep.WakeupCause());
    }

    if (!this->config.startInReceiveAlarm)
    {
        printf("[SENSOR][DETECT] threshold=%u windowMs=%u pulseMs=%u\n",
               this->config.detectionIrqThreshold,
               this->config.detectionWindowMs,
               this->config.detectionPulseMs);
    }

    this->SendStatusPacket(this->startedAtMs);

    return true;
}

void Sensor::Execute(const FrameTime &time)
{
    (void)time;

    if (this->state != SensorState::Ready && this->state != SensorState::Active)
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

    if (this->config.ledGreen != GPIO_NONE &&
        this->greenBlinkUntilAtMs != 0 &&
        nowMs >= this->greenBlinkUntilAtMs)
    {
        this->greenBlinkUntilAtMs = 0;
        this->greenLed.SetOff();
    }

    if (this->config.ledStatus != GPIO_NONE &&
        this->modeSignalUntilAtMs != 0 &&
        nowMs >= this->modeSignalUntilAtMs)
    {
        this->modeSignalUntilAtMs = 0;
        this->statusLed.SetOff();
    }

    if (this->config.ledStatus != GPIO_NONE &&
        this->statusLedBlinkUntilAtMs != 0 &&
        nowMs >= this->statusLedBlinkUntilAtMs)
    {
        this->statusLedBlinkUntilAtMs = 0;
        this->statusLed.SetOff();
    }

    if (this->mode == SensorMode::Alarm)
    {
        this->UpdateAlarm(nowMs);
        return;
    }

    if (this->mode == SensorMode::ReceiveAlarm)
    {
        this->UpdateReceiveAlarm(nowMs);

        if ((nowMs - this->lastHeartbeatAtMs) >= 5000)
        {
            this->lastHeartbeatAtMs = nowMs;
            printf("[SENSOR][ALIVE] mode=receive state=%u rx=%lu invalid=%lu lastRxAgo=%lums\n",
                   (unsigned int)this->state,
                   this->rxPackets,
                   this->rxInvalid,
                   nowMs - this->lastRxAtMs);
        }

#if SENSOR_BUZZER
        this->buzzer.Update(nowMs);
#endif
        return;
    }

    if ((nowMs - this->statusLastRadioAtMs) >= this->config.statusRadioIntervalMs)
        this->SendStatusPacket(nowMs);

    if (this->state == SensorState::Ready &&
        (this->mode == SensorMode::AlarmSend || this->mode == SensorMode::ReceiveAlarm) &&
        this->config.ledStatus != GPIO_NONE)
    {
        bool greenBlinkActive = this->greenBlinkUntilAtMs != 0;
        if (!greenBlinkActive && this->modeSignalUntilAtMs == 0 &&
            (nowMs - this->statusLedBlinkStartedAtMs) >= this->config.armedBlinkIntervalMs)
        {
            this->statusLedBlinkStartedAtMs = nowMs;
            this->statusLedBlinkUntilAtMs = nowMs + this->config.armedBlinkOnMs;
            this->statusLed.SetSolidOn();
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

#if SENSOR_BUZZER
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

#if SENSOR_BUZZER
        this->buzzer.SetOff();
        this->buzzer.Update(nowMs);
#endif
    }

#if SENSOR_BUZZER
    this->buzzer.Update(nowMs);
#endif

    if (this->config.sleep.enabled && (nowMs - this->lastDetectionAtMs) >= this->config.noDetectionSleepMs)
    {
        this->state = SensorState::Sleep;
        printf("[SENSOR][SLEEP] no detection for %ums, entering sleep\n", this->config.noDetectionSleepMs);
        esp_rom_printf("[SENSOR][SLEEP][ROM] entering sleep\n");
        fflush(stdout);
        this->Sleep();
        this->state = SensorState::Ready;
        this->BeginMode(SensorMode::AlarmSend, nowMs);
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
    this->state = SensorState::Disabled;
    this->mode = SensorMode::Disabled;
    this->Terminate();

#if SENSOR_BUZZER
    this->buzzer.SetOff();
    this->buzzer.Update(millis());
#endif

    if (this->config.ledGreen != GPIO_NONE)
        this->greenLed.SetOff();

    if (this->config.ledStatus != GPIO_NONE)
        this->statusLed.SetOff();

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
    this->state = SensorState::Sleep;

    if (this->config.ledGreen != GPIO_NONE)
    {
        this->greenLed.SetOff();
    }

    if (this->config.ledStatus != GPIO_NONE)
    {
        this->statusLed.SetOff();
        this->greenBlinkUntilAtMs = 0;
        this->statusLedBlinkUntilAtMs = 0;
    }

#if SENSOR_BUZZER
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
        this->state = SensorState::Ready;
        this->BeginMode(SensorMode::AlarmSend, millis());
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

#if SENSOR_DEBUG
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
            this->state = SensorState::Ready;
            this->BeginMode(SensorMode::AlarmSend, millis());
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
            this->state = SensorState::Ready;
            this->BeginMode(SensorMode::AlarmSend, nowMs);
            return;
        }

        this->state = SensorState::Fault;
        printf("[SENSOR][ERROR] failed to enter sleep (err=%d)\n", (int)sleepResult);
            esp_rom_printf("[SENSOR][ERROR][ROM] failed to enter sleep err=%d\n", (int)sleepResult);
            fflush(stdout);
        this->Terminate();
        this->BeginMode(SensorMode::AlarmSend, millis());
        return;
    }

    unsigned long wakeAtMs = millis();
    bool wokeFromConfiguredSource = this->sleep.WokeFromConfiguredSource();
    esp_sleep_wakeup_cause_t wakeCause = this->sleep.WakeupCause();
    fflush(stdout);

    this->startedAtMs = wakeAtMs;
    this->lastTelemetryAtMs = wakeAtMs;
    this->lastDetectionAtMs = wakeAtMs;
    this->greenBlinkUntilAtMs = 0;
    this->statusLedBlinkStartedAtMs = wakeAtMs;
    this->statusLedBlinkUntilAtMs = 0;
    this->irqEventHead = 0;
    this->irqEventCount = 0;
    this->detectionWindowIrqDelta = 0;
    this->detectionWindowLatched = false;
    this->redPulseUntilAtMs = 0;
    if (this->config.ledRed != GPIO_NONE)
        this->redLed.SetOff();

    if (this->config.ledGreen != GPIO_NONE)
        this->greenLed.SetOff();

    if (this->config.ledStatus != GPIO_NONE)
        this->statusLed.SetOff();

    this->pendingWakeFromConfiguredSource = wokeFromConfiguredSource;
    this->pendingWakeCause = wakeCause;
    this->pendingWakeLog = true;
    this->state = SensorState::Ready;
    this->BeginMode(SensorMode::AlarmSend, wakeAtMs);
}

void Sensor::BeginMode(SensorMode mode, unsigned long nowMs)
{
    (void)nowMs;
    this->mode = mode;
}

void Sensor::PushDetectionEvent(unsigned long nowMs)
{
    if (this->config.alarmDetectionWindowMs == 0)
        this->config.alarmDetectionWindowMs = 10000;

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
    this->state = SensorState::Active;
    this->BeginMode(SensorMode::Alarm, nowMs);
    this->alarmStartedAtMs = nowMs;
    this->alarmLastToggleAtMs = nowMs;
    this->alarmLastRadioAtMs = 0;
    this->alarmOutputOn = true;
    this->redPulseUntilAtMs = 0;

    if (this->config.ledGreen != GPIO_NONE)
    {
        // Keep startup power indicator visible for the configured boot window in all modes.
        if (this->greenBlinkUntilAtMs == 0 || nowMs >= this->greenBlinkUntilAtMs)
            this->greenLed.SetOff();
    }

    if (this->config.ledRed != GPIO_NONE)
        this->redLed.SetSolidOn();
    else if (this->config.ledStatus != GPIO_NONE)
        this->statusLed.SetSolidOn();

#if SENSOR_BUZZER
    this->buzzer.SetSolidOn();
    this->buzzer.Update(nowMs);
#endif

    printf("[SENSOR][ALARM] entered (detections=%u within %ums)\n",
           this->detectionEventCount,
           this->config.alarmDetectionWindowMs);

    this->SendAlarmPacket(nowMs);
}

void Sensor::SendAlarmPacket(unsigned long nowMs)
{
#if !SENSOR_TRANSMIT
    (void)nowMs;
    return;
#endif

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

    if (this->config.transceiver == nullptr)
    {
        printf("[SENSOR][ALARM][TX][WARN] transceiver not configured\n");
        return;
    }

    unsigned short packetLength = this->config.radioPacketLength;
    if (packetLength == 0 || packetLength > AlarmPacketLength)
        packetLength = AlarmPacketLength;

    if (!this->config.transceiver->Send(AlarmPacketBuffer, packetLength))
    {
        printf("[SENSOR][ALARM][TX][WARN] send failed len=%u\n", (unsigned int)packetLength);
        return;
    }

    this->alarmLastRadioAtMs = nowMs;

    printf("[SENSOR][ALARM][TX] sent len=%u seq=%u\n",
           (unsigned int)packetLength,
           (unsigned int)header.sequence);
}

void Sensor::SendStatusPacket(unsigned long nowMs)
{
#if !SENSOR_TRANSMIT
    (void)nowMs;
    return;
#endif

    if (this->config.transceiver == nullptr)
    {
        printf("[SENSOR][STATUS][TX][WARN] transceiver not configured\n");
        return;
    }

    memset(StatusPayloadBuffer, 0, sizeof(StatusPayloadBuffer));
    int payloadChars = snprintf(StatusPayloadBuffer,
                                sizeof(StatusPayloadBuffer),
                                "status|state=%u|mode=%u|uptime=%lu|seq=%u",
                                (unsigned int)this->state,
                                (unsigned int)this->mode,
                                nowMs - this->startedAtMs,
                                (unsigned int)this->alarmPackageSequence);

    if (payloadChars < 0)
    {
        printf("[SENSOR][STATUS][PKG][ERROR] failed to format payload\n");
        return;
    }

    unsigned short payloadLength = (unsigned short)payloadChars;
    if (payloadLength > AlarmPayloadMax)
        payloadLength = AlarmPayloadMax;

    memset(AlarmPacketBuffer, 0, sizeof(AlarmPacketBuffer));
    PackageHeader header;
    header.type = PackageType::Status;
    header.sequence = this->alarmPackageSequence++;
    header.flags = 0;
    header.meta = 0;

    unsigned short written = 0;
    if (!Package::Encode(header,
                         (const Byte *)StatusPayloadBuffer,
                         payloadLength,
                         AlarmPacketBuffer,
                         AlarmPacketLength,
                         &written))
    {
        printf("[SENSOR][STATUS][PKG][ERROR] encode failed payloadLen=%u\n", payloadLength);
        return;
    }

    unsigned short packetLength = this->config.radioPacketLength;
    if (packetLength == 0 || packetLength > AlarmPacketLength)
        packetLength = AlarmPacketLength;

    if (!this->config.transceiver->Send(AlarmPacketBuffer, packetLength))
    {
        printf("[SENSOR][STATUS][TX][WARN] send failed len=%u\n", (unsigned int)packetLength);
        return;
    }

    this->statusLastRadioAtMs = nowMs;
    printf("[SENSOR][STATUS][TX] sent len=%u seq=%u\n",
           (unsigned int)packetLength,
           (unsigned int)header.sequence);
}

void Sensor::ForceAlarmOutputsHigh(unsigned long nowMs)
{
    if (this->config.ledRed != GPIO_NONE)
        this->redLed.SetSolidOn();
    else if (this->config.ledStatus != GPIO_NONE)
        this->statusLed.SetSolidOn();

#if SENSOR_BUZZER
    this->buzzer.SetSolidOn();
    this->buzzer.Update(nowMs);
#endif

    this->alarmOutputOn = true;
}

void Sensor::UpdateAlarm(unsigned long nowMs)
{
    if ((nowMs - this->alarmLastRadioAtMs) >= this->config.alarmRadioIntervalMs)
    {
        // Keep local alarm indication high while radio TX may block for a while.
        this->ForceAlarmOutputsHigh(nowMs);
        this->SendAlarmPacket(nowMs);
        this->alarmLastToggleAtMs = millis();
        nowMs = this->alarmLastToggleAtMs;
    }

    if ((nowMs - this->alarmStartedAtMs) >= this->config.alarmDurationMs)
    {
        if (this->config.ledRed != GPIO_NONE)
            this->redLed.SetOff();
        else if (this->config.ledStatus != GPIO_NONE)
            this->statusLed.SetOff();

#if SENSOR_BUZZER
        this->buzzer.SetOff();
        this->buzzer.Update(nowMs);
#endif

        this->state = SensorState::Ready;
        this->BeginMode(this->config.startInReceiveAlarm ? SensorMode::ReceiveAlarm : SensorMode::AlarmSend,
                nowMs);
        this->lastDetectionAtMs = nowMs;
        this->detectionEventHead = 0;
        this->detectionEventCount = 0;
        this->detectionWindowLatched = false;
        this->SendStatusPacket(nowMs);
        printf("[SENSOR][ALARM] finished, back to ready/alarm-send\n");
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
        else if (this->config.ledStatus != GPIO_NONE)
        {
            if (this->alarmOutputOn)
                this->statusLed.SetSolidOn();
            else
                this->statusLed.SetOff();
        }

#if SENSOR_BUZZER
        if (this->alarmOutputOn)
            this->buzzer.SetSolidOn();
        else
            this->buzzer.SetOff();

        this->buzzer.Update(nowMs);
#endif
    }
}

bool Sensor::UpdateReceiveAlarm(unsigned long nowMs)
{
    if (this->config.transceiver == nullptr)
        return false;

    static Byte reassemblyBuffer[AlarmPacketLength] = {0};
    static unsigned short reassemblyUsed = 0;

    unsigned short packetLength = this->config.radioPacketLength;
    if (packetLength == 0 || packetLength > AlarmPacketLength)
        packetLength = AlarmPacketLength;

    int received = this->config.transceiver->Receive(AlarmPacketBuffer,
                                                     packetLength,
                                                     this->config.receiveTimeoutMs);
    if (received <= 0)
        return false;

    // Reassemble fixed-size packets across multiple reads. Some modules deliver
    // a single over-the-air packet in several UART chunks.
    unsigned short receivedBytes = (unsigned short)received;
    if ((unsigned int)reassemblyUsed + (unsigned int)receivedBytes > packetLength)
        reassemblyUsed = 0;

    memcpy(&reassemblyBuffer[reassemblyUsed], AlarmPacketBuffer, receivedBytes);
    reassemblyUsed = (unsigned short)(reassemblyUsed + receivedBytes);

    if (reassemblyUsed < packetLength)
        return false;

    memcpy(AlarmPacketBuffer, reassemblyBuffer, packetLength);
    reassemblyUsed = 0;

    PackageHeader header;
    static Byte payload[AlarmPacketLength] = {0};
    unsigned short payloadLength = 0;
    if (!Package::Decode(AlarmPacketBuffer,
                         (unsigned short)received,
                         &header,
                         payload,
                         sizeof(payload),
                         &payloadLength))
    {
        this->rxInvalid++;
        printf("[SENSOR][RX][WARN] invalid package bytes=%d\n", received);
        return false;
    }

    this->lastRxAtMs = nowMs;
    this->rxPackets++;

    if (header.type == PackageType::Alarm)
    {
        this->EnterAlarm(nowMs);
        printf("[SENSOR][RX] alarm seq=%u payloadLen=%u\n",
               (unsigned int)header.sequence,
               (unsigned int)payloadLength);
        return true;
    }

    if (header.type == PackageType::Status)
    {
        if (this->config.ledStatus != GPIO_NONE)
        {
            this->statusLedBlinkUntilAtMs = nowMs + this->config.statusLedOnMs;
            this->statusLed.SetSolidOn();
        }

        printf("[SENSOR][RX] status seq=%u payloadLen=%u\n",
               (unsigned int)header.sequence,
               (unsigned int)payloadLength);
        return true;
    }

    printf("[SENSOR][RX] type=%u seq=%u payloadLen=%u\n",
           (unsigned int)header.type,
           (unsigned int)header.sequence,
           (unsigned int)payloadLength);
    return true;
}

