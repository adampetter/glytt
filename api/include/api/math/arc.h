#pragma once
#include "api/math/point.h"

enum ArcDirection{
    ArcDirection_TopLeft,
    ArcDirection_TopRight,
    ArcDirection_BottomRight,
    ArcDirection_BottomLeft
};

struct Arc{
    public:
        unsigned short Radius;
        Point Center;
        ArcDirection Direction;

        Arc();
        Arc(Point center, unsigned short radius, ArcDirection direction = ArcDirection::ArcDirection_TopLeft);
        ~Arc();
};