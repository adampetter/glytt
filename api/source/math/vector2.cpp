#include "api/math/vector2.h"
#include "api/math/vector2.h"
#include <math.h>

Vector2::Vector2() : X(0.0f), Y(0.0f)
{
}

Vector2::Vector2(const float x, const float y) : X(x), Y(y)
{
}

Vector2 Vector2::operator+(const Vector2 &v) const
{
    return Vector2(this->X + v.X, this->Y + v.Y);
}

Vector2 Vector2::operator-(const Vector2 &v) const
{
    return Vector2(this->X - v.X, this->Y - v.Y);
}

Vector2 Vector2::operator*(const float scalar) const
{
    return Vector2(this->X * scalar, this->Y * scalar);
}

Vector2 Vector2::operator/(const float scalar) const
{
    return Vector2(this->X / scalar, this->Y / scalar);
}

void Vector2::operator+=(const Vector2 &v)
{
    this->X += v.X;
    this->Y += v.Y;
}

void Vector2::operator-=(const Vector2 &v)
{
    this->X -= v.X;
    this->Y -= v.Y;
}

void Vector2::operator*=(const float scalar)
{
    this->X *= scalar;
    this->Y *= scalar;
}

bool Vector2::operator==(const Vector2 &v) const
{
    return this->X == v.X && this->Y == v.Y;
}

bool Vector2::operator!=(const Vector2 &v) const
{
    return this->X != v.X || this->Y != v.Y;
}

Vector2 operator*(const float scalar, const Vector2 &vector)
{
    return Vector2(scalar * vector.X, scalar * vector.Y);
}

Vector2 operator/(const Vector2 &vector, const float scalar)
{
    return Vector2(vector.X / scalar, vector.Y / scalar);
}

float Vector2::Length() const
{
    return std::sqrt(this->X * this->X + this->Y * this->Y);
}

float Vector2::FastLength() const
{
    return fastsqrt(this->X * this->X + this->Y * this->Y);
}

float Vector2::LengthSquared() const
{
    return this->X * this->X + this->Y * this->Y;
}

void Vector2::Normalize()
{
    float length = Length();
    if (length > 0.0f)
    {
        this->X /= length;
        this->Y /= length;
    }
}

void Vector2::Rotate(const float angle)
{
    float cosAngle = std::cos(angle);
    float sinAngle = std::sin(angle);
    float newX = this->X * cosAngle - this->Y * sinAngle;
    float newY = this->X * sinAngle + this->Y * cosAngle;
    this->X = newX;
    this->Y = newY;
}

void Vector2::Clamp(const Vector2 &min, const Vector2 &max)
{
    this->X = std::max(min.X, std::min(max.X, X));
    this->Y = std::max(min.Y, std::min(max.Y, Y));
}

Vector2 Vector2::Perpendicular() const
{
    return Vector2(-Y, X);
}

void Vector2::Negate()
{
    this->X = -this->X;
    this->Y = -this->Y;
}

bool Vector2::IsZero() const
{
    return this->X == 0.0f && this->Y == 0.0f;
}

bool Vector2::IsUnit() const
{
    return std::abs(Length() - 1.0f) < 0.0001f;
}

float Vector2::Dot(const Vector2 &vector1, const Vector2 &vector2)
{
    return vector1.X * vector2.X + vector1.Y * vector2.Y;
}

float Vector2::Distance(const Vector2 &vector1, const Vector2 &vector2)
{
    float dx = vector2.X - vector1.X;
    float dy = vector2.Y - vector1.Y;
    return std::sqrt(dx * dx + dy * dy);
}

float Vector2::DistanceSquared(const Vector2 &vector1, const Vector2 &vector2)
{
    float dx = vector2.X - vector1.X;
    float dy = vector2.Y - vector1.Y;
    return dx * dx + dy * dy;
}

float Vector2::Angle(const Vector2 &vector1, const Vector2 &vector2)
{
    float dot = Dot(vector1, vector2);
    float len1 = vector1.Length();
    float len2 = vector2.Length();
    return std::acos(dot / (len1 * len2));
}

Vector2 Vector2::Lerp(const Vector2 &vector1, const Vector2 &vector2, float amount)
{
    return Vector2(
        vector1.X + (vector2.X - vector1.X) * amount,
        vector1.Y + (vector2.Y - vector1.Y) * amount);
}

Vector2 Vector2::Reflect(const Vector2 &vector, const Vector2 &normal)
{
    float dot = Dot(vector, normal);
    return vector - (2 * dot * normal);
}

float Vector2::Cross(const Vector2 &vector1, const Vector2 &vector2)
{
    return vector1.X * vector2.Y - vector1.Y * vector2.X;
}

Vector2 Vector2::Zero()
{
    return Vector2(0.0f, 0.0f);
}

Vector2 Vector2::One()
{
    return Vector2(1.0f, 1.0f);
}