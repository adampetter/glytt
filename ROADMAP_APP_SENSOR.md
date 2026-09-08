# App Sensor Roadmap (ESP32 + LIS3DH + E22900T30)

Goal: Build a robust motion alarm sensor app where LIS3DH detects movement and encrypted alarm messages are transmitted repeatedly over E22900T30, with optional PA1010D GPS position attached when available at startup.

## Baseline Decisions (Locked for v1)

1. Fixed radio packet length: 64 bytes.
2. Single generic package format in api (`Package`) used by all apps/components.
3. Message types in v1:
- `alarm`
- `status` (heartbeat/connection test)
- `ack` (acknowledge)
4. Receiver planning is deferred, but sender protocol must include `ack` and `status` types now.
5. Monitor power mode default: deep sleep.
6. Alarm mode: always awake until timeout completes.
7. Do not expose raw hardware identifiers in packet payload/header.
- Use app-level pseudonymous `deviceId` (non-reversible short ID), not raw eFuse MAC.
8. Optional remote LIS3DH over 1-3 m cable is supported.
- Default to a conservative I2C speed profile for remote mode.
9. Radio profile default for range: low air data rate.
- Start with `Rate_300` and keep `Rate_1200` as fallback if latency/reliability requires it.
10. Buzzer default state: OFF.

## Scope for this roadmap

1. This roadmap covers `sensor` only.
2. `receiver` planning is explicitly deferred.

## Architecture Split

Keep generic in `api`:
- Sensor drivers and abstractions already in api (`Lis3dh`, `PA1010D`, `E22900T30`, `AES256`).
- Reusable data models for alarm payload and optional location payload.
- Generic motion confidence helper (window/counter-based) that can be reused in other apps.
- Generic crypto helper usage pattern (block-safe encrypt/decrypt wrapper around `AES256`).

Keep app-specific in `sensor`:
- Sensor app state machine and lifecycle.
- Threshold policy values and alarm confidence tuning defaults.
- Alarm retry interval and timeout policy.
- Boot-time peripheral discovery policy (for optional GPS use).
- Power policy for monitor vs alarm modes.
- CLI/status output for sensor runtime diagnostics.

## Functional Requirements

1. Motion source:
- Use `Lis3dh` from api over I2C.
- Support configurable sensitivity/threshold profile.

2. Alarm trigger quality:
- Motion must be confirmed with high confidence before alarm is raised.
- Confidence should be based on repeated threshold crossings in a bounded time window.

3. Alarm transport:
- Send alarm over `E22900T30`.
- Repeat send on configurable interval until timeout is reached.

4. Alarm security:
- Encrypt alarm message using `AES256` from api.
- Ensure payload length is compatible with AES-CBC block size before encryption.

5. Optional GPS:
- At startup, probe for `PA1010D` on I2C.
- If available, include GPS coordinates in outgoing alarm messages.
- If absent, continue operation without GPS.

6. Power saving:
- During monitoring, ESP32 should prefer sleep/deepsleep to minimize battery draw.
- LIS3DH interrupt pin must be used as wake source on motion.
- During active alarm transmission, node must remain awake until alarm timeout completes.

7. Optional remote accelerometer mounting:
- LIS3DH module may be mounted 1-3 meters away from the main unit over cable.
- System should expose an install profile for local vs remote sensor wiring.
- Remote mode is optional and must not be required for baseline operation.

8. Local alarm indication and buzzer control:
- Device should provide local alarm indication (LED and/or buzzer) when alarm is active.
- Buzzer must be possible to toggle on/off at runtime and through persisted config.

9. Link supervision:
- Sensor should send a periodic `status` heartbeat frame to support link-health checks.
- Heartbeat interval should be configurable and low-duty to preserve battery.

## App State Machine (sensor)

States:
1. `Boot`
2. `ProbePeripherals`
3. `ArmAndSleep`
4. `WakeValidateMotion`
5. `AlarmActive`
6. `AlarmCooldown`
7. `Fault` (recoverable runtime errors)

Transitions:
1. `Boot -> ProbePeripherals`: initialize buses and config.
2. `ProbePeripherals -> ArmAndSleep`: LIS3DH and radio ready; GPS optional.
3. `ArmAndSleep -> WakeValidateMotion`: wake by LIS3DH interrupt or timer.
4. `WakeValidateMotion -> AlarmActive`: confidence threshold reached.
5. `WakeValidateMotion -> ArmAndSleep`: motion rejected as noise.
6. `AlarmActive -> AlarmCooldown`: alarm timeout reached.
7. `AlarmCooldown -> ArmAndSleep`: cooldown elapsed.
8. Any state -> `Fault`: hard dependency failure (e.g. LIS3DH or radio init failure).
9. `Fault -> ProbePeripherals`: periodic re-init attempts.

Power behavior by state:
1. `ArmAndSleep`: radio idle, CPU in sleep/deepsleep, wake source armed on LIS3DH interrupt pin.
2. `WakeValidateMotion`: CPU awake, short validation window to avoid false wake events.
3. `AlarmActive`: CPU/radio awake for deterministic repeated sends.
4. `AlarmCooldown`: optional light sleep between cooldown checks.

## Alarm Message Contract (v1)

Suggested logical fields before encryption:
1. Protocol version
2. Message type (`motion_alarm`)
3. Device ID
4. Session/event ID
5. Monotonic timestamp (ms)
6. Confidence score + trigger counters
7. Battery/system health flags (optional)
8. GPS payload present flag
9. GPS payload (lat/lon/hdop/fix age) when available
10. Message sequence number (for repeated sends)

Notes:
- Serialize deterministically (fixed order).
- Add integrity check (CRC or MAC) before/after encryption depending on chosen framing.
- Keep total framed packet size within E22900T30 practical payload budget.
- Use fixed 64-byte frame length for all message types.
- Do not include raw hardware identifiers (e.g. plain eFuse MAC) in clear fields.

## Package Protocol Baseline (v1)

1. Transport frame size:
- Fixed 64 bytes for every transmission.

2. Shared package format:
- Use generic `Package` api class for all app protocols.

3. v1 message types:
- `alarm`: motion alarm event and repeats.
- `status`: periodic heartbeat and sender runtime health.
- `ack`: acknowledgement of received alarm sequence/event.

4. Identity/privacy rule:
- `deviceId` must be pseudonymous and must not leak raw hardware identifiers.

5. Security note:
- AES payload encryption remains required for alarm content.
- MAC/tag-based authenticity is recommended as follow-up hardening when receiver planning starts.

## Phase Plan

## Phase 1 - Create `sensor` component skeleton

1. Add `sensor` component folder with include/source/CMake.
2. Add a `SensorApp` class and app entry glue.
3. Wire `sensor` to depend on `api`.

Acceptance:
- Project builds with `sensor` component included.
- App starts and prints boot banner/state.

## Phase 2 - Motion pipeline bring-up

1. Instantiate and configure `Lis3dh` using app config.
2. Add periodic read tick and raw motion telemetry.
3. Add configurable sensitivity profiles (low/medium/high) mapped to LIS3DH threshold/sample-rate parameters.
4. Add install profile for sensor placement:
- `local`: short cable/default electrical assumptions.
- `remote`: 1-3 m cable profile with lower bus speed and stricter read validation.
- Default remote profile I2C speed: 50 kHz (first choice for stability), with 100 kHz as optional tuned mode after validation.

Acceptance:
- Motion readings are stable.
- Sensitivity profile switching affects trigger behavior as expected.
- Remote profile remains stable with 1-3 m cable in lab test setup.

## Phase 2.5 - Sleep/deepsleep wake integration

1. Configure LIS3DH interrupt pin as ESP32 wake source.
2. Implement arm-sleep entry routine used in monitor mode.
3. On wake, capture wake reason and branch into motion validation.
4. Add fallback to light sleep if deepsleep constraints block required context retention.

Acceptance:
- Device wakes reliably from LIS3DH motion interrupt.
- Idle current is significantly lower in monitor mode versus fully awake polling.
- Wake reason telemetry confirms LIS3DH-driven wake events.

## Phase 3 - High-confidence trigger logic

1. Implement bounded time-window counter logic:
- Example: `N` threshold crossings within `T` ms to trigger.
2. Add anti-noise guards:
- Minimum inter-event spacing.
- Optional axis filtering.
- Require confidence re-validation immediately after wake before arming alarm state.
3. Expose tuning values in `SensorConfig`.

Acceptance:
- False positives are reduced in static tests.
- Intentional movement reaches alarm trigger consistently.

## Phase 4 - Radio alarm transport loop

1. Bring up `E22900T30` with explicit app config.
 - Apply long-range radio defaults (`Rate_300`, high TX power, fixed packet mode).
2. Implement alarm send scheduler:
- Send immediately on trigger.
- Repeat every `alarmIntervalMs`.
- Stop when `alarmTimeoutMs` reached.
- Keep system awake for the full alarm transmission window.
3. Add transmission counters and last-send status.
4. Add periodic heartbeat (`status`) transmission with configurable interval.
5. Define ACK handling hooks for future receiver integration (sender-side bookkeeping only in v1).

Acceptance:
- Alarm frames are sent repeatedly until timeout.
- Timeout stop is deterministic.

## Phase 5 - AES256 encryption integration

1. Introduce app-side message serializer.
2. Encrypt serialized payload with `AES256`.
3. Ensure plaintext framing is block-size safe (padding strategy).
4. Keep key/IV provisioning outside hard-coded source when possible.

Acceptance:
- Decrypting captured payload with same key/IV reproduces original message.
- Invalid key/IV states fail closed (no plaintext send).

## Phase 6 - Optional GPS enrichment at startup

1. In `ProbePeripherals`, probe/init `PA1010D`.
2. If GPS present, start periodic location refresh cache.
3. Attach freshest valid location to alarm payload.
4. If GPS missing or no fix, send alarm without location.

Acceptance:
- App runs both with and without GPS connected.
- GPS fields appear only when valid and available.

## Phase 7 - Operations and hardening

1. Add CLI/status commands:
- `sensor:state`
- `sensor:stats`
- `sensor:test-alarm`
- `sensor:config`
 - `sensor:buzzer on|off|toggle`
2. Add runtime stats:
- Trigger count, suppressed events, tx attempts/success/failures, gps availability ratio.
 - Local indicator/buzzer active ratio and mute state.
 - Heartbeat tx count and last heartbeat result.
3. Add watchdog-friendly non-blocking loop timing.
4. Add fault injection checklist (sensor disconnect, radio timeout, gps unavailable).
5. Add persistent config fields for:
- `buzzerEnabled`
- `sensorInstallProfile` (`local`/`remote`)
 - `airDataRateProfile` (`longRangeDefault`, `fallback1200`)

Acceptance:
- Field diagnostics possible from logs/CLI only.
- Recoverable faults do not require reboot.

## Initial Tuning Defaults (starting point)

1. LIS3DH sample rate: 50-100 Hz.
2. Confidence trigger: 3-5 qualified events within 1500-3000 ms.
3. Alarm repeat interval: 1000-2000 ms.
4. Alarm timeout: 60-120 s.
5. Cooldown after timeout: 10-30 s.
6. Monitor sleep mode: deepsleep preferred, light sleep fallback when required by retained runtime context.
7. Air data rate default: `Rate_300`.
8. Buzzer default: off (`buzzerEnabled=false`).

These are initial values only and should be calibrated on-device.

## Risks and Mitigations

1. Motion false positives from vibration/noise.
- Mitigation: confidence window + axis filtering + cooldown.

2. Lost radio packets.
- Mitigation: repeated alarm sends, sequence numbering, receiver-side dedupe later.

3. Crypto framing mistakes.
- Mitigation: deterministic serializer + block/padding tests + known-answer vectors.

4. GPS startup delays or no fix.
- Mitigation: optional GPS behavior and stale-fix guard.

5. Missed wake events due to interrupt/wiring mistakes.
- Mitigation: explicit wake-reason logging, interrupt self-test command, pull-up/down verification.

6. Excess battery drain from too-frequent wakeups.
- Mitigation: wake debounce window, post-wake validation, optional minimum re-arm interval.

7. Remote cable can increase I2C signal integrity issues.
- Mitigation: remote install profile (lower bus speed), shielding/twisted pair guidance, pull-up review.

8. Local buzzer drains battery or causes nuisance alarms.
- Mitigation: runtime toggle, persisted mute policy, optional auto-silence timeout.

## Definition of Done (sensor roadmap scope)

1. Motion alarm triggers only after high-confidence detection.
2. Alarm payload is encrypted via `AES256` before radio send.
3. Alarm messages repeat on interval and stop at timeout.
4. GPS is auto-detected at startup and included only when available.
5. `api` remains generic and contains no sensor-app-specific policy logic.
6. Monitor mode uses sleep/deepsleep with LIS3DH interrupt wake.
7. Alarm mode keeps device awake until alarm timeout is complete.
8. Optional 1-3 m remote LIS3DH profile is supported without regressing local mode.
9. Buzzer can be toggled at runtime and its state is reflected in diagnostics/config.
