#pragma once
#include "api/common/types.h"
#include "api/system/thread.h"
#include "api/io/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"

#define I2C_TIMEOUT_MS 500

enum I2cPort{
    I2cPort_0 = 0,
    I2cPort_1 = 1
};

enum I2cFrequency
{
    I2cFrequency_Unknown = 0,
    I2cFrequency_01M = 100000,
    I2cFrequency_04M = 400000
};

enum I2cMode
{
    I2cMode_Slave = 0,
    I2cMode_Master = 1
};

enum I2cAcknowledge{
    I2cAcknowledge_Default = 0,
    I2cAcknowledge_On = 0,
    I2cAcknowledge_Off = 1
};

struct I2cConfig
{
    I2cPort port = I2cPort::I2cPort_1;
    GpioNum sda = GPIO_NONE;
    GpioNum scl = GPIO_NONE;
    I2cMode mode = I2cMode::I2cMode_Master;
    I2cFrequency frequency = I2cFrequency::I2cFrequency_01M;
    bool internalPullup = false; // Preferably use 10K external pull-up resistors
    I2cAcknowledge acknowledge = I2cAcknowledge::I2cAcknowledge_Default;
};

// Thread Safe

class I2c
{
private:
    i2c_master_bus_handle_t busHandle = nullptr;
    i2c_master_dev_handle_t deviceHandle = nullptr;
    uint16_t currentAddress = I2C_DEVICE_ADDRESS_NOT_USED;
    bool ensureDevice(Byte address, unsigned short timeoutMs = I2C_TIMEOUT_MS);

protected:
    Mutex lock;
    I2cConfig config;
    bool started = false;

public:
    I2c(const I2cConfig& config);
    ~I2c();

    bool Read(Byte address, Byte *data, unsigned short length = 1, unsigned short timeout = I2C_TIMEOUT_MS);
    unsigned short Read(Byte address, Byte reg, Byte *data, unsigned short length, unsigned short timeout = I2C_TIMEOUT_MS);
    unsigned short Write(Byte address, Byte* data, unsigned short length = 1, unsigned short timeout = I2C_TIMEOUT_MS);
    unsigned short Write(Byte address, Byte reg, Byte data, unsigned short timeout = I2C_TIMEOUT_MS);
    I2cConfig Config();
    bool Exists(Byte address);
    Byte Scan();    
};