#pragma once
#include "api/math/helper.h"
#include <cmath>

struct Vector2
{
    float X;
    float Y;

    Vector2();
    Vector2(const float x, const float y);

    Vector2 operator+(const Vector2 &v) const;
    Vector2 operator-(const Vector2 &v) const;
    Vector2 operator*(const float scalar) const;
    Vector2 operator/(const float scalar) const;

    void operator+=(const Vector2 &v);
    void operator-=(const Vector2 &v);
    void operator*=(const float scalar);
    void operator/=(const float scalar);

    bool operator==(const Vector2 &v) const;
    bool operator!=(const Vector2 &v) const;

    float Length() const;
    float FastLength() const;
    float LengthSquared() const;
    void Normalize();
    void Rotate(const float angle);
    void Clamp(const Vector2 &min, const Vector2 &max);
    Vector2 Perpendicular() const;
    void Negate();
    bool IsZero() const;
    bool IsUnit() const;

    static float Dot(const Vector2 &vector1, const Vector2 &vector2);
    static float Distance(const Vector2 &vector1, const Vector2 &vector2);
    static float DistanceSquared(const Vector2 &vector1, const Vector2 &vector2);
    static float Angle(const Vector2 &vector1, const Vector2 &vector2);
    static Vector2 Lerp(const Vector2 &vector1, const Vector2 &vector2, float amount);
    static Vector2 Reflect(const Vector2 &vector, const Vector2 &normal);
    static float Cross(const Vector2 &vector1, const Vector2 &vector2);
    static Vector2 Zero();
    static Vector2 One();
};

Vector2 operator*(const float scalar, const Vector2 &vector);
Vector2 operator/(const Vector2 &vector, const float scalar);
