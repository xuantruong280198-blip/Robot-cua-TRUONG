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

## Latest CI verification (2026-11-11)

The dedicated Otto Desk ESP32-S3 N16R8 workflow passed on commit
`4038690ba37c88a5bb89a3a07b8db8d671fb09b5` in
[run 42919978051](https://github.com/xuantruong280198-blip/Robot-cua-TRUONG/actions/runs/42919978051).
The build includes the host-side JSON/config-page, GPIO-safety, I2S conversion,
servo pulse, head-motion, OTA-version, and ST7789 rotation tests, followed by
the ESP-IDF v6.0.2 Otto Desk profile build and merged-image packaging. The
provisioning artifact is `otto-desk-n16r8-provisioning-firmware`
(artifact ID 13811761500).

The generic ESP32-S3 smoke workflow also passed on the same commit in
[run 42919977777](https://github.com/xuantruong280198-blip/Robot-cua-TRUONG/actions/runs/42919977777).

These results prove that the current source compiles and its host-side checks
pass. They do **not** prove that the display, I2S audio, servos, or PIR work on
the physical module. The firmware keeps all peripherals disabled by default;
the runtime drivers start only after the saved JSON explicitly acknowledges
the board's peripheral routing, with a separate external-power confirmation
for the servos. No physical GPIO routing has been certified and no device test
has been performed. **Do not flash this provisioning artifact as a finished
robot release.**

## ST7789 orientation

The validated `display.rotation` field is applied to the LVGL display
transform for 0, 90, 180, and 270 degrees. A dedicated host-side test covers
all four orientations and the invalid-value fallback.

## Servo motion and safety

The MG90S PWM drivers enforce configured per-axis minimum and maximum angles
and convert bounded angles to 50 Hz PWM pulses. The board connects the
stateful emotion-to-motion planner to the two servo axes through a 20 ms task.
Emotion targets include listening, thinking, speaking, happy, sad, surprised,
and angry; all targets are clamped to each axis's configured limits. Servo
initialization remains gated by the routing acknowledgement and a separate
external-power confirmation. Use a regulated external servo supply with a
shared ground; never power the servos from the ESP32 3V3 rail.

## Audio and PIR runtime

The external INMP441 + MAX98357A I2S codec is selected only when both devices
are enabled and all six signal GPIOs pass validation. It uses independent I2S
RX/TX ports and retains the XiaoZhi dummy codec as a safe fallback if setup
fails. The HC-SR501 polling task waits 30 seconds after startup, then reacts
to a rising motion edge only while XiaoZhi is idle, with a 15-second cooldown.
Both paths are compiled and guarded, but require real-device signal and audio
testing before being considered validated.

## Release gate

Before calling this a flashable release, the project still needs a physical
carrier-board signal-routing audit, confirmed GPIO assignments for every
connected peripheral, verification of the 16 MB partition/flash-offset layout
against the actual module, and device-level tests for display, microphone,
speaker, servo direction/travel, PIR behavior, OTA, and rollback/recovery.


## Manual Wi-Fi provisioning trigger

The Otto Desk profile reserves **GPIO40** exclusively as a startup provisioning trigger. To force the Wi-Fi access point even when the saved home network is available:

1. Power the ESP32-S3 off.
2. Connect GPIO40 to GND with a Dupont jumper (do not connect it to 3V3).
3. Power the board on and keep GPIO40 connected to GND for at least the first second.
4. The firmware detects the stable low level, stops/overrides station connection, and enters Wi-Fi provisioning AP mode.
5. Disconnect the jumper after the board has started; then connect the tablet to the `Xiaozhi-xxxx` AP and open `http://192.168.4.1:8080/`.

GPIO40 is removed from the user Custom GPIO assignment pool so the trigger cannot conflict with a peripheral. GPIO0/BOOT is deliberately not used because it is an ESP32-S3 boot-strapping pin. This feature requires a firmware build containing this change; it has not been physically tested on the module.
