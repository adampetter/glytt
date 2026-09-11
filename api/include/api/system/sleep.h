#pragma once

#include "api/common/types.h"

#include "esp_sleep.h"

enum class SleepMode
{
    Light = 0,
    Deep = 1
};

struct SleepConfig
{
    bool enabled = false;
    GpioNum wakePin = GPIO_NONE;
    bool wakeOnLow = true;
    SleepMode mode = SleepMode::Light;
};

class Sleep
{
private:
    SleepConfig config;
    esp_err_t lastResult = ESP_OK;

public:
    Sleep();
    Sleep(const SleepConfig &config);

    void Configure(const SleepConfig &config);
    SleepConfig Config() const;

    bool Ready() const;
    bool Begin();
    esp_sleep_wakeup_cause_t WakeupCause() const;
    esp_err_t LastResult() const;
    bool WokeFromConfiguredSource() const;
};
