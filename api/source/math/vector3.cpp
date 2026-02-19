#include "api/math/vector3.h"

Vector3::Vector3() : Vector3(0, 0, 0)
{
}

Vector3::Vector3(float x, float y, float z)
{
    this->X = x;
    this->Y = y;
    this->Z = z;
};

Vector3::~Vector3()
{
}

bool Vector3::operator ==(const Vector3 &p)
{
    return this->X == p.X && this->Y == p.Y;
}

bool Vector3::operator !=(const Vector3 &p)
{
    return this->X != p.X || this->Y != p.Y;
}

float Vector3::Length()
{
    return sqrt(this->LengthSquared());
}

float Vector3::LengthSquared()
{
    return this->X * this->X + this->Y * this->Y + this->Z * this->Z;
}