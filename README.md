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

GPIO assignments are intentionally left unconfigured until the exact board pinout and wiring are verified. Do not connect hardware based on guessed pin numbers.

## Architecture

- **ESP32 firmware:** Wi-Fi, XiaoZhi audio/voice endpoint, display, GPIO and servo control.
- **Configuration UI:** device-local web page for hardware settings, GPIO mapping, calibration and JSON import/export.
- **Backend:** separate service for AI providers, web chat and music features.

## Safety and implementation notes

- The camera is disabled and must remain unused.
- Servo power must use a suitable external 5V supply; share ground with the ESP32. Never power servos from the 3.3V pin.
- Verify the amplifier's supported speaker impedance before connecting the speaker.
- Keep unverified GPIO values null in configuration until checked against the exact board schematic.

## Upstream

The firmware is intended to be based on [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32), initially targeting tag [v2.5.0](https://github.com/78/xiaozhi-esp32/tree/v2.5.0). Preserve upstream license and copyright notices when importing or modifying source.

## Roadmap

1. Import and pin the upstream XiaoZhi source.
2. Confirm board definition and build environment (ESP-IDF 6.1).
3. Add the custom desk-robot board profile and validated GPIO configuration.
4. Add display animation and safe pan/tilt control.
5. Add the configuration UI and JSON import/export.
6. Build and verify firmware artifacts through CI.
