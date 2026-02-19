#pragma once
#include "api/common/types.h"
#include "api/navigation/nmea.h"
#include "api/math/vector2.h"

#define GGA_MAX_TOKENS 15
#define GGA_CHECKSUM_OFFSET 3

// Example sentence
// $GPGGA,hhmmss.sss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,x.x,xxxx*hh<CR><LF>

class GGA
{
protected:
    bool valid;

public:
    Byte hour;
    Byte minute;
    Byte second;
    Vector2 location;
    char latHemisphere;
    char longHemisphere;
    short fixQualityIndicator;
    Byte satellites;
    double HDOP;
    double altitude;
    double geoidHeight;

    GGA();
    GGA(const char *sentence);
    ~GGA();

    bool Valid();
};