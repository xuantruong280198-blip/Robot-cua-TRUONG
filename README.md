# Robot-cua-TRUONG

Custom desktop AI robot based on XiaoZhi ESP32. This project is a desk assistant with an animated face and a pan/tilt head; it is not a walking robot.

## Hardware target

- Waveshare ESP32-S3-CAM N16R8, 40-pin header (camera intentionally disabled)
- ST7789 240x240 display
- INMP441 I2S microphone
- MAX98357A I2S amplifier and speaker
- Two 180-degree servos for pan and tilt
- HC-SR501 PIR motion sensor

GPIO assignments are deliberately unconfigured by default. Do not connect or enable hardware using guessed pin numbers. The exact carrier-board signal routing must be audited before setting `board.peripheral_routing_verified=true` in the runtime JSON.

## Implemented firmware components

- Dedicated Otto Desk board profile built on the pinned XiaoZhi ESP32 v2.5.0 source.
- Full-screen Otto face display integration with ST7789 rotation handling.
- Runtime JSON configuration with strict schema, duplicate-GPIO and unsafe-pin checks; configuration page supports load, validate/save, import and export.
- Optional INMP441/MAX98357A I2S codec with initialization rollback and dummy-codec fallback.
- Optional two-axis servo control, bounded motion planning and emotion-driven head movement.
- Optional HC-SR501 motion input with warm-up and trigger cooldown.
- Hardware-independent regression tests for configuration, JSON nesting, OTA version comparison, display rotation, head motion, servo pulse math, I2S PCM conversion and GPIO safety.

## Configuration and activation safety

The first-use configuration keeps every peripheral disabled and every GPIO unassigned. Hardware activation requires an explicit routing acknowledgement and complete, unique, safe GPIO assignments. Servo activation additionally requires explicit confirmation of a suitable external servo supply. The camera remains disabled.

The configuration API is available in Wi-Fi provisioning mode at `http://192.168.4.1:8080/`. Saved hardware configuration is applied after reboot.

## Build and release status

GitHub Actions builds the Otto Desk ESP32-S3 N16R8 target using ESP-IDF v6.0.2 and runs the hardware-independent regression tests. The CI artifact is a **provisioning/build-validation image**, not an approved final firmware release for flashing.

A passing CI build does not verify the physical carrier-board routing or connected modules. Before a flashable release is approved, the project still requires:
- Exact carrier-board schematic/routing audit and final GPIO assignment review.
- Physical tests of the ST7789, microphone, amplifier/speaker, both servo axes and PIR sensor.
- Verification of flash size, partition layout and offsets against the actual module.
- On-device OTA, rollback and recovery testing.

Do not treat CI success as hardware validation. Keep peripherals disabled until the wiring and power conditions have been checked.

## Architecture

- **ESP32 firmware:** XiaoZhi voice endpoint, Wi-Fi, display and peripheral control.
- **Device configuration UI:** Custom GPIO table and JSON import/export.
- **Separate backend (planned):** AI provider adapters, web chat, device dashboard and music orchestration.

See [architecture](docs/ARCHITECTURE.md) and [safe default configuration](config/default.json).

## Upstream

Base: [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32), pinned to [v2.5.0](https://github.com/78/xiaozhi-esp32/tree/v2.5.0). Preserve upstream license and copyright notices.


## OTA boot safety

- Keep `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` enabled.
- Do not enable `CONFIG_BOOTLOADER_SKIP_VALIDATE_ALWAYS`; the bootloader must retain application-image validation for recovery from flash corruption.
- A pending OTA image is confirmed only after synchronous local application setup (display, audio service, callbacks, timer and MCP tools), and before asynchronous network startup. A crash before confirmation leaves bootloader rollback available; Wi-Fi or OTA-server availability is not part of this local confirmation gate.
- The current partition table has two OTA slots and OTA metadata, but no factory recovery application. A failed pending update can roll back to the previously active slot while that image remains valid. If both OTA slots or OTA metadata are damaged, recovery requires ROM download mode and an external serial reflash; this is not an autonomous recovery path.
- The downloader validates the image header before beginning flash writes, checks the declared body length, calls `esp_ota_end()` before selecting the new boot partition, and leaves the old boot selection unchanged on earlier download/write failures. Power-loss behavior still requires on-device fault-injection tests.
