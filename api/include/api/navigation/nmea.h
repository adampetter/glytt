#pragma once
#include "api/navigation/nmea/gga.h"
#include "api/navigation/nmea/rmc.h"
#include <string>

#define NMEA_MAX_SEARCH 32
#define NMEA_MILLENNIA 2000

enum NmeaSentence
{
    NmeaSentence_None = 0,
    NmeaSentence_GGA = 1,
    NmeaSentence_RMC = 2
};

class Nmea
{

public:
    template <typename T>
    static T Parse(const char *sentencee);
    static NmeaSentence Type(const char *sentence);
    static void Split(const char *sentence, char delimiter, char **tokens, int maxTokens);
    static bool Parse(const char *nmea, short *degrees, double *minutes, char *hemisphere);
    static bool Date(const char *date, unsigned short *year, Byte *month, Byte *day);
};