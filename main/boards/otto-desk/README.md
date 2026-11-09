# Otto Desk AI — safe board scaffold

This is a dedicated XiaoZhi board profile for the stationary Otto Desk AI project.

## Current scope

- Dedicated Kconfig selector: `CONFIG_BOARD_TYPE_OTTO_DESK`
- Dedicated CMake board directory: `otto-desk`
- Wi-Fi lifecycle inherited from `WifiBoard`
- Dummy audio codec only, so application startup does not initialize unknown I2S pins
- No camera, no onboard codec, no CH32V003 expander, no battery logic, no walking/limb servos
- Otto full-screen display class now exists and uses the `otto-gif` asset collection; the board does not instantiate it until a verified runtime pin map is supplied
- No display panel, I2S, servo, camera, onboard codec or I/O expander is initialized at boot

## Waveshare carrier-board constraints

The upstream Waveshare documentation identifies an 18-pin LCD interface and a CH32V003F4U6 I/O expander on this carrier. LCD reset/backlight and other board-control signals must not be treated as ordinary ESP32-S3 GPIOs without following the schematic and expander protocol. The vendor states that V1 and V1.1 use the same programs; the physical board revision should still be recorded for diagnostics.

References:
- [Waveshare ESP32-S3-CAM-OVxxxx documentation](https://docs.waveshare.com/ESP32-S3-CAM-OVxxxx)
- [Waveshare V1.1 schematic](https://files.waveshare.com/wiki/ESP32-S3-CAM-OVxxxx/ESP32-S3-CAM-OVxxxx_Rev1.1.pdf)

## Hardware safety

All physical GPIO assignments remain intentionally absent. The ST7789, INMP441, MAX98357A, pan/tilt servos and PIR must not be enabled until a verified pin map and runtime configuration application layer are in place. Servo power must use a regulated external supply with common ground.

## Build configuration audit

The dedicated CI job combines the repository's shared ESP32-S3 defaults with `sdkconfig.otto-desk.defaults`. The shared defaults select 16 MB flash and the custom `partitions/v2/16m.csv` table. The Otto Desk workflow now asserts those generated sdkconfig values before compiling and records them in the build manifest. This is a configuration consistency check, not a physical flash test.

## Not a flashable Otto release

This scaffold's dedicated ESP32-S3 board-selection build passed in GitHub Actions run `42547497724` (commit `faf3f032346a804888d5b30620ea568f39f2ca13`). The CI artifact is named `otto-desk-safe-scaffold-not-for-hardware-flashing`. This proves the profile compiles; it does not prove that the partition map and flash offsets are suitable for the physical N16R8 module, and it does not validate hardware. Do not flash this artifact. A flashable release still requires partition/flash-offset audit, merged-image verification, real display/audio/servo drivers, and hardware validation.

## Safe configuration page

When the firmware enters its Wi-Fi provisioning access-point mode, it starts a separate Otto JSON editor at `http://192.168.4.1:8080/` (the existing captive portal remains on port 80). The editor can load the saved JSON, import a local `.json` file (maximum 3,072 bytes), export the editor contents, and submit a replacement. The endpoint accepts only the conservative safe schema: every device stays disabled and every GPIO remains null; all GPIO safety-policy flags must remain enabled. It stops when station Wi-Fi connection begins. This endpoint is a first provisioning tool, not a runtime hardware configurator.
