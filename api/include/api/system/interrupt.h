#pragma once
#include "api/common/types.h"

enum InterruptType{
    InterruptType_Low = 0,
    InterruptType_High = 1
};

class Interrupt
{
protected:
    InterruptType interruptType;
    GpioNum interruptPin = GPIO_NONE;

public:
    Interrupt(GpioNum pin = GPIO_NONE, InterruptType type = InterruptType::InterruptType_Low)
    {
        this->interruptPin = pin;
        this->interruptType = type;

        if (pin != GPIO_NONE)
            Gpio::Mode(pin, GpioMode::GpioMode_Input);
    };
    ~Interrupt(){};

    bool Interrupting()
    {
        if (this->interruptPin == GPIO_NONE)
            return false;
        else
            return Gpio::Read(this->interruptPin) == (bool)this->interruptType;
    }

    bool Interruptable(){
        return this->interruptPin != GPIO_NONE;
    }
};