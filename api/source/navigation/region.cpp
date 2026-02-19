#include "api/navigation/region.h"
#include <math.h>

Region::Region() : Region(Vector2(0, 0), Vector2(0, 0))
{
}

Region::Region(Vector2 start, Vector2 end)
{
    this->Start = start;
    this->End = end;
}

Region::~Region()
{
}

bool Region::operator==(const Region &s) const
{
    return this->Start == s.Start && this->End == s.End;
}

bool Region::operator!=(const Region &s) const
{
    return this->Start != s.Start || this->End != s.End;
}

bool Region::Intersect(const Region &rectangle) const
{
    return !(rectangle.Start.X >= this->End.X ||
             rectangle.End.X <= this->Start.X ||
             rectangle.Start.Y >= this->End.Y ||
             rectangle.End.Y <= this->Start.Y);
}

bool Region::Intersect(const Vector2 &coordinate) const
{
    return coordinate.X >= this->Start.X && coordinate.X <= this->End.X && coordinate.Y >= Start.Y && coordinate.Y <= this->End.Y;
}

bool Region::Intersect(const Vector2 &coordinate, const CoordinateType type) const
{
    if (type == CoordinateType::CoordinateType_WebMercator)
        return coordinate.X >= this->Start.X && coordinate.X <= this->End.X && coordinate.Y <= Start.Y && coordinate.Y >= this->End.Y;
    else
        return this->Intersect(coordinate);
}

Region Region::Intersection(const Region &rectangle) const
{
    // Find max of starts and min of ends
    Vector2 newStart(
        std::max(this->Start.X, rectangle.Start.X),
        std::max(this->Start.Y, rectangle.Start.Y));

    Vector2 newEnd(
        std::min(this->End.X, rectangle.End.X),
        std::min(this->End.Y, rectangle.End.Y));

    // If the newStart is greater than the newEnd in either dimension, then the rectangles do not overlap.
    // In that case, return an empty Region.
    if (newStart.X > newEnd.X || newStart.Y > newEnd.Y)
        return Region();

    // Return the intersected rectangle
    return Region(newStart, newEnd);
}

float Region::Width() const
{
    return abs(this->End.X - this->Start.X);
}

float Region::Height() const
{
    return abs(this->End.Y - this->Start.Y);
}

float Region::Area() const
{
    return this->Width() * this->Height();
}

void Region::Center(Vector2 *output) const
{
    *output = Vector2(this->Start.X + (this->End.X - this->Start.X) / 2, this->Start.Y + (this->End.Y - this->Start.Y) / 2);
}

bool Region::Empty() const
{
    return this->Area() == 0;
}

void Region::Displacement(const Vector2 &coordinate, const CoordinateType type, Vector2 *output) const
{
    if (type == CoordinateType::CoordinateType_WebMercator)
        *output = coordinate - Vector2(this->Start.X, this->End.Y);
    else
        *output = coordinate - this->Start;
}