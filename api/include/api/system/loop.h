#pragma once

#include "api/common/types.h"
#include "api/common/time.h"
#include "api/system/frametime.h"

class Loop
{
protected:
    bool enabled;
    unsigned int target_ms;
    float target_sec;
    Byte rate;
    Timer timer;
    FrameTime frametime;
    long int duration;
    long int elapsed;

    Loop(Byte rate);

public:
    ~Loop();

    virtual void Run();
    virtual void Execute(const FrameTime &time) = 0;
    virtual void Terminate();
    unsigned int Target();
    Byte Rate();
    void Debug();
};