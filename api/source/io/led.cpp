#include "api/io/led.h"

#include "api/io/gpio.h"

void Led::Begin(GpioNum pin, bool activeLow)
{
    this->pin = pin;
    this->activeLow = activeLow;
    this->initialized = pin != GPIO_NONE;

    if (!this->initialized)
        return;

    Gpio::Mode(pin, GpioMode::GpioMode_Output);
    this->SetOff();
}

void Led::SetOff()
{
    if (this->mode == LedMode::Off && !this->outputOn)
        return;

    this->mode = LedMode::Off;
    this->outputOn = false;
    this->write(false);
}

void Led::SetSolidOn()
{
    if (this->mode == LedMode::SolidOn && this->outputOn)
        return;

    this->mode = LedMode::SolidOn;
    this->outputOn = true;
    this->write(true);
}

void Led::SetBlink(unsigned int intervalMs, unsigned int onMs, unsigned long nowMs)
{
    if (intervalMs == 0)
        intervalMs = 1000;

    if (onMs == 0 || onMs > intervalMs)
        onMs = intervalMs / 2;

    bool samePattern = this->mode == LedMode::Blink && this->blinkIntervalMs == intervalMs && this->blinkOnMs == onMs;
    this->blinkIntervalMs = intervalMs;
    this->blinkOnMs = onMs;

    if (samePattern)
        return;

    this->mode = LedMode::Blink;
    this->phaseStartedAtMs = nowMs;
    this->outputOn = true;
    this->write(true);
}

void Led::Update(unsigned long nowMs)
{
    if (!this->initialized)
        return;

    if (this->mode != LedMode::Blink)
        return;

    if (this->outputOn)
    {
        if ((nowMs - this->phaseStartedAtMs) >= this->blinkOnMs)
        {
            this->outputOn = false;
            this->phaseStartedAtMs = nowMs;
            this->write(false);
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
            this->write(true);
        }
    }
}

void Led::write(bool on)
{
    if (!this->initialized)
        return;

    bool level = this->activeLow ? !on : on;
    Gpio::Write(this->pin, level);
}
