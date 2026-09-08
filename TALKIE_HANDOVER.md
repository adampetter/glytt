# Talkie Handover

Date: 2026-08-29

## Scope Implemented

1. Project now builds only talkie app component plus api dependency.
2. Talkie component includes:
   - Transceiver mode (auto TX+RX without PTT) with optional PTT fallback
   - Voice packet encode/decode transport
   - BLE bring-up and optional scan telemetry
   - Runtime stats and CLI diagnostics
   - Half-duplex RX-priority arbitration and minimum capture gate before TX
3. Generic api extensions added:
   - audio adapter interface
   - mock transceiver loopback for no-hardware development

## Key Files

- CMakeLists.txt
- api/include/api/transmission/audio_adapter.h
- api/include/api/transmission/mock_transceiver.h
- api/source/transmission/mock_transceiver.cpp
- api/include/api/transmission/voice_packet.h
- api/source/transmission/voice_packet.cpp
- api/CMakeLists.txt
- talkie/include/talkie.h
- talkie/source/talkie.cpp
- talkie/source/app.cpp
- ROADMAP_APP_TALKIE.md

## Runtime Behavior

1. app_main currently uses mock radio by default:
   - useMockRadio = true in talkie/source/app.cpp
2. Mock mode loops sent packets back to RX with configurable latency.
3. Talkie buffers RX frames in jitter queue before playback callback.
4. Half-duplex default mode suppresses TX while RX is active or recently active (rxPriorityHoldMs).
5. Capture payload is suppressed unless minimum payload cap is reached (minCapturePayloadBytes).
6. Sequence quality counters track gaps, out-of-order and duplicates.

## Existing CLI Commands

- ping
- talkie:state
- talkie:stats
- talkie:reset
- talkie:bt

## Build Status

1. Latest build succeeded with talkie-only setup and generated build/glytt.bin.
2. Pre-existing warnings remain in api/include/api/common/datetime.h.

## Next Steps

1. Replace placeholder capture callback with real audio pipeline.
2. Implement concrete AudioAdapter for chosen hardware path:
   - I2S mic + DAC
   - Bluetooth Classic headset path on classic ESP32 target
3. Add codec integration (for example Codec2) inside adapter or codec layer.
4. Optionally expose duplex profile switching at runtime (half/full experimental).
5. Add optional redundancy/FEC profile for weak RF links.
6. Add NVS persistence for TalkieConfig runtime profile.

## Hardware Bring-Up Switch

To use E22900T30 hardware instead of mock:
1. Set useMockRadio = false in talkie/source/app.cpp.
2. Verify GPIO pins for UART TX/RX, AUX, M0, M1.
3. Flash and monitor on target board.
