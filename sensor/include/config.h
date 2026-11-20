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

// E22900T30 built-in over-the-air transform key bytes (double encryption with AES app payload).
#ifndef SENSOR_RADIO_HW_ENCRYPT_BYTE_H
#define SENSOR_RADIO_HW_ENCRYPT_BYTE_H 0x00
#endif

#ifndef SENSOR_RADIO_HW_ENCRYPT_BYTE_L
#define SENSOR_RADIO_HW_ENCRYPT_BYTE_L 0x00
#endif

// Wake-on-radio interval used by both sender (WOR transmitter preamble cadence)
// and receiver (WOR listening cadence). 2000 ms is a battery-first baseline.
#ifndef SENSOR_RADIO_WOR_INTERVAL
#define SENSOR_RADIO_WOR_INTERVAL E22900T30WakeOnRadioInterval::WOR_Interval_2000ms
#endif

// 0 = config-capable startup (preferred), 1 = normal-mode-only startup.
#ifndef SENSOR_RADIO_STARTUP_NORMAL_ONLY
#define SENSOR_RADIO_STARTUP_NORMAL_ONLY 0
#endif

// Application-layer AES-256 payload encryption.
// Sender encrypts alarm/status payloads; receiver decrypts when encrypted flag is set.
#ifndef SENSOR_AES256_ENABLE
#define SENSOR_AES256_ENABLE 1
#endif

// Development defaults only. Override in sensor/include/override.h for deployment.
#ifndef SENSOR_AES256_KEY_BYTES
#define SENSOR_AES256_KEY_BYTES \
	0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, \
	0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, \
	0x55, 0xAA, 0xC3, 0x3C, 0x96, 0x69, 0x5A, 0xA5, \
	0x0F, 0xF0, 0x11, 0x22, 0x33, 0x44, 0x77, 0x88
#endif

#ifndef SENSOR_AES256_IV_BYTES
#define SENSOR_AES256_IV_BYTES \
	0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x17, 0x28, \
	0x39, 0x4A, 0x5B, 0x6C, 0x7D, 0x8E, 0x9F, 0x00
#endif

// 1 = require successful registry/config-capable radio init (skip normal-only fallback)
// 0 = allow startupInNormalModeOnly attempts
#define SENSOR_REQUIRE_RADIO_CONFIG_READY 1
