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

This scaffold is a board-selection and compile-integration milestone, not a usable robot firmware. Do not flash it to the physical board until an Otto Desk-specific build, partition/flash-offset audit, and hardware validation have passed.
