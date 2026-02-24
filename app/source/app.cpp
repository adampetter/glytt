#include <cstdio>
#include "api/motion/lis3dh.h"
#include "api/navigation/pa1010d.h"
#include "api/transmission/e22900t30.h"
#include "api/common/time.h"

// Main entry point
extern "C" void app_main(void)
{
    /*delay(1000);

    I2c *i2c = new I2c({.port = I2cPort::I2cPort_0,
                        .sda = (GpioNum)8,
                        .scl = (GpioNum)9,
                        .mode = I2cMode::I2cMode_Master,
                        .frequency = I2cFrequency::I2cFrequency_04M,
                        .internalPullup = true});

    i2c->Scan();

    Gpio::Mode((GpioNum)14, GpioMode::GpioMode_Output);
    Gpio::Write((GpioNum)14, false);

    delay(1000);

    const PA1010DConfig gpsConfig = {
        .i2c = i2c,
        .address = PA1010D_I2C_DEFAULT_ADDRESS,
        .refreshRateSeconds = 15};

    PA1010D *gps = new PA1010D(gpsConfig);

    delay(500);

    const E22900T30Config transceiverConfig = {
        .serial = {
            .uart = new Uart({
                .port = UartPort::UartPort_1,
                .baudRate = UartBaudrate::UartBaudrate_9600,
                .tx = (GpioNum)6,
                .rx = (GpioNum)5,
            }),
            .baudRate = UartBaudrate::UartBaudrate_9600,
            .parity = E22900T30SerialParity::Parity_8N1},
        .interupt = {.aux = (GpioNum)4, .expander = NULL},
        .mode = {.m0 = (GpioNum)15, .m1 = (GpioNum)7, .expander = NULL},
        .network = {.id = 0, .address = 0, .airDataRate = E22900T30AirDataRate::Rate_2400, .subPacketSize = E22900T30SubPacketSize::Size_240b, .power = E22900T30Power::Power_30dBm, .ambientNoise = E22900T30RSSIAmbientNoise::AmbientNoise_Disable}};

    E22900T30 *transceiver = new E22900T30(transceiverConfig);

    delay(500);

    transceiver->Send((Byte *)"Hello, World!", 13);
    printf("Message sent: Hello, World!\n");

    delay(500);

    const Lis3dhConfig config = {
        .i2c = i2c,
        .address = 0x18,
        .mode = Lis3dhMode::Lis3dhMode_ByPass,
        .sampleRate = Lis3dhSampleRate::Lis3dhSampleRate_200,
        .axis = Lis3dhAxis::Lis3dhAxis_All,
        .interrupt = GPIO_NONE,
        .interruptDuration = 1,
        .interruptThreshold = Lis3dhThreshold::Lis3dhThreshold_3,
        .wakeupDuration = 0,
        .wakeupThreshold = Lis3dhThreshold::Lis3dhThreshold_1};

    Lis3dh *lis3dh = new Lis3dh(&config);

    constexpr short kAxisDeadband = 48;
    constexpr long long kMovementThresholdSquared = 1024;
    constexpr int kLoopDelayMs = 10;
    constexpr int kLedHoldMs = 120;

    Acceleration acceleration, previousAcceleration;
    int ledHoldRemainingMs = 0;
    int elapsedTime = 0;

    while (true)
    {
        lis3dh->Read(&acceleration);
        Acceleration delta = acceleration - previousAcceleration;
        previousAcceleration = acceleration;

        delta = Acceleration::Deadband(delta, kAxisDeadband);

        const long long movementSquared = delta.LengthSquared();

        if (movementSquared >= kMovementThresholdSquared)
        {
            ledHoldRemainingMs = kLedHoldMs;
        }

        if (ledHoldRemainingMs > 0)
        {
            Gpio::Write((GpioNum)14, true);
            ledHoldRemainingMs -= kLoopDelayMs;
        }
        else
        {
            Gpio::Write((GpioNum)14, false);
        }

        // printf("Delta: X=%d, Y=%d, Z=%d, MovementSq=%lld\n", delta.X, delta.Y, delta.Z, movementSquared);

        if (elapsedTime >= 3000)
        {
            elapsedTime = 0;

            Location location;
            gps->Read(&location);

            printf("Location: Fix=%d, Satellites=%d, Lat=%.6f, Lon=%.6f, Alt=%.2f, Speed=%.2f kph\n",
                   location.fix,
                   location.satellites,
                   location.coordinate.X,
                   location.coordinate.Y,
                   location.altitude,
                   location.speed);
        }

        delay(kLoopDelayMs);
        elapsedTime += kLoopDelayMs;
    }

    delay(3000);
    printf("Exiting...\n");*/

    Cli cli = new Cli(CliConfig{});

    // Commands
    // help - list commands
    // motion:status
    // motion:activate
    // motion:deactivate
    // motion:sensitivity - get/set sensitivity level, 0 for off, 1-10 for sensitivity levels
    // motion:threshold - get/set the amount of times motion must be detected within a certain time frame to trigger an event, 0 for off, otherwise number of detections
    // motion:log - show log of motion events with timestamps
    // motion:log:clear - clear log of motion events
    // gps:status
    // gps:activate - activates gps, refresh interval can be set with gps:refresh [seconds]
    // gps:deactivate
    // gps:log - show log of gps readings with timestamps
    // gps:log:clear - clear log of gps readings
    // transmission:status
    // transmission:activate
    // transmission:deactivate
    // transmission:frequency
    // transmission:power
    // transmission:datarate
    // transmission:encryption - on/off
    // transmission:encryption:key [key]
    // transmission:send [message]
    // transmission:log - show log of sent/received messages with timestamps
    // transmission:log:clear - clear log of sent/received messages
    // transmission:repeat - on/off, repeats incoming messages if not its own message
    // transmission:repeat:filter - on/off, if on, only repeats messages that are not from its own id
    // transmission:autoreply - on/off, if on, automatically replies to incoming messages with a predefined message
    // transmission:autoreply:message [message] - sets the predefined message for autoreply
    // transmission:retry - on/off, if on, automatically retries sending messages that to be confirmed by the receiver
    // transmission:retry:count [count] - sets the number of retry attempts for failed messages
    // transmission:retry:interval [s] - sets the interval between retry attempts for failed messages
    // bluetooth:status
    // bluetooth:activate
    // bluetooth:deactivate
    // bluetooth:discoverable - on/off
    // bluetooth:pair [device] - pairs with a device, if no device specified, shows list of available devices to pair with
    // bluetooth:unpair [device] - unpairs from a device, if no device specified, shows list of currently paired devices to unpair
    // bluetooth:send [device] [message] - sends a message to a paired device
    // bluetooth:log - show log of sent/received messages with timestamps
    // bluetooth:log:clear - clear log of sent/received messages
    // relay:status
    // relay:activate
    // relay:deactivate
    // storage:status
    // storage:list
    // storage:clear - all, logs, config
    // storage:clear:logs - motion, gps, transmission
    // storage:clear:config - factory reset, clears all config and returns to default settings
    // trigger:list - lists all triggers with their conditions and actions
    // trigger:add [name] [condition] [action] - adds a trigger with a name, a condition (e.g. motion detected, gps fix acquired, message received) and an action (e.g. send message, activate output)
    // trigger:remove [name] - removes a trigger by name

    // Pseudo-code: register commands to cli
    // on input match, cli executes command
    // command can run job or start a background job. Command itself prints result or status.
    // cli loops and waits for input, can be done in a separate task/thread if needed 
}