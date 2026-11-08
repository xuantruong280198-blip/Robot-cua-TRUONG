#!/usr/bin/env python3
"""Validate the robot hardware JSON without requiring third-party packages."""

import json
import pathlib
import sys

CONFIG = pathlib.Path(__file__).resolve().parents[1] / "config" / "default.json"


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
    if board.get("camera_enabled") is not False:
        fail("camera_enabled must remain false for this project")

    gpio_values = []
    def collect(value, path="root"):
        if isinstance(value, dict):
            for key, child in value.items():
                if key == "gpio" and isinstance(child, int) and not isinstance(child, bool):
                    gpio_values.append((child, f"{path}.gpio"))
                else:
                    collect(child, f"{path}.{key}")
        elif isinstance(value, list):
            for index, child in enumerate(value):
                collect(child, f"{path}[{index}]")

    collect(data)
    seen = {}
    for gpio, path in gpio_values:
        if not 0 <= gpio <= 48:
            fail(f"{path} uses out-of-range GPIO {gpio}")
        if gpio in seen:
            fail(f"GPIO {gpio} is assigned more than once: {seen[gpio]} and {path}")
        seen[gpio] = path

    # Until the exact carrier-board schematic is confirmed, all hardware pins
    # must remain unassigned and output peripherals must remain disabled.
    def assert_unassigned(value, path="root"):
        if isinstance(value, dict):
            for key, child in value.items():
                if key == "gpio" and child is not None:
                    fail(f"{path}.gpio must be null until board pinout is verified")
                assert_unassigned(child, f"{path}.{key}")
        elif isinstance(value, list):
            for index, child in enumerate(value):
                assert_unassigned(child, f"{path}[{index}]")

    assert_unassigned(data)

    if data.get("servos", {}).get("external_power_verified") is not False:
        fail("servo external_power_verified must remain false until measured/verified")
    for name in ("pan", "tilt"):
        if data.get("servos", {}).get(name, {}).get("enabled") is not False:
            fail(f"servo {name} must remain disabled until wiring is verified")

    print("Hardware JSON validation passed (safe unassigned defaults).")


if __name__ == "__main__":
    main()
