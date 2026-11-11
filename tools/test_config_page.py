#!/usr/bin/env python3
"""Regression checks for the embedded Otto Desk GPIO draft table."""
from pathlib import Path
import unittest

SOURCE = Path("main/otto_desk_config_server.cc").read_text(encoding="utf-8")

GPIO_PATHS = (
    "display.gpio.sclk", "display.gpio.mosi", "display.gpio.cs",
    "display.gpio.dc", "display.gpio.reset", "display.gpio.backlight",
    "audio.microphone.i2s.ws", "audio.microphone.i2s.sck",
    "audio.microphone.i2s.sd", "audio.amplifier.i2s.din",
    "audio.amplifier.i2s.bclk", "audio.amplifier.i2s.lrclk",
    "servos.pan.gpio", "servos.tilt.gpio", "pir.gpio",
)


class GpioConfigPageTests(unittest.TestCase):
    def test_every_signal_has_a_json_binding(self):
        for path in GPIO_PATHS:
            with self.subTest(path=path):
                self.assertIn(f'data-gpio-path="{path}"', SOURCE)

    def test_table_reads_nested_json_values(self):
        self.assertIn("function configValue(config,path)", SOURCE)
        self.assertIn("function renderGpioTable(config)", SOURCE)
        self.assertIn("'GPIO '+value:'Chưa gán'", SOURCE)

    def test_load_save_and_import_refresh_the_table(self):
        self.assertGreaterEqual(SOURCE.count("renderGpioTable(config)"), 3)
        self.assertIn("const config=JSON.parse(t)", SOURCE)
        self.assertIn("const config=JSON.parse(box.value)", SOURCE)
        self.assertIn("const config=JSON.parse(text)", SOURCE)

    def test_gpio_routes_share_existing_xiaozhi_server(self):
        self.assertIn('root.uri = "/gpio"', SOURCE)
        self.assertIn('httpd_register_uri_handler(server, &get_config)', SOURCE)
        self.assertIn('OttoDeskConfigServer::RegisterRoutes(httpd_handle_t server)', SOURCE)
        self.assertNotIn('server_port = 8080', SOURCE)
        self.assertNotIn('httpd_start(', SOURCE)
    def test_assignments_explain_explicit_activation_interlock(self):
        self.assertIn("peripheral_routing_verified", SOURCE)
        self.assertIn("Thiết bị chỉ chạy khi đã xác nhận pinout", SOURCE)

    def test_system_status_and_reboot_reuse_existing_config_routes(self):
        self.assertIn('SetSystemInfoHeaders(request)', SOURCE)
        self.assertIn('X-Otto-Firmware', SOURCE)
        self.assertIn('X-Otto-Chip', SOURCE)
        self.assertIn('X-Otto-Free-Heap', SOURCE)
        self.assertIn('X-Otto-Action', SOURCE)
        self.assertIn('strcasecmp(action, "reboot")', SOURCE)
        self.assertIn('esp_chip_info(&chip_info)', SOURCE)
        self.assertIn('esp_restart()', SOURCE)
        self.assertIn("fetch('/api/config',{cache:'no-store'})", SOURCE)
        self.assertIn("fetch('/api/config',{method:'POST',headers:{'X-Otto-Action':'reboot'}})", SOURCE)
        self.assertIn('System · Thông tin thiết bị', SOURCE)
        # The Wi-Fi component already registers 21 handlers; Otto adds only 3.
        self.assertEqual(SOURCE.count('httpd_register_uri_handler(server,'), 3)
        self.assertNotIn('system_info.uri', SOURCE)


if __name__ == "__main__":
    unittest.main(verbosity=2)
