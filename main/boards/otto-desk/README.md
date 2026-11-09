# Otto Desk AI — safe board scaffold

This is a dedicated XiaoZhi board profile for the stationary Otto Desk AI project.

## Current scope

- Dedicated Kconfig selector: `CONFIG_BOARD_TYPE_OTTO_DESK`
- Dedicated CMake board directory: `otto-desk`
- Wi-Fi lifecycle inherited from `WifiBoard`
- Dummy audio codec only, so application startup does not initialize unknown I2S pins
- No camera, no onboard codec, no CH32V003 expander, no battery logic, no walking/limb servos
- No display or external audio hardware is initialized yet

## Hardware safety

All physical GPIO assignments remain intentionally absent. The ST7789, INMP441, MAX98357A, pan/tilt servos and PIR must not be enabled until a verified pin map and runtime configuration application layer are in place. Servo power must use a regulated external supply with common ground.

## Not a flashable Otto release

This scaffold's dedicated ESP32-S3 board-selection build passed in GitHub Actions run `42547497724` (commit `faf3f032346a804888d5b30620ea568f39f2ca13`). The CI artifact is named `otto-desk-safe-scaffold-not-for-hardware-flashing`. This proves the profile compiles; it does not prove that the partition map and flash offsets are suitable for the physical N16R8 module, and it does not validate hardware. Do not flash this artifact. A flashable release still requires partition/flash-offset audit, merged-image verification, real display/audio/servo drivers, and hardware validation.

## Safe configuration page

When the firmware enters its Wi-Fi provisioning access-point mode, it starts a separate Otto JSON editor at `http://192.168.4.1:8080/` (the existing captive portal remains on port 80). The editor can load the saved JSON and submit a replacement. The endpoint accepts only the conservative safe schema: every device stays disabled and every GPIO remains null. It stops when station Wi-Fi connection begins. This endpoint is a first provisioning tool, not a runtime hardware configurator.
