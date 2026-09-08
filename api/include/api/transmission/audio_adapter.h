#pragma once

#include "api/common/types.h"

class AudioAdapter
{
public:
    AudioAdapter() {}
    virtual ~AudioAdapter() {}

    virtual bool Start() = 0;
    virtual void Stop() = 0;

    virtual unsigned short Capture(Byte *payload,
                                   unsigned short payloadLength,
                                   Byte *codec,
                                   unsigned short *sampleRate) = 0;

    virtual bool Playback(const Byte *payload,
                          unsigned short payloadLength,
                          Byte codec,
                          unsigned short sampleRate,
                          unsigned short sequence) = 0;
};
