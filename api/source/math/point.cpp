#include "api/math/point.h"
#include "api/math/helper.h"
#include "api/io/gpio.h"
#include <math.h>

Point::Point() : Point(0, 0)
{
}

Point::Point(const Point& source){
    this->X = source.X;
    this->Y = source.Y;
}

Point::Point(int x, int y)
{
    this->X = x;
    this->Y = y;
};

Point::~Point()
{
}

Point Point::operator-(Point point) const
{
    return Point(this->X - point.X, this->Y - point.Y);
}

Point Point::operator+(Point point) const
{
    return Point(this->X + point.X, this->Y + point.Y);
}

Point Point::operator*(int value) const
{
    return Point(this->X * value, this->Y * value);
}

Point Point::operator/(int value) const
{
    return Point(this->X / value, this->Y / value);
}

void Point::operator-=(int value)
{
    this->X -= value;
    this->Y -= value;
}

void Point::operator+=(int value)
{
    this->X += value;
    this->Y += value;
}

void Point::operator*=(int value)
{
    this->X *= value;
    this->Y *= value;
}

void Point::operator/=(int value)
{
    this->X /= value;
    this->Y /= value;
}

void Point::operator-=(const Point &point)
{
    this->X -= point.X;
    this->Y -= point.Y;
}

void Point::operator+=(const Point &point)
{
    this->X += point.X;
    this->Y += point.Y;
}

void Point::operator*=(const Point &point)
{
    this->X *= point.X;
    this->Y *= point.Y;
}

void Point::operator/=(const Point &point)
{
    this->X /= point.X;
    this->Y /= point.Y;
}

bool Point::operator==(const Point &p) const
{
    return this->X == p.X && this->Y == p.Y;
}

bool Point::operator!=(const Point &p) const
{
    return this->X != p.X || this->Y != p.Y;
}

unsigned short Point::Length() const
{
    return sqrt(this->LengthSquared());
}

unsigned int Point::LengthSquared() const
{
    return this->X * this->X + this->Y * this->Y;
}

long long int Point::Pack() const
{
    long long int p;
    this->Pack(&p);
    return p;
}

void Point::Pack(long long int *output) const
{
    *output = ((long long int)this->X << 32) | this->Y;
}

bool Point::Zero(){
    return this->X == 0 && this->Y == 0;
}

void Point::Swap(Point &a, Point &b)
{
    swap(a.X, b.X);
    swap(a.Y, b.Y);
}

int Point::DistanceSquared(const Point &a, const Point &b)
{
    Point c = a - b;
    return c.X * c.X + c.Y * c.Y;
}

Point Point::Rotate(const Point &point, const Point &center, float angle)
{
    // Translate point relative to the origin
    float translatedX = point.X - center.X;
    float translatedY = point.Y - center.Y;

    // Apply rotation
    float rotatedX = translatedX * cos(angle) - translatedY * sin(angle);
    float rotatedY = translatedX * sin(angle) + translatedY * cos(angle);

    // Translate back to the original coordinate system
    float finalX = rotatedX + center.X;
    float finalY = rotatedY + center.Y;

    return Point(finalX, finalY);
}

Point Point::Unpack(const long long int &packed)
{
    Point p;
    Point::Unpack(packed, &p);
    return p;
}

void Point::Unpack(const long long int &packed, Point *output)
{
    output->X = packed >> 32;
    output->Y = packed & 0xFFFFFFFF;
}