#pragma once
#include <cmath>

struct Vector3
{
    float X;
    float Y;
    float Z;

    Vector3();
    Vector3(float x, float y, float z);
    ~Vector3();

    bool operator==(const Vector3 &p);
    bool operator!=(const Vector3 &p);

    float Length();
    float LengthSquared();
};
