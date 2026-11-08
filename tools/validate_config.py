#!/usr/bin/env python3
"""Validate the robot hardware JSON without requiring third-party packages."""

import json
import pathlib
import sys

CONFIG = pathlib.Path(__file__).resolve().parents[1] / "config" / "default.json"

# Signal names used by this project's nested device configuration.
PIN_KEYS = {
    "gpio", "ws", "sck", "sd", "din", "bclk", "lrclk",
    "sclk", "mosi", "miso", "cs", "dc", "reset", "backlight",
}


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    try:
        data = json.loads(CONFIG.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"cannot read valid JSON from {CONFIG}: {exc}")

    if data.get("schema_version") != 1:
        fail("schema_version must be 1")

    board = data.get("board", {})
    if board.get("manufacturer") != "Waveshare":
        fail("board.manufacturer must be Waveshare")
    if board.get("camera_enabled") is not False:
        fail("camera_enabled must remain false for this project")
    if board.get("onboard_codec_enabled") is not False:
        fail("onboard codec must remain disabled while external I2S audio is selected")
    if board.get("onboard_io_expander_enabled") is not False:
        fail("onboard I/O expander must remain disabled until its role is implemented")

    pin_values = []

    def collect(value, path="root"):
        if isinstance(value, dict):
            for key, child in value.items():
                child_path = f"{path}.{key}"
                if key in PIN_KEYS:
                    if child is not None and (
                        isinstance(child, bool) or not isinstance(child, int)
                    ):
                        fail(f"{child_path} must be an integer GPIO or null")
                    if child is not None:
                        pin_values.append((child, child_path))
                else:
                    collect(child, child_path)
        elif isinstance(value, list):
            for index, child in enumerate(value):
                collect(child, f"{path}[{index}]")

    collect(data)

    seen = {}
    for gpio, path in pin_values:
        if not 0 <= gpio <= 48:
            fail(f"{path} uses out-of-range GPIO {gpio}")
        if gpio in seen:
            fail(f"GPIO {gpio} is assigned more than once: {seen[gpio]} and {path}")
        seen[gpio] = path

    # The external-module pin map has not been approved yet. Keep every
    # signal unassigned so an unverified profile cannot drive hardware.
    def assert_unassigned(value, path="root"):
        if isinstance(value, dict):
            for key, child in value.items():
                child_path = f"{path}.{key}"
                if key in PIN_KEYS and (child is None or isinstance(child, (int, bool))):
                    if child is not None:
                        fail(f"{child_path} must be null until the exact header mapping is verified")
                else:
                    assert_unassigned(child, child_path)
        elif isinstance(value, list):
            for index, child in enumerate(value):
                assert_unassigned(child, f"{path}[{index}]")

    assert_unassigned(data)

    servos = data.get("servos", {})
    if servos.get("external_power_verified") is not False:
        fail("servo external_power_verified must remain false until verified")
    for name in ("pan", "tilt"):
        if servos.get(name, {}).get("enabled") is not False:
            fail(f"servo {name} must remain disabled until wiring is verified")
    if data.get("pir", {}).get("enabled") is not False:
        fail("PIR must remain disabled until wiring is verified")

    # Validate cross-field safety rules that JSON Schema alone does not
    # express clearly for this versioned hardware contract.
    servos = data.get("servos", {})
    any_servo_enabled = False
    for name in ("pan", "tilt"):
        servo = servos.get(name, {})
        minimum = servo.get("min_deg")
        center = servo.get("center_deg")
        maximum = servo.get("max_deg")
        if not (minimum <= center <= maximum):
            fail(f"servos.{name} must satisfy min_deg <= center_deg <= max_deg")
        if servo.get("enabled") is True:
            any_servo_enabled = True
            if servo.get("gpio") is None:
                fail(f"servos.{name}.gpio is required when that servo is enabled")

    if any_servo_enabled and servos.get("external_power_verified") is not True:
        fail("external servo power must be verified before enabling either servo")

    required_pin_groups = [
        ("display.enabled", data.get("display", {}).get("enabled"),
         data.get("display", {}).get("gpio", {})),
        ("audio.microphone.enabled", data.get("audio", {}).get("microphone", {}).get("enabled"),
         data.get("audio", {}).get("microphone", {}).get("i2s", {})),
        ("audio.amplifier.enabled", data.get("audio", {}).get("amplifier", {}).get("enabled"),
         data.get("audio", {}).get("amplifier", {}).get("i2s", {})),
    ]
    for label, enabled, pins in required_pin_groups:
        if enabled is True and any(value is None for value in pins.values()):
            fail(f"{label} requires every GPIO in its pin group to be assigned")

    pir = data.get("pir", {})
    if pir.get("enabled") is True and pir.get("gpio") is None:
        fail("pir.gpio is required when PIR is enabled")

    print("Hardware JSON validation passed (Waveshare target, safe unassigned defaults).")


if __name__ == "__main__":
    main()
