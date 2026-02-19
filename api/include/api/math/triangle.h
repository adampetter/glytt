#pragma once

#include "api/math/point.h"

struct Triangle
{
public:
    Point P1;
    Point P2;
    Point P3;

    Triangle(){

    }

    Triangle(const Point& p1, const Point& p2, const Point& p3){
        this->P1 = p1;
        this->P2 = p2;
        this->P3 = p3;
    }

    ~Triangle(){
        
    }
};