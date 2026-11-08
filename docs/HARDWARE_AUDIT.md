# Hardware audit — ESP32-S3 CAM N16R8

## Status: identification required before board code or wiring

The project description currently identifies the module only as "ESP32-S3 CAM N16R8". That identifies the MCU family and memory configuration, but does not uniquely identify the carrier-board schematic, connector pinout, power circuit, or exposed GPIOs.

Do not select a XiaoZhi board profile solely because another board also uses ESP32-S3 and has a camera connector.

## Upstream candidate inspected

The upstream v2.5.0 source contains `DFRobot ESP32-S3 AI Camera Module` at:

- `main/boards/dfrobot/df-s3-ai-cam/README.md`
- `main/boards/dfrobot/df-s3-ai-cam/config.h`

Its documented profile is specifically for DFRobot SKU DFR1154, with an OV3660 camera and board-specific peripherals. Its config assigns microphone GPIO 38/39, speaker GPIO 42/45/46, gain GPIO 41, LED GPIO 3, and camera pins including GPIO 4–21.

**This is not verified to be the user's Waveshare ESP32-S3-CAM N16R8 module. Do not reuse that pin map.** The profile's camera and peripheral assignments may conflict with the user's wiring or board routing.

## Confirmed project constraints

- Target memory: 16 MB flash and 8 MB PSRAM (N16R8).
- Camera must remain disabled.
- Display target: ST7789, 240×240.
- Audio devices: INMP441 microphone and MAX98357A amplifier.
- Two servos: pan and tilt; servo outputs remain disabled until pinout and power are checked.
- Optional PIR: HC-SR501.
- Unknown GPIO assignments must remain null.

## Required evidence before creating the custom board profile

1. Exact board manufacturer and product/SKU, or a clear photo of both sides and all printed labels.
2. Board schematic/pinout for the exact revision.
3. Confirmation of which GPIOs are physically exposed and which are reserved for flash/PSRAM, USB, camera, onboard peripherals, or boot strapping.
4. Confirmed display and I2S wiring.
5. Servo supply rating and common-ground arrangement.

## Next engineering step

Once the exact board is identified, add a dedicated XiaoZhi board directory and Kconfig entry, starting from the closest verified ESP32-S3 board implementation. Keep the camera driver out of the selected profile. Then build with ESP-IDF 6.1 and test each peripheral independently.

A successful compile is not proof that the pinout or electrical design is safe.
