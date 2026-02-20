#pragma once
#include "api/motion/acceleration.h"
#include "api/system/driver.h"

class Accelerometer : public Driver
{
public:
    Accelerometer(){};
    ~Accelerometer(){};

    virtual void Read(Acceleration *acceleration) = 0;
    virtual void Debug() = 0;
};