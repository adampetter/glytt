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

// 1 = require successful registry/config-capable radio init (skip normal-only fallback)
// 0 = allow startupInNormalModeOnly attempts
#define SENSOR_REQUIRE_RADIO_CONFIG_READY 1
