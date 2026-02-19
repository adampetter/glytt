#include "api/motion/lis3dh.h"

Lis3dh::Lis3dh(const Lis3dhConfig *config) : Accelerometer(), Interrupt(config->interrupt, InterruptType::InterruptType_Low)
{
    this->config = *config;
    this->i2c = config->i2c;

    // Set sample rate and enable axis
    this->i2c->Write(this->config.address, 0x20, (Byte)((Byte)this->config.sampleRate | (Byte)this->config.axis));

    // CTRL_REG4: BDU=1 (block data update) to avoid mixed high/low bytes during reads.
    this->i2c->Write(this->config.address, 0x23, 0x80);

    // Set mode
    this->i2c->Write(this->config.address, 0x2E, this->config.mode);

    // Config interupt pin if pin number is provided
    if (config->interrupt != GPIO_NONE)
    {
        // Set 4D detection is enabled on INT1
        //this->i2c->Write(this->config.address, 0x24, 0x4);

        // Set interupt threshold
        this->i2c->Write(this->config.address, 0x32, config->interruptThreshold);

        // Set interupt duration
        this->i2c->Write(this->config.address, 0x33, config->interruptDuration);

        // Set axis to trigger interupt (0xFF = all)
        this->i2c->Write(this->config.address, 0x30, 0xFF);

        // Enable interrupt 1
        this->i2c->Write(this->config.address, 0x22, 0x40);

        // Clearing interupt by reading from interupt registry
        Byte data;
        this->i2c->Read(this->config.address, 0x31, &data, sizeof(data));
    }

    if(config->wakeupDuration){

        //Set the wakeup threshold
        this->i2c->Write(this->config.address, 0x3E, config->wakeupThreshold);

        //Set wakeup duration. Afterwards returns to low power mode (10Hz)
        this->i2c->Write(this->config.address, 0x3F, config->wakeupDuration);
    }
}

Lis3dh::~Lis3dh()
{
}

void Lis3dh::Read(Acceleration *acceleration)
{
    // Burst-read all axis registers in one transaction (auto-increment bit = 0x80).
    Byte raw[6] = {0};
    if (this->i2c->Read(this->config.address, (Byte)(0x28 | 0x80), raw, sizeof(raw)) == sizeof(raw))
    {
        int16_t xRaw = (int16_t)((raw[1] << 8) | raw[0]);
        int16_t yRaw = (int16_t)((raw[3] << 8) | raw[2]);
        int16_t zRaw = (int16_t)((raw[5] << 8) | raw[4]);

        // LIS3DH output is left-justified; shift to signed 12-bit equivalent.
        this->previousAcceleration.X = (int16_t)(xRaw >> 4);
        this->previousAcceleration.Y = (int16_t)(yRaw >> 4);
        this->previousAcceleration.Z = (int16_t)(zRaw >> 4);
    }

    if (acceleration)
        *acceleration = this->previousAcceleration;
}

const char* Lis3dh::Name()
{
    return "LIS3DH";
}

bool Lis3dh::Interrupting(bool software)
{
    if (!software)
        return Interrupt::Interrupting();
    else
    {
        Byte data;
        this->i2c->Read(this->config.address, 0x31, &data, sizeof(data));
        return !(data & 0x40);
    }
}

void Lis3dh::Debug()
{
    I2cConfig config = this->i2c->Config();

    printf("----Debug (LIS3DH)----\n");
    printf("Sda-pin: %i\n", config.sda);
    printf("Scl-pin: %i\n", config.scl);
    printf("I2C port: %i\n", config.port);
    printf("I2C frequency: %i\n", config.frequency);

    Byte data = 0;
    if (!this->i2c->Read(this->config.address, 0x0F, &data, sizeof(data)))
        printf("Failed to connect device\n");
    else
    {
        printf("Device id: %i\n", data);
        printf("X: %i, Y: %i, Z: %i\n", this->previousAcceleration.X, this->previousAcceleration.Y, this->previousAcceleration.Z);
    }

    printf("----------------------\n");
}