# Waveshare ESP32-S3-CAM N16R8: upstream profile gap

## Verified vendor identity and limits

Waveshare's official product documentation identifies the ESP32-S3-CAM-OVxxxx family (OV5640 / OV3660 / GC2145 / GC0308) as an ESP32-S3R8 board with 16 MB flash and 8 MB PSRAM. It documents these onboard resources:

- 24-pin DVP camera connector;
- 18-pin SPI/QSPI LCD connector;
- dual-microphone array;
- ES8311 + ES7210 audio codec path and NS4150B speaker amplifier;
- CH32V003F4U6 I/O expander;
- TF card, battery connector and UART/I2C headers.

Official reference: https://docs.waveshare.com/ESP32-S3-CAM-OVxxxx

This documentation confirms the Waveshare product family, but **does not yet prove the exact 40-pin header order or revision of the board used in this Otto build**. The official page describes the LCD as an 18-pin interface; that is not the same thing as the external 40-pin header. Do not derive the 40-pin physical order from the module GPIO numbers or from the upstream XiaoZhi profile.

## Upstream XiaoZhi profile is not a drop-in Otto pin map

The imported XiaoZhi v2.5.0 source contains:

- Kconfig symbol: `CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_CAM_XXXX`
- CMake board directory: `main/boards/waveshare/esp32-s3-cam`
- upstream profile: `main/boards/waveshare/esp32-s3-cam/config.h`

That profile describes the Waveshare onboard peripherals, including its audio codec, CH32V003 expander and LCD connector. Otto uses a separate ST7789 240x240 display, INMP441 microphone and MAX98357A amplifier. Reusing the upstream profile unchanged could initialize hardware that Otto is not using or reserve GPIOs incorrectly.

## Confirmed Otto target contract

- ESP32-S3-CAM N16R8 target; 40-pin header form factor confirmed by the user.
- Camera must remain disabled.
- External ST7789 240x240 display.
- External INMP441 microphone.
- External MAX98357A I2S amplifier and speaker.
- Two 180-degree pan/tilt servos.
- HC-SR501 PIR sensor.
- Onboard codec, onboard microphone array and CH32V003 expander must remain disabled unless the user explicitly changes the hardware design.

## GPIO activation gate

The 40-pin count alone does not define the connector pin order, PCB revision, board-level routing or conflicts with onboard circuitry. Therefore the current JSON keeps `header_pinout_verified: false`, all peripheral GPIOs null, and hardware activation disabled.

Do not copy the upstream profile's GPIO assignments into Otto configuration. Do not infer that a GPIO is safe merely because it appears in the ESP32-S3 datasheet or in another manufacturer's ESP32-S3-CAM pin table.

Before enabling any hardware driver, the exact 40-pin header-to-GPIO mapping must be matched to the specific PCB documentation or a clear board photo showing its silkscreen and connector orientation. The firmware should then validate duplicate assignments, boot-strapping pins, flash/PSRAM reservations, USB/UART use and board-specific onboard routing.

## Integration decision

Keep the Otto board as a dedicated profile rather than selecting the upstream Waveshare board directory unchanged. The software components (Otto face, I2S codec, servo planner/driver, PIR driver and JSON configuration service) can be compiled and tested independently, but physical GPIO activation remains fail-closed until the connector map is verified.
