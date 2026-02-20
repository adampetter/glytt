#pragma once
#include "api/common/types.h"

#define EXPANDER_PIN_COUNT 8

/*
    Designed for the TI PCF8574 expander.
*/

class I2c;

struct ExpanderState
{
    bool pins[8] = {false};
};

struct MultiplexerConfig
{
    I2c *i2c = NULL;
    struct Address{
        Byte read = 0;
        Byte write = 0;
    } address;

    GpioNum interupt = GPIO_NONE;
    Byte pins = 8;
};

class Expander
{
protected:
    I2c *i2c = NULL;
    MultiplexerConfig config;

public:
    Expander(const MultiplexerConfig& config);
    ~Expander();

    ExpanderState Read();
    bool Read(GpioNum pin);
    void Write(GpioNum pin, bool value);
    void Write(const ExpanderState& state);
    void Reset(bool level);
    bool Interupt();
    void Debug();
};