# Hardware configuration schema v1

The canonical starter configuration is `default.json`.

## Safety defaults

- All GPIO fields are `null` until the exact Waveshare board revision and schematic are checked.
- Camera is permanently disabled for this project.
- Display, microphone, amplifier, PIR and servo drivers start disabled.
- Pan and tilt angle values are software targets only; they do not imply safe mechanical travel.
- Servo power verification must remain false until the external supply and shared ground are checked.

## Adding a GPIO mapping

Before assigning a pin:

1. Confirm the exact pin against the matching board schematic.
2. Check that it is exposed and not used by flash/PSRAM, USB, boot strapping, UART, camera, or another peripheral.
3. Check peripheral direction and electrical voltage requirements.
4. Ensure each physical GPIO is assigned to at most one active function.
5. Keep the hardware disabled until wiring has been independently checked.

## Planned configuration groups

- `board`: chip, flash/PSRAM capacity and camera policy.
- `display`: ST7789 dimensions, orientation and SPI signals.
- `audio.microphone`: INMP441 I2S input.
- `audio.amplifier`: MAX98357A I2S output and speaker details.
- `servos.pan`, `servos.tilt`: GPIO, software angle bounds, slew rate and enable state.
- `pir`: HC-SR501 event input.
- `gpio_policy`: validation requirements.

This schema is a project configuration contract, not yet a runtime firmware integration. The firmware adapter and local web editor must be implemented and tested before claiming live GPIO reconfiguration.
