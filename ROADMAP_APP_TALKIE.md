# App Talkie Roadmap (ESP32 + E22900T30)

Goal: Build a real-time simplex walkie-talkie app where E22900T30 is the inter-device radio link and Bluetooth or wired audio is used locally on each node.

## Current Status

1. Phase 1 completed.
2. Phase 2 partially completed:
	- Adapter hook points are in place.
	- Placeholder capture/playback callbacks are still active.
3. Phase 3 partially completed:
	- Jitter buffer and sequence gap counters are implemented.
	- Codec integration is not yet implemented.
4. talkie app currently defaults to mock transceiver loopback in app_main for no-hardware bring-up.

## Architecture Split

Keep generic in api:
- Voice packet wire format and validation.
- Transceiver abstraction and reusable transport helpers.

Keep app-specific in talkie:
- Push-to-talk state machine and timing.
- Runtime policy for frame interval, debounce, and status reporting.
- App commands and operator feedback.

## Phase 1 - Transport Bring-Up

1. Create Talkie component skeleton with app_main, talkie class, and CLI.
2. Add generic voice packet class in api for encode/decode.
3. Validate packet send/receive over E22900T30 with placeholder payload.

Acceptance:
- TX/RX packets flow and counters increase.

## Phase 2 - Real Audio Capture/Playback

1. Replace placeholder capture callback with microphone pipeline.
2. Add playback callback to feed DAC/headset output.
3. Keep frame timing fixed for deterministic latency.

Acceptance:
- Speech is audible over link in simplex PTT mode.

## Phase 3 - Codec and Jitter Buffer

1. Add codec integration to reduce payload bitrate.
2. Add receive jitter buffer and late-frame drop policy.
3. Keep tuneables in TalkieConfig.

Acceptance:
- Stable voice quality at target range and acceptable latency.

## Phase 4 - Reliability and Ops

1. Add sequence-gap logging and optional lightweight FEC/redundancy.
2. Add field-friendly CLI commands for runtime diagnostics.
3. Add optional persisted settings for radio profile.

Acceptance:
- Reliable field operation and quick diagnostics from logs/CLI.
