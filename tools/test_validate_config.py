#!/usr/bin/env python3
"""Regression tests for cross-field hardware safety validation."""

import contextlib
import copy
import importlib.util
import io
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "validate_config", ROOT / "tools" / "validate_config.py"
)
VALIDATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VALIDATOR)
DEFAULT = json.loads((ROOT / "config" / "default.json").read_text(encoding="utf-8"))


class HardwareConfigValidationTests(unittest.TestCase):
    def run_config(self, config):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "config.json"
            path.write_text(json.dumps(config), encoding="utf-8")
            output = io.StringIO()
            with patch.object(VALIDATOR, "CONFIG", path):
                with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
                    try:
                        VALIDATOR.main()
                    except SystemExit as exc:
                        return exc.code, output.getvalue()
            return 0, output.getvalue()

    def test_safe_default_passes(self):
        code, output = self.run_config(copy.deepcopy(DEFAULT))
        self.assertEqual(code, 0, output)

    def test_cli_validates_an_imported_json_file(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "imported.json"
            path.write_text(json.dumps(copy.deepcopy(DEFAULT)), encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(ROOT / "tools" / "validate_config.py"),
                 "--config", str(path)],
                capture_output=True, text=True, check=False,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("Hardware JSON validation passed", result.stdout)

    def test_duplicate_json_object_keys_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "duplicate.json"
            path.write_text('{"schema_version":1,"schema_version":1}', encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(ROOT / "tools" / "validate_config.py"),
                 "--config", str(path)],
                capture_output=True, text=True, check=False,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("duplicate JSON object key", result.stderr)

    def test_oversized_config_is_rejected_before_parsing(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "oversized.json"
            path.write_bytes(b" " * 3073)
            result = subprocess.run(
                [sys.executable, str(ROOT / "tools" / "validate_config.py"),
                 "--config", str(path)],
                capture_output=True, text=True, check=False,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("between 1 and 3072 bytes", result.stderr)

    def test_non_object_json_root_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "array.json"
            path.write_text("[]", encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(ROOT / "tools" / "validate_config.py"),
                 "--config", str(path)],
                capture_output=True, text=True, check=False,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("root must be a JSON object", result.stderr)

    def test_cli_rejects_invalid_imported_json_file(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "imported.json"
            config = copy.deepcopy(DEFAULT)
            config["display"]["gpio"]["sclk"] = 49
            path.write_text(json.dumps(config), encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(ROOT / "tools" / "validate_config.py"),
                 "--config", str(path)],
                capture_output=True, text=True, check=False,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("out-of-range GPIO 49", result.stderr)

    def test_servo_angle_order_is_enforced(self):
        config = copy.deepcopy(DEFAULT)
        config["servos"]["pan"].update(min_deg=100, center_deg=90, max_deg=160)
        code, output = self.run_config(config)
        self.assertNotEqual(code, 0)
        self.assertIn("min_deg <= center_deg <= max_deg", output)

    def test_safe_default_rejects_enabling_unverified_servo(self):
        config = copy.deepcopy(DEFAULT)
        config["servos"]["pan"]["enabled"] = True
        code, output = self.run_config(config)
        self.assertNotEqual(code, 0)
        self.assertIn("must remain disabled until wiring is verified", output)

    def test_enabled_display_requires_all_pins(self):
        config = copy.deepcopy(DEFAULT)
        config["display"]["enabled"] = True
        code, output = self.run_config(config)
        self.assertNotEqual(code, 0)
        self.assertIn("requires every GPIO", output)

    def test_duplicate_gpio_is_rejected(self):
        config = copy.deepcopy(DEFAULT)
        config["display"]["gpio"]["sclk"] = 5
        config["display"]["gpio"]["mosi"] = 5
        code, output = self.run_config(config)
        self.assertNotEqual(code, 0)
        self.assertIn("assigned more than once", output)

    def test_gpio_out_of_range_is_rejected(self):
        config = copy.deepcopy(DEFAULT)
        config["display"]["gpio"]["sclk"] = 49
        code, output = self.run_config(config)
        self.assertNotEqual(code, 0)
        self.assertIn("out-of-range GPIO 49", output)

    def test_boolean_is_not_accepted_as_gpio_number(self):
        config = copy.deepcopy(DEFAULT)
        config["display"]["gpio"]["sclk"] = True
        code, output = self.run_config(config)
        self.assertNotEqual(code, 0)
        self.assertIn("must be an integer GPIO or null", output)

    def test_non_integer_gpio_values_are_rejected(self):
        for invalid in ("GPIO5", 5.5):
            with self.subTest(invalid=invalid):
                config = copy.deepcopy(DEFAULT)
                config["display"]["gpio"]["sclk"] = invalid
                code, output = self.run_config(config)
                self.assertNotEqual(code, 0)
                self.assertIn("must be an integer GPIO or null", output)

    def test_servo_requires_verified_external_power(self):
        config = copy.deepcopy(DEFAULT)
        config["servos"]["external_power_verified"] = True
        config["servos"]["pan"]["enabled"] = True
        config["servos"]["pan"]["gpio"] = 4
        code, output = self.run_config(config)
        self.assertNotEqual(code, 0)
        self.assertIn("must be null until the exact header mapping is verified", output)

    def test_camera_must_remain_disabled(self):
        config = copy.deepcopy(DEFAULT)
        config["board"]["camera_enabled"] = True
        code, output = self.run_config(config)
        self.assertNotEqual(code, 0)
        self.assertIn("camera_enabled must remain false", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
