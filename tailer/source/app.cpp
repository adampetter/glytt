#include <cstdio>
#include "cli.h"
#include "tailer.h"
#include "api/io/gpio.h"
#include "api/io/i2c.h"
#include "api/motion/lis3dh.h"
#include "api/navigation/pa1010d.h"
#include "api/transmission/e22900t30.h"
#include "api/common/time.h"

// Main entry point
extern "C" void app_main(void)
{
    TailerConfig tailerConfig;
    tailerConfig.bluetoothEnabled = false;
    tailerConfig.baselineCaptureSeconds = 60;
    tailerConfig.movementSpeedThresholdKph = 2.5f;
    tailerConfig.movingTemporalWeight = 0.60f;
    tailerConfig.movingSpatialWeight = 0.40f;
    tailerConfig.stationaryTemporalWeight = 0.90f;
    tailerConfig.stationarySpatialWeight = 0.10f;
    tailerConfig.stationarySensitivityMultiplier = 2.2f;
    tailerConfig.stationaryModeEngageSeconds = 20;
    tailerConfig.movingModeEngageSeconds = 10;
    tailerConfig.dedupWindowMs = 900;
    tailerConfig.scoreFreshnessSeconds = 12;
    tailerConfig.persistenceAlertThreshold = 0.58f;
    tailerConfig.lowEnterScore = 0.42f;
    tailerConfig.lowExitScore = 0.30f;
    tailerConfig.mediumEnterScore = 0.72f;
    tailerConfig.mediumExitScore = 0.55f;
    tailerConfig.highEnterScore = 0.95f;
    tailerConfig.highExitScore = 0.85f;

    Tailer tailer(tailerConfig);

    if (!tailer.Start())
        printf("[APP][ERROR] Failed to start tailer\n");

    I2c *gpsI2c = new I2c({.port = I2cPort::I2cPort_0,
                           .sda = (GpioNum)14,
                           .scl = (GpioNum)13,
                           .mode = I2cMode::I2cMode_Master,
                           .frequency = I2cFrequency::I2cFrequency_01M,
                           .internalPullup = true});

    Byte i2cDevices = gpsI2c->Scan();
    printf("[GPS][INIT] I2C scan complete, found=%u device(s)\n", i2cDevices);

    PA1010D *gps = new PA1010D({.i2c = gpsI2c,
                                .address = PA1010D_I2C_DEFAULT_ADDRESS,
                                .refreshRateSeconds = 1});

    constexpr unsigned int gpsReportIntervalMs = 2000;
    unsigned int gpsElapsedMs = gpsReportIntervalMs;

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

    CLI cli(CliConfig{});
    cli.Register("ping", "Health check command", [](const std::string &args) {
        (void)args;
        return std::string("pong");
    });

    cli.Register("tailer:status", "Describe where tailer status is reported", [](const std::string &args) {
        (void)args;
        return std::string("Tailer status is printed periodically on monitor logs");
    });

    std::string startupHelp = cli.Dispatch("help");
    printf("[CLI][HELP] %s\n", startupHelp.c_str());

    const GpioNum greenLedPin = (GpioNum)11;
    const GpioNum yellowLedPin = (GpioNum)10;
    const GpioNum redLedPin = (GpioNum)9;
    const GpioNum buzzerPin = (GpioNum)12;
    const GpioNum baselineButtonPin = (GpioNum)1;

    Gpio::Mode(greenLedPin, GpioMode::GpioMode_Output);
    Gpio::Mode(yellowLedPin, GpioMode::GpioMode_Output);
    Gpio::Mode(redLedPin, GpioMode::GpioMode_Output);
    Gpio::Mode(buzzerPin, GpioMode::GpioMode_Output);
    Gpio::Mode(baselineButtonPin, GpioMode::GpioMode_Input);
    Gpio::Pull(baselineButtonPin, GpioPull::GpioPull_Up);

    Gpio::Write(greenLedPin, true);
    Gpio::Write(yellowLedPin, false);
    Gpio::Write(redLedPin, false);
    Gpio::Write(buzzerPin, false);

    bool previousButtonPressed = false;
    bool previousBaselineActive = false;
    long int buttonPressedAtMs = 0;
    bool longPressHandled = false;
    bool waitingSecondTap = false;
    long int firstTapReleasedAtMs = 0;
    long int lastButtonEdgeMs = 0;
    bool feedbackGreenOn = false;
    int feedbackTogglesRemaining = 0;
    long int feedbackNextToggleMs = 0;

    constexpr long int kLongPressMs = 1400;
    constexpr long int kDoublePressWindowMs = 1000;
    constexpr long int kButtonDebounceMs = 60;
    constexpr long int kFeedbackBlinkIntervalMs = 120;

    printf("[BUTTON][INIT] baseline button on GPIO1 active-low (press=GND)\n");

    while (true)
    {
        tailer.Tick();

        bool baselineActive = tailer.BaselineActive();
        float score = tailer.CurrentScore();
        TailerAlertLevel alertLevel = tailer.AlertLevel();
        long int nowMs = millis();

        if (previousBaselineActive && !baselineActive)
        {
            // Baseline completed: double green blink.
            feedbackGreenOn = false;
            feedbackTogglesRemaining = 4;
            feedbackNextToggleMs = nowMs;
        }

        bool buttonPressed = !Gpio::Read(baselineButtonPin);

        if (buttonPressed && !previousButtonPressed && (nowMs - lastButtonEdgeMs) >= kButtonDebounceMs)
        {
            lastButtonEdgeMs = nowMs;
            buttonPressedAtMs = nowMs;
            longPressHandled = false;

            if (waitingSecondTap && (nowMs - firstTapReleasedAtMs) <= kDoublePressWindowMs)
            {
                waitingSecondTap = false;
                longPressHandled = true;
                tailer.StartBaseline(tailerConfig.baselineCaptureSeconds);
                printf("[BUTTON][EVENT] double-press accepted -> baseline\n");

                // Double press accepted: double green blink.
                feedbackGreenOn = false;
                feedbackTogglesRemaining = 4;
                feedbackNextToggleMs = nowMs;
            }
            else
            {
                printf("[BUTTON][EVENT] pressed\n");
            }
        }

        if (buttonPressed && !longPressHandled && (nowMs - buttonPressedAtMs) >= kLongPressMs)
        {
            tailer.ClearLearnedBackground();
            waitingSecondTap = false;
            longPressHandled = true;
            printf("[BUTTON][EVENT] long-press accepted -> clear learned baseline\n");

            // Long press accepted: triple green blink.
            feedbackGreenOn = false;
            feedbackTogglesRemaining = 6;
            feedbackNextToggleMs = nowMs;
        }

        if (!buttonPressed && previousButtonPressed && (nowMs - lastButtonEdgeMs) >= kButtonDebounceMs)
        {
            lastButtonEdgeMs = nowMs;
            printf("[BUTTON][EVENT] released hold=%ldms\n", nowMs - buttonPressedAtMs);

            if (!longPressHandled)
            {
                waitingSecondTap = true;
                firstTapReleasedAtMs = nowMs;
                printf("[BUTTON][EVENT] first tap registered, waiting second tap\n");
            }
        }

        if (waitingSecondTap && (nowMs - firstTapReleasedAtMs) > kDoublePressWindowMs)
        {
            waitingSecondTap = false;
            printf("[BUTTON][EVENT] double-press window expired\n");
        }

        previousButtonPressed = buttonPressed;
        previousBaselineActive = baselineActive;

        bool greenOn = false;
        bool yellowOn = false;
        bool redOn = false;
        bool buzzerOn = false;

        if (feedbackTogglesRemaining > 0 && nowMs >= feedbackNextToggleMs)
        {
            feedbackGreenOn = !feedbackGreenOn;
            feedbackTogglesRemaining--;
            feedbackNextToggleMs = nowMs + kFeedbackBlinkIntervalMs;
        }
        else if (feedbackTogglesRemaining == 0)
        {
            feedbackGreenOn = false;
        }

        if (baselineActive)
        {
            // Baseline capture indicator: yellow pulse + rare short chirp.
            yellowOn = ((nowMs / 150) % 2) == 0;
            buzzerOn = (nowMs % 4000) < 50;
        }
        else
        {
            switch (alertLevel)
            {
            case TailerAlertLevel::Normal:
                greenOn = true;
                break;
            case TailerAlertLevel::Low:
                yellowOn = ((nowMs / 500) % 2) == 0;
                break;
            case TailerAlertLevel::Medium:
                yellowOn = ((nowMs / 170) % 2) == 0;
                buzzerOn = (nowMs % 3000) < 90;
                break;
            case TailerAlertLevel::High:
                redOn = ((nowMs / 100) % 2) == 0;
                buzzerOn = (nowMs % 700) < 140;
                break;
            }
        }

        // During feedback blink sequences, drive green LED directly so the blink
        // remains visible even when Normal mode would otherwise keep green solid.
        if (feedbackTogglesRemaining > 0 || feedbackGreenOn)
            greenOn = feedbackGreenOn;

        Gpio::Write(greenLedPin, greenOn);
        Gpio::Write(yellowLedPin, yellowOn);
        Gpio::Write(redLedPin, redOn);
        Gpio::Write(buzzerPin, buzzerOn);

        if (gpsElapsedMs >= gpsReportIntervalMs)
        {
            gpsElapsedMs = 0;

            Location location;
            gps->Read(&location);

                 tailer.UpdateLocation(location);

                    printf("[GPS][STATUS] fix=%d sat=%d lat=%.6f lon=%.6f alt=%.2f speed=%.2f course=%.2f score=%.2f level=%d baseline=%u\n",
                   location.fix,
                   location.satellites,
                   location.coordinate.X,
                   location.coordinate.Y,
                   location.altitude,
                   location.speed,
                         location.course,
                         score,
                             (int)alertLevel,
                             tailer.BaselineSecondsLeft());
        }

        delay(50);
        gpsElapsedMs += 50;
    }

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