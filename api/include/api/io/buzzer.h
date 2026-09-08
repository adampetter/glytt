#pragma once

#include "api/common/types.h"

enum class BuzzerMode
{
    Off = 0,
    SolidOn = 1,
    Blink = 2
};

class Buzzer
{
private:
    GpioNum pin = GPIO_NONE;
    bool activeLow = false;
    bool initialized = false;

    bool enabled = false;
    BuzzerMode mode = BuzzerMode::Off;
    unsigned int blinkIntervalMs = 1000;
    unsigned int blinkOnMs = 200;
    unsigned long phaseStartedAtMs = 0;
    bool outputOn = false;

    void write(bool on);

public:
    Buzzer() = default;

    void Begin(GpioNum pin, bool activeLow = false);
    void SetEnabled(bool enabled);
    bool Enabled() const;
    void SetOff();
    void SetSolidOn();
    void SetBlink(unsigned int intervalMs, unsigned int onMs, unsigned long nowMs);
    void Update(unsigned long nowMs);
};
