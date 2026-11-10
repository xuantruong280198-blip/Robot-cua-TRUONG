#!/usr/bin/env python3
"""Validate the robot hardware JSON without requiring third-party packages."""

import argparse
import json
import pathlib
import sys

CONFIG = pathlib.Path(__file__).resolve().parents[1] / "config" / "default.json"

# Signal names used by this project's nested device configuration.
PIN_KEYS = {
    "gpio", "ws", "sck", "sd", "din", "bclk", "lrclk",
    "sclk", "mosi", "miso", "cs", "dc", "reset", "backlight",
}

# Keep this in sync with boards/otto-desk/otto_desk_gpio_safety.h.
# These pins are withheld on the target module regardless of carrier routing.
BLOCKED_GPIO_RANGES = ((26, 37),)
BLOCKED_GPIOS = {0, 3, 19, 20, 43, 44, 45, 46}


def is_blocked_gpio(gpio: int) -> bool:
    return gpio in BLOCKED_GPIOS or any(
        first <= gpio <= last for first, last in BLOCKED_GPIO_RANGES
    )


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def _reject_duplicate_keys(pairs):
    """Reject duplicate object keys instead of silently accepting the last value."""
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON object key: {key}")
        result[key] = value
    return result


def _has_safe_json_nesting(raw: bytes, max_depth: int = 32) -> bool:
    """Bound parser recursion while ignoring brackets inside JSON strings."""
    depth = 0
    in_string = False
    escaped = False
    for byte in raw:
        if in_string:
            if escaped: escaped = False
            elif byte == 92: escaped = True
            elif byte == 34: in_string = False
            continue
        if byte == 34: in_string = True
        elif byte in (123, 91):
            depth += 1
            if depth > max_depth: return False
        elif byte in (125, 93):
            depth -= 1
            if depth < 0: return False
    return depth == 0 and not in_string

def main(config_path: pathlib.Path | None = None) -> None:
    """Validate a supplied configuration file (defaults to the checked-in safe config)."""
    if config_path is None:
        config_path = CONFIG
    try:
        raw = config_path.read_bytes()
        if not raw or len(raw) > 3072:
            fail("configuration file must be between 1 and 3072 bytes")
        if not _has_safe_json_nesting(raw):
            fail("configuration JSON nesting must not exceed 32 levels")
        data = json.loads(raw.decode("utf-8"), object_pairs_hook=_reject_duplicate_keys)
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, ValueError) as exc:
        fail(f"cannot read valid JSON from {config_path}: {exc}")
    if not isinstance(data, dict):
        fail("configuration root must be a JSON object")

    if data.get("schema_version") != 1:
        fail("schema_version must be 1")

    def require_object(parent, key, path):
        value = parent.get(key)
        if not isinstance(value, dict):
            fail(f"{path} must be a JSON object")
        return value

    board = require_object(data, "board", "board")
    if board.get("manufacturer") != "Waveshare":
        fail("board.manufacturer must be Waveshare")
    if board.get("header_pin_count") != 40:
        fail("board.header_pin_count must be 40 for the user-confirmed target board")
    if board.get("header_labels_confirmed") is not True:
        fail("board.header_labels_confirmed must remain true because the supplied 40-pin header photo was confirmed")
    routing_verified = board.get("peripheral_routing_verified")
    if not isinstance(routing_verified, bool):
        fail("board.peripheral_routing_verified must be a boolean confirmation")
    if board.get("camera_enabled") is not False:
        fail("camera_enabled must remain false for this project")
    if board.get("onboard_codec_enabled") is not False:
        fail("onboard codec must remain disabled while external I2S audio is selected")
    if board.get("onboard_io_expander_enabled") is not False:
        fail("onboard I/O expander must remain disabled until its role is implemented")

    display = require_object(data, "display", "display")
    display_gpio = require_object(display, "gpio", "display.gpio")
    audio = require_object(data, "audio", "audio")
    microphone = require_object(audio, "microphone", "audio.microphone")
    mic_i2s = require_object(microphone, "i2s", "audio.microphone.i2s")
    amplifier = require_object(audio, "amplifier", "audio.amplifier")
    amp_i2s = require_object(amplifier, "i2s", "audio.amplifier.i2s")
    servos = require_object(data, "servos", "servos")
    pan = require_object(servos, "pan", "servos.pan")
    tilt = require_object(servos, "tilt", "servos.tilt")
    pir = require_object(data, "pir", "pir")
    policy = require_object(data, "gpio_policy", "gpio_policy")

    pin_values = []

    def collect(value, path="root"):
        if isinstance(value, dict):
            for key, child in value.items():
                child_path = f"{path}.{key}"
                if key in PIN_KEYS and not isinstance(child, (dict, list)):
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
        # ESP32-S3 exposes GPIO0-21 and GPIO26-48; GPIO22-25 do not exist.
        if not (0 <= gpio <= 21 or 26 <= gpio <= 48):
            fail(f"{path} uses out-of-range GPIO {gpio} or unavailable ESP32-S3 GPIO")
        if is_blocked_gpio(gpio):
            fail(f"{path} uses GPIO {gpio}, blocked by the N16R8 module safety policy")
        if gpio in seen:
            fail(f"GPIO {gpio} is assigned more than once: {seen[gpio]} and {path}")
        seen[gpio] = path

    # All enable switches must be JSON booleans; strings and numbers are rejected.
    for label, enabled in (
        ("display.enabled", display.get("enabled")),
        ("audio.microphone.enabled", microphone.get("enabled")),
        ("audio.amplifier.enabled", amplifier.get("enabled")),
        ("servos.pan.enabled", pan.get("enabled")),
        ("servos.tilt.enabled", tilt.get("enabled")),
        ("pir.enabled", pir.get("enabled")),
    ):
        if not isinstance(enabled, bool):
            fail(f"{label} must be a boolean")

    # Peripheral activation is opt-in: default JSON stays disabled. Enabling
    # any output requires the explicit board-routing confirmation.
    enabled_peripherals = (
        display.get("enabled") is True,
        microphone.get("enabled") is True,
        amplifier.get("enabled") is True,
        pan.get("enabled") is True,
        tilt.get("enabled") is True,
        pir.get("enabled") is True,
    )
    if any(enabled_peripherals) and routing_verified is not True:
        fail("board.peripheral_routing_verified must be true before enabling peripherals")
    if (microphone.get("enabled") is True) != (amplifier.get("enabled") is True):
        fail("INMP441 and MAX98357A must be enabled together for duplex I2S audio")

    if servos.get("external_power_verified") not in (True, False):
        fail("servo external_power_verified must be a boolean")
    if any((pan.get("enabled") is True, tilt.get("enabled") is True)) and (
        servos.get("external_power_verified") is not True
    ):
        fail("external servo power must be verified before enabling either servo")

    required_safety_flags = (
        "allow_unassigned",
        "reject_duplicate_assignments",
        "reject_bootstrap_conflicts",
        "reject_flash_psram_conflicts",
        "reject_usb_uart_conflicts",
        "require_board_pinout_validation",
    )
    for flag in required_safety_flags:
        if policy.get(flag) is not True:
            fail(f"gpio_policy.{flag} must remain true in the safe configuration")

    # Validate cross-field safety rules that JSON Schema alone does not
    # express clearly for this versioned hardware contract.
    any_servo_enabled = False
    for name, servo in (("pan", pan), ("tilt", tilt)):
        minimum = servo.get("min_deg")
        center = servo.get("center_deg")
        maximum = servo.get("max_deg")
        angles = (minimum, center, maximum)
        if any(isinstance(value, bool) or not isinstance(value, int) for value in angles):
            fail(f"servos.{name} angle limits must be integers")
        if not (0 <= minimum < center < maximum <= 180):
            fail(f"servos.{name} must satisfy 0 <= min_deg < center_deg < max_deg <= 180")
        if servo.get("enabled") is True:
            any_servo_enabled = True
            if servo.get("gpio") is None:
                fail(f"servos.{name}.gpio is required when that servo is enabled")



    required_pin_groups = [
        ("display.enabled", display.get("enabled"), display_gpio),
        ("audio.microphone.enabled", microphone.get("enabled"), mic_i2s),
        ("audio.amplifier.enabled", amplifier.get("enabled"), amp_i2s),
    ]
    for label, enabled, pins in required_pin_groups:
        if enabled is True and any(value is None for value in pins.values()):
            fail(f"{label} requires every GPIO in its pin group to be assigned")

    if pir.get("enabled") is True and pir.get("gpio") is None:
        fail("pir.gpio is required when PIR is enabled")

    print("Hardware JSON validation passed (Waveshare target, opt-in peripheral activation).")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=pathlib.Path, default=CONFIG,
                        help="JSON file to validate (default: config/default.json)")
    args = parser.parse_args()
    main(args.config)
