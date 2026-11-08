# Robot-cua-TRUONG

Custom desktop AI robot based on XiaoZhi ESP32.

## Project goal

Build a desk-only assistant with an animated face display and a pan/tilt head. This is not a walking robot.

## Planned hardware

- ESP32-S3 CAM N16R8 development board (camera intentionally unused)
- ST7789 240x240 display
- INMP441 I2S microphone
- MAX98357A I2S audio amplifier and speaker
- Two 180-degree servos for pan and tilt
- HC-SR501 PIR motion sensor

GPIO assignments remain unconfigured until the exact board pinout and wiring are verified. Do not connect hardware based on guessed pin numbers.

## Architecture

- **ESP32 firmware:** XiaoZhi voice endpoint, Wi-Fi, display and peripheral control.
- **Device configuration UI:** Custom GPIO table, JSON import/export and servo calibration.
- **Separate backend:** AI provider adapters, web chat, device dashboard and music orchestration.

See [architecture](docs/ARCHITECTURE.md) and [safe default configuration](config/default.json).

## Safety

- Camera disabled.
- Servo power requires a suitable external regulated supply and common ground with ESP32; never power servos from 3.3V.
- Verify the MAX98357A breakout's supported speaker impedance.
- Unverified GPIO values remain null.

## Upstream

Base: [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32), pinned to [v2.5.0](https://github.com/78/xiaozhi-esp32/tree/v2.5.0). Preserve upstream license and copyright notices.

The repository includes a GitHub Actions workflow to import this pinned source. ESP-IDF 6.0.1 or later is required; upstream recommends the `release/v6.1` SDK branch.

## Roadmap

1. Import upstream source and verify its build pipeline.
2. Inspect board definitions and add a dedicated ESP32-S3 CAM profile.
3. Validate GPIOs against the exact board schematic.
4. Add Otto face UI and safe pan/tilt/PIR behavior.
5. Add configuration UI and JSON schema validation.
6. Produce and verify BIN artifacts in CI.


<!-- Trigger upstream import after workflow permission adjustment. -->
