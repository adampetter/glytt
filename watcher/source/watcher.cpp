#include "watcher.h"
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

Watcher::Watcher(const WatcherConfig &config)
    : Loop(config.rate), config(config), sleep(config.sleep)
{
}

bool Watcher::Start()
{
    this->state = WatcherState::Ready;
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
    this->greenBlinkStartedAtMs = this->startedAtMs;
    this->greenBlinkUntilAtMs = 0;
    this->blueBlinkStartedAtMs = this->startedAtMs;
    this->blueBlinkUntilAtMs = 0;
    this->modeSignalUntilAtMs = 0;

    if (this->config.ledGreen != GPIO_NONE)
    {
        this->greenLed.Begin(this->config.ledGreen, this->config.ledActiveLow);
        this->greenLed.SetSolidOn();
        this->greenBlinkUntilAtMs = this->startedAtMs + 2000;
    }

    if (this->config.ledBlue != GPIO_NONE)
    {
        this->blueLed.Begin(this->config.ledBlue, this->config.ledActiveLow);
        this->blueLed.SetSolidOn();
        this->modeSignalUntilAtMs = this->startedAtMs + 2000;
    }

    if (this->config.ledRed != GPIO_NONE)
    {
        this->redLed.Begin(this->config.ledRed, this->config.ledActiveLow);
        this->redLed.SetOff();
    }

    this->BeginMode(this->config.startInReceiveAlarm ? WatcherMode::ReceiveAlarm : WatcherMode::AlarmSend,
                    this->startedAtMs);

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
        this->state = WatcherState::Fault;
        printf("[WATCHER][ERROR] invalid sleep wake pin: %d\n", this->config.sleep.wakePin);
        return false;
    }

    printf("[WATCHER] started target=%ums\n", this->Target());

    if (!this->config.startInReceiveAlarm && this->config.sleep.enabled)
    {
         printf("[WATCHER][SLEEP] wakePin=%d wakeLevel=%s noDetectionSleepMs=%ums wakeCause=%d\n",
               this->config.sleep.wakePin,
               this->config.sleep.wakeOnLow ? "low" : "high",
             this->config.noDetectionSleepMs,
                             (int)this->sleep.WakeupCause());
    }

    if (!this->config.startInReceiveAlarm)
    {
        printf("[WATCHER][DETECT] threshold=%u windowMs=%u pulseMs=%u\n",
               this->config.detectionIrqThreshold,
               this->config.detectionWindowMs,
               this->config.detectionPulseMs);
    }

    this->SendStatusPacket(this->startedAtMs);

    return true;
}

void Watcher::Execute(const FrameTime &time)
{
    (void)time;

    if (this->state != WatcherState::Ready && this->state != WatcherState::Active)
        return;

    if (this->pendingWakeLog)
    {
        this->pendingWakeLog = false;
        if (this->pendingWakeFromConfiguredSource)
        {
            printf("[WATCHER][SLEEP] wake detected from configured source, detection timer reset\n");
                esp_rom_printf("[WATCHER][SLEEP][ROM] wake configured source\n");
        }
        else
        {
            printf("[WATCHER][SLEEP] wake detected from other source (cause=%d), detection timer reset\n",
                   (int)this->pendingWakeCause);
                esp_rom_printf("[WATCHER][SLEEP][ROM] wake other source cause=%d\n", (int)this->pendingWakeCause);
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

    if (this->config.ledBlue != GPIO_NONE &&
        this->modeSignalUntilAtMs != 0 &&
        nowMs >= this->modeSignalUntilAtMs)
    {
        this->modeSignalUntilAtMs = 0;
        this->blueLed.SetOff();
    }

    if (this->config.ledBlue != GPIO_NONE &&
        this->blueBlinkUntilAtMs != 0 &&
        nowMs >= this->blueBlinkUntilAtMs)
    {
        this->blueBlinkUntilAtMs = 0;
        this->blueLed.SetOff();
    }

    if (this->state == WatcherState::Ready || this->state == WatcherState::Active)
    {
        bool blueSignalActive = this->modeSignalUntilAtMs != 0 || this->blueBlinkUntilAtMs != 0;
        if (this->config.ledGreen != GPIO_NONE && !blueSignalActive)
        {
            if ((nowMs - this->greenBlinkStartedAtMs) >= this->config.armedBlinkIntervalMs)
            {
                this->greenBlinkStartedAtMs = nowMs;
                this->greenBlinkUntilAtMs = nowMs + this->config.armedBlinkOnMs;
                this->greenLed.SetSolidOn();
            }
        }
    }

    if (this->mode == WatcherMode::Alarm)
    {
        this->UpdateAlarm(nowMs);
        return;
    }

    if (this->mode == WatcherMode::ReceiveAlarm)
    {
        this->UpdateReceiveAlarm(nowMs);

        if ((nowMs - this->lastHeartbeatAtMs) >= 5000)
        {
            this->lastHeartbeatAtMs = nowMs;
            printf("[WATCHER][ALIVE] mode=receive state=%u rx=%lu invalid=%lu lastRxAgo=%lums\n",
                   (unsigned int)this->state,
                   this->rxPackets,
                   this->rxInvalid,
                   nowMs - this->lastRxAtMs);
        }

#if BUZZER
        this->buzzer.Update(nowMs);
#endif
        return;
    }

    if ((nowMs - this->statusLastRadioAtMs) >= this->config.statusRadioIntervalMs)
        this->SendStatusPacket(nowMs);

    if (this->state == WatcherState::Ready &&
        (this->mode == WatcherMode::AlarmSend || this->mode == WatcherMode::ReceiveAlarm) &&
        this->config.ledBlue != GPIO_NONE)
    {
        bool greenBlinkActive = this->greenBlinkUntilAtMs != 0;
        if (!greenBlinkActive && this->modeSignalUntilAtMs == 0 &&
            (nowMs - this->blueBlinkStartedAtMs) >= this->config.armedBlinkIntervalMs)
        {
            this->blueBlinkStartedAtMs = nowMs;
            this->blueBlinkUntilAtMs = nowMs + this->config.armedBlinkOnMs;
            this->blueLed.SetSolidOn();
        }
    }

    if (this->config.accelerometer != nullptr)
    {
        this->lastInterrupt = this->config.accelerometer->Interrupting(true);
        if (this->lastInterrupt)
        {
            this->interruptCount++;

            if (this->irqEventCount < Watcher::MaxIrqEventsInWindow)
            {
                unsigned int writeIndex = (this->irqEventHead + this->irqEventCount) % Watcher::MaxIrqEventsInWindow;
                this->irqEventTimesMs[writeIndex] = nowMs;
                this->irqEventCount++;
            }
            else
            {
                this->irqEventTimesMs[this->irqEventHead] = nowMs;
                this->irqEventHead = (this->irqEventHead + 1) % Watcher::MaxIrqEventsInWindow;
            }
        }

        while (this->irqEventCount > 0)
        {
            unsigned long oldestAtMs = this->irqEventTimesMs[this->irqEventHead];
            if ((nowMs - oldestAtMs) <= this->config.detectionWindowMs)
                break;

            this->irqEventHead = (this->irqEventHead + 1) % Watcher::MaxIrqEventsInWindow;
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

                printf("[WATCHER][DETECT] detection confirmed (irqDelta=%u/%u in %ums)\n",
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
        printf("[WATCHER][WARN] no accelerometer configured\n");
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
        this->state = WatcherState::Sleep;
        printf("[WATCHER][SLEEP] no detection for %ums, entering sleep\n", this->config.noDetectionSleepMs);
        esp_rom_printf("[WATCHER][SLEEP][ROM] entering sleep\n");
        fflush(stdout);
        this->Sleep();
        this->state = WatcherState::Ready;
        this->BeginMode(WatcherMode::AlarmSend, nowMs);
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
                         printf("[WATCHER] uptime=%lums samples=%lu irq=%d irqCount=%lu winDelta=%u acc(x=%d y=%d z=%d |len|=%.2f)\n",
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
                        printf("[WATCHER] uptime=%lums samples=%lu acc=unavailable\n",
                   nowMs - this->startedAtMs,
                   this->sampleCount);
        }
    }
}

void Watcher::Stop()
{
    this->state = WatcherState::Disabled;
    this->mode = WatcherMode::Disabled;
    this->Terminate();

#if BUZZER
    this->buzzer.SetOff();
    this->buzzer.Update(millis());
#endif

    if (this->config.ledGreen != GPIO_NONE)
        this->greenLed.SetOff();

    if (this->config.ledBlue != GPIO_NONE)
        this->blueLed.SetOff();

    if (this->config.ledRed != GPIO_NONE)
        this->redLed.SetOff();

    printf("[WATCHER] stopped\n");
}

WatcherState Watcher::State() const
{
    return this->state;
}

void Watcher::Sleep()
{
    this->state = WatcherState::Sleep;

    if (this->config.ledGreen != GPIO_NONE)
    {
        this->greenLed.SetOff();
    }

    if (this->config.ledBlue != GPIO_NONE)
    {
        this->blueLed.SetOff();
        this->greenBlinkUntilAtMs = 0;
        this->blueBlinkUntilAtMs = 0;
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
        printf("[WATCHER][SLEEP] wake pin already active on GPIO%d, delaying sleep\n", this->config.sleep.wakePin);
        fflush(stdout);
        this->state = WatcherState::Ready;
        this->BeginMode(WatcherMode::AlarmSend, millis());
        return;
    }

    if (this->config.ledGreen != GPIO_NONE)
        this->greenLed.SetOff();

    printf("[WATCHER][SLEEP] sleeping on GPIO%d %s\n",
           this->config.sleep.wakePin,
           this->config.sleep.wakeOnLow ? "LOW" : "HIGH");
    fflush(stdout);

    bool enteredSleep = false;
    esp_err_t sleepResult = ESP_ERR_NOT_SUPPORTED;

#if DEBUG
    printf("[WATCHER][SLEEP] developer bypass active, skipping Sleep::Begin\n");
    esp_rom_printf("[WATCHER][SLEEP][ROM] developer bypass active\n");
#else
    enteredSleep = this->sleep.Begin();
    sleepResult = this->sleep.LastResult();
#endif

    if (!enteredSleep)
    {
        if (this->config.sleep.mode == SleepMode::Deep)
        {
            this->state = WatcherState::Ready;
            this->BeginMode(WatcherMode::AlarmSend, millis());
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
                printf("[WATCHER][SLEEP] light sleep bypassed by build config (err=%d), retry window reset\n", (int)sleepResult);
                esp_rom_printf("[WATCHER][SLEEP][ROM] light sleep bypassed by build config err=%d\n", (int)sleepResult);
            }
            else
            {
                printf("[WATCHER][SLEEP] light sleep rejected (err=%d), retry window reset\n", (int)sleepResult);
                esp_rom_printf("[WATCHER][SLEEP][ROM] light sleep rejected err=%d\n", (int)sleepResult);
            }
            fflush(stdout);
            this->state = WatcherState::Ready;
            this->BeginMode(WatcherMode::AlarmSend, nowMs);
            return;
        }

        this->state = WatcherState::Fault;
        printf("[WATCHER][ERROR] failed to enter sleep (err=%d)\n", (int)sleepResult);
            esp_rom_printf("[WATCHER][ERROR][ROM] failed to enter sleep err=%d\n", (int)sleepResult);
            fflush(stdout);
        this->Terminate();
        this->BeginMode(WatcherMode::AlarmSend, millis());
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
    this->blueBlinkStartedAtMs = wakeAtMs;
    this->blueBlinkUntilAtMs = 0;
    this->irqEventHead = 0;
    this->irqEventCount = 0;
    this->detectionWindowIrqDelta = 0;
    this->detectionWindowLatched = false;
    this->redPulseUntilAtMs = 0;
    if (this->config.ledRed != GPIO_NONE)
        this->redLed.SetOff();

    if (this->config.ledGreen != GPIO_NONE)
        this->greenLed.SetOff();

    if (this->config.ledBlue != GPIO_NONE)
        this->blueLed.SetOff();

    this->pendingWakeFromConfiguredSource = wokeFromConfiguredSource;
    this->pendingWakeCause = wakeCause;
    this->pendingWakeLog = true;
    this->state = WatcherState::Ready;
    this->BeginMode(WatcherMode::AlarmSend, wakeAtMs);
}

void Watcher::BeginMode(WatcherMode mode, unsigned long nowMs)
{
    (void)nowMs;
    this->mode = mode;
}

void Watcher::PushDetectionEvent(unsigned long nowMs)
{
    if (this->config.alarmDetectionWindowMs == 0)
        this->config.alarmDetectionWindowMs = 10000;

    while (this->detectionEventCount > 0)
    {
        unsigned long oldestAtMs = this->detectionEventTimesMs[this->detectionEventHead];
        if ((nowMs - oldestAtMs) <= this->config.alarmDetectionWindowMs)
            break;

        this->detectionEventHead = (this->detectionEventHead + 1) % Watcher::MaxDetectionEventsInWindow;
        this->detectionEventCount--;
    }

    if (this->detectionEventCount < Watcher::MaxDetectionEventsInWindow)
    {
        unsigned int writeIndex = (this->detectionEventHead + this->detectionEventCount) % Watcher::MaxDetectionEventsInWindow;
        this->detectionEventTimesMs[writeIndex] = nowMs;
        this->detectionEventCount++;
    }
    else
    {
        this->detectionEventTimesMs[this->detectionEventHead] = nowMs;
        this->detectionEventHead = (this->detectionEventHead + 1) % Watcher::MaxDetectionEventsInWindow;
    }
}

void Watcher::EnterAlarm(unsigned long nowMs)
{
    this->state = WatcherState::Active;
    this->BeginMode(WatcherMode::Alarm, nowMs);
    this->alarmStartedAtMs = nowMs;
    this->alarmLastToggleAtMs = nowMs;
    this->alarmLastRadioAtMs = 0;
    this->alarmOutputOn = true;
    this->redPulseUntilAtMs = 0;
    this->greenBlinkUntilAtMs = 0;

    if (this->config.ledGreen != GPIO_NONE)
        this->greenLed.SetOff();

    if (this->config.ledRed != GPIO_NONE)
        this->redLed.SetSolidOn();
    else if (this->config.ledBlue != GPIO_NONE)
        this->blueLed.SetSolidOn();

#if BUZZER
    this->buzzer.SetSolidOn();
    this->buzzer.Update(nowMs);
#endif

    printf("[WATCHER][ALARM] entered (detections=%u within %ums)\n",
           this->detectionEventCount,
           this->config.alarmDetectionWindowMs);

    this->SendAlarmPacket(nowMs);
}

void Watcher::SendAlarmPacket(unsigned long nowMs)
{
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
        printf("[WATCHER][ALARM][PKG][ERROR] failed to format payload\n");
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
        printf("[WATCHER][ALARM][PKG][ERROR] encode failed payloadLen=%u\n", payloadLength);
        return;
    }

    printf("[WATCHER][ALARM][PKG] type=%u seq=%u bytes=%u payload=\"%.*s\"\n",
           (unsigned int)header.type,
           (unsigned int)header.sequence,
           (unsigned int)written,
           (int)payloadLength,
           AlarmPayloadBuffer);

    if (this->config.transceiver == nullptr)
    {
        printf("[WATCHER][ALARM][TX][WARN] transceiver not configured\n");
        return;
    }

    unsigned short packetLength = this->config.radioPacketLength;
    if (packetLength == 0 || packetLength > AlarmPacketLength)
        packetLength = AlarmPacketLength;

    if (!this->config.transceiver->Send(AlarmPacketBuffer, packetLength))
    {
        printf("[WATCHER][ALARM][TX][WARN] send failed len=%u\n", (unsigned int)packetLength);
        return;
    }

    this->alarmLastRadioAtMs = nowMs;

    printf("[WATCHER][ALARM][TX] sent len=%u seq=%u\n",
           (unsigned int)packetLength,
           (unsigned int)header.sequence);
}

void Watcher::SendStatusPacket(unsigned long nowMs)
{
    if (this->config.transceiver == nullptr)
    {
        printf("[WATCHER][STATUS][TX][WARN] transceiver not configured\n");
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
        printf("[WATCHER][STATUS][PKG][ERROR] failed to format payload\n");
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
        printf("[WATCHER][STATUS][PKG][ERROR] encode failed payloadLen=%u\n", payloadLength);
        return;
    }

    unsigned short packetLength = this->config.radioPacketLength;
    if (packetLength == 0 || packetLength > AlarmPacketLength)
        packetLength = AlarmPacketLength;

    if (!this->config.transceiver->Send(AlarmPacketBuffer, packetLength))
    {
        printf("[WATCHER][STATUS][TX][WARN] send failed len=%u\n", (unsigned int)packetLength);
        return;
    }

    this->statusLastRadioAtMs = nowMs;
    printf("[WATCHER][STATUS][TX] sent len=%u seq=%u\n",
           (unsigned int)packetLength,
           (unsigned int)header.sequence);
}

void Watcher::UpdateAlarm(unsigned long nowMs)
{
    if ((nowMs - this->alarmLastRadioAtMs) >= this->config.alarmRadioIntervalMs)
        this->SendAlarmPacket(nowMs);

    if ((nowMs - this->alarmStartedAtMs) >= this->config.alarmDurationMs)
    {
        if (this->config.ledRed != GPIO_NONE)
            this->redLed.SetOff();
        else if (this->config.ledBlue != GPIO_NONE)
            this->blueLed.SetOff();

#if BUZZER
        this->buzzer.SetOff();
        this->buzzer.Update(nowMs);
#endif

        this->state = WatcherState::Ready;
        this->BeginMode(this->config.startInReceiveAlarm ? WatcherMode::ReceiveAlarm : WatcherMode::AlarmSend,
                nowMs);
        this->lastDetectionAtMs = nowMs;
        this->detectionEventHead = 0;
        this->detectionEventCount = 0;
        this->detectionWindowLatched = false;
        this->SendStatusPacket(nowMs);
        printf("[WATCHER][ALARM] finished, back to ready/alarm-send\n");
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
        else if (this->config.ledBlue != GPIO_NONE)
        {
            if (this->alarmOutputOn)
                this->blueLed.SetSolidOn();
            else
                this->blueLed.SetOff();
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

bool Watcher::UpdateReceiveAlarm(unsigned long nowMs)
{
    if (this->config.transceiver == nullptr)
        return false;

    unsigned short packetLength = this->config.radioPacketLength;
    if (packetLength == 0 || packetLength > AlarmPacketLength)
        packetLength = AlarmPacketLength;

    int received = this->config.transceiver->Receive(AlarmPacketBuffer,
                                                     packetLength,
                                                     this->config.receiveTimeoutMs);
    if (received <= 0)
        return false;

    this->lastRxAtMs = nowMs;

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
        printf("[WATCHER][RX][WARN] invalid package bytes=%d\n", received);
        return false;
    }

    this->rxPackets++;

    if (header.type == PackageType::Alarm)
    {
        this->EnterAlarm(nowMs);
        printf("[WATCHER][RX] alarm seq=%u payloadLen=%u\n",
               (unsigned int)header.sequence,
               (unsigned int)payloadLength);
        return true;
    }

    if (header.type == PackageType::Status)
    {
        if (this->config.ledBlue != GPIO_NONE)
        {
            this->blueBlinkUntilAtMs = nowMs + this->config.statusBlueOnMs;
            this->blueLed.SetSolidOn();
        }

        printf("[WATCHER][RX] status seq=%u payloadLen=%u\n",
               (unsigned int)header.sequence,
               (unsigned int)payloadLength);
        return true;
    }

    printf("[WATCHER][RX] type=%u seq=%u payloadLen=%u\n",
           (unsigned int)header.type,
           (unsigned int)header.sequence,
           (unsigned int)payloadLength);
    return true;
}
