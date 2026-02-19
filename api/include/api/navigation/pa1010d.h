/* ##################

    This driver supports GPS chipset PA1010D over i2c.
    PA1010D supports both i2c over 100 KHz and 400 KHz.

################## */

#pragma once
#include "api/common/types.h"
#include "api/navigation/gps.h"
#include "api/navigation/nmea/gga.h"
#include "api/navigation/nmea/rmc.h"

#define PA1010D_I2C_DEFAULT_ADDRESS 0x10
#define PA1010D_CONTINOUS_MODE "$PMTK225,0*2B"
#define PA1010D_STANDBY_MODE "$PMTK161,0*28"
#define PA1010D_BACKUP_MODE "$PMTK225,4*2F"
// #define PA1010D_PERIODIC_MODE $PMTK225,Type, Run_time, Sleep_time,2nd_Run_time,2nd_Sleep_time*checksum
#define PA1010D_BUFFER_SIZE 128
#define PA1010D_SENTENCE_SIZE 128
#define PA1010D_MAX_TRIES 30
#define PA1010D_TRY_DELAY 10
#define PA1010D_FAILURE_DELAY 1000

class I2c;
class Location;

struct PA1010DConfig
{
    I2c *i2c = NULL;
    Byte address = PA1010D_I2C_DEFAULT_ADDRESS;
    unsigned int refreshRateSeconds = 15;
};

class PA1010D
{
    
protected:
    PA1010DConfig config;

    Byte buffer[PA1010D_BUFFER_SIZE];
    char sentence[PA1010D_SENTENCE_SIZE];
    GGA gga;
    RMC rmc;

public:
    PA1010D(const PA1010DConfig &config);
    ~PA1010D();

    void Read(Location* location);
    void Debug();
};