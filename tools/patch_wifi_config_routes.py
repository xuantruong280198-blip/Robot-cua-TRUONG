#!/usr/bin/env python3
"""Patch pinned esp-wifi-connect to expose Otto routes on XiaoZhi's port-80 server."""
from pathlib import Path

sources = list(Path("managed_components").glob("*/wifi_configuration_ap.cc"))
if len(sources) != 1:
    raise SystemExit(f"Expected one managed wifi_configuration_ap.cc, found {len(sources)}")
source = sources[0]
text = source.read_text(encoding="utf-8")
include_anchor = '#include "ssid_manager.h"'
declaration = '''#include "ssid_manager.h"

// Required board extension: register board-specific routes on this HTTP server.
extern "C" esp_err_t otto_desk_register_config_routes(httpd_handle_t);'''
if "otto_desk_register_config_routes" not in text:
    if include_anchor not in text:
        raise SystemExit("esp-wifi-connect include anchor changed; refusing unsafe patch")
    text = text.replace(include_anchor, declaration, 1)
server_anchor = '    ESP_LOGI(TAG, "Web server started");'
registration = '''    // Otto Desk Custom GPIO and JSON share XiaoZhi's existing port-80 server.
    const esp_err_t board_routes_err = otto_desk_register_config_routes(server_);
    if (board_routes_err != ESP_OK) {
        ESP_LOGE(TAG, "Board configuration routes failed: %s", esp_err_to_name(board_routes_err));
    }

''' + server_anchor
if "Board configuration routes failed" not in text:
    if server_anchor not in text:
        raise SystemExit("Web server completion anchor changed; refusing unsafe patch")
    text = text.replace(server_anchor, registration, 1)
source.write_text(text, encoding="utf-8")

assets = list(Path("managed_components").glob("*/assets/wifi_configuration.html"))
if len(assets) != 1:
    raise SystemExit(f"Expected one Wi-Fi HTML asset, found {len(assets)}")
page = assets[0]
html = page.read_text(encoding="utf-8")
if 'href="/gpio"' not in html:
    marker = '    <div class="card">\n    <!-- Tabs -->'
    if marker not in html:
        raise SystemExit("Wi-Fi page tab anchor changed; refusing unsafe patch")
    link = '''
    <div style="max-width:460px;margin:0 auto 12px;text-align:right">
        <a href="/gpio" style="display:inline-block;padding:9px 14px;border-radius:9px;background:#2563eb;color:white;text-decoration:none;font-weight:600">Otto Desk · Custom GPIO / JSON →</a>
    </div>
'''
    html = html.replace(marker, link + marker, 1)
page.write_text(html, encoding="utf-8")
print(f"Patched {source} and {page}")
