# Waveshare ESP32-S3-CAM N16R8: upstream profile gap

## Finding

The imported XiaoZhi v2.5.0 source already contains an upstream board entry:

- Kconfig symbol: `CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_CAM_XXXX`
- CMake board directory: `main/boards/waveshare/esp32-s3-cam`
- Upstream profile: `main/boards/waveshare/esp32-s3-cam/config.h`
- Upstream README names the ESP32-S3-CAM-OV5640 / OV3660 / GC2145 / GC0308 product family.

This is useful as a reference, but it is **not yet a verified match** for this Otto build.

## Why it cannot be selected unchanged

The upstream profile configures a different peripheral arrangement:

- I2S MCLK/BCLK/WS/DIN/DOUT on GPIO 10/11/12/13/14.
- I2C on GPIO 7/8 and a CH32V003 custom I/O expander.
- Display SPI on GPIO 1/2/3/5/6, plus I2C display signals.
- Camera signal definitions are present.
- Display size is selected from 2-inch, 2.8-inch, 3.5-inch, 1.83-inch options; the Otto target is a separate 240x240 ST7789 module.
- Audio is configured as an onboard codec/reference-input arrangement, not the project's separate INMP441 + MAX98357A modules.

Therefore, do not copy these pin assignments into the Otto JSON or firmware. In particular, the upstream profile's GPIO 12, 13, 14, 41, 42, 45, 46, 47 and 48 are not approved for Otto use merely because they appear in this file.

## Otto target hardware contract

- ESP32-S3-CAM N16R8; camera unused and must remain disabled.
- ST7789 240x240 display.
- INMP441 microphone.
- MAX98357A I2S amplifier and external speaker.
- Two 180-degree pan/tilt servos, powered from a separate regulated 5V supply; grounds must be common with ESP32.
- HC-SR501 PIR sensor.
- GPIO assignments remain unassigned until the exact board revision, connector mapping and schematic are matched.

## Integration decision

Do not add a new Kconfig board selector or point the Otto target at the upstream `esp32-s3-cam` directory yet. First verify the exact Waveshare PCB revision and its pinout against the board in hand. Then create a dedicated Otto board profile that:

1. disables camera support;
2. uses the separate INMP441 and MAX98357A audio paths;
3. initializes the 240x240 ST7789 panel;
4. exposes the safe GPIO mapping to the Custom GPIO configuration layer;
5. leaves servos and PIR disabled until their wiring is verified.

This avoids a firmware that compiles for a similarly named board but drives the wrong pins or peripherals.
