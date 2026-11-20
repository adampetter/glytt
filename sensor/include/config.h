#pragma once

#define SENSOR_DEBUG 0
#define SENSOR_BUZZER 1
#define SENSOR_LED 1
#define SENSOR_TRANSMIT 1

// Battery-focused sender heartbeat period (20 min).
#define SENSOR_STATUS_HEARTBEAT_MS (20 * 60 * 1000)

// AlarmSend STATUS_LED pulse: 0.1s every 30s.
#define SENSOR_ALARMSEND_STATUS_LED_BLINK_INTERVAL_MS 30000
#define SENSOR_ALARMSEND_STATUS_LED_BLINK_ON_MS 100

// Sensor runtime mode selected at build/flash time.
#define SENSOR_MODE_ALARM_SEND 0
#define SENSOR_MODE_RECEIVE_ALARM 1
#define SENSOR_MODE_DISABLED 2

// Optional local role override. Generate this file via tools/set-sensor-role.ps1.
#if __has_include("override.h")
#include "override.h"
#endif

// Safe default: disabled unless a role flag overrides it.
#ifndef SENSOR_MODE
#define SENSOR_MODE SENSOR_MODE_DISABLED
#endif

// Primary radio profile for this hardware. Override in sensor/include/override.h when needed.
#ifndef SENSOR_RADIO_UART_PORT
#define SENSOR_RADIO_UART_PORT UartPort::UartPort_1
#endif

#ifndef SENSOR_RADIO_TX_PIN
#define SENSOR_RADIO_TX_PIN 6
#endif

#ifndef SENSOR_RADIO_RX_PIN
#define SENSOR_RADIO_RX_PIN 5
#endif

#ifndef SENSOR_RADIO_AUX_PIN
#define SENSOR_RADIO_AUX_PIN GPIO_NONE
#endif

#ifndef SENSOR_RADIO_M0_PIN
#define SENSOR_RADIO_M0_PIN 4
#endif

#ifndef SENSOR_RADIO_M1_PIN
#define SENSOR_RADIO_M1_PIN 7
#endif

#ifndef SENSOR_RADIO_BAUDRATE
#define SENSOR_RADIO_BAUDRATE UartBaudrate::UartBaudrate_9600
#endif

// 0 = config-capable startup (preferred), 1 = normal-mode-only startup.
#ifndef SENSOR_RADIO_STARTUP_NORMAL_ONLY
#define SENSOR_RADIO_STARTUP_NORMAL_ONLY 0
#endif

// 1 = require successful registry/config-capable radio init (skip normal-only fallback)
// 0 = allow startupInNormalModeOnly attempts
#define SENSOR_REQUIRE_RADIO_CONFIG_READY 1
