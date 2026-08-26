# App Tailer Roadmap (ESP32-S3)

Goal: Build an ESP32-S3 version inspired by Chasing Your Tail NG using this codebase, with reusable logic in api and app-specific logic in a new app_tailer component.

## Feasibility Summary

Status: FEASIBLE.

Reasons:
- ESP32-S3 supports Wi-Fi promiscuous mode for passive 802.11 frame capture.
- ESP-IDF already provides required Wi-Fi stack APIs.
- This repository already has a strong hardware abstraction style (C#-like OOP), suitable for a generic Wi-Fi class under api.
- app_receiver and app_sensor are empty, so adding app_tailer is low risk for structure.

Main limitations to design around:
- ESP32-S3 in Wi-Fi promiscuous mode can sniff one channel at a time.
- Reliable multi-channel discovery requires channel hopping.
- Capturing + heavy analytics on-device must be optimized for RAM/CPU.

## Architecture Split

Keep generic in api:
- Generic Wi-Fi driver wrapper (init, promiscuous mode, channel control, callbacks).
- 802.11 frame parser (MAC/BSSID/SSID extraction from management frames).
- Probe event model and ring buffer.
- Ignore list filtering primitives (MAC/SSID match helpers).
- Sliding window tracker primitives.

Keep app-specific in app_tailer:
- Tailing/scoring policy and thresholds.
- Session lifecycle, output formatting, and reporting strategy.
- Any domain heuristics and UI/CLI command behavior.

## Component Plan

### Phase 1 - Create app_tailer component skeleton
1. Add new component folder app_tailer with CMakeLists and source/include layout.
2. Register app_tailer in top-level CMake EXTRA_COMPONENT_DIRS.
3. Wire app_tailer to depend on api.
4. Keep existing app component unchanged until feature parity is proven.

Acceptance:
- Project builds with app_tailer included and no behavior changes yet.

### Phase 2 - Implement generic Wi-Fi class in api
1. Add api/include/api/transmission/wifi.h (currently empty) with clear interface:
   - Start/Stop
   - SetChannel
   - EnablePromiscuous
   - RegisterFrameCallback
2. Add api/source/transmission/wifi.cpp using esp_wifi APIs.
3. Add lightweight event struct for parsed probe-like events.
4. Add safe callback bridge (static C callback -> class instance).

Acceptance:
- Can start Wi-Fi in sniff mode and receive frame callbacks without crash.

### Phase 3 - Add frame parsing + filtering primitives in api
1. Parse management frames and extract:
   - Source MAC
   - BSSID
   - SSID (if present)
   - RSSI
   - Channel/timestamp
2. Add fast ignore filters for MAC and SSID.
3. Normalize MAC format to uppercase canonical string.
4. Define bounded queues to avoid memory growth.

Acceptance:
- Probe request events are decoded and filtered correctly in test logs/runtime prints.

### Phase 4 - Sliding windows and persistence logic in api
1. Implement 4 windows like CYT concept:
   - Recent (0-5m)
   - Medium (5-10m)
   - Old (10-15m)
   - Oldest (15-20m)
2. Add rotation/update methods driven by periodic ticks.
3. Add reusable match checks:
   - Reappeared in previous windows
   - Repeat SSID probes
4. Keep scoring hooks generic so app_tailer can decide thresholds.

Acceptance:
- Deterministic window rotation and expected repeat detection.

### Phase 5 - app_tailer orchestration
1. Build AppTailer controller in app_tailer:
   - Configure Wi-Fi capture
   - Configure channel hop strategy
   - Feed parsed events into generic trackers
2. Add scoring policy and alert levels in app_tailer only.
3. Add output sink (UART logs first, optional storage later).
4. Add config object with defaults inspired by CYT intervals.

Acceptance:
- End-to-end local run that logs repeat/persistence detections.

### Phase 6 - Optional advanced features
1. Persist events to storage (NVS/SD card) for offline analysis.
2. Add BLE or GNSS location attachment if hardware is added.
3. Add export format for external visualization.
4. Add remote upload option (if required and lawful in your context).

Acceptance:
- Optional features are modular and do not bloat base runtime.

## Hardware Recommendations

Minimum viable hardware:
- ESP32-S3 dev board with stable power.
- External antenna capable board preferred for sensitivity.

Recommended upgrades:
- Better 2.4 GHz antenna for capture quality.
- Optional u-blox GNSS module for location correlation.
- Optional microSD module for local event history.
- Optional RTC if you need strong timestamp continuity offline.

Nice-to-have for reliability:
- USB power bank with low noise output for field runs.
- Enclosure with antenna clearance.

## Practical Feasibility Call

Yes, this is absolutely doable on ESP32-S3.

What is realistic on-device now:
- Real-time probe detection
- MAC/SSID ignore filtering
- Time-window persistence checks
- Basic threat scoring and alerting

What should be deferred or offloaded first:
- Heavy report generation
- Rich geo analytics/KML generation
- Expensive historical correlation over very large datasets

## Suggested First Implementation Slice

1. Implement api Wi-Fi class with promiscuous callback.
2. Parse probe request frame metadata only.
3. Log normalized event stream.
4. Add 5/10/15/20 minute window rotation and repeat detection.
5. Move to app_tailer policy scoring.
