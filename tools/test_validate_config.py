#!/usr/bin/env python3
"""Regression tests for cross-field hardware safety validation."""

import contextlib
import copy
import importlib.util
import io
import json
import pathlib
import re
import shutil
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

    def test_configuration_page_javascript_syntax(self):
        node = shutil.which("node")
        if node is None:
            self.skipTest("Node.js is not installed")
        source = (ROOT / "main" / "otto_desk_config_server.cc").read_text(encoding="utf-8")
        match = re.search(r"<script>\s*(.*?)\s*</script>", source, re.DOTALL)
        self.assertIsNotNone(match, "configuration page script block was not found")
        script = match.group(1)
        self.assertNotIn(r"\nasync function", script,
                         "literal backslash-n must not split JavaScript statements")
        result = subprocess.run(
            [node, "--check"], input=script, capture_output=True,
            text=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_config_api_embedded_first_run_default_matches_checked_in_schema(self):
        source = (ROOT / "main" / "otto_desk_config_server.cc").read_text(encoding="utf-8")
        match = re.search(
            r'constexpr char kDefaultConfig\[\] = R"json\((.*?)\)json";',
            source,
            re.DOTALL,
        )
        self.assertIsNotNone(match, "embedded first-run default JSON was not found")
        embedded = json.loads(match.group(1))
        self.assertEqual(embedded, DEFAULT)
        code, output = self.run_config(embedded)
        self.assertEqual(code, 0, output)

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

    def test_gpio_safety_policy_cannot_be_weakened(self):
        for flag in (
            "allow_unassigned",
            "reject_duplicate_assignments",
            "reject_bootstrap_conflicts",
            "reject_flash_psram_conflicts",
            "reject_usb_uart_conflicts",
            "require_board_pinout_validation",
        ):
            with self.subTest(flag=flag):
                config = copy.deepcopy(DEFAULT)
                config["gpio_policy"][flag] = False
                code, output = self.run_config(config)
                self.assertNotEqual(code, 0)
                self.assertIn(f"gpio_policy.{flag} must remain true", output)


    def test_malformed_nested_objects_are_rejected_without_tracebacks(self):
        mutations = (
            lambda c: c.update(board=None),
            lambda c: c.update(display=[]),
            lambda c: c["display"].update(gpio=None),
            lambda c: c["audio"].update(microphone="bad"),
            lambda c: c["servos"].update(pan=None),
            lambda c: c.update(pir=False),
            lambda c: c.update(gpio_policy=[]),
        )
        for mutate in mutations:
            with self.subTest(mutation=mutate):
                config = copy.deepcopy(DEFAULT)
                mutate(config)
                code, output = self.run_config(config)
                self.assertNotEqual(code, 0)
                self.assertIn("must be a JSON object", output)
                self.assertNotIn("Traceback", output)

    def test_servo_angles_must_be_integer_and_within_range(self):
        for values in ((True, 90, 160), (-1, 90, 160), (20, 90, 181), (20, 90.5, 160)):
            with self.subTest(values=values):
                config = copy.deepcopy(DEFAULT)
                config["servos"]["pan"].update(
                    min_deg=values[0], center_deg=values[1], max_deg=values[2]
                )
                code, output = self.run_config(config)
                self.assertNotEqual(code, 0)
                self.assertTrue("integers" in output or "0 <= min_deg" in output, output)

if __name__ == "__main__":
    unittest.main(verbosity=2)
