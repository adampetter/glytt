#pragma once
#include "api/common/types.h"
#include "api/navigation/coordinate.h"
#include "api/navigation/region.h"
#include "api/math/vector2.h"
#include "api/math/rectangle.h"
#include "api/math/point.h"

class Tile
{
public:
    static void Index(const Vector2 &webmercator, const Byte zoom, Point *output);
    static void Expand(const Point &index, const unsigned int &width, const unsigned int &height, Rectangle *output);
    static void Bounds(int x, int y, int z, Region* region);
    static void Clamp(int x, int y, int z, Point* output);
};