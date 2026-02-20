#pragma once
#include "api/transmission/transceiver.h"
#include "api/io/expander.h"
#include "api/common/types.h"
#include "api/io/uart.h"

#ifdef TRANSCEIVER_PACKAGE_SIZE
#undef TRANSCEIVER_PACKAGE_SIZE
#endif
#define TRANSCEIVER_PACKAGE_SIZE 240
#define E22900T30S_WAIT 50
#define E22900T30S_STATIC_WAIT 1000
#define E22900T30S_TIMEOUT 5000


enum E22900T30SMode
{
    Mode_Normal = 0,
    Mode_Wakeup = 1,
    Mode_Sleep = 2,
    Mode_Config = 4
};

enum E22900T30SBaudRate
{
    BaudRate_1200 = 0,
    BaudRate_4800 = 64,
    BaudRate_9600 = 96,
    BaudRate_19200 = 128,
    BaudRate_38400 = 160,
    BaudRate_57600 = 192,
    BaudRate_115200 = 224
};

enum E22900T30SSerialParity
{
    Parity_8N1 = 0,
    Parity_8O1 = 8,
    Parity_8E1 = 16
};

enum E22900T30SAirDataRate
{
    Rate_300 = 0,
    Rate_1200 = 1,
    Rate_2400 = 2, // Default
    Rate_4800 = 3,
    Rate_9600 = 4,
    Rate_19200 = 5,
    Rate_38400 = 6,
    Rate_62500 = 7
};

enum E22900T30SSubPacketSize
{
    Size_240b = 0, // Default
    Size_128b = 64,
    Size_64b = 128,
    Size_32b = 192
};

enum E22900T30SRSSIAmbientNoise
{
    AmbientNoise_Enable = 32,
    AmbientNoise_Disable = 0 // Default
};

enum E22900T30SPower
{
    Power_30dBm = 0, // 1W. Default
    Power_27dBm = 1, // 0.5W
    Power_24dBm = 2, // 0.25W
    Power_21dBm = 3  // 0.125W
};

enum E22900T30SRSSI
{
    RSSI_Enabled = 128,
    RSSI_Disabled = 0 // Default
};

enum E22900T30STransmissionMode
{
    TransmissionMode_Fixed = 64,
    TransmissionMode_Transparent = 0 // Default
};

enum E22900T30SReply
{
    Reply_Enabled = 32,
    Reply_Disabled = 0
};

enum E22900T30SLBT
{
    LBT_Enabled = 16,
    LBT_Disabled = 0 // Default
};

enum E22900T30SWakeOnRadio
{
    WOR_Transmitter = 8,
    WOR_Receiver = 0 // Default
};

enum E22900T30SWakeOnRadioInterval
{
    WOR_Interval_500ms = 0,
    WOR_Interval_1000ms = 1,
    WOR_Interval_1500ms = 2,
    WOR_Interval_2000ms = 3, // Default
    WOR_Interval_2500ms = 4,
    WOR_Interval_3000ms = 5,
    WOR_Interval_3500ms = 6,
    WOR_Interval_4000ms = 7,
};

struct E22900T30SConfig
{
    struct Serial
    {
        Uart *uart = NULL;
        UartBaudrate baudRate = UartBaudrate::UartBaudrate_9600;
        E22900T30SSerialParity parity = E22900T30SSerialParity::Parity_8N1;
    } serial;

    struct Interupt
    {
        GpioNum aux = GPIO_NONE;
        Expander *expander = NULL;
    } interupt;

    struct Mode
    {
        GpioNum m0 = GPIO_NONE;
        GpioNum m1 = GPIO_NONE;
        Expander *expander = NULL;

    } mode;

    struct Network
    {
        Byte id = 0;
        unsigned short address = 0;
        E22900T30SAirDataRate airDataRate = E22900T30SAirDataRate::Rate_2400;
        E22900T30SSubPacketSize subPacketSize = E22900T30SSubPacketSize::Size_240b;
        E22900T30SPower power = E22900T30SPower::Power_30dBm;
        Byte channel = 18;
        E22900T30SRSSI rssi = E22900T30SRSSI::RSSI_Disabled;
        E22900T30SRSSIAmbientNoise ambientNoise = E22900T30SRSSIAmbientNoise::AmbientNoise_Disable;
        E22900T30STransmissionMode txmode = E22900T30STransmissionMode::TransmissionMode_Transparent;
        E22900T30SReply reply = E22900T30SReply::Reply_Disabled;
        E22900T30SLBT lbt = E22900T30SLBT::LBT_Disabled;
    } network;

    struct Security
    {
        Byte encryptByteH = 0; // Default. Write only. Used by module to as factor for transforming and encrypting over-air-signal
        Byte encryptByteL = 0; // Default. Write only. Used by module to as factor for transforming and encrypting over-air-signal
    } security;

    struct WOR
    {
        E22900T30SWakeOnRadio mode = E22900T30SWakeOnRadio::WOR_Receiver;
        E22900T30SWakeOnRadioInterval interval = E22900T30SWakeOnRadioInterval::WOR_Interval_2000ms;
    } wor;
};

class E22900T30S : public Transceiver
{
private:
    E22900T30SConfig config;

protected:
    Uart *uart = NULL;
    E22900T30SMode tmode;

    bool registry(E22900T30SConfig *output);
    bool configure(E22900T30SConfig *config);
    void mode(E22900T30SMode mode);
    E22900T30SMode mode();
    void wait(unsigned int timeout);
    unsigned long long freq(Byte channel);

public:
    E22900T30S(const E22900T30SConfig &config);
    ~E22900T30S();

    int Receive(Byte *buffer, unsigned short length, unsigned int timeout);
    bool Send(Byte *buffer, unsigned short length = 1);
    void Debug();
};