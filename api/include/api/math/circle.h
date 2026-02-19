#pragma once

#include "api/math/point.h"

struct Circle{
    public:
        unsigned short Radius;
        Point Center;

        Circle();
        Circle(Point position, unsigned short radius);
        ~Circle();

        bool Intersect(Circle circle);
        bool Intersect(Point point);
};