#include "api/io/spi.h"
#include "api/common/time.h"
#include <cstring>

void spi_slave_pre(spi_slave_transaction_t *transaction)
{
    // Handshake
    GpioNum gpio = *(GpioNum *)transaction->user;
    if (gpio != GPIO_NONE)
        Gpio::Write(gpio, false);
}

void spi_slave_post(spi_slave_transaction_t *transaction)
{
    // Handshake
    GpioNum gpio = *(GpioNum *)transaction->user;
    if (gpio != GPIO_NONE)
        Gpio::Write(gpio, true);
}

void Spi::dispose()
{
    spi_bus_remove_device(this->deviceHandle);
    // spi_slave_hd_deinit

    if (this->type == SpiType::SpiType_Master)
        spi_bus_free((spi_host_device_t)this->masterConfig.bus);
    else
        spi_bus_free((spi_host_device_t)this->slaveConfig.bus);
}

SpiLines Spi::getLines(GpioNum *data)
{
    int dataCount = 0;
    for (int i = 0; i < SPI_MAX_DATA_LINES; i++)
        if (data[i] > 0)
            dataCount++;
        else
            break;

    if (dataCount != 0 && dataCount != 4 && dataCount != 8)
    {
        printf("Invalid amount of data GPIOs: %d (supported: 0, 4, 8). Falling back to single line.\n", dataCount);
        return SpiLines::SpiLines_Single;
    }
    else if (dataCount == 8)
        return SpiLines::SpiLines_Octal;
    else if (dataCount == 4)
        return SpiLines::SpiLines_Quad;
    else
        return SpiLines::SpiLines_Single;
}

unsigned int Spi::getBusFlags(SpiLines lines)
{
    if (lines == SpiLines::SpiLines_Quad)
        return SPICOMMON_BUSFLAG_QUAD;
    else if (lines == SpiLines::SpiLines_Octal)
        return SPICOMMON_BUSFLAG_OCTAL;
    else
        return 0;
}

Spi::Spi(const SpiMasterConfig &config)
{
    this->master(config);
    printf("SPI (Master) initialized.\n"); // TMP
}

Spi::Spi(const SpiSlaveConfig &config)
{
    this->slave(config);
    printf("SPI (Slave) initialized.\n"); // TMP
}

Spi::~Spi()
{
    this->dispose();
}

void Spi::slave(const SpiSlaveConfig &config)
{
    this->slaveConfig = config;
    this->type = SpiType::SpiType_Slave;

    this->lines = this->getLines((GpioNum *)config.data);

    // Config for the SPI bus
    spi_bus_config_t busConfig = {};
    busConfig.mosi_io_num = (this->lines != SpiLines::SpiLines_Single && config.mosi < 0 ? config.data[0] : config.mosi);
    busConfig.miso_io_num = (this->lines != SpiLines::SpiLines_Single && config.miso < 0 ? config.data[1] : config.miso);
    busConfig.sclk_io_num = config.sck;
    busConfig.quadwp_io_num = (this->lines != SpiLines::SpiLines_Single ? config.data[2] : GPIO_NONE);
    busConfig.quadhd_io_num = (this->lines != SpiLines::SpiLines_Single ? config.data[3] : GPIO_NONE);
    busConfig.data4_io_num = GPIO_NONE;
    busConfig.data5_io_num = GPIO_NONE;
    busConfig.data6_io_num = GPIO_NONE;
    busConfig.data7_io_num = GPIO_NONE;
    busConfig.max_transfer_sz = config.transferSize;
    busConfig.flags = this->getBusFlags(this->lines);
    busConfig.isr_cpu_id = (esp_intr_cpu_affinity_t)config.affinity;

    // Single line mode
    if (this->lines == SpiLines::SpiLines_Single)
    {
        // Config for the SPI slave interface
        spi_slave_interface_config_t slaveConfig = {};
        slaveConfig.spics_io_num = config.cs;
        slaveConfig.flags = 0;
        slaveConfig.queue_size = 3;
        slaveConfig.mode = config.mode;
        slaveConfig.post_setup_cb = this->slaveConfig.handshake != GPIO_NONE ? spi_slave_pre : NULL;
        slaveConfig.post_trans_cb = this->slaveConfig.handshake != GPIO_NONE ? spi_slave_post : NULL;

        // TODO ? Enable pull-ups on SPI lines so we don't detect rogue pulses when no master is connected.
        // OR Use external pull-ups on CLK and CS pins.
        
        // Initialize SPI slave interface
        ESP_ERROR_CHECK(spi_slave_initialize(SPI2_HOST, &busConfig, &slaveConfig, SPI_DMA_CH_AUTO));
    }
    // Multiline mode (Quad/Octal)
    else
    {
        spi_slave_hd_slot_config_t slaveConfig = {};
        slaveConfig.mode = config.mode;
        slaveConfig.spics_io_num = (unsigned int)config.cs;
        slaveConfig.flags = 0;
        slaveConfig.command_bits = (Byte)(this->lines != SpiLines::SpiLines_Single ? 8 : 0);
        slaveConfig.address_bits = (Byte)(this->lines != SpiLines::SpiLines_Single ? 8 : 0);
        slaveConfig.dummy_bits = (Byte)(this->lines != SpiLines::SpiLines_Single ? 8 : 0);
        slaveConfig.queue_size = 3;
        slaveConfig.dma_chan = SPI_DMA_CH_AUTO;
        slaveConfig.cb_config = {};

        spi_slave_hd_init((spi_host_device_t)config.bus, &busConfig, &slaveConfig);
    }

    // Spi handshake signal
    if (this->slaveConfig.handshake != GPIO_NONE)
    {
        Gpio::Mode(this->slaveConfig.handshake, GpioMode::GpioMode_Output);
        Gpio::Write(this->slaveConfig.handshake, true);
    }

    delay(SPI_WAIT);
}

void Spi::master(const SpiMasterConfig &config)
{
    this->masterConfig = config;
    this->type = SpiType::SpiType_Master;

    this->lines = this->getLines((GpioNum *)config.data);

    // Config for the SPI bus
    spi_bus_config_t busConfig = {};
    busConfig.mosi_io_num = (this->lines != SpiLines::SpiLines_Single && config.mosi < 0 ? config.data[0] : config.mosi);
    busConfig.miso_io_num = (this->lines != SpiLines::SpiLines_Single && config.miso < 0 ? config.data[1] : config.miso);
    busConfig.sclk_io_num = config.sck;
    busConfig.quadwp_io_num = (this->lines != SpiLines::SpiLines_Single ? config.data[2] : -1);
    busConfig.quadhd_io_num = (this->lines != SpiLines::SpiLines_Single ? config.data[3] : -1);
    busConfig.data4_io_num = GPIO_NONE;
    busConfig.data5_io_num = GPIO_NONE;
    busConfig.data6_io_num = GPIO_NONE;
    busConfig.data7_io_num = GPIO_NONE;
    busConfig.max_transfer_sz = config.transferSize;
    busConfig.flags = this->getBusFlags(this->lines);
    busConfig.isr_cpu_id = (esp_intr_cpu_affinity_t)config.affinity;

    // Config for the SPI device on the other side of the bus
    spi_device_interface_config_t deviceConfig = {};
    deviceConfig.command_bits = (Byte)(this->lines != SpiLines::SpiLines_Single ? 8 : 0);
    deviceConfig.address_bits = (Byte)(this->lines != SpiLines::SpiLines_Single ? 8 : 0);
    deviceConfig.dummy_bits = (Byte)(this->lines != SpiLines::SpiLines_Single ? 8 : 0);
    deviceConfig.mode = config.mode;
    deviceConfig.duty_cycle_pos = 128; // 50% duty cycle
    deviceConfig.cs_ena_posttrans = 3; // Keep the CS low 3 cycles after transaction, to stop slave from missing the last bit when CS has less propagation delay than CLK
    deviceConfig.clock_speed_hz = config.frequency;
    deviceConfig.spics_io_num = config.cs;
    deviceConfig.flags = SPI_DEVICE_HALFDUPLEX;
    deviceConfig.queue_size = 3;

    ESP_ERROR_CHECK(spi_bus_initialize((spi_host_device_t)config.bus, &busConfig, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device((spi_host_device_t)config.bus, &deviceConfig, &this->deviceHandle));

    delay(SPI_WAIT);
}

Byte Spi::Transfer(Byte *data, size_t length)
{
    if (this->type != SpiType::SpiType_Master)
        return 0;

    spi_transaction_t transaction;
    memset(&transaction, 0, sizeof(transaction));

    transaction.tx_buffer = data;
    transaction.length = length * BITS_PER_Byte;

    if (this->lines == SpiLines::SpiLines_Quad)
        transaction.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_MODE_DIOQIO_ADDR;
    else if (this->lines == SpiLines::SpiLines_Octal)
        transaction.flags = SPI_TRANS_MODE_OCT;

    ESP_ERROR_CHECK(spi_device_transmit(
        this->deviceHandle,
        &transaction));

    return transaction.rx_data[0];
}

int Spi::Receive(Byte *output, unsigned int length, unsigned int timeout)
{
    if (this->type != SpiType::SpiType_Slave)
        return 0;
    else if (length % 4 != 0)
        return 0;

    if (this->lines == SpiLines::SpiLines_Single)
    {
        spi_slave_transaction_t transaction;
        memset(&transaction, 0, sizeof(transaction));

        transaction.length = length * BITS_PER_Byte;
        transaction.rx_buffer = (void *)output;
        transaction.user = &this->slaveConfig.handshake;

        ESP_ERROR_CHECK(spi_slave_transmit((spi_host_device_t)this->slaveConfig.bus, &transaction, timeout));

        return transaction.trans_len / BITS_PER_Byte;
    }
    else
    {
        spi_slave_hd_data_t transaction;
        memset(&transaction, 0, sizeof(transaction));
        transaction.data = output;
        transaction.len = length;

        // printf("Queued\n");

        ESP_ERROR_CHECK(spi_slave_hd_queue_trans((spi_host_device_t)this->slaveConfig.bus, SPI_SLAVE_CHAN_RX, &transaction, timeout));

        // printf("Getting results\n");

        spi_slave_hd_data_t *transactionPtr = &transaction;

        if (this->slaveConfig.handshake != GPIO_NONE)
            Gpio::Write(this->slaveConfig.handshake, false);

        ESP_ERROR_CHECK(spi_slave_hd_get_trans_res((spi_host_device_t)this->slaveConfig.bus, SPI_SLAVE_CHAN_RX, &transactionPtr, timeout));

        if (this->slaveConfig.handshake != GPIO_NONE)
            Gpio::Write(this->slaveConfig.handshake, true);

        // printf("Done\n");

        return transactionPtr->trans_len / BITS_PER_Byte;
    }
}

SpiFrequency Spi::Frequency()
{
    if (this->type == SpiType::SpiType_Master)
        return this->masterConfig.frequency;
    else
        return (SpiFrequency)0;
}

unsigned int Spi::Frequency(bool actual)
{
    if (this->type != SpiType::SpiType_Master)
        return 0;

    if (actual)
    {
        int freq = this->masterConfig.frequency;
        // spi_device_get_actual_freq(this->handle, &freq);
        return freq;
    }
    else
        return (unsigned int)this->masterConfig.frequency;
}

void Spi::Frequency(SpiFrequency frequency)
{
    if (this->type != SpiType::SpiType_Master)
        return;

    this->masterConfig.frequency = frequency;

    this->dispose();
    this->master(this->masterConfig);
}