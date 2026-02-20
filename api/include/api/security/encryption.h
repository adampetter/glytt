#pragma once

#include "api/common/types.h"
#include <iostream>
#include <random>

class Encryption
{
protected:
    Encryption()
    {
    }

public:
    ~Encryption()
    {
    }

    static void Generate(Byte *array, unsigned short length)
    {
        std::random_device rd;
        std::mt19937 gen(rd());                      
        std::uniform_int_distribution<> distrib(0, 255);

        for (size_t i = 0; i < length; ++i)
            array[i] = static_cast<unsigned char>(distrib(gen));
    }

    virtual int Encrypt(Byte *data, int length, Byte *output) = 0;
    virtual int Decrypt(Byte *data, int length, Byte *output) = 0;
    virtual bool Ready() = 0;
};