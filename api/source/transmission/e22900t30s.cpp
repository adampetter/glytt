#include "api/transmission/e22900t30s.h"
#include "api/common/time.h"
#include "api/io/gpio.h"
#include "api/common/utility.h"
#include <cstring>

E22900T30S::E22900T30S(const E22900T30SConfig &config)
{
    this->config = config;
    this->uart = config.serial.uart;

    if (this->uart == NULL)
    {
#if DEBUG
        printf("(E22900T30S) Undefined Uart\n");
#endif
        standStill();
    }

    // Gpio modes
    if (this->config.interupt.aux == GPIO_NONE)
    {
#if DEBUG
        printf("(E22900T30S) Undefined aux pin\n");
#else
        standStill();
#endif
    }

    if (this->config.interupt.expander == NULL)
        Gpio::Mode(this->config.interupt.aux, GpioMode::GpioMode_Input);

    if (this->config.mode.m0 == GPIO_NONE || this->config.mode.m1 == GPIO_NONE)
    {
#if DEBUG
        printf("(E22900T30S) Undefined mode pin\n");
#else
        standStill();
#endif
    }

    if (!this->config.mode.expander)
    {
        Gpio::Mode(this->config.mode.m0, GpioMode::GpioMode_Output);
        Gpio::Mode(this->config.mode.m1, GpioMode::GpioMode_Output);
    }

    // Start mode
    mode(E22900T30SMode::Mode_Normal);

    E22900T30SConfig tmp;
    bool result = registry(&tmp);

    if (result)
        result = configure(&this->config);

    // Clear uart buffers
    this->uart->Flush();
}

E22900T30S::~E22900T30S()
{
    if (this->uart)
        delete this->uart;
}

unsigned long long E22900T30S::freq(Byte channel)
{
    return 850125000 + channel * 1000000;
}

// Reads registry from module and outputs values to config reference. Returns true if successful.
bool E22900T30S::registry(E22900T30SConfig *output)
{
#if DEBUG
    printf("(E22900T30S) Reading registry...");
#endif

    bool success = false;
    unsigned char attempts = 6;

    Byte parameters[10] = {0};
    mode(E22900T30SMode::Mode_Config);

    do
    {
        // See https://www.manualslib.com/manual/2415956/Ebyte-E22-900t30dc.html (page 15)
        this->uart->Flush();
        /*this->uart->Write(0xC1); // Command
        this->uart->Write(0x00); // Address
        this->uart->Write(0x07); // Length*/

        parameters[0] = 0xC1;
        parameters[1] = 0x00;
        parameters[2] = 0x07;
        this->uart->Write(parameters, 3);
        memset(parameters, 0, 10);
        delay(E22900T30S_WAIT);

        int received = 0;
        long int start = millis();
        while (received < (int)sizeof(parameters) && (millis() - start) < E22900T30S_TIMEOUT)
        {
            int count = this->uart->Read((Byte *)&parameters[received], (Byte)(sizeof(parameters) - received));
            if (count > 0)
                received += count;
            else
                delay(E22900T30S_WAIT);
        }

        if (received < (int)sizeof(parameters))
        {
#ifdef DEBUG
            printf("(E22900T30S) Error: Failed to read from registry (%d/%d bytes)\n", received, (int)sizeof(parameters));
            printf("Printing parameters:\n");

            for (int i = 0; i < 10; i++)
                printDecHexBin(parameters[i]);
#else
            standStill();
#endif
        }

        mode(E22900T30SMode::Mode_Normal);

        if (parameters[0] == 0xC1 && parameters[1] == 0x00 && parameters[2] == 0x07)
        {
            output->network.address = (unsigned short)*(parameters + 3);
            output->network.id = parameters[5];

            Byte reg0 = parameters[6];
            output->serial.baudRate = (UartBaudrate)((reg0 >> 5) << 5);
            output->serial.parity = (E22900T30SSerialParity)((((unsigned char)(reg0 << 3)) >> 6) << 3);
            output->network.airDataRate = (E22900T30SAirDataRate)(((unsigned char)(reg0 << 5)) >> 5);

            Byte reg1 = parameters[7];
            output->network.subPacketSize = (E22900T30SSubPacketSize)((reg1 >> 5) << 5);
            output->network.ambientNoise = (E22900T30SRSSIAmbientNoise)((((unsigned char)(reg1 << 2)) >> 7) << 5);
            output->network.power = (E22900T30SPower)((unsigned char)(reg1 << 6) >> 6);

            Byte reg2 = parameters[8];
            output->network.channel = reg2;

            Byte reg3 = parameters[9];
            output->network.rssi = (E22900T30SRSSI)((reg3 >> 7) << 7);
            output->network.txmode = (E22900T30STransmissionMode)((((unsigned char)(reg3 << 1)) >> 7) << 6);
            output->network.reply = (E22900T30SReply)((((unsigned char)(reg3 << 2)) >> 7) << 5);
            output->network.lbt = (E22900T30SLBT)((((unsigned char)(reg3 << 3)) >> 7) << 4);
            output->wor.mode = (E22900T30SWakeOnRadio)((((unsigned char)(reg3 << 4)) >> 7) << 3);
            output->wor.interval = (E22900T30SWakeOnRadioInterval)(((unsigned char)(reg3 << 5)) >> 5);
            success = true;
            break;
        }
        else
        {
#ifdef DEBUG
            if (attempts > 0)
                printf("(E22900T30S) Error: Wrong registry response\n Retrying...\n");
#endif
            wait(100);
            attempts--;
        }

    } while (attempts > 0);

#ifdef DEBUG
    if (!success)
        printf("(E22900T30S) Error: Failed to read registry from module!\n");
    else
        printf("done\n");
#endif

    return success;
}

bool E22900T30S::configure(E22900T30SConfig *config)
{
#ifdef DEBUG
    printf("(E22900T30S) Writing to registry...");
#endif

    mode(E22900T30SMode::Mode_Config);

    Byte parameters[12] = {0};
    parameters[0] = 0xC0; // Command
    parameters[1] = 0x00; // Address
    parameters[2] = 0x09; // Length
    parameters[3] = *(Byte *)&config->network.address;
    parameters[4] = *(((Byte *)&config->network.address) + 1);
    parameters[5] = config->network.id;
    parameters[6] = (Byte)(config->serial.baudRate | config->serial.parity | config->network.airDataRate);
    parameters[7] = (Byte)(config->network.subPacketSize | config->network.ambientNoise | config->network.power);
    parameters[8] = config->network.channel;
    parameters[9] = (Byte)(config->network.rssi | config->network.txmode | config->network.reply | config->network.lbt | config->wor.mode | config->wor.interval);
    parameters[10] = config->security.encryptByteH;
    parameters[11] = config->security.encryptByteL;

    this->uart->Flush();
    int written = this->uart->Write((Byte *)&parameters, (Byte)sizeof(parameters));
    bool success = written == sizeof(parameters);

#ifdef DEBUG
    if (!success)
    {
        printf("(E22900T30S) Error: Writing to Uart\n");
        mode(E22900T30SMode::Mode_Normal);
        return false;
    }
#endif

    wait(1000);

    Byte response[3] = {0};
    int responseCount = this->uart->Read((Byte *)&response, (Byte)sizeof(response));
    success = responseCount == (int)sizeof(response) && response[0] == 0xC1 && response[1] == 0x00 && response[2] == 0x09;
    this->uart->Flush();

#ifdef DEBUG
    if (!success)
        printf("(E22900T30S) Error: Module response invalid\n");
    else
        printf("done\n");
#endif

    mode(E22900T30SMode::Mode_Normal);
    return success;
}

void E22900T30S::mode(E22900T30SMode mode)
{
    wait(E22900T30S_TIMEOUT);

    // Default mode (Normal)
    bool m0 = false;
    bool m1 = false;

    // Determine mode values
    if (mode == E22900T30SMode::Mode_Wakeup)
        m0 = true;
    else if (mode == E22900T30SMode::Mode_Sleep)
    {
        m0 = true;
        m1 = true;
    }
    else if (mode == E22900T30SMode::Mode_Config)
        m1 = true;

    if (this->config.mode.expander)
    {
        Expander *expander = this->config.mode.expander;
        expander->Write(this->config.mode.m0, m0);
        expander->Write(this->config.mode.m1, m1);
    }
    else
    {
        Gpio::Write(this->config.mode.m0, m0);
        Gpio::Write(this->config.mode.m1, m1);
    }

    this->tmode = mode;

    // Wait for mode change
    delay(E22900T30S_WAIT);

    // Clear any uart buffers
    this->uart->Flush();

    wait(1000);
}

E22900T30SMode E22900T30S::mode(void)
{
    return this->tmode;
}

// Waiting for E22900T30S to finish (AUX = High). Timeout as max waiting time
void E22900T30S::wait(unsigned int timeout)
{
    long int time = millis();

    // Kris: make sure millis() is not about to reach max data type limit and start over
    if (((long int)(time + timeout)) == 0)
        time = 0;

    // Kris: if AUX pin was supplied and look for HIGH state
    // note you can omit using AUX if no pins are available, but you will have to use delay() to let module finish
    // per data sheet control after aux goes high is 2ms so delay for at least that long
    // some MCU are slow so give 50 ms

    if (this->config.interupt.aux >= 0)
    {
        do
        {
            delay(E22900T30S_WAIT);

            if ((millis() - time) > timeout)
                break;

        } while (this->config.interupt.expander ? !this->config.interupt.expander->Read(this->config.interupt.aux) : !Gpio::Read(this->config.interupt.aux));
    }
    else
    {
        // Kris: if you can't use aux pin, use 4K7 pullup with Arduino
        // you may need to adjust this value if transmissions fail
        delay(E22900T30S_STATIC_WAIT);
    }
}

int E22900T30S::Receive(Byte *buffer, unsigned short size, unsigned int timeout)
{
    long int stime = millis();
    int count = -1;

    if (((long int)(stime + timeout)) == 0)
        stime = 0;

    do
    {
        if (this->uart->Available() >= size)
            count = this->uart->Read(buffer, size);
        else
            delay(E22900T30S_WAIT);

    } while (count < 0 && (millis() - stime) < timeout);

    if (count == size)
        this->uart->Flush();

    return count;
}

bool E22900T30S::Send(Byte *buffer, unsigned short length)
{
    if (this->tmode != E22900T30SMode::Mode_Normal)
        return false;

    int written = this->uart->Write(buffer, length);
    wait(10000); // Max wait is more than the longest possible transmission
    return written == length;
}

void E22900T30S::Debug()
{
    printf("E22900T30S Config:\n");
    printf("Address: %d\n", this->config.network.address);
    printf("ID: %d\n", this->config.network.id);
    printf("Air Data Rate: %d\n", this->config.network.airDataRate);
    printf("Sub Packet Size: %d\n", this->config.network.subPacketSize);
    printf("Power: %d\n", this->config.network.power);
    printf("Channel: %d\n", this->config.network.channel);
    printf("RSSI: %d\n", this->config.network.rssi);
    printf("Transmission Mode: %d\n", this->config.network.txmode);
    printf("Reply: %d\n", this->config.network.reply);
    printf("LBT: %d\n", this->config.network.lbt);
}
