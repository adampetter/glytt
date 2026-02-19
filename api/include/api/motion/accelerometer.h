#pragma once
#include "api/motion/acceleration.h"

class Accelerometer
{
public:
    Accelerometer(){};
    ~Accelerometer(){};

    virtual void Read(Acceleration *acceleration) = 0;
    virtual void Debug(void) = 0;
};