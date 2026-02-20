#pragma once
#include "api/common/types.h"
#include "api/system/driver.h"

class Transceiver
{
public:
    Transceiver() {

    };
    ~Transceiver() {

    };

    virtual int Receive(Byte *buffer, unsigned short length, unsigned int timeout) = 0;
    virtual bool Send(Byte *buffer, unsigned short length = 1) = 0;
    virtual void Debug() = 0;
};