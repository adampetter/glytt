#include "api/io/buzzer.h"

#include "api/io/gpio.h"

void Buzzer::Begin(GpioNum pin, bool activeLow)
{
    this->pin = pin;
    this->activeLow = activeLow;
    this->initialized = pin != GPIO_NONE;

    if (!this->initialized)
        return;

    Gpio::Mode(pin, GpioMode::GpioMode_Output);
    this->write(false);
}

void Buzzer::SetEnabled(bool enabled)
{
    this->enabled = enabled;
}

bool Buzzer::Enabled() const
{
    return this->enabled;
}

void Buzzer::SetOff()
{
    this->mode = BuzzerMode::Off;
}

void Buzzer::SetSolidOn()
{
    this->mode = BuzzerMode::SolidOn;
}

void Buzzer::SetBlink(unsigned int intervalMs, unsigned int onMs, unsigned long nowMs)
{
    if (intervalMs == 0)
        intervalMs = 1000;

    if (onMs == 0 || onMs > intervalMs)
        onMs = intervalMs / 2;

    bool samePattern = this->mode == BuzzerMode::Blink && this->blinkIntervalMs == intervalMs && this->blinkOnMs == onMs;
    this->blinkIntervalMs = intervalMs;
    this->blinkOnMs = onMs;

    if (samePattern)
        return;

    this->mode = BuzzerMode::Blink;
    this->phaseStartedAtMs = nowMs;
    this->outputOn = true;
}

void Buzzer::Update(unsigned long nowMs)
{
    bool shouldBeOn = false;

    if (this->mode == BuzzerMode::SolidOn)
        shouldBeOn = true;
    else if (this->mode == BuzzerMode::Blink)
    {
        if (this->outputOn)
        {
            if ((nowMs - this->phaseStartedAtMs) >= this->blinkOnMs)
            {
                this->outputOn = false;
                this->phaseStartedAtMs = nowMs;
            }
        }
        else
        {
            unsigned int offMs = this->blinkIntervalMs - this->blinkOnMs;
            if (offMs == 0)
                offMs = this->blinkOnMs;

            if ((nowMs - this->phaseStartedAtMs) >= offMs)
            {
                this->outputOn = true;
                this->phaseStartedAtMs = nowMs;
            }
        }

        shouldBeOn = this->outputOn;
    }

    shouldBeOn = this->enabled && shouldBeOn;

    this->outputOn = shouldBeOn;
    this->write(shouldBeOn);
}

void Buzzer::write(bool on)
{
    if (!this->initialized)
        return;

    bool level = this->activeLow ? !on : on;
    Gpio::Write(this->pin, level);
}
