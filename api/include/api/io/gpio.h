#pragma once

#include "api/common/types.h"
#include "driver/gpio.h"

enum GpioMode
{
    GpioMode_Disable = GPIO_MODE_DISABLE,
    GpioMode_Input = GPIO_MODE_INPUT,
    GpioMode_Output = GPIO_MODE_OUTPUT,
    GpioMode_Output_OpenDrain = GPIO_MODE_OUTPUT_OD,
    GpioMode_Input_Output_OpenDrain = GPIO_MODE_INPUT_OUTPUT_OD,
    GpioMode_Input_Output = GPIO_MODE_INPUT_OUTPUT
};

enum GpioPull
{
    GpioPull_Up = GPIO_PULLUP_ONLY,
    GpioPull_Down = GPIO_PULLDOWN_ONLY,
    GpioPull_Up_Down = GPIO_PULLUP_PULLDOWN,
    GpioPull_Float = GPIO_FLOATING,
};

class Gpio
{
public:
    static void Reset(GpioNum gpio);
    static void Mode(GpioNum gpio, GpioMode mode);
    static void Pull(GpioNum gpio, GpioPull pull);
    static bool Read(GpioNum gpio);
    static void Write(GpioNum gpio, bool value);
    static void Interupt(GpioNum gpio, bool highlow, void(*callback)());
};