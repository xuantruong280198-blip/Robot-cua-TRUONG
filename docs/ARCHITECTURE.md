# Architecture

## Scope
A desk-only voice assistant. The head pans and tilts; there are no legs or walking behavior. The camera connector on the ESP32-S3 CAM board is intentionally unused.

## Runtime layers

1. **ESP32-S3 firmware**
   - XiaoZhi voice endpoint, Wi-Fi provisioning and audio transport.
   - ST7789 display and animated face.
   - Validated GPIO abstraction for peripherals.
   - Servo pan/tilt control with software limits, slew-rate limiting and safe startup.
   - PIR input as an optional event source.

2. **Device configuration UI**
   - Local device-hosted page.
   - Hardware profile and Custom GPIO table.
   - JSON export/import with schema versioning and validation.
   - Servo calibration, min/max angles, center position and speed.
   - Never apply a GPIO mapping until it passes board-specific validation.

3. **Separate backend**
   - Provider adapters for AI/chat.
   - Web chat and conversation/session handling.
   - Music search/playback orchestration where the source and provider terms permit it.
   - Device API and status dashboard.

## Configuration rules

- Unknown GPIOs remain `null`; never infer pin numbers from a different ESP32-S3 board.
- Keep physical wiring state separate from planned/reserved peripherals.
- Camera is disabled.
- Servo outputs default disabled until wiring and external power are verified.
- Validate GPIO range, duplicate assignments, input/output capability, boot-strapping constraints and conflicts with flash/PSRAM/USB/UART/display/I2S.
- JSON must include a schema version and reject unknown incompatible versions.

## Hardware safety

- Power servos from a suitable external regulated supply, not the ESP32 3.3V rail.
- Join servo-supply ground and ESP32 ground.
- Confirm stall current and supply capacity before attaching both servos.
- Verify the MAX98357A breakout's supported supply and speaker impedance against its actual documentation.
- A successful firmware build does not prove electrical or mechanical safety.

## Upstream integration

The firmware base is pinned to `78/xiaozhi-esp32` tag `v2.5.0`. Keep upstream license and copyright notices intact. The upstream project requires ESP-IDF 6.0.1 or later and recommends the `release/v6.1` SDK branch.
