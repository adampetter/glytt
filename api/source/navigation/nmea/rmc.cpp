#include "api/navigation/nmea/rmc.h"
#include "api/navigation/coordinate.h"
#include "api/common/utility.h"

RMC::RMC()
{
    this->latHemisphere = '\0';
    this->longHemisphere = '\0';
    this->status = '\0';
    this->speed = 0;
    this->course = 0;
    // this->magneticVariation = 0;
    // this->magVarDirection = '\0';
    this->valid = false;
}

RMC::RMC(const char *sentence)
{
    // Check if the sentence is a valid GGA sentence
    if (contains(sentence, "RMC"))
    {
        char *tokens[RMC_MAX_TOKENS];

        // Split the sentence using the ',' delimiter
        Nmea::Split(sentence, ',', tokens, RMC_MAX_TOKENS);

        // Parse time of fix (hhmmss.sss)
        int time = atoi(tokens[1]);
        this->datetime.Hour(time / 10000);
        this->datetime.Minute((time % 10000) / 100);
        this->datetime.Second(time % 100);

        // Parse status
        this->status = *tokens[2];

        // Parse latitude (llll.ll)
        short degrees = 0;
        double minutes = 0;
        char hemisphere = 0;

        Nmea::Parse(tokens[3], &degrees, &minutes, &hemisphere);
        this->location.Y = Coordinate::ToDecimalDegrees(degrees, minutes);
        this->latHemisphere = hemisphere;

        // Parse longitude (yyyyy.yy)
        degrees = 0;
        minutes = 0;
        hemisphere = 0;

        Nmea::Parse(tokens[5], &degrees, &minutes, &hemisphere);
        this->location.X = Coordinate::ToDecimalDegrees(degrees, minutes);
        this->longHemisphere = hemisphere;

        // Parse speed in knots
        this->speed = atof(tokens[7]);

        // Parse course in degrees
        this->course = atof(tokens[8]);

        // Parse date
        unsigned short year = this->datetime.Year();
        Byte month = this->datetime.Month();
        Byte day = this->datetime.Day();
        Nmea::Date(tokens[9], &year, &month, &day);

        // TODO: add magnetic variation

        this->valid = true;
    }
    else
    {
        this->latHemisphere = '\0';
        this->longHemisphere = '\0';
        this->status = '\0';
        this->speed = 0;
        this->course = 0;
        // this->magneticVariation = 0;
        // this->magVarDirection = '\0';
        this->valid = false;
    }
}

RMC::~RMC()
{
}

bool RMC::Valid()
{
    return this->valid;
}