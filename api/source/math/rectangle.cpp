#include "api/math/rectangle.h"
#include "api/math/helper.h"
#include <math.h>

Rectangle::Rectangle() : Rectangle(Point(0, 0), Point(0, 0))
{
}

Rectangle::Rectangle(const Rectangle &source)
{
    this->Start = source.Start;
    this->End = source.End;
}

Rectangle::Rectangle(Point start, Point end)
{
    this->Start = start;
    this->End = end;
}

Rectangle::Rectangle(int width, int height)
{
    this->Start = Point(0, 0);
    this->End = Point(width, height);
}

Rectangle::~Rectangle()
{
}

Rectangle Rectangle::operator-(Point point) const
{
    return Rectangle(this->Start - point, this->End - point);
}

Rectangle Rectangle::operator+(Point point) const
{
    return Rectangle(this->Start + point, this->End + point);
}

bool Rectangle::operator==(const Rectangle &s) const
{
    return this->Start == s.Start && this->End == s.End;
}

bool Rectangle::operator!=(const Rectangle &s) const
{
    return this->Start != s.Start || this->End != s.End;
}

void Rectangle::operator-=(const Point &point)
{
    this->Start -= point;
    this->End -= point;
}

void Rectangle::operator+=(const Point &point)
{
    this->Start += point;
    this->End += point;
}

bool Rectangle::Intersect(const Rectangle &rectangle) const
{
    return !(rectangle.Start.X >= this->End.X ||
             rectangle.End.X <= this->Start.X ||
             rectangle.Start.Y >= this->End.Y ||
             rectangle.End.Y <= this->Start.Y);
}

bool Rectangle::Intersect(const Point &point) const
{
    return point.X >= this->Start.X && point.X <= this->End.X && point.Y >= Start.Y && point.Y <= this->End.Y;
}

bool Rectangle::Intersect(const Circle&  circle) const{
    int nearestX = max(this->Start.X, min(circle.Center.X, this->End.X));
    int nearestY = max(this->Start.Y, min(circle.Center.Y, this->End.Y));

    int deltaX = circle.Center.X - nearestX;
    int deltaY = circle.Center.Y - nearestY;

    return (deltaX * deltaX + deltaY * deltaY) <= (circle.Radius * circle.Radius);
}

// Calculates the intersecting rectangle (overlap)
Rectangle Rectangle::Intersection(const Rectangle &rectangle) const
{
    // Find max of starts and min of ends
    Point newStart(
        std::max(this->Start.X, rectangle.Start.X),
        std::max(this->Start.Y, rectangle.Start.Y));

    Point newEnd(
        std::min(this->End.X, rectangle.End.X),
        std::min(this->End.Y, rectangle.End.Y));

    // If the newStart is greater than the newEnd in either dimension, then the rectangles do not overlap.
    // In that case, return an empty Rectangle.
    if (newStart.X > newEnd.X || newStart.Y > newEnd.Y)
        return Rectangle();

    // Return the intersected rectangle
    return Rectangle(newStart, newEnd);
}

int Rectangle::Width() const
{
    return abs(this->End.X - this->Start.X);
}

int Rectangle::Height() const
{
    return abs(this->End.Y - this->Start.Y);
}

void Rectangle::Width(int width)
{
    this->End.X = this->Start.X + width;
}

void Rectangle::Height(int height)
{
    this->End.Y = this->Start.Y + height;
}

unsigned int Rectangle::Area() const
{
    return this->Width() * this->Height();
}

Point Rectangle::Center() const
{
    return Point(this->Start.X + this->Width() / 2, this->Start.Y + this->Height() / 2);
}

void Rectangle::Center(const Point center){
    Point diff = center - this->Center();
    this->Start += diff;
    this->End += diff;
}

Point Rectangle::Corner(int index) const
{
    if (index <= 0)
        return Point(this->Left(), this->Top());
    else if (index == 1)
        return Point(this->Right(), this->Top());
    else if (index == 2)
        return Point(this->Right(), this->Bottom());
    else
        return Point(this->Left(), this->Bottom());
}

bool Rectangle::Square() const
{
    return this->Width() == this->Height();
}

bool Rectangle::Empty() const
{
    return this->Area() == 0;
}

int Rectangle::Left() const
{
    return this->Start.X;
}

void Rectangle::Left(int left)
{
    int width = this->Width();
    this->Start.X = left;
    this->End.X = left + width;
}

int Rectangle::Right() const
{
    return this->End.X;
}

void Rectangle::Right(int right)
{
    int diff = right - this->End.X;
    this->Start.X += diff;
    this->End.X += diff;
}

int Rectangle::Top() const
{
    return this->Start.Y;
}

void Rectangle::Top(int top)
{
    int height = this->Height();
    this->Start.Y = top;
    this->End.Y = top + height;
}

int Rectangle::Bottom() const
{
    return this->End.Y;
}

void Rectangle::Bottom(int bottom)
{
    int diff = bottom - this->End.Y;
    this->Start.Y += diff;
    this->End.Y += diff;
}

void Rectangle::Expand(const int amount)
{
    this->Start -= amount;
    this->End += amount;
}

void Rectangle::Expand(const int x, const int y)
{
    this->Start.X -= x;
    this->End.X += x;
    this->Start.Y -= y;
    this->End.Y += y;
}

void Rectangle::Contract(const int amount)
{
    this->Start += amount;
    this->End -= amount;
}

void Rectangle::Contract(const int x, const int y)
{
    this->Start.X += x;
    this->End.X -= x;
    this->Start.Y += y;
    this->End.Y -= y;
}

void Rectangle::Reset()
{
    this->End.X = this->Width();
    this->Start.X = 0;

    this->End.Y = this->Height();
    this->Start.Y = 0;
}

Rectangle Rectangle::Reset(const Rectangle &rectangle)
{
    Rectangle r = rectangle;
    r.Reset();
    return r;
}

Rectangle Rectangle::Expand(const Rectangle &rectangle, const int amount)
{
    Rectangle r = rectangle;
    r.Expand(amount);
    return r;
}

Rectangle Rectangle::Expand(const Rectangle &rectangle, const int x, const int y)
{
    Rectangle r = rectangle;
    r.Expand(x, y);
    return r;
}

Rectangle Rectangle::Contract(const Rectangle &rectangle, const int amount)
{
    Rectangle r = rectangle;
    r.Contract(amount);
    return r;
}

Rectangle Rectangle::Contract(const Rectangle &rectangle, const int x, const int y)
{
    Rectangle r = rectangle;
    r.Contract(x, y);
    return r;
}

Rectangle Rectangle::Centered(const Rectangle &rectangle, const Point center){
    Rectangle r = rectangle;
    r.Center(center);
    return r;
}