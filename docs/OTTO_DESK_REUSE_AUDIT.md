# Otto Desk: upstream code reuse audit

Inspected the imported XiaoZhi v2.5.0 source in this repository.

## Existing Otto board is not a desk-robot driver

The existing `main/boards/otto-robot` implementation is designed around a different physical robot:

- `config.h` declares six limb/hand servo channels and two alternative fixed hardware pin maps.
- The camera variant uses camera detection and camera initialization; the board source includes `esp32_camera.h`.
- Automatic hardware detection samples GPIO15 and GPIO16.
- `otto_controller.cc` contains walking, turning, jumping, leg/hand movement and multi-servo sequence actions.
- The board source starts a WebSocket control server on port 8080 and initializes an Otto movement controller.

These assumptions must not be carried into the desk build. The desk build has only two head servos (pan/tilt), no legs, no camera and no camera-version autodetection.

## Reusable parts

### Full-screen Otto face

- `OttoEmojiDisplay` derives from `SpiLcdDisplay`.
- It sizes the preview image to the complete display width and height.
- It switches to the dark theme and selects the Otto GIF emoji collection through the existing assets configuration.
- It maps XiaoZhi listening/speaking/connecting/standby states to compact status indicators.

This display class is a strong candidate for reuse, subject to confirming that its LVGL image assets fit the selected 240x240 panel and available flash/PSRAM budget.

### Separate external I2S audio

- `NoAudioCodecSimplex` accepts independent speaker and microphone pin groups.
- This matches the electrical topology of an external MAX98357A output and INMP441 input more closely than `BoxAudioCodec`.
- `BoxAudioCodec` constructs ES8311 and ES7210 codec devices over I2C, so it is not the correct codec path for the external-module configuration.

The actual I2S GPIO values must be supplied by the validated board configuration; do not copy values from another board's `config.h`.

The source constructor maps the speaker as `spk_bclk`, `spk_ws`, `spk_dout` and the microphone as `mic_sck`, `mic_ws`, `mic_din`. It creates separate TX and RX I2S channels. The desk profile must map the JSON fields to this exact constructor order and must not use the PDM-specific `NoAudioCodecSimplexPdm` class for an INMP441 standard-I2S microphone.

## Desk-specific implementation boundary

The new board profile should own only:

1. ST7789 240x240 panel initialization and Otto face display.
2. External simplex I2S microphone/speaker initialization.
3. Two-channel pan/tilt servo control, with independent angle limits and slew rates.
4. Optional PIR input.
5. Safe local configuration and status endpoints.

The biped controller, leg/hand channels, camera detection, camera driver, battery/charging assumptions and the existing WebSocket movement command protocol are out of scope unless a later requirement explicitly calls for a compatible feature.

## Build gate

Do not register a selectable `otto-desk` firmware target until its board source, board config, Kconfig entry and CMake selection are present together. A directory or JSON profile alone does not create a buildable ESP-IDF target. The first firmware build must use a complete, internally consistent profile and must not enable peripherals with unassigned pins.
