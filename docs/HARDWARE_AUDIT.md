# Hardware audit — Waveshare ESP32-S3-CAM N16R8

## Status: board family identified; Otto pin assignments remain unapproved

The project target is the Waveshare ESP32-S3-CAM family using ESP32-S3R8, 16 MB flash and 8 MB PSRAM. The official Waveshare documentation identifies V1 and V1.1 hardware revisions and states that these two revisions share the same programs. The physical revision on this particular board has not been confirmed, so revision-specific assumptions must remain disabled.

Official references:
- Product documentation: https://docs.waveshare.com/ESP32-S3-CAM-OVxxxx
- Resources and schematics: https://docs.waveshare.com/ESP32-S3-CAM-OVxxxx/Resources-And-Documents
- V1.1 schematic: https://files.waveshare.com/wiki/ESP32-S3-CAM-OVxxxx/ESP32-S3-CAM-OVxxxx_Rev1.1.pdf

## Important board facts from Waveshare

The official product family includes:
- ESP32-S3R8, 16 MB flash and 8 MB PSRAM.
- 18-pin LCD interface supporting SPI and QSPI.
- Onboard ES8311/ES7210 audio codec and dual-microphone circuitry.
- CH32V003F4U6 I/O expander.
- Camera interface, SD-card interface, speaker output and battery/power circuitry.
- V1 and V1.1 revisions; Waveshare documents program compatibility between them.

This means the upstream XiaoZhi profile at `main/boards/waveshare/esp32-s3-cam` is genuinely for the Waveshare ESP32-S3-CAM-OVxxxx family, unlike the DFRobot profile. However, the upstream implementation actively initializes the onboard I2C expander, codec, LCD interface and camera. It is not a drop-in Otto profile because this project currently targets external INMP441 + MAX98357A modules and a separate 240×240 ST7789 display, with the camera unused.

## Upstream candidate inspected

- Kconfig symbol: `CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_CAM_XXXX`
- Board directory: `main/boards/waveshare/esp32-s3-cam`
- Files inspected: `config.h`, `config.json`, `esp32-s3-cam-xxxx.cc`, `README.md`

The upstream config uses I2S GPIO 10/11/12/13/14, I2C GPIO 7/8, and LCD SPI GPIO 1/2/3/5/6, plus a CH32V003 expander. Do not reuse those assignments for Otto's external modules without checking the exact header routing and any onboard electrical connections. The board's camera and onboard peripherals also occupy resources that must be considered even though Otto will not use the camera.

## Confirmed Otto project constraints

- Camera disabled in firmware.
- External display target: ST7789, 240×240.
- External microphone: INMP441.
- External amplifier: MAX98357A.
- Two 180° servos: pan and tilt; separate regulated 5V supply and common ground with ESP32 required.
- Optional PIR: HC-SR501.
- GPIO assignments for the external modules remain null until their exact header connections and conflicts are validated.
- Do not enable both onboard codec audio and external I2S audio paths simultaneously by accident.

## Engineering decision

Skip the photo request and use the official Waveshare documentation/schematic as the board-family reference. Do not block software architecture work on a photo.

Still do not invent a pin map: N16R8 identifies memory capacity, not which breakout header pin the user connected each module to. Keep all Otto external-module GPIOs unassigned in the checked-in default configuration. Build a dedicated Otto profile by reusing only safe board-level facts, explicitly disabling camera and unused onboard peripherals, and keeping the Custom GPIO layer responsible for later verified assignments.

A successful compile is not proof that the external wiring or electrical design is safe.
