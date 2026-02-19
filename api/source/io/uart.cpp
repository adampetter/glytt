#include "api/io/uart.h"
#include "esp_err.h"

Uart::Uart(const UartConfig& config)
{
    this->config = config;

    uart_config_t c = {
        .baud_rate = (int)config.baudRate,
        .data_bits = (uart_word_length_t)config.data_bits,
        .parity = (uart_parity_t)config.parity,
        .stop_bits = (uart_stop_bits_t)config.stop_bits,
        .flow_ctrl = (uart_hw_flowcontrol_t)config.flow_ctrl,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = (uart_sclk_t)config.source_clk};

    ESP_ERROR_CHECK(
        uart_driver_install(
            (uart_port_t)config.port,
            config.rxBufferSize * 2,
            config.txBufferSize,
            config.queueSize,
            config.queueHandle,
            config.interuptFlags));

    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config((uart_port_t)config.port, &c));

    // Set UART pins(TX, RX, RTS, CTS)
    ESP_ERROR_CHECK(
        uart_set_pin(
            (uart_port_t)config.port,
            config.tx,
            config.rx,
            config.rst,
            config.cst));
}

Uart::~Uart()
{
    ESP_ERROR_CHECK(uart_driver_delete((uart_port_t)this->config.port));
}

/*
    Returns number of bytes available to read in Rx-buffer
*/
int Uart::Available()
{
    size_t size;
    ESP_ERROR_CHECK(uart_get_buffered_data_len((uart_port_t)this->config.port, &size));
    return size;
}

/*
    Writes bytes to Tx. Returns number of bytes that was successfully written.
*/
int Uart::Write(Byte *data, int length)
{
    return uart_write_bytes(
        (uart_port_t)this->config.port,
        data,
        length);
}

/*
    Reads bytes from Rx-buffer. Writes read bytes to buffer. Returns number of bytes written to output buffer.
*/
int Uart::Read(Byte *buffer, int length)
{    
    return uart_read_bytes(
        (uart_port_t)this->config.port,
        buffer,
        length,
        this->config.readTimeout);
}

/*
    Empty Rx-buffer
*/
void Uart::Flush()
{
    ESP_ERROR_CHECK(
        uart_flush(
            (uart_port_t)this->config.port));
}

void Uart::Disable(UartPort port){
    uart_set_hw_flow_ctrl((uart_port_t)port, UART_HW_FLOWCTRL_DISABLE, 0);
}

UartBaudrate Uart::BaudRate(){
    return this->config.baudRate;
}