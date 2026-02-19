#include "api/io/i2c.h"
#include "api/common/time.h"
#include "api/common/utility.h"

bool I2c::ensureDevice(Byte address, unsigned short timeoutMs)
{
    if (this->busHandle == nullptr)
        return false;

    if (address > 0x7F)
        return false;

    if (this->deviceHandle == nullptr)
    {
        i2c_device_config_t deviceConfig = {};
        deviceConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        deviceConfig.device_address = address;
        deviceConfig.scl_speed_hz = (uint32_t)this->config.frequency;
        deviceConfig.scl_wait_us = timeoutMs * 1000;
        deviceConfig.flags.disable_ack_check = this->config.acknowledge == I2cAcknowledge_Off;

        if (i2c_master_bus_add_device(this->busHandle, &deviceConfig, &this->deviceHandle) != ESP_OK)
            return false;

        this->currentAddress = address;
        return true;
    }

    if (this->currentAddress != address)
    {
        if (i2c_master_device_change_address(this->deviceHandle, address, timeoutMs) != ESP_OK)
            return false;

        this->currentAddress = address;
    }

    return true;
}

I2c::I2c(const I2cConfig &config)
{
    this->config = config;

    i2c_master_bus_config_t busConfig = {};
    busConfig.i2c_port = (i2c_port_num_t)config.port;
    busConfig.sda_io_num = (gpio_num_t)config.sda;
    busConfig.scl_io_num = (gpio_num_t)config.scl;
    busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
    busConfig.glitch_ignore_cnt = 7;    
    busConfig.intr_priority = 0;
    busConfig.trans_queue_depth = 0;
    busConfig.flags.enable_internal_pullup = config.internalPullup;

    if (i2c_new_master_bus(&busConfig, &this->busHandle) != ESP_OK)
    {
        
#if DEBUG
        printf("I2C (Port %i) failed to initialize master bus\n", this->config.port);
#endif
        this->started = false;
    }
    else
    {

#if DEBUG
        printf("I2C (Port %i) master bus initialized\n", this->config.port);
#endif

        this->started = true;
    }
}

I2c::~I2c()
{
    if (this->deviceHandle != nullptr)
        i2c_master_bus_rm_device(this->deviceHandle);

    if (this->busHandle != nullptr)
        i2c_del_master_bus(this->busHandle);
}

bool I2c::Read(Byte address, Byte *data, unsigned short length, unsigned short timeout)
{
    bool result = false;

    if (lock.Take(timeout, timeout > 0))
    {
        if (this->ensureDevice(address, timeout))
            result = i2c_master_receive(this->deviceHandle, data, length, timeout) == ESP_OK;

        lock.Release();
    }

    return result;
}

unsigned short I2c::Read(Byte address, Byte reg, Byte *data, unsigned short length, unsigned short timeout)
{
    unsigned short result = 0;

    if (lock.Take(timeout, timeout > 0))
    {
        if (this->ensureDevice(address, timeout))
            result = i2c_master_transmit_receive(this->deviceHandle, &reg, sizeof(reg), data, length, timeout) == ESP_OK ? length : 0;

        lock.Release();
    }

    return result;
}

unsigned short I2c::Write(Byte address, Byte *data, unsigned short length, unsigned short timeout)
{
    unsigned short result = 0;

    if (lock.Take(timeout, timeout > 0))
    {
        if (this->ensureDevice(address, timeout))
            result = i2c_master_transmit(this->deviceHandle, data, length, timeout) == ESP_OK ? length : 0;

        lock.Release();
    }

    return result;
}

unsigned short I2c::Write(Byte address, Byte reg, Byte data, unsigned short timeout)
{
    Byte buffer[2] = {reg, data};
    return this->Write(address, buffer, sizeof(buffer), timeout);
}

I2cConfig I2c::Config()
{
    return this->config;
}

bool I2c::Exists(Byte address)
{
    if (this->busHandle == nullptr)
        return false;

    if (address > 0x7F)
        return false;

    return i2c_master_probe(this->busHandle, address, I2C_TIMEOUT_MS) == ESP_OK;
}

Byte I2c::Scan()
{
#if DEBUG
    printf("------I2c (Scan, Port %i)------\n", this->config.port);
#endif
    Byte count = 0;

    // 7-bit I2C addresses span 0x00..0x7F, but 0x00..0x07 and 0x78..0x7F are reserved.
    // We therefore scan only the normal device range 0x08..0x77.
    // This avoids noisy/protocol-reserved addresses and reduces false errors/timeouts.
    for (Byte i = 0x08; i <= 0x77; i++)
    {
        if (i2c_master_probe(this->busHandle, i, I2C_TIMEOUT_MS) == ESP_OK)
        {
#if DEBUG
            printf("Response from: ");
            printDecHexBin(i);
#endif
            count++;
        }
    }

#if DEBUG
    if (!count)
        printf("No responses found\n");

    printf("----------------------\n");
#endif

    return count;
}