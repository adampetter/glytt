#pragma once

#include "api/navigation/location.h"


class Gps
{
public:
    Gps();
    ~Gps();

    virtual void Read(Location* location) = 0;    
    virtual void Debug() = 0;
};