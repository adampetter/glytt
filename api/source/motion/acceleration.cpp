#include "api/motion/acceleration.h"

#include <cmath>

const Acceleration Acceleration::Empty = {};

Acceleration::Acceleration()
{
    this->X = 0;
    this->Y = 0;
    this->Z = 0;
}

Acceleration::Acceleration(short x, short y, short z)
{
    this->X = x;
    this->Y = y;
    this->Z = z;
}

Acceleration::~Acceleration()
{
}

long long Acceleration::LengthSquared() const
{
    const long long x = static_cast<long long>(this->X);
    const long long y = static_cast<long long>(this->Y);
    const long long z = static_cast<long long>(this->Z);

    return x * x + y * y + z * z;
}

float Acceleration::Length() const
{
    return std::sqrt(static_cast<float>(this->LengthSquared()));
}

short Acceleration::Deadband(short value, short deadband)
{
    return (value >= -deadband && value <= deadband) ? 0 : value;
}

Acceleration Acceleration::Deadband(const Acceleration &value, short deadband)
{
    return Acceleration(
        Deadband(value.X, deadband),
        Deadband(value.Y, deadband),
        Deadband(value.Z, deadband));
}

Acceleration Acceleration::operator+(const Acceleration &a) const
{
    return Acceleration(this->X + a.X, this->Y + a.Y, this->Z + a.Z);
}

Acceleration Acceleration::operator-(const Acceleration &a) const
{
    return Acceleration(this->X - a.X, this->Y - a.Y, this->Z - a.Z);
}

Acceleration &Acceleration::operator+=(const Acceleration &a)
{
    this->X += a.X;
    this->Y += a.Y;
    this->Z += a.Z;

    return *this;
}

Acceleration &Acceleration::operator-=(const Acceleration &a)
{
    this->X -= a.X;
    this->Y -= a.Y;
    this->Z -= a.Z;

    return *this;
}

bool Acceleration::operator==(const Acceleration &a) const
{
    return this->X == a.X && this->Y == a.Y && this->Z == a.Z;
}

bool Acceleration::operator!=(const Acceleration &a) const
{
    return this->X != a.X || this->Y != a.Y || this->Z != a.Z;
}