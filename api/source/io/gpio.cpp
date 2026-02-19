#include "api/io/gpio.h"
#include "api/common/types.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc.h"

static bool IsValidPin(GpioNum gpio)
{
    if (gpio == GPIO_NONE)
        return false;

    const int pin = static_cast<int>(gpio);
    return GPIO_IS_VALID_GPIO((gpio_num_t)pin);
}

void Gpio::Reset(GpioNum gpio)
{
    if (!IsValidPin(gpio))
        return;

    gpio_reset_pin((gpio_num_t)gpio);
}

void Gpio::Mode(GpioNum gpio, GpioMode mode)
{
    if (!IsValidPin(gpio))
        return;

    Gpio::Reset(gpio);
    gpio_set_direction((gpio_num_t)gpio, (gpio_mode_t)mode);
}

void Gpio::Pull(GpioNum gpio, GpioPull pull)
{
    if (!IsValidPin(gpio))
        return;

    gpio_set_pull_mode((gpio_num_t)gpio, (gpio_pull_mode_t)pull);
}

bool Gpio::Read(GpioNum gpio)
{
    if (!IsValidPin(gpio))
        return false;

    return gpio_get_level((gpio_num_t)gpio);
}

void Gpio::Write(GpioNum gpio, bool value)
{
    if (!IsValidPin(gpio))
        return;

    gpio_set_level((gpio_num_t)gpio, value);
}

void Gpio::Interupt(GpioNum gpio, bool highlow, void(*callback)()){
    (void)gpio;
    (void)highlow;
    (void)callback;
}