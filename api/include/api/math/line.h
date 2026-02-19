#pragma once
#include "api/math/point.h"

struct Line
{
public:
    Point Start;
    Point End;

    Line(){

    }

    Line(const Point &start, const Point &end)
    {
        this->Start = start;
        this->End = end;
    }
    
    ~Line(){

    }
};
