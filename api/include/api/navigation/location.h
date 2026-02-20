#pragma once

#include "api/common/types.h"
#include "api/common/datetime.h"
#include "api/math/vector2.h"
#include "api/navigation/coordinate.h"

struct Location{
    bool fix = false;
    Byte satellites = 0;
    Vector2 coordinate = Vector2::Zero();
    DateTime datetime = DateTime::Now();
    float altitude = 0.0f;
    float speed = 0.0f;
    double course = 0.0;
};