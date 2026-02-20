#pragma once

#include "api/navigation/location.h"
#include "api/system/driver.h"

class Gps : public Driver
{
public:
    Gps();
    ~Gps();

    virtual void Read(Location* location) = 0;    
    virtual void Debug() = 0;
};