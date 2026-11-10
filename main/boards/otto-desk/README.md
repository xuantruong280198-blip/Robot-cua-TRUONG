# Otto Desk AI — safe board scaffold

This is a dedicated XiaoZhi board profile for the stationary Otto Desk AI project.

## Current scope

- Dedicated Kconfig selector: `CONFIG_BOARD_TYPE_OTTO_DESK`
- Dedicated CMake board directory: `otto-desk`
- Wi-Fi lifecycle inherited from `WifiBoard`
- Dummy audio codec remains the fallback; the external INMP441 + MAX98357A codec initializes only when both audio devices are enabled and the explicit routing interlock is confirmed
- ST7789 and Otto full-screen display initialize from the saved JSON only when the display is enabled and all required GPIOs are present
- MG90S PAN/TILT PWM drivers initialize at their configured center positions only when enabled, routing is confirmed, and external servo power is explicitly confirmed; XiaoZhi emotion changes are eased through a 20 ms head-motion task into the two servo axes
- HC-SR501 input initializes only when enabled with a valid GPIO; after a 30-second sensor warm-up, a rising motion event starts listening only from idle, with a 15-second cooldown
- No camera, onboard codec, CH32V003 expander, battery logic, or walking/limb servos

## Confirmed board identity and pinout status

The user has confirmed the physical module from the supplied photo as an ESP32-S3-CAM N16R8 with a 40-pin header, 16 MB flash and 8 MB PSRAM. The physical header label order shown in that photo is recorded below and must not be asked for again or silently changed. This records the visible header labels/order; it does not by itself prove the carrier PCB trace routing, peripheral wiring, or electrical suitability of any GPIO.

**User-confirmed 40-pin header label order (viewed from the same orientation as the supplied photo; top to bottom):**

| Left column | Right column |
|---|---|
| 3V3 | TX |
| EN | RX |
| 4 | 1 |
| 5 | 2 |
| 6 | 42 |
| 7 | 41 |
| 15 | 40 |
| 16 | 39 |
| 17 | 38 |
| 18 | 37 |
| 8 | 36 |
| 3 | 35 |
| 46 | 0 |
| 9 | 45 |
| 10 | 48 |
| 11 | 47 |
| 12 | 21 |
| 13 | 20 |
| 14 | 19 |
| 5V | GND |

The board identity and visible 40-pin header order are confirmed from the user's photo. The firmware records `header_labels_confirmed=true` because the photo confirms the visible header order. A separate `peripheral_routing_verified=false` interlock remains until carrier-board trace routing and each connected peripheral's actual wiring are audited.

The upstream Waveshare ESP32-S3-CAM-OVxxxx documentation and schematic are retained as comparison references only. They are **not** proof that this 40-pin module uses the same carrier layout, LCD connector, CH32V003 expander, camera routing, or onboard codec. In particular, do not apply the upstream 18-pin LCD or expander pin assignments to this board without matching the exact PCB and schematic.

References for comparison only:
- [Waveshare ESP32-S3-CAM-OVxxxx documentation](https://docs.waveshare.com/ESP32-S3-CAM-OVxxxx)
- [Waveshare V1.1 schematic](https://files.waveshare.com/wiki/ESP32-S3-CAM-OVxxxx/ESP32-S3-CAM-OVxxxx_Rev1.1.pdf)

## Hardware safety

The checked-in default JSON keeps every peripheral disabled and every GPIO unassigned. The configuration editor accepts a saved configuration only after GPIO range, denylist, and duplicate-assignment checks. Setting `board.peripheral_routing_verified=true` is an explicit operator acknowledgement that the exact physical signal routing has been checked; it is not inferred from the visible header labels. Each enabled device must have its required GPIOs. INMP441 and MAX98357A must be enabled together. Servo activation additionally requires `servos.external_power_verified=true`; power the MG90S servos from a regulated external supply and share ground with the ESP32. Saved configuration is applied after reboot.

## Build configuration audit

The dedicated CI job combines the repository's shared ESP32-S3 defaults with `sdkconfig.otto-desk.defaults`. The shared defaults select 16 MB flash and the custom `partitions/v2/16m.csv` table. The Otto Desk workflow now asserts those generated sdkconfig values before compiling and records them in the build manifest. This is a configuration consistency check, not a physical flash test.

## Latest source-build verification (2026-11-09)

The most recent verified source build is tracked by the current GitHub Actions runs; this section is updated after the new opt-in runtime path finishes compilation.

The config API reports whether the saved routing acknowledgement permits runtime activation and states that changes apply after reboot. The display/audio/servo/PIR initialization paths are compiled into the Otto board profile. The head emotion planner is connected to servo PWM output when both the servo configuration and safety interlocks permit it. PIR motion starts listening from the idle state after sensor warm-up; this behavior still needs real-device tuning against the sensor's sensitivity and hold-time settings. CI compilation does not replace a real-device test of the ST7789, I2S audio, servo direction/limits, or PIR signal.

## Not a flashable Otto release

This scaffold's dedicated ESP32-S3 board-selection build passed in GitHub Actions run `42547497724` (commit `faf3f032346a804888d5b30620ea568f39f2ca13`). The CI artifact is named `otto-desk-safe-scaffold-not-for-hardware-flashing`. This proves the profile compiles; it does not prove that the partition map and flash offsets are suitable for the physical N16R8 module, and it does not validate hardware. Do not flash this artifact. A flashable release still requires partition/flash-offset audit, merged-image verification, real display/audio/servo drivers, and hardware validation.

## Safe configuration page

When the firmware enters its Wi-Fi provisioning access-point mode, it starts a separate Otto JSON editor at `http://192.168.4.1:8080/` (the existing captive portal remains on port 80). The editor can load the saved JSON, import a local `.json` file (maximum 3,072 bytes), export the editor contents, and submit a replacement. GPIO assignments are checked against the ESP32-S3 range, module denylist, and duplicate-assignment rules. The default keeps all devices disabled. After the operator has physically checked the signal routing, setting `board.peripheral_routing_verified=true` permits enabled devices to initialize on the next reboot; INMP441 and MAX98357A must be enabled together, and servos additionally require `servos.external_power_verified=true`. The endpoint stops when station Wi-Fi connection begins.


## Two-axis head motion planner

`otto_desk_head_motion.h` provides a hardware-independent planner for the
pan/tilt head. It validates per-axis min/center/max limits and speed, clamps
requested targets, computes a bounded move duration, and provides ease-out
cubic interpolation for smooth arrivals. The board runs this planner in a
20 ms FreeRTOS task and forwards each interpolated frame to the enabled PAN
and TILT PWM drivers. Both axes are clamped to their configured limits.

Servo outputs remain off by default. Enable them only after the physical signal
routing is checked and a regulated external servo supply with common ground is
confirmed. Never power MG90S servos from the ESP32 3V3 rail.

## Emotion-driven head pose planning

The hardware-independent planner now maps the XiaoZhi tokens `listening`,
`thinking`, `speaking`, `happy`, `sad`, `surprised` and `angry` to
small relative pan/tilt offsets. Unknown or null tokens resolve to neutral.
Every target is clamped against the configured per-axis limits. The planner
is covered by host-side C++ tests in CI.

The Otto display forwards emotion changes to the head-motion sequence, which
smoothly updates the enabled servo axes. The direction of the tilt offset still
depends on bracket mounting; verify the physical direction at low travel limits
during first calibration.

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
`IsMotionDetected()` to the board's polling task. When enabled with a GPIO,
the task waits 30 seconds for sensor warm-up, then starts XiaoZhi listening
on a rising motion event only while the application is idle, with a 15-second
cooldown.

## OTA safety checks

The shared XiaoZhi OTA path now rejects firmware downloads larger than the
inactive update partition, checks the application descriptor before starting
flash writes, aborts an active OTA session after network read failures, and
refuses to finish or select an image unless the received byte count exactly
matches the declared content length. This is software-side validation; a
release still needs a verified partition layout and a real-device rollback /
recovery test.

## Latest CI verification (2026-11-10)

The dedicated Otto Desk workflow passed on commit `19fd9e2a5b4074f74f76e2e0db600e865f173ca8` in [run 42675679927](https://github.com/xuantruong280198-blip/Robot-cua-TRUONG/actions/runs/42675679927). The run completed all host-side JSON/config-page, head-motion and GPIO-safety tests, selected the Otto Desk ESP32-S3 profile, and compiled the firmware with ESP-IDF v6.0.2. The generated ESP32-S3 image is 0x1FFE10 bytes; the configured app partition has 0x1F01F0 bytes free. The artifact is `otto-desk-safe-scaffold-not-for-hardware-flashing` (artifact ID 13701081112).

The generic ESP32-S3 workflow also passed on commit `49f562224f647f67db683aa0500fefe6f24204b4` in [run 42675686330](https://github.com/xuantruong280198-blip/Robot-cua-TRUONG/actions/runs/42675686330).

These are reproducible CI/source-build results only. The Otto board still uses `DummyAudioCodec`; the display and external I2S, servo and PIR drivers are not started. No header GPIO assignment has been approved, and no physical-device test has been performed. The CI artifact must not be flashed as a finished robot firmware.


## ST7789 orientation

The display rotation field in the saved JSON is applied to the LVGL display transform for 0, 90, 180 and 270 degrees. Invalid values fall back to native orientation; the JSON validator independently rejects unsupported values.

## Servo mechanical travel limits

The MG90S PWM driver now accepts per-axis minimum and maximum angles and clamps every requested position to that configured envelope before converting it to a pulse. Invalid mechanical limits fail closed and prevent driver initialization. The pure host-side servo test covers both limit clamping and pulse conversion. This improves the output guard but does not authorize PWM: the exact board pin map and external servo supply remain unverified, so the board still does not instantiate the servo driver.


## Stateful emotion-to-motion sequence

`HeadMotionSequence` carries the current pan/tilt pose between emotion
changes. It supports ease-out movement using each axis's configured speed,
retargeting during motion, exact target arrival, and fail-closed behavior for
invalid axis limits. A future board task can advance the sequence and forward
poses to the guarded PWM drivers. The sequence itself never enables GPIO.
