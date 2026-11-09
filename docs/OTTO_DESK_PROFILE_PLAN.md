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
- [x] Add a dedicated `CONFIG_BOARD_TYPE_OTTO_DESK` selector, CMake board directory and minimal safe board factory. The current scaffold uses a dummy audio codec and initializes no GPIO, display, camera, servo, PIR or onboard peripheral; it is not yet a usable robot firmware.
- [x] Add a bounded NVS JSON storage primitive (`OttoDeskConfigStore`) with read/write/clear operations and firmware-side JSON safety checks. It rejects non-object/invalid JSON, oversized payloads, enabled peripherals, and any assigned GPIO until a verified pin map exists.
- [x] Initialize NVS and read/revalidate a saved JSON config during application startup; invalid or missing stored data is logged without crashing the boot path. The loaded JSON is not yet applied to hardware drivers.
- [ ] Add the local HTTP configuration API, browser editor/import/export, and explicit default-config recovery behavior.

- [x] Add firmware-side conservative safe-state validation before accepting JSON into NVS, including exact object key sets, fixed board identity, duplicate-key rejection, and disabled/unassigned peripheral checks.
- [ ] Implement full firmware-side schema validation matching `config/schema-v1.schema.json` before enabling configurable peripherals; the current exact-shape validator is a conservative safe-state gate only.
- [ ] Add Otto full-screen face rendering and head-servo state mapping.
- [x] Add an ESP32-S3 generic compile smoke workflow and downloadable build artifact. This validates the current repository default-board build only; it is not an Otto Desk firmware release and must not be flashed to the Waveshare module.
- [x] Add a dedicated Otto Desk CI build workflow and verify the selected `CONFIG_BOARD_TYPE_OTTO_DESK` profile compiles on ESP32-S3. Run `42547497724` completed successfully on 2026-11-09 and uploaded `otto-desk-safe-scaffold-not-for-hardware-flashing` (14-day retention). This proves board-selection/build integration only; partition-map suitability, merged flash image/offset audit and physical hardware validation remain pending. Do not flash this artifact.
- [ ] Validate the final firmware on the physical board; CI success alone is not hardware validation.

## Important distinction

Skipping additional board photos does not authorize guessing GPIO assignments. Firmware structure and the configuration interface can be developed now with safe unassigned defaults; actual peripherals remain disabled until their GPIO mapping is entered and validated.


## CI artifact safety (2026-11-09)

The current workflow runs `idf.py set-target esp32s3` followed by `idf.py build` without selecting `CONFIG_BOARD_TYPE_OTTO_DESK` (that board choice does not exist yet). ESP-IDF therefore builds the repository's default ESP32-S3 board configuration, not the Waveshare Otto Desk hardware profile.

The workflow artifact is deliberately named `generic-esp32s3-smoke-not-for-flashing` and includes `BUILD-MANIFEST.txt` to make this distinction visible. Its `xiaozhi.bin` is evidence that the generic firmware tree compiles; it is not a validated firmware image for the user's N16R8 board. Do not flash it to the physical robot.

Next implementation gate: add a dedicated Kconfig choice, CMake board directory and exactly one board factory, then add a board-specific CI job. Keep display, external I2S, servos and PIR disabled until a verified pin map and runtime config application layer exist.


## Wi-Fi configuration portal integration findings (2026-11-09)

The tracked `main/boards/common/wifi_board.cc` initializes `WifiManager`, starts its captive AP through `StartConfigAp()`, and displays `GetApWebUrl()`. The tracked `WifiBoard` interface does not expose an HTTP-server handle or route-registration hook. The implementation of the external Wi-Fi manager dependency is not present in the repository tree, so its private internals and extension points have not been verified.

Do not assume that the Otto JSON editor can be mounted into the existing captive portal. Before adding endpoints, inspect the exact resolved dependency version and its public API. If it has no supported route extension, implement a separate, explicitly documented Otto configuration server on a non-conflicting port after network readiness. Require bounded request bodies, strict JSON validation, NVS save/reload behavior, and no peripheral activation in the first iteration.

The generic CI artifact remains explicitly named `generic-esp32s3-smoke-not-for-flashing`; it is still not an Otto Desk image. A separate Otto Desk Kconfig selector, board factory and successful board-specific compile now exist, but partition/flash-offset verification, merged-image verification and hardware validation remain incomplete. Keep both artifacts clearly distinguished and do not flash either one.


## Dedicated board scaffold (2026-11-09)

Added `main/boards/otto-desk/` with a board factory, build metadata and safety README. The board factory returns a `DummyAudioCodec` and inherits Wi-Fi behavior; it does not configure any GPIO or initialize physical hardware. This avoids borrowing the unrelated Waveshare camera or ElectronBot pin maps while allowing the board selection path to be integrated. The next step is an Otto-specific compile job that selects `CONFIG_BOARD_TYPE_OTTO_DESK`, then fix any compile/configuration issues before adding display and external I2S drivers.


## Otto Desk CI result (2026-11-09)

The dedicated workflow `.github/workflows/otto-desk-smoke.yml` completed successfully: run `42547497724`, commit `faf3f032346a804888d5b30620ea568f39f2ca13`. Its ESP-IDF 6.0.2 job selected `CONFIG_BOARD_TYPE_OTTO_DESK`, compiled the safe scaffold, and uploaded artifact `otto-desk-safe-scaffold-not-for-hardware-flashing` (artifact ID `13643729485`, SHA-256 `3bf65af09c2d27d1b1bc9b0e04d55223c53a29a4711b996ebd3727d3e92cb45b`, expires 2026-11-23). The workflow manifest explicitly records `hardware_validation=not-performed` and `gpio_assignments=unconfigured`. This artifact is a CI proof package, not a flashable robot firmware. Partition layout, flash offsets, full image assembly and physical hardware validation are still open gates.


## Safe provisioning configuration API (2026-11-09)

Added `OttoDeskConfigServer` on port 8080. The server starts only after the Wi-Fi provisioning AP reports `WifiConfigModeEnter`, and stops when station connection/config-mode exit is reported. The existing captive portal remains on its own port 80. The new page provides a small JSON editor; `GET /api/config` reads the validated NVS value and `POST /api/config` accepts a body only when it is 1–3072 bytes and passes `OttoDeskConfigStore::Save`. There is no route to enable hardware, no GPIO write path, and no clear/reset route. The stored schema remains intentionally all-disabled/unassigned.

The external `78/esp-wifi-connect` public API confirms the config AP uses an HTTP server and emits `WifiConfigModeEnter` after AP startup; the board hook now uses that event rather than trying to register routes inside the dependency's private HTTP server. The Otto endpoint uses port 8080 to avoid the existing portal on port 80.

This is a provisioning-only first iteration. It has not yet passed the latest dedicated build, and it has not been exercised on physical hardware. Do not assume the page is available in normal station mode; it is intentionally stopped when Wi-Fi station connection begins.

## JSON provisioning editor and policy hardening (2026-11-09)

The port-8080 provisioning page now supports local JSON file import (maximum 3,072 bytes), in-browser JSON parsing, export to a local JSON file, and NVS submission through the bounded API. This remains a safe-state editor only: no peripheral or GPIO can be enabled.

Firmware-side validation now requires every GPIO policy flag to remain true, including duplicate assignment, bootstrap, flash/PSRAM, USB/UART and board-pinout checks. The host-side validator applies the same fail-closed policy and regression tests reject attempts to turn off any of these flags.

Latest relevant commits: 2082a5b (JSON import/export), 749e60b (firmware fail-closed policy), 7b2d43d (README), 0e193a1 (host validator regression test). CI is still running for the latest changes; do not treat them as passed until the corresponding runs finish.

## GPIO planning matrix in provisioning page (2026-11-09)

The Wi-Fi provisioning page now presents a read-only hardware signal matrix for ST7789, INMP441, MAX98357A, pan/tilt servos and HC-SR501 PIR. It explicitly labels every signal unassigned and states that the table is not a verified board pinout. The JSON editor remains fail-closed and cannot activate any peripheral.

Host-side configuration validation passed on commit f89d658. The dedicated Otto Desk compile and generic ESP-IDF compile for the GPIO matrix UI commit 1d145549 are still running; wait for CI before calling this revision build-verified.
