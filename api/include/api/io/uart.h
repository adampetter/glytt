#pragma once
#include "api/common/types.h"
#include "driver/uart.h"
#include "esp_intr_alloc.h"
#include <string>

using namespace std;

enum UartPort
{
    UartPort_0 = UART_NUM_0, // Reserved for serial communication / flashing
    UartPort_1 = UART_NUM_1,
    UartPort_2 = UART_NUM_2
};

enum UartBaudrate
{
    UartBaudrate_1200 = 1200,
    UartBaudrate_4800 = 4800,
    UartBaudrate_9600 = 9600,
    UartBaudrate_19200 = 19200,
    UartBaudrate_38400 = 38400,
    UartBaudrate_57600 = 57600,
    UartBaudrate_115200 = 115200
};

struct UartConfig
{
    UartPort port = UartPort::UartPort_1;
    UartBaudrate baudRate = UartBaudrate::UartBaudrate_9600;
    GpioNum tx = GPIO_NONE;
    GpioNum rx = GPIO_NONE;
    GpioNum rst = GPIO_NONE;
    GpioNum cst = GPIO_NONE;

    unsigned char data_bits = UART_DATA_8_BITS;
    unsigned char parity = UART_PARITY_DISABLE;
    unsigned char stop_bits = UART_STOP_BITS_1;
    int flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    int source_clk = UART_SCLK_APB;
    int rxBufferSize = 256;
    int txBufferSize = 0;
    int queueSize = 0;
    QueueHandle_t *queueHandle = nullptr;
    int interuptFlags = 0;
    int readTimeout = 100;
};

class Uart
{
private:
    UartConfig config;

public:
    Uart(const UartConfig &config);
    ~Uart();

    int Available();
    int Write(Byte *data, int length = 1);
    int Read(Byte *buffer, int length = 1);
    void Flush();
    UartBaudrate BaudRate();

    static void Disable(UartPort port);
};