#pragma once

#include "api/common/types.h"

enum class LedMode
{
    Off = 0,
    SolidOn = 1,
    Blink = 2
};

class Led
{
private:
    GpioNum pin = GPIO_NONE;
    bool activeLow = false;
    bool initialized = false;

    LedMode mode = LedMode::Off;
    unsigned int blinkIntervalMs = 1000;
    unsigned int blinkOnMs = 200;
    unsigned long phaseStartedAtMs = 0;
    bool outputOn = false;

    void write(bool on);

public:
    Led() = default;

    void Begin(GpioNum pin, bool activeLow);
    void SetOff();
    void SetSolidOn();
    void SetBlink(unsigned int intervalMs, unsigned int onMs, unsigned long nowMs);
    void Update(unsigned long nowMs);
};
