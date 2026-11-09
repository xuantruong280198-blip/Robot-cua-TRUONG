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

    def test_user_confirmed_40_pin_header_is_recorded_but_pinout_stays_unverified(self):
        self.assertEqual(DEFAULT["board"]["header_pin_count"], 40)
        self.assertIs(DEFAULT["board"]["header_pinout_verified"], False)
        code, output = self.run_config(copy.deepcopy(DEFAULT))
        self.assertEqual(code, 0, output)

    def test_header_pinout_cannot_be_marked_verified_without_review(self):
        config = copy.deepcopy(DEFAULT)
        config["board"]["header_pinout_verified"] = True
        code, output = self.run_config(config)
        self.assertNotEqual(code, 0)
        self.assertIn("header_pinout_verified must remain false", output)

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
        self.assertIn("min_deg < center_deg < max_deg", output)

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

    def test_unavailable_esp32s3_gpio_numbers_are_rejected(self):
        for gpio in (22, 23, 24, 25):
            with self.subTest(gpio=gpio):
                config = copy.deepcopy(DEFAULT)
                config["display"]["gpio"]["sclk"] = gpio
                code, output = self.run_config(config)
                self.assertNotEqual(code, 0)
                self.assertIn(f"unavailable ESP32-S3 GPIO", output)


    def test_n16r8_module_blocked_gpio_is_rejected(self):
        blocked = (0, 3, 19, 20, *range(26, 38), 43, 44, 45, 46)
        for gpio in blocked:
            with self.subTest(gpio=gpio):
                config = copy.deepcopy(DEFAULT)
                config["display"]["gpio"]["sclk"] = gpio
                code, output = self.run_config(config)
                self.assertNotEqual(code, 0)
                self.assertIn(
                    f"GPIO {gpio}, blocked by the N16R8 module safety policy",
                    output,
                )

    def test_unblocked_gpio_still_requires_verified_header_mapping(self):
        config = copy.deepcopy(DEFAULT)
        config["display"]["gpio"]["sclk"] = 4
        code, output = self.run_config(config)
        self.assertNotEqual(code, 0)
        self.assertIn(
            "must be null until the exact header mapping is verified",
            output,
        )

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

    def test_servo_center_must_be_strictly_inside_limits(self):
        for values in ((90, 90, 160), (20, 90, 90), (90, 90, 90)):
            with self.subTest(values=values):
                config = copy.deepcopy(DEFAULT)
                config["servos"]["pan"].update(
                    min_deg=values[0], center_deg=values[1], max_deg=values[2]
                )
                code, output = self.run_config(config)
                self.assertNotEqual(code, 0)
                self.assertIn("min_deg < center_deg < max_deg", output)

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


class OttoDeskDisplayContractTests(unittest.TestCase):
    """Guard the Otto face layout and safe preview fallback."""

    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "main" / "boards" / "otto-desk" / "otto_desk_display.cc").read_text(
            encoding="utf-8"
        )

    def test_otto_face_uses_full_panel_and_preserves_image_aspect_ratio(self):
        self.assertIn('SetEmotion("neutral")', self.source)
        self.assertNotIn('SetEmotion("staticstate")', self.source)
        self.assertIn("lv_obj_set_size(emoji_box_, width_, height_)", self.source)
        self.assertIn("const uint32_t scale = std::max(scale_x, scale_y);", self.source)
        self.assertIn("lv_image_set_scale(emoji_image_", self.source)
        self.assertIn("lv_obj_center(emoji_image_)", self.source)
        self.assertIn("void OttoDeskDisplay::SetEmotion(const char* emotion)", self.source)
        self.assertIn('std::strcmp(resolved_emotion, "robot_2") == 0', self.source)
        self.assertIn('std::strcmp(resolved_emotion, "staticstate") == 0', self.source)
        self.assertIn('resolved_emotion = "neutral"', self.source)
        self.assertIn("LcdDisplay::SetEmotion(resolved_emotion);", self.source)

    def test_otto_face_hides_legacy_chrome_without_assuming_optional_objects_exist(self):
        for object_name in (
            "top_bar_", "status_bar_", "content_", "side_bar_", "bottom_bar_",
            "status_label_", "notification_label_", "network_label_", "mute_label_",
            "battery_label_", "chat_message_label_",
        ):
            with self.subTest(object_name=object_name):
                self.assertIn(object_name, self.source)
        self.assertIn("if (object != nullptr)", self.source)
        self.assertIn("if (emoji_box_ != nullptr)", self.source)

    def test_preview_clear_restores_available_face_layer_safely(self):
        self.assertIn("if (image == nullptr)", self.source)
        self.assertIn("if (emoji_box_ != nullptr)", self.source)
        self.assertIn("else if (gif_controller_ != nullptr)", self.source)
        self.assertIn("else if (emoji_label_ != nullptr)", self.source)

    def test_xiaozhi_device_states_keep_emotion_names_asset_compatible(self):
        application = (ROOT / "main" / "application.cc").read_text(encoding="utf-8")
        listening = application.split("case kDeviceStateListening:", 1)[1].split(
            "case kDeviceStateSpeaking:", 1
        )[0]
        speaking = application.split("case kDeviceStateSpeaking:", 1)[1].split(
            "case kDeviceStateNotifying:", 1
        )[0]

        # The Otto 1.3+ collection has neutral and thinking, but no dedicated
        # listening/speaking GIF. Listening therefore uses a real neutral asset;
        # speaking keeps the emotion delivered by the LLM instead of inventing
        # an unsupported status-to-emotion alias.
        self.assertIn('display->SetEmotion("neutral")', listening)
        self.assertIn('display->SetStatus(Lang::Strings::SPEAKING)', speaking)
        self.assertNotIn("SetEmotion(", speaking)


class RuntimeConfigDecodeContractTests(unittest.TestCase):
    """Guard the C++ decoder against drifting away from the JSON schema."""

    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "main" / "otto_desk_config_store.cc").read_text(encoding="utf-8")
        cls.header = (ROOT / "main" / "otto_desk_config_store.h").read_text(encoding="utf-8")
        start = cls.source.index("esp_err_t OttoDeskConfigStore::Decode(")
        end = cls.source.index("\nesp_err_t OttoDeskConfigStore::Load(", start)
        cls.decode = cls.source[start:end]

    def test_decode_validates_before_parsing_and_fails_closed(self):
        self.assertLess(self.decode.index("ValidateSafeJson(json)"),
                        self.decode.index("cJSON_ParseWithLength"))
        self.assertIn("config = OttoDeskRuntimeConfig{};", self.decode)
        self.assertIn("config.hardware_activation_permitted = false;", self.decode)
        self.assertIn("if (err != ESP_OK) return err;", self.decode)

    def test_decoder_maps_every_default_schema_gpio_key(self):
        mappings = (
            ("display_gpio", "sclk", "display_sclk"),
            ("display_gpio", "mosi", "display_mosi"),
            ("display_gpio", "cs", "display_cs"),
            ("display_gpio", "dc", "display_dc"),
            ("display_gpio", "reset", "display_reset"),
            ("display_gpio", "backlight", "display_backlight"),
            ("mic_i2s", "ws", "microphone_ws"),
            ("mic_i2s", "sck", "microphone_sck"),
            ("mic_i2s", "sd", "microphone_sd"),
            ("amp_i2s", "din", "amplifier_din"),
            ("amp_i2s", "bclk", "amplifier_bclk"),
            ("amp_i2s", "lrclk", "amplifier_lrclk"),
        )
        for source, key, target in mappings:
            with self.subTest(source=source, key=key):
                self.assertIn(f'ReadOptionalInt({source}, "{key}")', self.decode)
                self.assertIn(f"config.{target}", self.decode)

    def test_decoder_maps_device_enable_flags_and_servo_limits(self):
        for field in (
            "display_enabled", "microphone_enabled", "amplifier_enabled",
            "servo_external_power_verified", "pir_enabled",
        ):
            with self.subTest(field=field):
                self.assertIn(f"config.{field}", self.decode)
        for field in ("min_deg", "center_deg", "max_deg", "speed_deg_per_sec"):
            with self.subTest(field=field):
                self.assertIn(f'"{field}"', self.source)
        self.assertIn('ReadOptionalInt(pir, "gpio")', self.decode)
        self.assertIn("config.pan = ReadServo", self.decode)
        self.assertIn("config.tilt = ReadServo", self.decode)

    def test_decoder_carries_all_gpio_safety_policy_flags(self):
        for field in (
            "allow_unassigned", "reject_duplicate_assignments",
            "reject_bootstrap_conflicts", "reject_flash_psram_conflicts",
            "reject_usb_uart_conflicts", "require_board_pinout_validation",
        ):
            with self.subTest(field=field):
                self.assertIn(f"config.gpio_policy.{field}", self.decode)
                self.assertRegex(
                    self.header,
                    rf"bool {field} = true;",
                )

    def test_config_portal_is_scoped_to_wifi_provisioning_mode(self):
        board_source = (ROOT / "main" / "boards" / "otto-desk" / "otto_desk.cc").read_text(encoding="utf-8")
        self.assertIn("event == NetworkEvent::WifiConfigModeEnter", board_source)
        self.assertIn("OttoDeskConfigServer::Start()", board_source)
        self.assertIn("event == NetworkEvent::Connected", board_source)
        self.assertIn("event == NetworkEvent::WifiConfigModeExit", board_source)
        self.assertIn("OttoDeskConfigServer::Stop()", board_source)

    def test_gpio_inventory_lists_each_signal_individually(self):
        source = (ROOT / "main" / "otto_desk_config_server.cc").read_text(encoding="utf-8")
        for signal in (
            "SCLK", "MOSI", "CS", "DC", "RESET", "BACKLIGHT",
            "WS", "SCK", "SD", "DIN", "BCLK", "LRCLK",
            "PAN signal", "TILT signal", "HC-SR501",
        ):
            with self.subTest(signal=signal):
                self.assertIn(signal, source)
        self.assertIn("Chưa gán · khóa an toàn", source)
        self.assertIn("không phải sơ đồ chân đã xác minh", source)

    def test_config_post_requires_json_content_type(self):
        source = (ROOT / "main" / "otto_desk_config_server.cc").read_text(encoding="utf-8")
        self.assertIn('httpd_req_get_hdr_value_str(request, "Content-Type"', source)
        self.assertIn('!IsJsonContentType(content_type)', source)
        self.assertIn('bool IsJsonContentType(const char* value)', source)
        self.assertIn('strncasecmp(value, kJsonMediaType, kJsonMediaTypeLength)', source)
        self.assertIn("while (*value == ' '", source)
        self.assertIn("return *value ==", source)
        self.assertIn('415 Unsupported Media Type', source)
        self.assertIn('if (request->content_len == 0)', source)
        self.assertIn('400 Bad Request', source)
        self.assertIn('config_body_must_not_be_empty', source)
        self.assertIn('if (request->content_len > OttoDeskConfigStore::kMaxConfigBytes)', source)
        self.assertIn('413 Payload Too Large', source)
        self.assertIn('config_size_must_not_exceed_3072_bytes', source)
        self.assertIn('content_type_must_be_application_json', source)
        self.assertIn('return httpd_resp_sendstr(request, "{\\\"error\\\":\\\"content_type_must_be_application_json\\\"}");', source)

    def test_config_api_never_reports_hardware_activation(self):
        source = (ROOT / "main" / "otto_desk_config_server.cc").read_text(encoding="utf-8")
        self.assertIn("\\\"hardware_activation\\\":false", source)
        self.assertIn("kMaxConfigBytes", source)
        self.assertIn("OttoDeskConfigStore::Save(json)", source)
        self.assertIn("Cache-Control", source)
        self.assertIn("X-Otto-Config-Source", source)

    def test_main_entrypoint_keeps_gpio_and_peripheral_activation_locked(self):
        main_source = (ROOT / "main" / "main.cc").read_text(encoding="utf-8")
        board_source = (ROOT / "main" / "boards" / "otto-desk" / "otto_desk.cc").read_text(encoding="utf-8")
        forbidden_calls = (
            "gpio_config(", "gpio_set_level(", "i2s_new_channel(",
            "esp_lcd_new_panel(", "ledc_timer_config(", "ledc_channel_config(",
        )
        for call in forbidden_calls:
            with self.subTest(call=call):
                self.assertNotIn(call, main_source)
                self.assertNotIn(call, board_source)
        self.assertNotIn("#include <driver/gpio.h>", main_source)
        self.assertIn("hardware activation remains locked", main_source)
        self.assertIn("no GPIO or peripheral drivers are started", main_source)

    def test_runtime_config_defaults_cannot_activate_hardware(self):
        self.assertIn("bool hardware_activation_permitted = false;", self.header)
        for field in (
            "display_enabled", "microphone_enabled", "amplifier_enabled", "pir_enabled",
        ):
            with self.subTest(field=field):
                self.assertRegex(self.header, rf"bool {field} = false;")


class OtaSafetyRegressionTests(unittest.TestCase):
    """Guard the firmware updater against incomplete or oversized images."""

    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "main" / "ota.cc").read_text(encoding="utf-8")
        start = cls.source.index("bool Ota::Upgrade(")
        end = cls.source.index("bool Ota::StartUpgrade(", start)
        cls.upgrade = cls.source[start:end]

    def test_download_is_bounded_by_inactive_partition_size(self):
        self.assertIn("content_length > update_partition->size", self.upgrade)

    def test_http_connection_is_closed_when_status_check_fails(self):
        status_check = self.upgrade.index("auto status_code = http->GetStatusCode();")
        content_length = self.upgrade.index("size_t content_length = http->GetBodyLength();")
        status_block = self.upgrade[status_check:content_length]
        self.assertGreaterEqual(status_block.count("http->Close();"), 2)

    def test_invalid_app_descriptor_is_rejected_before_flash_session(self):
        invalid_header = self.upgrade.index(
            "new_app_info.magic_word != ESP_APP_DESC_MAGIC_WORD"
        )
        ota_begin = self.upgrade.index("esp_ota_begin(")
        self.assertLess(invalid_header, ota_begin)
        self.assertIn("http->Close();", self.upgrade[invalid_header:ota_begin])

    def test_image_descriptor_is_checked_before_ota_begin(self):
        self.assertIn("new_app_info.magic_word != ESP_APP_DESC_MAGIC_WORD", self.upgrade)
        self.assertLess(
            self.upgrade.index("new_app_info.magic_word != ESP_APP_DESC_MAGIC_WORD"),
            self.upgrade.index("esp_ota_begin("),
        )

    def test_truncated_or_headerless_images_are_never_selected(self):
        self.assertIn("if (!image_header_checked)", self.upgrade)
        self.assertIn("total_read != content_length", self.upgrade)
        self.assertLess(
            self.upgrade.index("total_read != content_length"),
            self.upgrade.index("esp_ota_end(update_handle)"),
        )
        self.assertLess(
            self.upgrade.index("esp_ota_end(update_handle)"),
            self.upgrade.index("esp_ota_set_boot_partition(update_partition)"),
        )

    def test_failed_reads_abort_an_started_ota_session(self):
        self.assertGreaterEqual(
            self.upgrade.count("if (image_header_checked) esp_ota_abort(update_handle);"),
            2,
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
