#include "api/system/sleep.h"

#include <cstdint>

#include "driver/gpio.h"
#include "esp_rom_sys.h"

#ifndef SLEEP_TEMP_BYPASS_LIGHT_SLEEP_ON_USB
#define SLEEP_TEMP_BYPASS_LIGHT_SLEEP_ON_USB 0
#endif

Sleep::Sleep()
    : config()
{
}

Sleep::Sleep(const SleepConfig &config)
    : config(config)
{
}

void Sleep::Configure(const SleepConfig &config)
{
    this->config = config;
}

SleepConfig Sleep::Config() const
{
    return this->config;
}

bool Sleep::Ready() const
{
    if (!this->config.enabled)
        return true;

    if (this->config.wakePin == GPIO_NONE)
        return false;

    gpio_num_t wakePin = (gpio_num_t)this->config.wakePin;

    if (this->config.mode == SleepMode::Deep)
        return esp_sleep_is_valid_wakeup_gpio(wakePin);

    return GPIO_IS_VALID_GPIO(wakePin);
}

bool Sleep::Begin()
{
    if (!this->config.enabled)
    {
        this->lastResult = ESP_ERR_INVALID_STATE;
        return false;
    }

    if (!this->Ready())
    {
        this->lastResult = ESP_ERR_INVALID_ARG;
        return false;
    }

    if (esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL) != ESP_OK)
    {
        this->lastResult = ESP_FAIL;
        return false;
    }

    gpio_num_t wakePin = (gpio_num_t)this->config.wakePin;
    gpio_config_t wakeConfig = {};
    wakeConfig.pin_bit_mask = 1ULL << (uint64_t)this->config.wakePin;
    wakeConfig.mode = GPIO_MODE_INPUT;
    wakeConfig.intr_type = GPIO_INTR_DISABLE;
    wakeConfig.pull_up_en = this->config.wakeOnLow ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    wakeConfig.pull_down_en = this->config.wakeOnLow ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE;

    if (gpio_config(&wakeConfig) != ESP_OK)
    {
        this->lastResult = ESP_FAIL;
        return false;
    }

    if (this->config.mode == SleepMode::Deep)
    {
        uint64_t wakeMask = 1ULL << (uint64_t)this->config.wakePin;
        esp_sleep_ext1_wakeup_mode_t deepWakeMode = this->config.wakeOnLow
                                                         ? ESP_EXT1_WAKEUP_ANY_LOW
                                                         : ESP_EXT1_WAKEUP_ANY_HIGH;

        if (esp_sleep_enable_ext1_wakeup_io(wakeMask, deepWakeMode) != ESP_OK)
        {
            this->lastResult = ESP_FAIL;
            return false;
        }

        esp_rom_printf("[SLEEP][ROM] entering deep sleep\n");
        esp_deep_sleep_start();
        this->lastResult = ESP_OK;
        return false;
    }

    gpio_int_type_t lightLevel = this->config.wakeOnLow ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL;
    if (gpio_wakeup_enable(wakePin, lightLevel) != ESP_OK)
    {
        this->lastResult = ESP_FAIL;
        return false;
    }

    if (esp_sleep_enable_gpio_wakeup() != ESP_OK)
    {
        this->lastResult = ESP_FAIL;
        return false;
    }

    // Fallback wake to avoid getting stuck in light sleep if GPIO wake is missed.
    // GPIO wake still has priority and will wake earlier when motion IRQ arrives.
    static const uint64_t lightSleepFallbackWakeUs = 2000000ULL;
    if (esp_sleep_enable_timer_wakeup(lightSleepFallbackWakeUs) != ESP_OK)
    {
        this->lastResult = ESP_FAIL;
        return false;
    }

#if SLEEP_TEMP_BYPASS_LIGHT_SLEEP_ON_USB && (CONFIG_USJ_ENABLE_USB_SERIAL_JTAG || CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED || CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG)
    esp_rom_printf("[SLEEP][ROM] bypass light sleep while USB Serial/JTAG console is active\n");
    this->lastResult = ESP_ERR_NOT_SUPPORTED;
    return false;
#endif

    esp_rom_printf("[SLEEP][ROM] entering light sleep (pin=%d level=%s)\n",
                   (int)this->config.wakePin,
                   this->config.wakeOnLow ? "LOW" : "HIGH");
    this->lastResult = esp_light_sleep_start();
    esp_rom_printf("[SLEEP][ROM] light sleep returned err=%d cause=%d\n",
                   (int)this->lastResult,
                   (int)this->WakeupCause());

    return this->lastResult == ESP_OK;
}

esp_sleep_wakeup_cause_t Sleep::WakeupCause() const
{
    return esp_sleep_get_wakeup_cause();
}

esp_err_t Sleep::LastResult() const
{
    return this->lastResult;
}

bool Sleep::WokeFromConfiguredSource() const
{
    if (!this->config.enabled || this->config.wakePin == GPIO_NONE)
        return false;

    uint64_t wakeMask = 1ULL << (uint64_t)this->config.wakePin;
    esp_sleep_wakeup_cause_t cause = this->WakeupCause();

    if (cause == ESP_SLEEP_WAKEUP_GPIO)
        return true;

    if (cause == ESP_SLEEP_WAKEUP_EXT1)
    {
        uint64_t status = esp_sleep_get_ext1_wakeup_status();
        return (status & wakeMask) != 0;
    }

    return false;
}
