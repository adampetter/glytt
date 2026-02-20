#include "api/io/expander.h"
#include "api/io/i2c.h"

Expander::Expander(const MultiplexerConfig &config)
{
    this->config = config;
    this->i2c = config.i2c;
}

Expander::~Expander()
{
}

ExpanderState Expander::Read()
{
    Byte data = 0;
    if (this->i2c->Read(this->config.address.read, &data))
    {
        ExpanderState state;

        for (Byte i = 0; i < this->config.pins && i < BITS_PER_BYTE; i++)
            state.pins[i] = data & (1 << i);

        return state;
    }
    else
        return ExpanderState();
}

bool Expander::Read(GpioNum pin)
{
    ExpanderState state = this->Read();
    return state.pins[pin];
}

void Expander::Write(GpioNum pin, bool value)
{
    ExpanderState state = this->Read();
    state.pins[pin] = (bool)value;

    this->Write(state);
}

void Expander::Write(const ExpanderState &state)
{
    Byte data = 0;
    for (Byte i = 0; i < this->config.pins && i < BITS_PER_BYTE; i++)
        data |= state.pins[i] ? (1 << i) : 0;

    this->i2c->Write(this->config.address.write, &data, 1);
}

void Expander::Reset(bool level)
{
    ExpanderState state;
    for (Byte i = 0; i < EXPANDER_PIN_COUNT; i++)
        state.pins[i] = level;
        
    this->Write(state);
}

bool Expander::Interupt()
{
    return Gpio::Read(this->config.interupt);
}

void Expander::Debug()
{
    Byte data = 0;
    int count = 0;
    if ((count = this->i2c->Read(this->config.address.read, &data)) > 0)
    {
        printf("Expander(%#x/%#x): read %i bytes\n", this->config.address.read, this->config.address.write, count);
        ExpanderState state;

        for (Byte i = 0; i < this->config.pins && i < BITS_PER_BYTE; i++)
        {
            state.pins[i] = data & (1 << i);
            printf("P%i: %s\n", i, state.pins[i] ? "High" : "Low");
        }
    }
    else
    {
        printf("Expander(%#x/%#x): failed to read\n", this->config.address.read, this->config.address.write);
    }
}