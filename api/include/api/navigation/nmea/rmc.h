#pragma once
#include "api/common/types.h"
#include "api/common/datetime.h"
#include "api/navigation/nmea.h"
#include "api/math/vector2.h"

#define RMC_MAX_TOKENS 15
#define RMC_CHECKSUM_OFFSET 3

// Example sentence
// $GNRMC,064951.000,A,2307.1256,N,12016.4438,E,0.03,165.48,260406,3.05,W,A*2C

class RMC
{
public:
    char status;
    Vector2 location;
    char latHemisphere;
    char longHemisphere;
    float speed;
    double course;
    DateTime datetime;
    //float magneticVariation;
    //char magVarDirection;
    bool valid;

    RMC();
    RMC(const char *sentence);
    ~RMC();

    bool Valid();
};