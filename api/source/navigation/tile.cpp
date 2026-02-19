#include "api/navigation/tile.h"
#include <stdio.h>

void Tile::Index(const Vector2 &webmercator, const Byte zoom, Point *output)
{
    double n = (1 << zoom); // 2^zoom: total number of tiles along one axis

    // Convert Web Mercator coordinates to normalized coordinates [0, 1]
    double x = (webmercator.X + EARTH_RADIUS * M_PI) / (2 * EARTH_RADIUS * M_PI);
    double y = (M_PI * EARTH_RADIUS - webmercator.Y) / (2 * EARTH_RADIUS * M_PI);

    // Convert normalized coordinates to tile indices
    output->X = static_cast<int>(x * n);
    output->Y = static_cast<int>(y * n);
}

void Tile::Expand(const Point &index, const unsigned int &width, const unsigned int &height, Rectangle *output)
{
    const unsigned int halfWidth = width / 2;
    const unsigned int halfHeight = height / 2;

    output->Start = Point(index.X - halfWidth, index.Y - halfHeight);
    output->End = Point(index.X + halfWidth, index.Y + halfHeight);    
}

void Tile::Bounds(int x, int y, int z, Region* region) {    
    // Ensure output is not null
    if (!region) return;

    // Size of the world in tiles (both in x and y direction)
    int numTiles = 1 << z;  // equivalent to pow(2, z)

    // Size of one tile in Web Mercator coordinates
    double tileSizeMeters = (2.0 * EARTH_HALF_CIRCUMFERENCE) / numTiles;

    // Calculate Web Mercator bounds
    region->Start.X = x * tileSizeMeters - EARTH_HALF_CIRCUMFERENCE;
    region->Start.Y = EARTH_HALF_CIRCUMFERENCE - (y * tileSizeMeters);  // Note: Y is inverted, origin is top-left
    region->End.X = region->Start.X + tileSizeMeters;
    region->End.Y = region->Start.Y - tileSizeMeters;  // Note: Y is inverted
}

void Tile::Clamp(int x, int y, int z, Point* output) {
    // Maximum index value on the x-axis (longitude) given the zoom level.
    int maxX = (1 << z) - 1;

    // Clamp/wrap the x value.
    output->X = ((x % (maxX + 1)) + (maxX + 1)) % (maxX + 1);

    // Clamp the y value (latitude) to the valid range [0, 2^z - 1].
    output->Y = std::max(0, std::min((1 << z) - 1, y));
}