#include "api/transmission/e22900t30.h"
#include "api/common/time.h"
#include "api/io/gpio.h"
#include "api/common/utility.h"
#include <cstring>

namespace
{
    Byte ToE22BaudRegister(UartBaudrate baudrate)
    {
        switch (baudrate)
        {
        case UartBaudrate::UartBaudrate_1200:
            return E22900T30BaudRate::BaudRate_1200;
        case UartBaudrate::UartBaudrate_4800:
            return E22900T30BaudRate::BaudRate_4800;
        case UartBaudrate::UartBaudrate_9600:
            return E22900T30BaudRate::BaudRate_9600;
        case UartBaudrate::UartBaudrate_19200:
            return E22900T30BaudRate::BaudRate_19200;
        case UartBaudrate::UartBaudrate_38400:
            return E22900T30BaudRate::BaudRate_38400;
        case UartBaudrate::UartBaudrate_57600:
            return E22900T30BaudRate::BaudRate_57600;
        case UartBaudrate::UartBaudrate_115200:
            return E22900T30BaudRate::BaudRate_115200;
        default:
            return E22900T30BaudRate::BaudRate_9600;
        }
    }

    UartBaudrate FromE22BaudRegister(Byte registerValue)
    {
        Byte baudBits = registerValue & 0xE0;
        switch (baudBits)
        {
        case E22900T30BaudRate::BaudRate_1200:
            return UartBaudrate::UartBaudrate_1200;
        case E22900T30BaudRate::BaudRate_4800:
            return UartBaudrate::UartBaudrate_4800;
        case E22900T30BaudRate::BaudRate_9600:
            return UartBaudrate::UartBaudrate_9600;
        case E22900T30BaudRate::BaudRate_19200:
            return UartBaudrate::UartBaudrate_19200;
        case E22900T30BaudRate::BaudRate_38400:
            return UartBaudrate::UartBaudrate_38400;
        case E22900T30BaudRate::BaudRate_57600:
            return UartBaudrate::UartBaudrate_57600;
        case E22900T30BaudRate::BaudRate_115200:
            return UartBaudrate::UartBaudrate_115200;
        default:
            return UartBaudrate::UartBaudrate_9600;
        }
    }
}

E22900T30::E22900T30(const E22900T30Config &config)
{
    this->config = config;
    this->uart = config.serial.uart;

    if (this->uart == NULL)
    {
#if E22900T30_DEBUG
        printf("(E22900T30) Undefined Uart\n");
#endif
        standStill();
    }

    if (this->config.startupInNormalModeOnly)
    {
        this->tmode = E22900T30Mode::Mode_Normal;
        this->ready = true;
#if E22900T30_DEBUG
        printf("(E22900T30) Startup in normal mode only (registry/config bypassed)\n");
#endif
        this->uart->Flush();
        return;
    }

    // Gpio modes
    if (this->config.interupt.aux == GPIO_NONE)
    {
#if E22900T30_DEBUG
        printf("(E22900T30) AUX pin undefined; using delay-based waits\n");
#endif
    }

    if (this->config.interupt.aux != GPIO_NONE && this->config.interupt.expander == NULL)
    {
        Gpio::Mode(this->config.interupt.aux, GpioMode::GpioMode_Input);
        // Keep AUX from floating if board wiring is weak/noisy.
        Gpio::Pull(this->config.interupt.aux, GpioPull::GpioPull_Up);
    }

    if (this->config.mode.m0 == GPIO_NONE || this->config.mode.m1 == GPIO_NONE)
    {
#if E22900T30_DEBUG
        printf("(E22900T30) Undefined mode pin\n");
#else
        standStill();
#endif
    }

    if (!this->config.mode.expander)
    {
        // Keep input path enabled so debug readback can observe driven state.
        Gpio::Mode(this->config.mode.m0, GpioMode::GpioMode_Input_Output);
        Gpio::Mode(this->config.mode.m1, GpioMode::GpioMode_Input_Output);
        // Start from deterministic normal mode levels before first mode() call.
        Gpio::Write(this->config.mode.m0, false);
        Gpio::Write(this->config.mode.m1, false);

#if E22900T30_DEBUG
        const int m0Low = (int)Gpio::Read(this->config.mode.m0);
        const int m1Low = (int)Gpio::Read(this->config.mode.m1);
        Gpio::Write(this->config.mode.m0, true);
        Gpio::Write(this->config.mode.m1, true);
        delay(E22900T30_WAIT);
        const int m0High = (int)Gpio::Read(this->config.mode.m0);
        const int m1High = (int)Gpio::Read(this->config.mode.m1);
        printf("(E22900T30) Pin drive test M0 low/high=%d/%d, M1 low/high=%d/%d\n", m0Low, m0High, m1Low, m1High);
        if (m0High == 0 || m1High == 0)
            printf("(E22900T30) WARN: mode pin readback did not go high; check pin mapping, shorts, pull-down strength, or board pin capabilities\n");
        Gpio::Write(this->config.mode.m0, false);
        Gpio::Write(this->config.mode.m1, false);
#endif
    }

    // Let the radio fully boot before mode switching and register access.
    delay(E22900T30_POWERUP_SETTLE_MS);

    // Start mode
    mode(E22900T30Mode::Mode_Normal);

    bool result = false;
    if (this->config.interupt.aux == GPIO_NONE)
    {
        // No AUX means busy signaling is unavailable; skip long registry probe and
        // go straight to configuration write in Mode2 over 9600 8N1.
        result = configure(&this->config);
    }
    else
    {
        E22900T30Config tmp;
        result = registry(&tmp);

#if E22900T30_DEBUG
        if (!result)
            printf("(E22900T30) Registry read failed; trying direct configure\n");
#endif

        result = configure(&this->config);
    }

    this->ready = result;

    if (!this->ready)
    {
        // Some installations cannot reliably enter config mode (M0/M1 wiring or timing).
        // Keep module in normal mode, but report readiness accurately so callers can probe.
        mode(E22900T30Mode::Mode_Normal);
#if E22900T30_DEBUG
        printf("(E22900T30) Init failed; falling back to normal mode\n");
#endif
    }

    // Clear uart buffers
    this->uart->Flush();
}

E22900T30::~E22900T30()
{
    if (this->uart)
        delete this->uart;
}

unsigned long long E22900T30::freq(Byte channel)
{
    return 850125000 + channel * 1000000;
}

// Reads registry from module and outputs values to config reference. Returns true if successful.
bool E22900T30::registry(E22900T30Config *output)
{
#if E22900T30_DEBUG
    printf("(E22900T30) Reading registry...");
#endif

    bool success = false;
    // Per E22-T manual, register access in configuration mode is fixed to 9600 8N1.
    const UartBaudrate baudCandidates[] = {UartBaudrate::UartBaudrate_9600};

    Byte response[12] = {0};
    const Byte readCommands[][3] = {
        {0xC1, 0x00, 0x09},
        {0xC1, 0x00, 0x07},
        {0xC1, 0xC1, 0xC1},
    };

    for (unsigned int b = 0; b < (sizeof(baudCandidates) / sizeof(baudCandidates[0])) && !success; b++)
    {
        this->uart->SetBaudRate(baudCandidates[b]);
#if E22900T30_DEBUG
        printf("\n(E22900T30) Registry probe baud=%d\n", (int)baudCandidates[b]);
#endif

        unsigned char attempts = 3;
        mode(E22900T30Mode::Mode_Config);

        do
        {
            int received = 0;
            memset(response, 0, sizeof(response));

            for (unsigned int c = 0; c < (sizeof(readCommands) / sizeof(readCommands[0])) && !success; c++)
            {
                // See E22 manual (command/read section): modules can respond to different C1 read forms.
                this->uart->Flush();
                this->uart->Write((Byte *)&readCommands[c], 3);
                memset(response, 0, sizeof(response));
                delay(E22900T30_WAIT);

                received = 0;
                long int start = millis();
                while (received < (int)sizeof(response) && (millis() - start) < E22900T30_TIMEOUT)
                {
                    int count = this->uart->Read((Byte *)&response[received], (Byte)(sizeof(response) - received));
                    if (count > 0)
                        received += count;
                    else
                        delay(E22900T30_WAIT);
                }

                if (received >= 10 && response[0] == 0xC1 && response[1] == 0x00 && (response[2] == 0x07 || response[2] == 0x09))
                {
                    output->network.address = (unsigned short)(((unsigned short)response[3] << 8) | response[4]);
                    output->network.id = response[5];

                    Byte reg0 = response[6];
                    output->serial.baudRate = FromE22BaudRegister(reg0);
                    output->serial.parity = (E22900T30SerialParity)((((unsigned char)(reg0 << 3)) >> 6) << 3);
                    output->network.airDataRate = (E22900T30AirDataRate)(((unsigned char)(reg0 << 5)) >> 5);

                    Byte reg1 = response[7];
                    output->network.subPacketSize = (E22900T30SubPacketSize)((reg1 >> 5) << 5);
                    output->network.ambientNoise = (E22900T30RSSIAmbientNoise)((((unsigned char)(reg1 << 2)) >> 7) << 5);
                    output->network.power = (E22900T30Power)((unsigned char)(reg1 << 6) >> 6);

                    Byte reg2 = response[8];
                    output->network.channel = reg2;

                    Byte reg3 = response[9];
                    output->network.rssi = (E22900T30RSSI)((reg3 >> 7) << 7);
                    output->network.txmode = (E22900T30TransmissionMode)((((unsigned char)(reg3 << 1)) >> 7) << 6);
                    output->network.reply = (E22900T30Reply)((((unsigned char)(reg3 << 2)) >> 7) << 5);
                    output->network.lbt = (E22900T30LBT)((((unsigned char)(reg3 << 3)) >> 7) << 4);
                    output->wor.mode = (E22900T30WakeOnRadio)((((unsigned char)(reg3 << 4)) >> 7) << 3);
                    output->wor.interval = (E22900T30WakeOnRadioInterval)(((unsigned char)(reg3 << 5)) >> 5);
                    output->security.encryptByteH = response[10];
                    output->security.encryptByteL = response[11];
                    success = true;
                    break;
                }

#if E22900T30_DEBUG
                printf("(E22900T30) Read cmd %u response: %d bytes\n", c + 1, received);
#endif
            }

            if (!success)
            {
#if E22900T30_DEBUG
                printf("(E22900T30) Error: Failed to read valid registry response\n");
                printf("Printing response:\n");
                for (int i = 0; i < (int)sizeof(response); i++)
                    printDecHexBin(response[i]);
#endif
            }

            if (success)
                break;
            else
            {
#if E22900T30_DEBUG
                if (attempts > 1)
                    printf("(E22900T30) Error: Wrong registry response\n Retrying...\n");
#endif
                wait(100);
                attempts--;
            }

        } while (attempts > 0);

        mode(E22900T30Mode::Mode_Normal);
    }

#if E22900T30_DEBUG
    if (!success)
        printf("(E22900T30) Error: Failed to read registry from module!\n");
    else
        printf("done\n");
#endif

    return success;
}

bool E22900T30::configure(E22900T30Config *config)
{
#if E22900T30_DEBUG
    printf("(E22900T30) Writing to registry...");
#endif

    // Per E22-T manual, configuration mode serial is fixed at 9600 8N1.
    this->uart->SetBaudRate(UartBaudrate::UartBaudrate_9600);

    mode(E22900T30Mode::Mode_Config);

    Byte parameters[12] = {0};
    parameters[0] = 0xC0; // Command
    parameters[1] = 0x00; // Address
    parameters[2] = 0x09; // Length
    parameters[3] = *(Byte *)&config->network.address;
    parameters[4] = *(((Byte *)&config->network.address) + 1);
    parameters[5] = config->network.id;
    parameters[6] = (Byte)(ToE22BaudRegister(config->serial.baudRate) | config->serial.parity | config->network.airDataRate);
    parameters[7] = (Byte)(config->network.subPacketSize | config->network.ambientNoise | config->network.power);
    parameters[8] = config->network.channel;
    parameters[9] = (Byte)(config->network.rssi | config->network.txmode | config->network.reply | config->network.lbt | config->wor.mode | config->wor.interval);
    parameters[10] = config->security.encryptByteH;
    parameters[11] = config->security.encryptByteL;

    this->uart->Flush();
    int written = this->uart->Write((Byte *)&parameters, (Byte)sizeof(parameters));
    bool success = written == sizeof(parameters);

#if E22900T30_DEBUG
    if (!success)
    {
        printf("(E22900T30) Error: Writing to Uart\n");
        mode(E22900T30Mode::Mode_Normal);
        return false;
    }
#endif

    // Some E22 variants echo write payload while others don't. Do not trust ACK shape alone.
    // Verify by reading registry back and matching effective settings.
    wait(200);

    Byte response[12] = {0};
    int responseCount = 0;
    long int start = millis();
    while (responseCount < (int)sizeof(response) && (millis() - start) < 300)
    {
        int count = this->uart->Read((Byte *)&response[responseCount], (Byte)(sizeof(response) - responseCount));
        if (count > 0)
            responseCount += count;
        else
            delay(E22900T30_WAIT);
    }

    mode(E22900T30Mode::Mode_Normal);

    E22900T30Config applied = {};
    bool readBack = registry(&applied);
    // Require a deterministic readback, but validate only link-critical fields
    // to avoid false negatives from module-specific non-critical bits.
    bool verified = readBack &&
                    applied.network.address == config->network.address &&
                    applied.network.id == config->network.id &&
                    applied.network.airDataRate == config->network.airDataRate &&
                    applied.network.subPacketSize == config->network.subPacketSize &&
                    applied.network.power == config->network.power &&
                    applied.network.channel == config->network.channel &&
                    applied.network.txmode == config->network.txmode &&
                    applied.security.encryptByteH == config->security.encryptByteH &&
                    applied.security.encryptByteL == config->security.encryptByteL;

    success = success && verified;
    this->uart->Flush();

    if (!verified)
    {
        printf("[E22900T30][CFG] verify failed readBack=%d req(addr=%u id=%u ch=%u dr=%u pwr=%u txm=%u enc=%u/%u) got(addr=%u id=%u ch=%u dr=%u pwr=%u txm=%u enc=%u/%u)\n",
               readBack ? 1 : 0,
               (unsigned int)config->network.address,
               (unsigned int)config->network.id,
               (unsigned int)config->network.channel,
               (unsigned int)config->network.airDataRate,
               (unsigned int)config->network.power,
               (unsigned int)config->network.txmode,
               (unsigned int)config->security.encryptByteH,
               (unsigned int)config->security.encryptByteL,
               (unsigned int)applied.network.address,
               (unsigned int)applied.network.id,
               (unsigned int)applied.network.channel,
               (unsigned int)applied.network.airDataRate,
               (unsigned int)applied.network.power,
               (unsigned int)applied.network.txmode,
               (unsigned int)applied.security.encryptByteH,
               (unsigned int)applied.security.encryptByteL);
    }

#if E22900T30_DEBUG
    if (!success)
        printf("(E22900T30) Error: Configure verify failed (writeEchoBytes=%d, readBack=%d)\n", responseCount, readBack ? 1 : 0);
    else
        printf("done\n");
#endif
    return success;
}

void E22900T30::mode(E22900T30Mode mode)
{
    wait(E22900T30_TIMEOUT);

    // Default mode (Normal)
    bool m0 = false;
    bool m1 = false;

    // Determine mode values
    if (mode == E22900T30Mode::Mode_Wakeup)
        m0 = true;
    else if (mode == E22900T30Mode::Mode_Sleep)
    {
        m0 = true;
        m1 = true;
    }
    else if (mode == E22900T30Mode::Mode_Config)
    {
        // E22-T Mode2 (configuration mode): M1=1, M0=0.
        m1 = true;
    }

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
    delay(E22900T30_WAIT);

#if E22900T30_DEBUG
    if (!this->config.mode.expander)
    {
        const int m0Level = (int)Gpio::Read(this->config.mode.m0);
        const int m1Level = (int)Gpio::Read(this->config.mode.m1);

        if (this->config.interupt.aux != GPIO_NONE)
        {
            const int auxLevel = (int)Gpio::Read(this->config.interupt.aux);
            printf("(E22900T30) Mode=%d M0=%d M1=%d AUX=%d\n", (int)mode, m0Level, m1Level, auxLevel);
        }
        else
        {
            printf("(E22900T30) Mode=%d M0=%d M1=%d AUX=n/a\n", (int)mode, m0Level, m1Level);
        }
    }
#endif

    // Clear any uart buffers
    this->uart->Flush();

    wait(1000);
}

E22900T30Mode E22900T30::mode(void)
{
    return this->tmode;
}

// Waiting for E22900T30 to finish (AUX = High). Timeout as max waiting time
void E22900T30::wait(unsigned int timeout)
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
        bool timedOut = false;
        do
        {
            delay(E22900T30_WAIT);

            if ((millis() - time) > timeout)
            {
                timedOut = true;
                break;
            }

        } while (this->config.interupt.expander ? !this->config.interupt.expander->Read(this->config.interupt.aux) : !Gpio::Read(this->config.interupt.aux));

        // AUX is optional on E22 modules. If it appears stuck low, switch to
        // delay-based waits so init/tx flow can still progress.
        if (timedOut)
        {
#if E22900T30_DEBUG
            printf("(E22900T30) AUX timeout; disabling AUX wait fallback\n");
#endif
            this->config.interupt.aux = GPIO_NONE;
            delay(E22900T30_STATIC_WAIT);
        }
    }
    else
    {
        // Kris: if you can't use aux pin, use 4K7 pullup with Arduino
        // you may need to adjust this value if transmissions fail
        delay(E22900T30_STATIC_WAIT);
    }
}

int E22900T30::Receive(Byte *buffer, unsigned short size, unsigned int timeout)
{
    long int stime = millis();
    int received = 0;

    if (((long int)(stime + timeout)) == 0)
        stime = 0;

    do
    {
        int available = this->uart->Available();
        if (available > 0)
        {
            int remaining = (int)size - received;
            int chunk = available < remaining ? available : remaining;
            int count = this->uart->Read((Byte *)&buffer[received], (Byte)chunk);
            if (count > 0)
                received += count;
            else
                delay(E22900T30_WAIT);
        }
        else
            delay(E22900T30_WAIT);

    } while (received < (int)size && (millis() - stime) < timeout);

    if (received == (int)size)
        this->uart->Flush();

    return received > 0 ? received : -1;
}

bool E22900T30::Send(Byte *buffer, unsigned short length)
{
    if (this->tmode != E22900T30Mode::Mode_Normal)
        return false;

    int written = this->uart->Write(buffer, length);

    // Keep send path short to avoid freezing alarm LED/buzzer cadence.
    delay(E22900T30_WAIT);

    return written == length;
}

bool E22900T30::Ready() const
{
    return this->ready;
}

void E22900T30::Debug()
{
    printf("E22900T30 Config:\n");
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
