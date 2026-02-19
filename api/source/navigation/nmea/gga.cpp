#include "api/navigation/nmea/gga.h"
#include "api/navigation/nmea.h"
#include "api/navigation/coordinate.h"
#include "api/common/utility.h"
#include "api/common/time.h"
#include <iostream>
#include <sstream>
#include <cstring>

GGA::GGA()
{
    this->hour = 0;
    this->minute = 0;
    this->second = 0;
    this->fixQualityIndicator = 0;
    this->satellites = 0;
    this->HDOP = 0;
    this->altitude = 0;
    this->geoidHeight = 0;
    this->valid = false;
}

GGA::GGA(const char *sentence)
{
    // Check if the sentence is a valid GGA sentence
    if (contains(sentence, "GGA"))
    {
        char *tokens[GGA_MAX_TOKENS];

        // Split the sentence using the ',' delimiter
        Nmea::Split(sentence, ',', tokens, GGA_MAX_TOKENS);

        // Parse time of fix (hhmmss.sss)
        int time = atoi(tokens[1]);
        this->hour = time / 10000;
        this->minute = (time % 10000) / 100;
        this->second = time % 100;

        // Parse latitude (llll.ll)
        short degrees = 0;
        double minutes = 0;
        char hemisphere = 0;

        Nmea::Parse(tokens[2], &degrees, &minutes, &hemisphere);
        this->location.Y = Coordinate::ToDecimalDegrees(degrees, minutes);
        this->latHemisphere = hemisphere;

        // Parse longitude (yyyyy.yy)
        degrees = 0;
        minutes = 0;
        hemisphere = 0;

        Nmea::Parse(tokens[4], &degrees, &minutes, &hemisphere);
        this->location.X = Coordinate::ToDecimalDegrees(degrees, minutes);
        this->longHemisphere = hemisphere;

        // Parse GPS fix quality indicator
        this->fixQualityIndicator = atoi(tokens[6]);

        // Parse number of satellites being tracked
        this->satellites = atoi(tokens[7]);

        // Parse HDOP
        this->HDOP = atof(tokens[8]);

        // Parse altitude above mean sea level
        this->altitude = atof(tokens[9]);

        // Parse geoid height (height of the geoid above WGS84 ellipsoid)
        if (tokens[11][0] != '\0')
            this->geoidHeight = atof(tokens[11]);
        else
            this->geoidHeight = 0.0; // Default value if the field is empty

        this->valid = true;
    }
    else
    {
        this->hour = 0;
        this->minute = 0;
        this->second = 0;
        this->fixQualityIndicator = 0;
        this->satellites = 0;
        this->HDOP = 0;
        this->altitude = 0;
        this->geoidHeight = 0;
        this->valid = false;
    }
}

GGA::~GGA()
{
}

bool GGA::Valid()
{
    return this->valid;
}