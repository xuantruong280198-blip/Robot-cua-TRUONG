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


## Two-axis head motion planner

`otto_desk_head_motion.h` provides a hardware-independent planner for the
pan/tilt head. It validates per-axis min/center/max limits and speed, clamps
requested targets, computes a bounded move duration, and provides ease-out
cubic interpolation for smooth arrivals. The module has no GPIO, LEDC, or
servo-driver calls and is not yet connected to physical outputs.

Servo activation remains deliberately disabled until the exact 40-pin header
pinout is verified and the external servo supply is confirmed. Do not connect
servo power to the ESP32 3V3 rail.

## Emotion-driven head pose planning

The hardware-independent planner now maps the XiaoZhi tokens `listening`,
`thinking`, `speaking`, `happy`, `sad`, `surprised` and `angry` to
small relative pan/tilt offsets. Unknown or null tokens resolve to neutral.
Every target is clamped against the configured per-axis limits. The planner
is covered by host-side C++ tests in CI.

This is the behavior layer only: it does not move a servo, infer bracket
orientation, or replace calibration. In particular, the sign of the tilt
offset depends on how the pan/tilt bracket is mounted and must be checked
before any PWM output is enabled.

## ESP32-S3 N16R8 GPIO safety policy

The servo and HC-SR501 drivers now share `otto_desk_gpio_safety.h`.
The conservative chip-level denylist blocks GPIO0/3/19/20, GPIO26-37
(octal flash/PSRAM routing risk), and GPIO43-46 (UART0/strapping roles).
Host-side tests run in the Otto Desk CI before the ESP-IDF build.

This denylist is not a board pinout. GPIOs that pass it are not automatically
approved for use: activation still requires the exact 40-pin carrier mapping,
conflict checks, and the existing explicit safety interlocks.

## Guarded PIR input driver

`otto_desk_pir.h/.cc` now provides a small HC-SR501 input driver. It
requires an explicitly validated board pinout, rejects reserved/strapping
GPIOs, configures a plain digital input without an interrupt, and exposes
`IsMotionDetected()` for a future board polling task. It is compiled by the
Otto Desk board-source glob. It is not yet instantiated by the board because
the saved runtime configuration keeps PIR disabled and its GPIO unassigned.

## OTA safety checks

The shared XiaoZhi OTA path now rejects firmware downloads larger than the
inactive update partition, checks the application descriptor before starting
flash writes, aborts an active OTA session after network read failures, and
refuses to finish or select an image unless the received byte count exactly
matches the declared content length. This is software-side validation; a
release still needs a verified partition layout and a real-device rollback /
recovery test.
