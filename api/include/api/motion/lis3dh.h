#pragma once
#include "api/motion/accelerometer.h"
#include "api/io/i2c.h"
#include "api/common/types.h"
#include "api/system/interrupt.h"

enum Lis3dhSampleRate
{
    Lis3dhSampleRate_None = 0,
    Lis3dhSampleRate_1 = 0x10,
    Lis3dhSampleRate_10 = 0x20,
    Lis3dhSampleRate_25 = 0x48,
    Lis3dhSampleRate_50 = 0x40,
    Lis3dhSampleRate_100 = 0x50,
    Lis3dhSampleRate_200 = 0x60,
    Lis3dhSampleRate_400 = 0x70,
    Lis3dhSampleRate_1600_LOW_POWER = 0x80
};

enum Lis3dhAxis
{
    Lis3dhAxis_None,
    Lis3dhAxis_X = 0x1,
    Lis3dhAxis_Y = 0x2,
    Lis3dhAxis_Z = 0x4,
    Lis3dhAxis_All = Lis3dhAxis_X | Lis3dhAxis_Y | Lis3dhAxis_Z
};

enum Lis3dhMode
{
    Lis3dhMode_ByPass = 0x0,
    Lis3dhMode_Fifo = 0x40,
    Lis3dhMode_Stream = 0x80,
    Lis3dhMode_Stream_Fifo = 0xC0
};

enum Lis3dhThreshold
{
    Lis3dhThreshold_0 = 2,
    Lis3dhThreshold_1 = 3,
    Lis3dhThreshold_2 = 4,
    Lis3dhThreshold_3 = 8,
    Lis3dhThreshold_4 = 16,
    Lis3dhThreshold_5 = 32,
    Lis3dhThreshold_6 = 48,
    Lis3dhThreshold_7 = 64,
    Lis3dhThreshold_8 = 96,
    Lis3dhThreshold_9 = 127
};

struct Lis3dhConfig
{
    I2c *i2c = 0;
    Byte address = 0;
    Lis3dhMode mode = Lis3dhMode::Lis3dhMode_ByPass;
    Lis3dhSampleRate sampleRate = Lis3dhSampleRate::Lis3dhSampleRate_10;
    Lis3dhAxis axis = Lis3dhAxis::Lis3dhAxis_All;
    GpioNum interrupt = GPIO_NONE;
    Byte interruptDuration = 1;
    Lis3dhThreshold interruptThreshold = Lis3dhThreshold::Lis3dhThreshold_3;   
    Byte wakeupDuration = 2; 
    Lis3dhThreshold wakeupThreshold = Lis3dhThreshold::Lis3dhThreshold_5;
};

class Lis3dh : public Accelerometer, Interrupt
{
protected:
    Lis3dhConfig config;
    I2c *i2c;
    Acceleration previousAcceleration;
    bool triggered;

public:
    Lis3dh(const Lis3dhConfig *config);
    ~Lis3dh();

    void Read(Acceleration *acceleration);
    const char* Name();
    bool Interrupting(bool software = false);
    void Debug(); 
};