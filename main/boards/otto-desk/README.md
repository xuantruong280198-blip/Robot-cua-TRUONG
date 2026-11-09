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

## Confirmed board identity and pinout status

The user has confirmed the physical module as an ESP32-S3-CAM N16R8 with a 40-pin header, 16 MB flash and 8 MB PSRAM. The exact PCB revision, header pin order, and mapping of each header position to ESP32-S3 GPIO are not yet verified.

The upstream Waveshare ESP32-S3-CAM-OVxxxx documentation and schematic are retained as comparison references only. They are **not** proof that this 40-pin module uses the same carrier layout, LCD connector, CH32V003 expander, camera routing, or onboard codec. In particular, do not apply the upstream 18-pin LCD or expander pin assignments to this board without matching the exact PCB and schematic.

References for comparison only:
- [Waveshare ESP32-S3-CAM-OVxxxx documentation](https://docs.waveshare.com/ESP32-S3-CAM-OVxxxx)
- [Waveshare V1.1 schematic](https://files.waveshare.com/wiki/ESP32-S3-CAM-OVxxxx/ESP32-S3-CAM-OVxxxx_Rev1.1.pdf)

## Hardware safety

All physical GPIO assignments remain intentionally absent. The ST7789, INMP441, MAX98357A, pan/tilt servos and PIR must not be enabled until a verified pin map and runtime configuration application layer are in place. Servo power must use a regulated external supply with common ground.

## Build configuration audit

The dedicated CI job combines the repository's shared ESP32-S3 defaults with `sdkconfig.otto-desk.defaults`. The shared defaults select 16 MB flash and the custom `partitions/v2/16m.csv` table. The Otto Desk workflow now asserts those generated sdkconfig values before compiling and records them in the build manifest. This is a configuration consistency check, not a physical flash test.

## Latest source-build verification (2026-11-09)

The latest verified source commit is `e3280cf63266ee2c8be85f1ae7064f39d7e484b6`. The Otto Desk board-profile compile passed in [run 42593198331](https://github.com/xuantruong280198-blip/Robot-cua-TRUONG/actions/runs/42593198331), including the configuration-page regression tests, ESP-IDF configuration, and the dedicated Otto Desk build. The generic ESP32-S3 smoke build also passed in [run 42593198437](https://github.com/xuantruong280198-blip/Robot-cua-TRUONG/actions/runs/42593198437).

The latest change also separates empty config requests (HTTP 400) from oversized requests (HTTP 413) and fixes the JSON error-response string escaping. The resulting artifact remains named `otto-desk-safe-scaffold-not-for-hardware-flashing`. The display class is compiled, but it is not instantiated by the board at runtime. GPIO assignments remain unconfigured and all peripherals remain disabled. Source compilation is verified; operation on the physical ST7789, microphone, amplifier, and servos is not.

## Not a flashable Otto release

This scaffold's dedicated ESP32-S3 board-selection build passed in GitHub Actions run `42547497724` (commit `faf3f032346a804888d5b30620ea568f39f2ca13`). The CI artifact is named `otto-desk-safe-scaffold-not-for-hardware-flashing`. This proves the profile compiles; it does not prove that the partition map and flash offsets are suitable for the physical N16R8 module, and it does not validate hardware. Do not flash this artifact. A flashable release still requires partition/flash-offset audit, merged-image verification, real display/audio/servo drivers, and hardware validation.

## Safe configuration page

When the firmware enters its Wi-Fi provisioning access-point mode, it starts a separate Otto JSON editor at `http://192.168.4.1:8080/` (the existing captive portal remains on port 80). The editor can load the saved JSON, import a local `.json` file (maximum 3,072 bytes), export the editor contents, and submit a replacement. The endpoint accepts only the conservative safe schema: every device stays disabled and every GPIO remains null; all GPIO safety-policy flags must remain enabled. It stops when station Wi-Fi connection begins. This endpoint is a first provisioning tool, not a runtime hardware configurator.
