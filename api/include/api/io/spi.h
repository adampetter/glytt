#pragma once

#include "api/common/types.h"
#include "api/io/gpio.h"
#include "driver/spi_master.h"
#include "driver/spi_slave.h"
#include "driver/spi_slave_hd.h"

/*
    Watch
    https://www.youtube.com/watch?v=MCi7dCBhVpQ
*/

#define SPI_HANDLE spi_device_handle_t
#define SPI_TRANS_SIZE_DEFAULT 0
#define SPI_WAIT 10
#define SPI_QUEUE_SIZE 3
#define SPI_DEFAULT_TIMEOUT portMAX_DELAY
#define SPI_MAX_DATA_LINES 8

enum SpiType
{
    SpiType_Master = 0,
    SpiType_Slave = 1
};

enum SpiBus
{
    SpiBus_None = -1,
    SpiBus_Default = spi_host_device_t::SPI3_HOST,
    SpiBus_1 = spi_host_device_t::SPI1_HOST, // Used by the CPU to access in-package or off-package flash/PSRAM
    SpiBus_2 = spi_host_device_t::SPI2_HOST, // (HSPI) A general purpose SPI controller with its own DMA channel. Max 80Mhz as master, 60Mhz as slave.
    SpiBus_3 = spi_host_device_t::SPI3_HOST, // (VSPI) A general purpose SPI controller with access to a DMA channel shared between several peripherals
};

enum SpiFrequency
{
    Spi_Frequency_Unknown = 0,
    Spi_Frequency_01M = 100000,              // 0.1MHz
    Spi_Frequency_05M = 500000,              // 0.5MHz
    Spi_Frequency_1M = 1000000,              // 1MHz
    Spi_Frequency_2M = (APB_CLK_FREQ / 40),  // 2MHz
    Spi_Frequency_4M = (APB_CLK_FREQ / 20),  // 4MHz
    Spi_Frequency_8M = SPI_MASTER_FREQ_8M,   // 8MHz
    Spi_Frequency_9M = SPI_MASTER_FREQ_9M,   // 8.89MHz
    Spi_Frequency_10M = SPI_MASTER_FREQ_10M, // 10MHz
    Spi_Frequency_11M = SPI_MASTER_FREQ_11M, // 11.43MHz
    Spi_Frequency_12M = 12000000,            // 12MHz
    Spi_Frequency_13M = SPI_MASTER_FREQ_13M, // 13.33MHz
    Spi_Frequency_16M = SPI_MASTER_FREQ_16M, // 16MHz
    Spi_Frequency_20M = SPI_MASTER_FREQ_20M, // 20MHz
    Spi_Frequency_22M = 22000000,
    Spi_Frequency_24M = 24000000,
    Spi_Frequency_26M = SPI_MASTER_FREQ_26M, // 26.67MHz
    Spi_Frequency_28M = 28000000,
    Spi_Frequency_30M = 30000000,
    Spi_Frequency_32M = 32000000,
    Spi_Frequency_36M = 36000000,
    Spi_Frequency_40M = SPI_MASTER_FREQ_40M, // 40MHz
    Spi_Frequency_50M = 50000000,
    Spi_Frequency_60M = 60000000,
    Spi_Frequency_70M = 70000000,
    Spi_Frequency_80M = SPI_MASTER_FREQ_80M // 80MHz
};

// Represents pair of CPOL and CPHA
// https://www.corelis.com/education/tutorials/spi-tutorial/#spi-mode
enum SpiMode
{
    SpiMode_0 = 0, // CPOL:0, CPHA:0
    SpiMode_1 = 1, // CPOL:0, CPHA:1
    SpiMode_2 = 2, // CPOL:1, CPHA:0
    SpiMode_3 = 3  // CPOL:1, CPHA:1
};

enum SpiAffinity
{
    SpiAffinity_Auto = ESP_INTR_CPU_AFFINITY_AUTO,
    SpiAffinity_Cpu0 = ESP_INTR_CPU_AFFINITY_0,
    SpiAffinity_Cpu1 = ESP_INTR_CPU_AFFINITY_1,
};

struct SpiSlaveConfig
{
    SpiBus bus = SpiBus::SpiBus_Default;
    SpiMode mode = SpiMode::SpiMode_0;
    GpioNum mosi = GPIO_NONE;
    GpioNum miso = GPIO_NONE;
    GpioNum sck = GPIO_NONE;
    GpioNum cs = GPIO_NONE;
    GpioNum data[SPI_MAX_DATA_LINES] = {
        GPIO_NONE,
        GPIO_NONE,
        GPIO_NONE,
        GPIO_NONE,
        GPIO_NONE,
        GPIO_NONE,
        GPIO_NONE,
        GPIO_NONE};
    int transferSize = SPI_TRANS_SIZE_DEFAULT;
    GpioNum handshake = GPIO_NONE;
    SpiAffinity affinity = SpiAffinity::SpiAffinity_Auto;
};

struct SpiMasterConfig
{
    SpiBus bus = SpiBus::SpiBus_Default;
    SpiMode mode = SpiMode::SpiMode_0;
    GpioNum mosi = GPIO_NONE;
    GpioNum miso = GPIO_NONE;
    GpioNum sck = GPIO_NONE;
    GpioNum cs = GPIO_NONE;
    GpioNum data[SPI_MAX_DATA_LINES] = {
        GPIO_NONE,
        GPIO_NONE,
        GPIO_NONE,
        GPIO_NONE,
        GPIO_NONE,
        GPIO_NONE,
        GPIO_NONE,
        GPIO_NONE};
    int transferSize = SPI_TRANS_SIZE_DEFAULT;
    SpiFrequency frequency = SpiFrequency::Spi_Frequency_20M;
    SpiAffinity affinity = SpiAffinity::SpiAffinity_Auto;
};

enum SpiLines
{
    SpiLines_Single = 1,
    SpiLines_Quad = 4,
    SpiLines_Octal = 8
};

class Spi
{
protected:
    spi_device_handle_t deviceHandle;

    SpiType type;
    SpiMasterConfig masterConfig;
    SpiSlaveConfig slaveConfig;
    SpiLines lines = SpiLines::SpiLines_Single;

    void dispose();
    SpiLines getLines(GpioNum *data);
    unsigned int getBusFlags(SpiLines lines);

public:
    Spi(const SpiSlaveConfig &config);
    Spi(const SpiMasterConfig &config);
    ~Spi();

    void slave(const SpiSlaveConfig &config);
    void master(const SpiMasterConfig &config);

    Byte Transfer(Byte *data, size_t length);
    int Receive(Byte *output, unsigned int length, unsigned int timeout = SPI_DEFAULT_TIMEOUT);

    SpiFrequency Frequency();
    unsigned int Frequency(bool actual);
    void Frequency(SpiFrequency frequency);

    // static void Open(const SpiBusConfig &config);
    // static void Close(const SpiBusConfig &config);
    // static void Close(SpiBus bus);
};