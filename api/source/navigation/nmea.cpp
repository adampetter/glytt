#include "api/navigation/nmea.h"
#include "api/common/utility.h"
#include <cstring>
#include <cmath>
#include <ctype.h>

template <>
GGA Nmea::Parse<GGA>(const char *sentence)
{
    if (Nmea::Type(sentence) == NmeaSentence::NmeaSentence_GGA)
        return GGA(sentence);
    else
        return GGA();
}

template <>
RMC Nmea::Parse<RMC>(const char *sentence)
{
    if (Nmea::Type(sentence) == NmeaSentence::NmeaSentence_RMC)
        return RMC(sentence);
    else
        return RMC();
}

NmeaSentence Nmea::Type(const char *sentence)
{
    if ((contains(sentence, "$GNGGA") || contains(sentence, "$GPGGA")) && indexof(sentence, "*") == (strlen(sentence) - GGA_CHECKSUM_OFFSET))
        return NmeaSentence::NmeaSentence_GGA;
    else if ((contains(sentence, "$GNRMC") || contains(sentence, "$GPRMC")) && indexof(sentence, "*") == (strlen(sentence) - RMC_CHECKSUM_OFFSET))
        return NmeaSentence::NmeaSentence_RMC;
    else
        return NmeaSentence::NmeaSentence_None;
}

void Nmea::Split(const char *sentence, char delimiter, char **tokens, int maxTokens)
{
    int tokenIndex = 0;
    int sentenceLength = strlen(sentence);
    char *tokenStart = const_cast<char *>(sentence);

    for (int i = 0; i <= sentenceLength; ++i)
    {
        if (sentence[i] == delimiter || sentence[i] == '\0')
        {
            tokens[tokenIndex] = tokenStart;
            tokenIndex++;
            tokenStart = const_cast<char *>(sentence + i + 1);

            if (tokenIndex >= maxTokens)
                break;
        }
    }
}

bool Nmea::Parse(const char *nmea, short *degrees, double *minutes, char *hemisphere)
{
    char *c = (char *)nmea;
    int i = 0;
    int dot = -1;
    int comma = -1;

    // Find the index where the decimal dot is at
    while (*c != '\0' && i < NMEA_MAX_SEARCH)
    {
        if (*c == '.' && dot < 0)
            dot = i;

        if (*c == ',' && comma < 0)
        {
            comma = i;

            if (dot > 0)
                break;
            else
                return false;
        }

        i++;
        c++;
    }

    // Invalid nmea, could not find the decimal dot
    if (dot < 0 || comma < 0 || i == NMEA_MAX_SEARCH - 1)
        return false;

    // Backstep to index of first degrees character
    c = (char *)nmea + dot - 2;

    // Extract minutes
    *minutes = atof(c);

    // Extract degrees
    char min[4] = {'\0', '\0', '\0', '\0'};
    c = (char *)nmea;
    i = dot - 2;

    for (unsigned char k = 0; k < i; k++)
    {
        min[k] = *c;
        c++;
    }

    *degrees = atoi(min);

    // Extract hemisphere
    *hemisphere = *(nmea + comma + 1);

    return true;
}

bool Nmea::Date(const char *date, unsigned short *year, Byte *month, Byte *day)
{
    // Supported format: ddmmyy and ddmmyyyy
    unsigned short length = 0;
    char* c = (char*)date;
    while(*c != '\0' && *c != ','){
        length++;
        c++;
    }

    if (length < 6 || length > 8)
        return false;

    char num[5];
    memset(num, '\0', sizeof(num));

    // Extract year
    c = (char *)(date + length) - (length == 6 ? 2 : 2 + 8 - length);
    memcpy(num, c, (date + length) - c);

    *year = atoi(num);

    if(*year < 1000)
        *year += NMEA_MILLENNIA;

    // Extract month
    c -= 2;
    memset(num, '\0', sizeof(num));
    memcpy(num, c, 2);
    *month = atoi(num);

    // Extract day
    c -= 2;
    memcpy(num, c, 2);
    *day = atoi(num);

    return true;
}