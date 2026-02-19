#pragma once
#include "api/math/vector2.h"
#include "api/math/point.h"
#include "api/common/types.h"

// Earth's radius in meters
#define EARTH_RADIUS 6378137
#define EARTH_HALF_CIRCUMFERENCE 20037508.34278918 //EARTH_RADIUS * M_PI
#define EARTH_FULL_CIRCUMFERENCE 40075016.68557836 //EARTH_HALF_CIRCUMFERENCE * 2

enum CoordinateType
{
    CoordinateType_LatLong = 0,
    CoordinateType_WebMercator = 1,
    CoordinateType_Cartesian = 2
};

class Coordinate
{
public:
    static void LatLongToWebMercator(const Vector2 &latlong, Vector2 *webmercator);
    static void WebMercatorToLatLong(const Vector2 &webmercator, Vector2 *latlong);
    static void WebMercatorScaleFactor(const double &latitude, double *output);
    static void WebMercatorUnitsPerPixel(const double &scaleFactor, const double &metersPerPixel, double *output);
    static void WebMercatorToPixels(const Vector2 &webMercatorOffset, const double &scaleFactor, const double &metersPerPixel, Point *output);
    static void DegreesToRadians(const float &degrees, float *output);
    static void RadiansToDegrees(const float &radians, float *output);
    static float ToDecimalDegrees(const short degrees, const float minutes);
    static void ToDegreesMinutes(const float decimaldegrees, short *degrees, float *minutes);
};