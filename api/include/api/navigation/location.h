#pragma once

#include "api/common/types.h"
#include "api/common/datetime.h"
#include "api/math/vector2.h"
#include "api/navigation/coordinate.h"

struct Location{
    bool fix;
    Byte satellites;
    Vector2 coordinate;
    DateTime datetime;
    float altitude;
    float speed;
    double course;
};