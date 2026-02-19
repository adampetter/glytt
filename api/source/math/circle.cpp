#include "api/math/circle.h"
#include "api/io/gpio.h"
#include <math.h>

Circle::Circle() : Circle(Point(0, 0), 0)
{
}

Circle::Circle(Point center, unsigned short radius)
{
    this->Center = center;
    this->Radius = radius;
}

Circle::~Circle()
{
}

bool Circle::Intersect(Circle circle)
{
    return Point(circle.Center.X - this->Center.X, circle.Center.Y - this->Center.Y).LengthSquared() <= pow(circle.Radius + this->Radius, 2);
}

bool Circle::Intersect(Point point)
{
    return Point(this->Center.X - point.X, this->Center.Y - point.Y).LengthSquared() <= this->Radius * this->Radius;
}