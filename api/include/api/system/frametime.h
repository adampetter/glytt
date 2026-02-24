#pragma once
#include "braver/common/datetime.h"

struct FrameTime{
    DateTime now;
    float elapsed = 0;
    int elapsedMs = 0;
    bool slow = false;
};