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
                if key in PIN_KEYS and child is not None:
                    if isinstance(child, bool) or not isinstance(child, int):
                        fail(f"{child_path} must be an integer GPIO or null")
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
                if key in PIN_KEYS and child is not None:
                    fail(f"{child_path} must be null until the exact header mapping is verified")
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

    print("Hardware JSON validation passed (Waveshare target, safe unassigned defaults).")


if __name__ == "__main__":
    main()
