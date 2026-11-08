# Otto Desk board profile implementation plan

## Decision

Build a dedicated `otto-desk` board profile on top of XiaoZhi v2.5.0. Do not reuse the existing `otto-robot` board implementation as-is.

The existing upstream `otto-robot` profile is a different product: it contains six limb/hand servo channels, hardware-version detection on GPIO15/16, camera detection and walking/dance control. This project is a stationary desk robot with only pan/tilt head servos and no camera.

## Reusable upstream components

- `NoAudioCodecSimplex` supports separate I2S microphone and speaker pin groups, matching the INMP441 + MAX98357A architecture more closely than `BoxAudioCodec`, which initializes ES8311/ES7210 codec devices over I2C.
- The existing ST7789 panel initialization patterns in board implementations can be reused, but all pin assignments must come from the Otto configuration and must not be copied from another board.
- The existing Otto animation assets/display code may be evaluated separately from the biped motion controller.

## Profile requirements

1. Register a distinct `CONFIG_BOARD_TYPE_OTTO_DESK` Kconfig choice and CMake board directory.
2. Keep camera support and camera initialization out of the desk profile.
3. Use the external INMP441 and MAX98357A audio path; do not initialize the Waveshare onboard codec or CH32V003 expander.
4. Initialize the external 240x240 ST7789 panel.
5. Keep pan/tilt servos and PIR disabled at first boot. Enable them only after valid GPIO configuration and safety checks.
6. Use one versioned GPIO configuration model for the local web page and JSON import/export. Reject duplicate GPIO assignments and unsupported pin roles.
7. Keep all unknown signal assignments unset. The build must not silently substitute GPIO numbers from the upstream Waveshare camera profile or the biped Otto profile.

## Implementation sequence

- [x] Record the hardware contract and safe JSON defaults.
- [x] Add configuration validation, JSON Schema checks, regression tests and a passing GitHub Actions workflow.
- [x] Add host-side validation for externally supplied/imported JSON files (`--config`) with valid/invalid import regression tests.
- [x] Confirm the upstream board profile mismatch and identify reusable audio/display components.
- [x] Audit the existing Otto biped source and isolate reusable full-screen face and external I2S components.
- [ ] Add the dedicated board selector and minimal desk-profile source (blocked until the runtime GPIO config and board initialization path are implemented together).
- [ ] Integrate the runtime GPIO configuration service and persistent JSON storage (NVS adapter not implemented yet).
- [ ] Add firmware-side schema/safety validation before accepting JSON into NVS; host validation alone is not sufficient.
- [ ] Add Otto full-screen face rendering and head-servo state mapping.
- [ ] Add the firmware build workflow and verify a downloadable merged BIN artifact.
- [ ] Validate the final firmware on the physical board; CI success alone is not hardware validation.

## Important distinction

Skipping additional board photos does not authorize guessing GPIO assignments. Firmware structure and the configuration interface can be developed now with safe unassigned defaults; actual peripherals remain disabled until their GPIO mapping is entered and validated.
