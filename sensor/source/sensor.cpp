#include "sensor.h"

#include "api/common/time.h"
#include "api/transmission/package.h"

#include "esp_sleep.h"

#include <cstdio>
#include <cstring>

Sensor::Sensor(const SensorConfig &config)
{
    this->config = config;

    if (this->config.radio.packetLength > TRANSCEIVER_PACKAGE_SIZE)
        this->config.radio.packetLength = TRANSCEIVER_PACKAGE_SIZE;

    if (this->config.radio.packetLength <= Package::HeaderSize)
        this->config.radio.packetLength = 64;

    if (this->config.radio.heartbeatIntervalMs == 0)
        this->config.radio.heartbeatIntervalMs = 15000;

    if (this->config.power.monitorSleepMs == 0)
        this->config.power.monitorSleepMs = 250;

    if (this->config.motion.requiredEvents == 0)
        this->config.motion.requiredEvents = 1;
}

bool Sensor::Start()
{
    if (this->config.radio.transceiver == nullptr || this->config.sensors.accelerometer == nullptr)
        return false;

    this->greenLed.Begin(this->config.pins.ledGreen, this->config.indicators.ledActiveLow);
    this->redLed.Begin(this->config.pins.ledRed, this->config.indicators.ledActiveLow);
    this->buzzer.Begin(this->config.pins.buzzer, false);
    this->buzzer.SetEnabled(this->config.indicators.buzzerEnabled);

    this->gpsAvailable = this->config.sensors.gps != nullptr;
    this->lastLocation = Location{};
    this->previousAcceleration = Acceleration::Empty;

    this->state = SensorState::ProbePeripherals;
    this->stats = SensorStats{};
    this->sequence = 0;
    this->motionEventsInWindow = 0;
    this->lastStatusAtMs = millis();
    this->startedAtMs = this->lastStatusAtMs;
    this->warningStartedAtMs = 0;
    this->warningBlinkUntilAtMs = 0;
    this->lastAlarmAtMs = 0;
    this->alarmStartedAtMs = 0;
    this->cooldownStartedAtMs = 0;
    this->firstMotionEventAtMs = 0;
    this->lastAmplitudeOnlyHitAtMs = 0;
    this->amplitudeOnlyHits = 0;
    this->lastMotionDeltaSquared = 0;
    this->warningConfidenceScore = 0;
    this->warningConfirmedEvents = 0;
    this->warningInterruptEvents = 0;
    this->warningStrongIrqEvents = 0;
    this->warningPeakDeltaSquared = 0;
    this->preWarningIrqHits = 0;
    this->preWarningFirstAtMs = 0;
    this->lastTelemetryAtMs = 0;
    this->warningHasInterruptEvidence = false;
    this->lastMotionEventHadInterrupt = false;

    // Prime acceleration baseline so the first delta does not spike on startup.
    this->config.sensors.accelerometer->Read(&this->previousAcceleration);
    this->config.sensors.accelerometer->Interrupting(true);

    printf("[SENSOR][INIT] started packet=%u airRate=%u deepSleep=%s buzzer=%s gps=%s id=%u\n",
           this->config.radio.packetLength,
           (unsigned int)this->config.radio.airDataRate,
           this->config.power.deepSleepPreferred ? "on" : "off",
           this->config.indicators.buzzerEnabled ? "on" : "off",
           this->gpsAvailable ? "on" : "off",
           this->config.privacy.pseudonymousDeviceId);
    printf("[SENSOR][LED] green=monitor (blink), red=alarm (blink) / fault (solid)\n");

    this->state = SensorState::ArmAndSleep;
    this->updateOutputs(millis());
    return true;
}

void Sensor::Tick()
{
    const unsigned short preWarningRequiredIrqHits = 2;
    const unsigned int preWarningWindowMs = 2600;
    const unsigned int preWarningMinGapMs = 80;
    const unsigned short alarmRequiredIrqEvents = 2;
    const unsigned short alarmRequiredStrongIrqEvents = 2;

    unsigned long nowMs = millis();

    this->pollIncomingFrames();

    if (this->preWarningFirstAtMs != 0 && (nowMs - this->preWarningFirstAtMs) > preWarningWindowMs)
    {
        this->preWarningIrqHits = 0;
        this->preWarningFirstAtMs = 0;
    }

    if (this->warningStartedAtMs != 0 && (nowMs - this->warningStartedAtMs) > this->config.alarm.warningWindowMs)
    {
        this->warningStartedAtMs = 0;
        this->warningConfidenceScore = 0;
        this->warningConfirmedEvents = 0;
        this->warningInterruptEvents = 0;
        this->warningStrongIrqEvents = 0;
        this->warningPeakDeltaSquared = 0;
        this->warningHasInterruptEvidence = false;
    }

    // If warning evidence is already strong enough, do not require another
    // post-delay impact event just to transition into alarm.
    if (this->state != SensorState::AlarmActive &&
        this->state != SensorState::AlarmCooldown &&
        this->warningStartedAtMs != 0)
    {
        bool warningDelayPassed = (nowMs - this->warningStartedAtMs) >= this->config.alarm.minSecondDetectionDelayMs;
        bool confidencePassed = this->warningConfidenceScore >= 3 && this->warningConfirmedEvents >= 2;
        bool secureEvidencePassed = this->warningHasInterruptEvidence &&
                                  this->warningInterruptEvents >= alarmRequiredIrqEvents &&
                                  this->warningStrongIrqEvents >= alarmRequiredStrongIrqEvents;

        if (warningDelayPassed && confidencePassed && secureEvidencePassed)
        {
            this->state = SensorState::AlarmActive;
            this->alarmStartedAtMs = nowMs;
            this->lastAlarmAtMs = 0;
            this->warningStartedAtMs = 0;
            this->warningBlinkUntilAtMs = 0;
            this->warningConfidenceScore = 0;
            this->warningConfirmedEvents = 0;
            this->warningInterruptEvents = 0;
            this->warningStrongIrqEvents = 0;
            this->warningPeakDeltaSquared = 0;
            this->warningHasInterruptEvidence = false;
            printf("[SENSOR][ALARM] delayed promotion from warning evidence (irqEvents>=%u strongIrq>=%u)\n",
                   (unsigned int)alarmRequiredIrqEvents,
                   (unsigned int)alarmRequiredStrongIrqEvents);
        }
    }

    if (this->state == SensorState::WarningActive)
    {
        if (nowMs >= this->warningBlinkUntilAtMs)
            this->state = SensorState::ArmAndSleep;
    }

    if (this->state == SensorState::ArmAndSleep)
    {
        this->updateOutputs(nowMs);
        this->applySleepInMonitor();
        this->stats.wakeEvents++;
        this->state = SensorState::WakeValidateMotion;
    }

    if (this->state == SensorState::WakeValidateMotion)
    {
        if (this->detectMotionEvent())
        {
            if (this->motionEventsInWindow == 0 || (nowMs - this->firstMotionEventAtMs) > this->config.motion.eventWindowMs)
            {
                this->firstMotionEventAtMs = nowMs;
                this->motionEventsInWindow = 1;
            }
            else
                this->motionEventsInWindow++;

            if (this->motionEventsInWindow >= this->config.motion.requiredEvents)
            {
                this->stats.wakeConfirmed++;
                bool hasWarningWindow = this->warningStartedAtMs != 0 && (nowMs - this->warningStartedAtMs) <= this->config.alarm.warningWindowMs;
                bool warningDelayPassed = this->warningStartedAtMs != 0 &&
                                          (nowMs - this->warningStartedAtMs) >= this->config.alarm.minSecondDetectionDelayMs;

                if (hasWarningWindow)
                {
                    this->preWarningIrqHits = 0;
                    this->preWarningFirstAtMs = 0;

                    if (this->lastMotionDeltaSquared > this->warningPeakDeltaSquared)
                        this->warningPeakDeltaSquared = this->lastMotionDeltaSquared;

                    unsigned int strongIrqThreshold = (this->config.motion.minDeltaSquared * 22U) / 10U;
                    if (strongIrqThreshold < 650U)
                        strongIrqThreshold = 650U;

                    if (this->lastMotionEventHadInterrupt)
                    {
                        this->warningHasInterruptEvidence = true;
                        if (this->warningInterruptEvents < 65535)
                            this->warningInterruptEvents++;

                        if (this->lastMotionDeltaSquared >= strongIrqThreshold && this->warningStrongIrqEvents < 65535)
                            this->warningStrongIrqEvents++;
                    }

                    unsigned short scoreAdd = this->lastMotionEventHadInterrupt ? 2 : 1;
                    unsigned int nextScore = (unsigned int)this->warningConfidenceScore + (unsigned int)scoreAdd;
                    this->warningConfidenceScore = (unsigned short)(nextScore > 65535U ? 65535U : nextScore);

                    if (this->warningConfirmedEvents < 65535)
                        this->warningConfirmedEvents++;

                    bool confidencePassed = this->warningConfidenceScore >= 3 && this->warningConfirmedEvents >= 2;
                    bool secureEvidencePassed = this->warningHasInterruptEvidence &&
                                              this->warningInterruptEvents >= alarmRequiredIrqEvents &&
                                              this->warningStrongIrqEvents >= alarmRequiredStrongIrqEvents;
                    if (warningDelayPassed && confidencePassed && secureEvidencePassed)
                    {
                        this->state = SensorState::AlarmActive;
                        this->alarmStartedAtMs = nowMs;
                        this->lastAlarmAtMs = 0;
                        this->warningStartedAtMs = 0;
                        this->warningBlinkUntilAtMs = 0;
                        this->warningConfidenceScore = 0;
                        this->warningConfirmedEvents = 0;
                        this->warningInterruptEvents = 0;
                        this->warningStrongIrqEvents = 0;
                        this->warningPeakDeltaSquared = 0;
                        this->warningHasInterruptEvidence = false;
                           printf("[SENSOR][ALARM] confidence gate passed in warning window (irqEvents>=%u strongIrq>=%u)\n",
                               (unsigned int)alarmRequiredIrqEvents,
                               (unsigned int)alarmRequiredStrongIrqEvents);
                    }
                    else
                    {
                        this->state = SensorState::ArmAndSleep;
                        printf("[SENSOR][WARN] holding: delay=%u/%u score=%u events=%u irq=%u irqSeen=%u irqEvents=%u strongIrq=%u peakD2=%u\n",
                               (unsigned int)(nowMs - this->warningStartedAtMs),
                               this->config.alarm.minSecondDetectionDelayMs,
                               this->warningConfidenceScore,
                               this->warningConfirmedEvents,
                               this->lastMotionEventHadInterrupt ? 1U : 0U,
                               this->warningHasInterruptEvidence ? 1U : 0U,
                               this->warningInterruptEvents,
                               this->warningStrongIrqEvents,
                               this->warningPeakDeltaSquared);
                    }
                }
                else
                {
                    unsigned int strongIrqThreshold = (this->config.motion.minDeltaSquared * 22U) / 10U;
                    if (strongIrqThreshold < 650U)
                        strongIrqThreshold = 650U;

                    bool isStrongIrq = this->lastMotionEventHadInterrupt && this->lastMotionDeltaSquared >= strongIrqThreshold;
                    if (isStrongIrq)
                    {
                        if (this->preWarningFirstAtMs == 0 || (nowMs - this->preWarningFirstAtMs) > preWarningWindowMs)
                        {
                            this->preWarningFirstAtMs = nowMs;
                            this->preWarningIrqHits = 1;
                        }
                        else if (this->preWarningIrqHits == 1 && (nowMs - this->preWarningFirstAtMs) < preWarningMinGapMs)
                        {
                            // Too close to the first hit, likely same vibration burst.
                            this->preWarningIrqHits = 1;
                        }
                        else if (this->preWarningIrqHits < 65535)
                            this->preWarningIrqHits++;
                    }
                    else
                    {
                        this->preWarningIrqHits = 0;
                        this->preWarningFirstAtMs = 0;
                    }

                    if (this->preWarningIrqHits < preWarningRequiredIrqHits)
                    {
                        this->state = SensorState::ArmAndSleep;
                        printf("[SENSOR][PREWARN] candidate %u/%u d2=%u strong=%u irq=%u\n",
                               this->preWarningIrqHits,
                               (unsigned int)preWarningRequiredIrqHits,
                               this->lastMotionDeltaSquared,
                               isStrongIrq ? 1U : 0U,
                               this->lastMotionEventHadInterrupt ? 1U : 0U);

                        this->motionEventsInWindow = 0;
                        this->updateOutputs(nowMs);
                        return;
                    }

                    this->state = SensorState::WarningActive;
                    this->warningStartedAtMs = nowMs;
                    this->warningBlinkUntilAtMs = nowMs + 350;
                    this->warningConfidenceScore = this->lastMotionEventHadInterrupt ? 2 : 1;
                    this->warningConfirmedEvents = 1;
                    this->warningPeakDeltaSquared = this->lastMotionDeltaSquared;

                    this->warningInterruptEvents = this->lastMotionEventHadInterrupt ? 1 : 0;
                    this->warningStrongIrqEvents = (this->lastMotionEventHadInterrupt && this->lastMotionDeltaSquared >= strongIrqThreshold) ? 1 : 0;
                    this->warningHasInterruptEvidence = this->lastMotionEventHadInterrupt;
                    this->preWarningIrqHits = 0;
                    this->preWarningFirstAtMs = 0;

                    if (this->sendWarningFrame(nowMs))
                        this->stats.warningTx++;
                    else
                        this->stats.warningTxErrors++;

                    printf("[SENSOR][WARN] first detection -> warning sent, waiting 10s for second detection\n");
                }

                this->motionEventsInWindow = 0;
                this->updateOutputs(nowMs);
            }
            else
                this->state = SensorState::ArmAndSleep;
        }
        else
        {
            this->stats.wakeRejected++;
            this->state = SensorState::ArmAndSleep;
        }
    }

    if (this->state == SensorState::AlarmActive)
    {
        if (this->lastAlarmAtMs == 0 || (nowMs - this->lastAlarmAtMs) >= this->config.alarm.intervalMs)
        {
            this->lastAlarmAtMs = nowMs;
            if (this->sendAlarmFrame(nowMs))
                this->stats.alarmTx++;
            else
                this->stats.alarmTxErrors++;
        }

        if ((nowMs - this->alarmStartedAtMs) >= this->config.alarm.timeoutMs)
        {
            this->state = SensorState::AlarmCooldown;
            this->cooldownStartedAtMs = nowMs;
            this->warningStartedAtMs = 0;
            this->warningBlinkUntilAtMs = 0;
            this->warningConfidenceScore = 0;
            this->warningConfirmedEvents = 0;
            this->warningInterruptEvents = 0;
            this->warningStrongIrqEvents = 0;
            this->warningPeakDeltaSquared = 0;
            this->warningHasInterruptEvidence = false;
            this->updateOutputs(nowMs);
            printf("[SENSOR][ALARM] timeout reached, cooldown\n");
        }
    }

    if (this->state == SensorState::AlarmCooldown)
    {
        if ((nowMs - this->cooldownStartedAtMs) >= this->config.alarm.cooldownMs)
        {
            this->state = SensorState::ArmAndSleep;
            this->updateOutputs(nowMs);
        }
    }

    if ((nowMs - this->lastStatusAtMs) >= this->config.radio.heartbeatIntervalMs)
    {
        this->lastStatusAtMs = nowMs;
        if (!this->sendStatusFrame(nowMs))
            this->stats.statusTxErrors++;
        else
            this->stats.statusTx++;

         printf("[SENSOR][STAT] state=%s statusTx=%u statusErr=%u warnTx=%u warnErr=%u alarmTx=%u alarmErr=%u\n",
               stateName(this->state),
               this->stats.statusTx,
               this->stats.statusTxErrors,
             this->stats.warningTx,
             this->stats.warningTxErrors,
               this->stats.alarmTx,
               this->stats.alarmTxErrors);
    }

    this->updateOutputs(nowMs);
}

void Sensor::Stop()
{
    this->state = SensorState::Boot;
    this->updateOutputs(millis());
}

SensorState Sensor::State() const
{
    return this->state;
}

SensorStats Sensor::Stats() const
{
    return this->stats;
}

bool Sensor::BuzzerEnabled() const
{
    return this->buzzer.Enabled();
}

void Sensor::SetBuzzerEnabled(bool enabled)
{
    this->config.indicators.buzzerEnabled = enabled;
    this->buzzer.SetEnabled(enabled);
    this->buzzer.Update(millis());
}

void Sensor::ToggleBuzzer()
{
    this->SetBuzzerEnabled(!this->buzzer.Enabled());
}

void Sensor::updateOutputs(unsigned long nowMs)
{
    unsigned int greenIntervalMs = this->config.indicators.greenBlinkIntervalMs;
    if (greenIntervalMs == 0)
        greenIntervalMs = 1200;

    unsigned int greenOnMs = this->config.indicators.greenBlinkOnMs;
    if (greenOnMs == 0 || greenOnMs > greenIntervalMs)
        greenOnMs = greenIntervalMs / 2;

    unsigned int redBlinkIntervalMs = this->config.indicators.redAlarmBlinkIntervalMs;
    if (redBlinkIntervalMs == 0)
        redBlinkIntervalMs = 400;

    unsigned int redOnMs = redBlinkIntervalMs / 2;
    if (redOnMs == 0)
        redOnMs = 1;

    if (this->state == SensorState::AlarmActive)
    {
        this->greenLed.SetOff();
        this->redLed.SetBlink(redBlinkIntervalMs, redOnMs, nowMs);
        this->buzzer.SetBlink(redBlinkIntervalMs, redOnMs, nowMs);
    }
    else if (this->state == SensorState::WarningActive)
    {
        this->greenLed.SetOff();
        this->redLed.SetBlink(300, 150, nowMs);
        this->buzzer.SetBlink(300, 150, nowMs);
    }
    else if (this->state == SensorState::Fault)
    {
        this->greenLed.SetOff();
        this->redLed.SetSolidOn();
        this->buzzer.SetOff();
    }
    else
    {
        this->greenLed.SetBlink(greenIntervalMs, greenOnMs, nowMs);
        this->redLed.SetOff();
        this->buzzer.SetOff();
    }

    this->greenLed.Update(nowMs);
    this->redLed.Update(nowMs);
    this->buzzer.Update(nowMs);
}

void Sensor::applySleepInMonitor()
{
    if (!this->config.power.deepSleepPreferred)
    {
        delay(this->config.power.monitorSleepMs);
        return;
    }

    if (this->config.pins.motionInterrupt != GPIO_NONE)
    {
        gpio_num_t wakeGpio = (gpio_num_t)this->config.pins.motionInterrupt;
        gpio_wakeup_enable(wakeGpio, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();
    }

    esp_sleep_enable_timer_wakeup((uint64_t)this->config.power.monitorSleepMs * 1000ULL);
    esp_light_sleep_start();
}

bool Sensor::detectMotionEvent()
{
    const unsigned short amplitudeOnlyRequiredHits = 2;
    const unsigned int amplitudeOnlyMaxGapMs = 450;
    const unsigned int burstWindowMs = 90;
    const unsigned int burstStepMs = 8;
    const unsigned short burstRequiredHits = 2;
    const unsigned int maxPlausibleDeltaSquared = 90000;
    const unsigned int telemetryIntervalMs = 20;

    unsigned long nowMs = millis();

    auto median3 = [](short a, short b, short c) {
        if ((a <= b && b <= c) || (c <= b && b <= a))
            return b;

        if ((b <= a && a <= c) || (c <= a && a <= b))
            return a;

        return c;
    };

    auto readStableAcceleration = [&]() {
        Acceleration s1;
        Acceleration s2;
        Acceleration s3;
        this->config.sensors.accelerometer->Read(&s1);
        this->config.sensors.accelerometer->Read(&s2);
        this->config.sensors.accelerometer->Read(&s3);

        return Acceleration(
            median3(s1.X, s2.X, s3.X),
            median3(s1.Y, s2.Y, s3.Y),
            median3(s1.Z, s2.Z, s3.Z));
    };

    if ((nowMs - this->startedAtMs) < this->config.motion.startupGraceMs)
    {
        // During startup grace we continuously refresh baseline and clear any latched interrupt.
        this->config.sensors.accelerometer->Interrupting(true);
        this->previousAcceleration = readStableAcceleration();
        this->lastAmplitudeOnlyHitAtMs = 0;
        this->amplitudeOnlyHits = 0;
        this->lastMotionDeltaSquared = 0;
        this->lastMotionEventHadInterrupt = false;
        return false;
    }

    bool interruptLatched = this->config.sensors.accelerometer->Interrupting(true);

    auto validateBurst = [&](unsigned int threshold, bool irqPath) {
        if (threshold == 0)
            threshold = 1;

        unsigned long burstStartMs = millis();
        unsigned short hits = 0;
        unsigned int peak = this->lastMotionDeltaSquared;
        Acceleration previous = this->previousAcceleration;

        // Include the initial candidate sample in the burst decision.
        if (this->lastMotionDeltaSquared >= threshold)
            hits = 1;

        while ((millis() - burstStartMs) < burstWindowMs)
        {
            delay(burstStepMs);

            Acceleration sample = readStableAcceleration();
            Acceleration sampleDelta = sample - previous;
            previous = sample;

            sampleDelta = Acceleration::Deadband(sampleDelta, (short)this->config.motion.deadband);
            unsigned int sampleDeltaSquared = sampleDelta.LengthSquared();

            // Reject physically implausible spikes that are usually bus/noise glitches.
            if (sampleDeltaSquared > maxPlausibleDeltaSquared)
                continue;

            if (sampleDeltaSquared > peak)
                peak = sampleDeltaSquared;

            if (sampleDeltaSquared >= threshold && hits < 65535)
                hits++;
        }

        this->previousAcceleration = previous;
        this->lastMotionDeltaSquared = peak;

        if (hits < burstRequiredHits)
        {
            printf("[SENSOR][MOTION] burst reject irq=%u hits=%u/%u peakD2=%u thr=%u\n",
                   irqPath ? 1U : 0U,
                   (unsigned int)hits,
                   (unsigned int)burstRequiredHits,
                   peak,
                   threshold);
            return false;
        }

        return true;
    };

    Acceleration current = readStableAcceleration();
    Acceleration delta = current - this->previousAcceleration;
    this->previousAcceleration = current;

    delta = Acceleration::Deadband(delta, (short)this->config.motion.deadband);
    unsigned int minDeltaSquared = this->config.motion.minDeltaSquared;
    if (interruptLatched)
    {
        // Keep value-validation mandatory, but allow slightly higher sensitivity on confirmed IRQ events.
        unsigned int interruptAdjusted = (this->config.motion.minDeltaSquared * 30U) / 100U;
        if (interruptAdjusted < 700U)
            interruptAdjusted = 700U;
        minDeltaSquared = interruptAdjusted == 0 ? 1 : interruptAdjusted;
    }

    unsigned int deltaSquared = delta.LengthSquared();

    if (deltaSquared > maxPlausibleDeltaSquared)
    {
        this->lastAmplitudeOnlyHitAtMs = 0;
        this->amplitudeOnlyHits = 0;
        this->lastMotionDeltaSquared = 0;
        this->lastMotionEventHadInterrupt = false;
        printf("[SENSOR][MOTION] glitch reject irq=%u d2=%u\n", interruptLatched ? 1U : 0U, deltaSquared);
        return false;
    }

    this->lastMotionDeltaSquared = deltaSquared;

    if (this->lastTelemetryAtMs == 0 || (nowMs - this->lastTelemetryAtMs) >= telemetryIntervalMs)
    {
        this->lastTelemetryAtMs = nowMs;
        printf("[SENSOR][ACC] t=%lu irq=%u ax=%d ay=%d az=%d dx=%d dy=%d dz=%d d2=%u\n",
               nowMs,
               interruptLatched ? 1U : 0U,
               current.X,
               current.Y,
               current.Z,
               delta.X,
               delta.Y,
               delta.Z,
               deltaSquared);
    }

    if (deltaSquared < minDeltaSquared)
    {
        this->lastAmplitudeOnlyHitAtMs = 0;
        this->amplitudeOnlyHits = 0;
        this->lastMotionEventHadInterrupt = false;
        return false;
    }

    if (interruptLatched)
    {
        if (!validateBurst(minDeltaSquared, true))
        {
            this->lastAmplitudeOnlyHitAtMs = 0;
            this->amplitudeOnlyHits = 0;
            this->lastMotionEventHadInterrupt = false;
            return false;
        }

        this->lastAmplitudeOnlyHitAtMs = 0;
        this->amplitudeOnlyHits = 0;
        this->lastMotionEventHadInterrupt = true;
        return true;
    }

    // If dedicated interrupt wiring is present, require IRQ evidence.
    // This avoids periodic amplitude-only noise causing false warnings while idle.
    if (this->config.pins.motionInterrupt != GPIO_NONE)
    {
        this->lastAmplitudeOnlyHitAtMs = 0;
        this->amplitudeOnlyHits = 0;
        this->lastMotionEventHadInterrupt = false;
        return false;
    }

    // Without IRQ we require a clearly stronger signal across several reads
    // to avoid stationary jitter becoming warnings.
    unsigned int amplitudeOnlyMinDeltaSquared = this->config.motion.minDeltaSquared;
    if (deltaSquared < amplitudeOnlyMinDeltaSquared)
    {
        this->lastAmplitudeOnlyHitAtMs = 0;
        this->amplitudeOnlyHits = 0;
        this->lastMotionEventHadInterrupt = false;
        return false;
    }

    if (this->lastAmplitudeOnlyHitAtMs == 0 || (nowMs - this->lastAmplitudeOnlyHitAtMs) > amplitudeOnlyMaxGapMs)
        this->amplitudeOnlyHits = 1;
    else if (this->amplitudeOnlyHits < 255)
        this->amplitudeOnlyHits++;

    this->lastAmplitudeOnlyHitAtMs = nowMs;

    if (this->amplitudeOnlyHits < amplitudeOnlyRequiredHits)
    {
        this->lastMotionEventHadInterrupt = false;
        return false;
    }

    this->lastAmplitudeOnlyHitAtMs = 0;
    this->amplitudeOnlyHits = 0;

    printf("[SENSOR][MOTION] amplitude-only confirmed (%u samples), delta2=%u\n",
           (unsigned int)amplitudeOnlyRequiredHits,
           deltaSquared);

    this->lastMotionEventHadInterrupt = false;

    return true;
}

void Sensor::pollIncomingFrames()
{
    int count = this->config.radio.transceiver->Receive(this->rxBuffer,
                                                        this->config.radio.packetLength,
                                                        this->config.radio.receiveTimeoutMs);

    if (count != this->config.radio.packetLength)
    {
        if (count <= 0)
            this->stats.rxTimeouts++;

        return;
    }

    PackageHeader header;
    unsigned short payloadLength = 0;
    if (!Package::Decode(this->rxBuffer,
                         this->config.radio.packetLength,
                         &header,
                         this->payloadBuffer,
                         sizeof(this->payloadBuffer),
                         &payloadLength))
    {
        this->stats.rxInvalid++;
        return;
    }

    if (header.type == PackageType::Ack)
    {
        this->stats.ackRx++;
        printf("[SENSOR][RX] ack seq=%u\n", header.sequence);
    }
    else if (header.type == PackageType::Status)
        this->stats.statusRx++;
}

bool Sensor::sendAlarmFrame(unsigned long nowMs)
{
    if (this->gpsAvailable)
    {
        this->config.sensors.gps->Read(&this->lastLocation);
        this->stats.gpsReads++;
        if (this->lastLocation.fix)
            this->stats.gpsFixes++;
    }

    memset(this->payloadBuffer, 0, sizeof(this->payloadBuffer));
    if (this->gpsAvailable && this->lastLocation.fix)
    {
        long latE5 = (long)(this->lastLocation.coordinate.X * 100000.0f);
        long lonE5 = (long)(this->lastLocation.coordinate.Y * 100000.0f);

        snprintf((char *)this->payloadBuffer,
                 sizeof(this->payloadBuffer),
                 "ev:%lu id:%u gps:1 latE5:%ld lonE5:%ld sat:%u",
                 nowMs,
                 this->config.privacy.pseudonymousDeviceId,
                 latE5,
                 lonE5,
                 this->lastLocation.satellites);
    }
    else
    {
        snprintf((char *)this->payloadBuffer,
                 sizeof(this->payloadBuffer),
                 "ev:%lu id:%u gps:0",
                 nowMs,
                 this->config.privacy.pseudonymousDeviceId);
    }

    return this->transmitFrame(PackageType::Alarm, (const char *)this->payloadBuffer);
}

bool Sensor::sendWarningFrame(unsigned long nowMs)
{
    memset(this->payloadBuffer, 0, sizeof(this->payloadBuffer));
    snprintf((char *)this->payloadBuffer,
             sizeof(this->payloadBuffer),
             "warn:%lu id:%u win:%u",
             nowMs,
             this->config.privacy.pseudonymousDeviceId,
             this->config.alarm.warningWindowMs);

    return this->transmitFrame(PackageType::Control, (const char *)this->payloadBuffer);
}

bool Sensor::sendStatusFrame(unsigned long nowMs)
{
    memset(this->payloadBuffer, 0, sizeof(this->payloadBuffer));
    snprintf((char *)this->payloadBuffer,
             sizeof(this->payloadBuffer),
             "hb:%lu st:%u bz:%u id:%u gps:%u",
             nowMs,
             (unsigned int)this->state,
             this->config.indicators.buzzerEnabled ? 1U : 0U,
             this->config.privacy.pseudonymousDeviceId,
             this->gpsAvailable ? 1U : 0U);

    return this->transmitFrame(PackageType::Status, (const char *)this->payloadBuffer);
}

bool Sensor::transmitFrame(PackageType type, const char *payloadText)
{
    if (payloadText == nullptr)
        return false;

    memset(this->txBuffer, 0, this->config.radio.packetLength);

    unsigned short maxPayloadLength = Package::MaxPayloadLength(this->config.radio.packetLength);
    unsigned short payloadLength = (unsigned short)strlen(payloadText);
    if (payloadLength > maxPayloadLength)
        payloadLength = maxPayloadLength;

    PackageHeader header;
    header.type = type;
    header.sequence = this->sequence++;

    unsigned short written = 0;
    if (!Package::Encode(header,
                         (const Byte *)payloadText,
                         payloadLength,
                         this->txBuffer,
                         this->config.radio.packetLength,
                         &written))
        return false;

    printf("[SENSOR][TX] type=%u seq=%u payloadLen=%u payload='%.*s'\n",
           (unsigned int)type,
           (unsigned int)header.sequence,
           (unsigned int)payloadLength,
           (int)payloadLength,
           payloadText);

    return this->config.radio.transceiver->Send(this->txBuffer, this->config.radio.packetLength);
}

const char *Sensor::stateName(SensorState state)
{
    switch (state)
    {
    case SensorState::Boot:
        return "boot";
    case SensorState::ProbePeripherals:
        return "probe";
    case SensorState::ArmAndSleep:
        return "arm-sleep";
    case SensorState::WakeValidateMotion:
        return "wake-validate";
    case SensorState::WarningActive:
        return "warning";
    case SensorState::AlarmActive:
        return "alarm";
    case SensorState::AlarmCooldown:
        return "cooldown";
    case SensorState::Fault:
        return "fault";
    default:
        return "unknown";
    }
}
