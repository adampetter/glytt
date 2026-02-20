#include "api/navigation/pa1010d.h"
#include "api/navigation/nmea.h"
#include "api/io/i2c.h"
#include "api/common/time.h"
#include <cstring>

PA1010D::PA1010D(const PA1010DConfig &config)
{
    this->config = config;
    memset(this->buffer, '\0', PA1010D_BUFFER_SIZE);
    memset(this->sentence, '\0', PA1010D_SENTENCE_SIZE);
}

PA1010D::~PA1010D()
{
}

void PA1010D::Read(Location *location)
{
    if(location == NULL)
        return;

    // Limiter
    Byte limit = 0;

    // Iterators
    char *start;
    char *it;
    int i;

    // Reset data
    this->gga = GGA();
    this->rmc = RMC();

    do
    {
        // Reset i2c buffer
        memset(this->buffer, '\0', PA1010D_BUFFER_SIZE);

        // Fetch nmea data from gps
        if (!this->config.i2c->Read(this->config.address, this->buffer, PA1010D_BUFFER_SIZE))
            return;

        start = (char *)this->buffer;
        it = start;
        i = 0;

        // Iterate through the received data over i2c and extract the nmea-sentences
        while (*it != '\0' && i < PA1010D_BUFFER_SIZE)
        {
            if (*it == '\n')
            {
                unsigned short length = it - start;
            memset(this->sentence + length, '\0', PA1010D_SENTENCE_SIZE - length);
                memcpy(this->sentence, start, length);

                NmeaSentence type = Nmea::Type(this->sentence);

                if (!this->gga.Valid() && type == NmeaSentence::NmeaSentence_GGA)
                {
                    this->gga = Nmea::Parse<GGA>(this->sentence);

                    // Debug
                    /*printf("### GGA ###\n");
                    printf("Time: %i:%i:%i\n", gga.hour, gga.minute, gga.second);
                    printf("Latitude: %i degrees    %f minutes  %c hemisphere\n", gga.latitude.degrees, gga.latitude.minutes, gga.latitude.hemisphere);
                    printf("Longitude: %i degrees    %f minutes  %c hemisphere\n", gga.longitude.degrees, gga.longitude.minutes, gga.longitude.hemisphere);
                    printf("Satellites: %i\n", gga.satellites);
                    printf("Fix: %s\n", gga.fixQualityIndicator ? "Yes" : "No");*/
                }
                else if (!this->rmc.Valid() && type == NmeaSentence::NmeaSentence_RMC)
                {
                    this->rmc = Nmea::Parse<RMC>(this->sentence);

                    // Debug
                    /*printf("### RMC ###\n");
                    printf("Status: %c\n", rmc.status);
                    printf("Time: %i:%i:%i\n", rmc.datetime.hour, rmc.datetime.minute, rmc.datetime.second);
                    printf("Latitude: %i degrees    %f minutes  %c hemisphere\n", rmc.latitude.degrees, rmc.latitude.minutes, rmc.latitude.hemisphere);
                    printf("Longitude: %i degrees    %f minutes  %c hemisphere\n", rmc.longitude.degrees, rmc.longitude.minutes, rmc.longitude.hemisphere);
                    printf("Year: %i   Month: %i   Day: %i\n", rmc.datetime.year, rmc.datetime.month, rmc.datetime.day);
                    printf("Speed: %f\n", rmc.speed);
                    printf("Course: %f\n", rmc.course);*/
                }

                start = it + 1;
            }

            it++;
            i++;

            if (this->gga.Valid() && this->rmc.Valid())
                break;
        }

        // Add to number of reads done
        limit++;

        // Wait a little bit before trying again
        delay(PA1010D_TRY_DELAY);

    } while (limit < PA1010D_MAX_TRIES);

    if (this->gga.Valid() && this->rmc.Valid())
    {
        const bool hasFix = this->gga.fixQualityIndicator > 0 && this->rmc.status == 'A';

        location->datetime = rmc.datetime;
        location->satellites = gga.satellites;
        location->fix = hasFix;

        if (hasFix)
        {
            location->coordinate = rmc.location;
            location->altitude = gga.altitude;
            location->speed = rmc.speed * 1.852; // Convert from knots to kph
            location->course = rmc.course;
        }
        else
        {
            location->coordinate = Vector2::Zero();
            location->altitude = 0;
            location->speed = 0;
            location->course = 0;
        }
    }
    else
    {
        location->coordinate = Vector2::Zero();
        location->satellites = 0;
        location->altitude = 0;
        location->speed = 0;
        location->course = 0;
        location->fix = false;
    }
}