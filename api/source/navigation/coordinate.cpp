#include "api/navigation/coordinate.h"
#include <cmath>
#include <cstdio>

// Convert latitude and longitude to Web Mercator projected coordinates (x, y)
void Coordinate::LatLongToWebMercator(const Vector2 &latlong, Vector2 *webmercator)
{
    webmercator->X = latlong.X * (EARTH_HALF_CIRCUMFERENCE / 180.0);
    webmercator->Y = std::log(std::tan((90.0 + latlong.Y) * (M_PI / 180.0) / 2.0)) * EARTH_RADIUS;

    // Reversed
    // webmercator->X = latlong.Y * (EARTH_HALF_CIRCUMFERENCE / 180.0);
    // webmercator->Y = std::log(std::tan((90.0 + latlong.X) * (M_PI / 180.0) / 2.0)) * EARTH_RADIUS;
}

// Convert Web Mercator projected coordinates (x, y) back to latitude and longitude
void Coordinate::WebMercatorToLatLong(const Vector2 &webmercator, Vector2 *latlong)
{
    latlong->X = (webmercator.X / EARTH_HALF_CIRCUMFERENCE) * 180.0;
    latlong->Y = 180.0 / M_PI * (2.0 * std::atan(std::exp(webmercator.Y / EARTH_RADIUS)) - M_PI / 2.0);

    printf("Latlong from webMercator: X%f   Y%f\n", latlong->X, latlong->Y);

    // Reversed
    // latlong->Y = (webmercator.X / EARTH_HALF_CIRCUMFERENCE) * 180.0;
    // latlong->X = (webmercator.Y / EARTH_HALF_CIRCUMFERENCE) * 180.0;
    // latlong->X = 180.0 / M_PI * (2.0 * std::atan(std::exp(latlong->X * M_PI / 180.0)) - M_PI / 2.0);
}

// Calculates the web mercator scale factor based on latitude distortion
void Coordinate::WebMercatorScaleFactor(const double &latitudeRadians, double *output)
{
    *output = 1.0 / cos(latitudeRadians);
}

// Calculates the true meters per pixels based on the latitude scaling factor distortion of web mercator
void Coordinate::WebMercatorUnitsPerPixel(const double &scaleFactor, const double &metersPerPixel, double *output)
{
    *output = metersPerPixel / scaleFactor;
}

void Coordinate::WebMercatorToPixels(const Vector2 &webMercatorOffset, const double &scaleFactor, const double &metersPerPixel, Point *output)
{
    // Check for division by zero
    if (metersPerPixel == 0.0 || scaleFactor == 0.0)
    {
        // Handle this case appropriately
        return;
    }

    // Convert Web Mercator offsets to pixels
    output->X = webMercatorOffset.X / metersPerPixel;
    output->Y = (webMercatorOffset.Y / metersPerPixel) * scaleFactor;
}

// Convert degrees to radians
void Coordinate::DegreesToRadians(const float &degrees, float *output)
{
    *output = degrees * M_PI / 180.0;
}

// Convert radians to degrees
void Coordinate::RadiansToDegrees(const float &radians, float *output)
{
    *output = radians * 180.0 / M_PI;
}

float Coordinate::ToDecimalDegrees(const short degrees, const float minutes)
{
    return static_cast<float>(degrees) + (minutes / 60.0);
}

void Coordinate::ToDegreesMinutes(const float decimaldegrees, short *degrees, float *minutes)
{
    // Extract the integer degrees part
    *degrees = static_cast<short>(decimaldegrees);

    // Calculate the decimal minutes part
    float decimal_part = std::fabs(decimaldegrees - *degrees);
    *minutes = decimal_part * 60.0;

    // If the input was negative, make the output degrees negative
    if (decimaldegrees < 0)
        *degrees = -*degrees;
}