#include "otto_desk_config_server.h"

#include "otto_desk_config_store.h"

#include <esp_err.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <nvs.h>

#include <string>
#include <strings.h>

namespace {
constexpr char TAG[] = "OttoDeskConfig";
constexpr uint16_t kPort = 8080;
constexpr char kDefaultConfig[] = R"json({
  "schema_version": 1,
  "project": "Robot-cua-TRUONG",
  "firmware_base": {
    "repository": "https://github.com/78/xiaozhi-esp32",
    "ref": "v2.5.0"
  },
  "board": {
    "manufacturer": "Waveshare",
    "product_family": "ESP32-S3-CAM-N16R8",
    "pcb_revision": null,
    "profile": "otto-esp32-s3-cam-40pin",
    "upstream_reference_profile": "waveshare/esp32-s3-cam",
    "chip": "esp32s3",
    "flash_mb": 16,
    "psram_mb": 8,
    "camera_enabled": false,
    "onboard_codec_enabled": false,
    "onboard_io_expander_enabled": false,
    "header_pin_count": 40,
    "header_labels_confirmed": true,
    "peripheral_routing_verified": false
  },
  "display": {
    "type": "st7789",
    "width": 240,
    "height": 240,
    "rotation": null,
    "gpio": {
      "sclk": null,
      "mosi": null,
      "cs": null,
      "dc": null,
      "reset": null,
      "backlight": null
    },
    "enabled": false
  },
  "audio": {
    "microphone": {
      "type": "inmp441",
      "i2s": {
        "ws": null,
        "sck": null,
        "sd": null
      },
      "enabled": false
    },
    "amplifier": {
      "type": "max98357a",
      "i2s": {
        "din": null,
        "bclk": null,
        "lrclk": null
      },
      "enabled": false,
      "speaker_impedance_ohm": null
    }
  },
  "servos": {
    "pan": {
      "gpio": null,
      "min_deg": 20,
      "center_deg": 90,
      "max_deg": 160,
      "speed_deg_per_sec": 60,
      "enabled": false
    },
    "tilt": {
      "gpio": null,
      "min_deg": 35,
      "center_deg": 90,
      "max_deg": 145,
      "speed_deg_per_sec": 45,
      "enabled": false
    },
    "external_power_verified": false
  },
  "pir": {
    "type": "hc-sr501",
    "gpio": null,
    "enabled": false
  },
  "gpio_policy": {
    "allow_unassigned": true,
    "reject_duplicate_assignments": true,
    "reject_bootstrap_conflicts": true,
    "reject_flash_psram_conflicts": true,
    "reject_usb_uart_conflicts": true,
    "require_board_pinout_validation": true
  }
})json";
httpd_handle_t server = nullptr;

bool IsJsonContentType(const char* value) {
    if (value == nullptr) return false;

    while (*value == ' ' || *value == '\t') ++value;
    constexpr char kJsonMediaType[] = "application/json";
    constexpr size_t kJsonMediaTypeLength = sizeof(kJsonMediaType) - 1;
    if (strncasecmp(value, kJsonMediaType, kJsonMediaTypeLength) != 0) return false;

    value += kJsonMediaTypeLength;
    while (*value == ' ' || *value == '\t') ++value;
    return *value == '\0' || *value == ';';
}

esp_err_t RootHandler(httpd_req_t* request) {
    static constexpr char kPage[] = R"html(<!doctype html>
<html lang="vi"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Otto Desk - Cấu hình</title>
<style>
body{font:16px system-ui;max-width:850px;margin:24px auto;padding:0 14px;background:#111827;color:#f3f4f6}
textarea{box-sizing:border-box;width:100%;height:62vh;background:#0b1220;color:#d1fae5;border:1px solid #475569;border-radius:8px;padding:12px;font:13px monospace}
button{padding:10px 16px;margin:8px 8px 8px 0;border:0;border-radius:7px;font-weight:600}
small{color:#cbd5e1}#status{white-space:pre-wrap}
</style>
<h2>Otto Desk AI - Cấu hình an toàn</h2>
<p>API này chỉ hoạt động trong chế độ Wi-Fi cấu hình. Có thể lưu GPIO dưới dạng bản nháp sau khi kiểm tra giới hạn ESP32-S3 và trùng chân. Bản nháp KHÔNG kích hoạt phần cứng: mọi thiết bị phải để tắt, sơ đồ chân header 40 chân vẫn chưa được xác minh. Bảng dưới đây là danh sách tín hiệu, không phải sơ đồ chân đã xác minh; giá trị đang lưu nằm trong JSON.</p>
<section style="overflow-x:auto;margin:16px 0"><table style="width:100%;border-collapse:collapse;background:#1f2937"><thead><tr><th style="text-align:left;padding:9px">Thiết bị</th><th style="text-align:left;padding:9px">Tín hiệu</th><th style="text-align:left;padding:9px">GPIO</th><th style="text-align:left;padding:9px">Trạng thái</th></tr></thead><tbody>
<tr><td>ST7789</td><td>SCLK</td><td data-gpio-path="display.gpio.sclk">Chưa gán</td><td data-gpio-state>Đang tắt</td></tr>
<tr><td>ST7789</td><td>MOSI</td><td data-gpio-path="display.gpio.mosi">Chưa gán</td><td data-gpio-state>Chưa kích hoạt · bản nháp</td></tr>
<tr><td>ST7789</td><td>CS</td><td data-gpio-path="display.gpio.cs">Chưa gán</td><td data-gpio-state>Chưa kích hoạt · bản nháp</td></tr>
<tr><td>ST7789</td><td>DC</td><td data-gpio-path="display.gpio.dc">Chưa gán</td><td data-gpio-state>Chưa kích hoạt · bản nháp</td></tr>
<tr><td>ST7789</td><td>RESET</td><td data-gpio-path="display.gpio.reset">Chưa gán</td><td data-gpio-state>Chưa kích hoạt · bản nháp</td></tr>
<tr><td>ST7789</td><td>BACKLIGHT</td><td data-gpio-path="display.gpio.backlight">Chưa gán</td><td data-gpio-state>Chưa kích hoạt · bản nháp</td></tr>
<tr><td>INMP441</td><td>WS</td><td data-gpio-path="audio.microphone.i2s.ws">Chưa gán</td><td data-gpio-state>Chưa kích hoạt · bản nháp</td></tr>
<tr><td>INMP441</td><td>SCK</td><td data-gpio-path="audio.microphone.i2s.sck">Chưa gán</td><td data-gpio-state>Chưa kích hoạt · bản nháp</td></tr>
<tr><td>INMP441</td><td>SD</td><td data-gpio-path="audio.microphone.i2s.sd">Chưa gán</td><td data-gpio-state>Chưa kích hoạt · bản nháp</td></tr>
<tr><td>MAX98357A</td><td>DIN</td><td data-gpio-path="audio.amplifier.i2s.din">Chưa gán</td><td data-gpio-state>Chưa kích hoạt · bản nháp</td></tr>
<tr><td>MAX98357A</td><td>BCLK</td><td data-gpio-path="audio.amplifier.i2s.bclk">Chưa gán</td><td data-gpio-state>Chưa kích hoạt · bản nháp</td></tr>
<tr><td>MAX98357A</td><td>LRCLK</td><td data-gpio-path="audio.amplifier.i2s.lrclk">Chưa gán</td><td data-gpio-state>Chưa kích hoạt · bản nháp</td></tr>
<tr><td>Servo đầu ngang</td><td>PAN signal</td><td data-gpio-path="servos.pan.gpio">Chưa gán</td><td data-gpio-state>Chưa kích hoạt · bản nháp</td></tr>
<tr><td>Servo đầu dọc</td><td>TILT signal</td><td data-gpio-path="servos.tilt.gpio">Chưa gán</td><td data-gpio-state>Chưa kích hoạt · bản nháp</td></tr>
<tr><td>PIR HC-SR501</td><td>OUT</td><td data-gpio-path="pir.gpio">Chưa gán</td><td data-gpio-state>Chưa kích hoạt · bản nháp</td></tr>
</tbody></table></section>
<p><strong>Phần cứng đang khóa an toàn:</strong> màn hình, mic, ampli, hai servo và PIR đều chưa được firmware kích hoạt. Chỉ mở khóa sau khi xác minh pinout thực tế; servo còn cần xác nhận nguồn ngoài riêng.</p>
<textarea id="json" spellcheck="false" placeholder="Dán nội dung config/default.json tại đây..."></textarea><br>
<button onclick="loadConfig()">Tải cấu hình đã lưu</button><button onclick="saveConfig()">Kiểm tra và lưu</button><button onclick="exportConfig()">Xuất JSON</button><label style="display:inline-block;padding:10px 16px;margin:8px;background:#334155;border-radius:7px;cursor:pointer">Nhập JSON<input id="file" type="file" accept=".json,application/json" hidden onchange="importConfig(event)"></label>
<p id="status">Chưa tải cấu hình.</p>
<script>
const box=document.getElementById('json'),status=document.getElementById('status');
function configValue(config,path){return path.split('.').reduce((value,key)=>value&&typeof value==='object'?value[key]:undefined,config);}
function renderGpioTable(config){document.querySelectorAll('[data-gpio-path]').forEach(cell=>{const value=configValue(config,cell.dataset.gpioPath);cell.textContent=Number.isInteger(value)?'GPIO '+value:'Chưa gán';});document.querySelectorAll('[data-gpio-state]').forEach(cell=>{cell.textContent='Chưa kích hoạt · bản nháp';});}
async function loadConfig(){status.textContent='Đang tải...';try{const r=await fetch('/api/config');const t=await r.text();if(!r.ok)throw Error(t);const config=JSON.parse(t);box.value=JSON.stringify(config,null,2);renderGpioTable(config);status.textContent=r.headers.get('X-Otto-Config-Source')==='default'?'Đã tải cấu hình mặc định an toàn (chưa có cấu hình lưu).':'Đã tải cấu hình đã lưu.';}catch(e){status.textContent='Chưa có cấu hình lưu hoặc lỗi: '+e.message;}}
async function saveConfig(){status.textContent='Đang kiểm tra và lưu...';try{const config=JSON.parse(box.value);const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:box.value});const t=await r.text();if(!r.ok)throw Error(t);renderGpioTable(config);status.textContent='Đã lưu JSON an toàn. Đã lưu. Thiết bị chỉ chạy khi đã xác nhận pinout và bật đúng mục trong JSON.';}catch(e){status.textContent='Không lưu được: '+e.message;}}
async function importConfig(event){const file=event.target.files&&event.target.files[0];if(!file)return;if(file.size>3072){status.textContent='File vượt quá giới hạn 3072 byte.';event.target.value='';return;}try{const text=await file.text();const config=JSON.parse(text);box.value=text;renderGpioTable(config);status.textContent='Đã nhập '+file.name+'. Giá trị hiển thị chỉ là bản nháp; nhấn Kiểm tra và lưu để firmware xác thực.';}catch(e){status.textContent='File JSON không hợp lệ: '+e.message;}event.target.value='';}
function exportConfig(){try{JSON.parse(box.value);const blob=new Blob([box.value],{type:'application/json'});const url=URL.createObjectURL(blob);const a=document.createElement('a');a.href=url;a.download='otto-desk-config.json';a.click();URL.revokeObjectURL(url);status.textContent='Đã xuất otto-desk-config.json.';}catch(e){status.textContent='Không thể xuất: JSON chưa hợp lệ.';}}
</script></html>)html";
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    return httpd_resp_send(request, kPage, HTTPD_RESP_USE_STRLEN);
}

esp_err_t GetConfigHandler(httpd_req_t* request) {
    std::string json;
    const esp_err_t err = OttoDeskConfigStore::Load(json);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // First-use experience: return the same fail-closed schema as config/default.json.
        // Every peripheral remains disabled and every GPIO remains null.
        httpd_resp_set_type(request, "application/json");
        httpd_resp_set_hdr(request, "Cache-Control", "no-store");
        httpd_resp_set_hdr(request, "X-Otto-Config-Source", "default");
        return httpd_resp_sendstr(request, kDefaultConfig);
    }
    if (err != ESP_OK) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return httpd_resp_sendstr(request, "{\"error\":\"stored_config_unavailable\"}");
    }
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_hdr(request, "X-Otto-Config-Source", "saved");
    return httpd_resp_send(request, json.c_str(), json.size());
}

esp_err_t SaveConfigHandler(httpd_req_t* request) {
    char content_type[64] = {};
    const esp_err_t header_err =
        httpd_req_get_hdr_value_str(request, "Content-Type", content_type, sizeof(content_type));
    if (header_err != ESP_OK || !IsJsonContentType(content_type)) {
        httpd_resp_set_status(request, "415 Unsupported Media Type");
        httpd_resp_set_type(request, "application/json");
        return httpd_resp_sendstr(request, "{\"error\":\"content_type_must_be_application_json\"}");
    }

    if (request->content_len == 0) {
        httpd_resp_set_status(request, "400 Bad Request");
        httpd_resp_set_type(request, "application/json");
        return httpd_resp_sendstr(request, "{\"error\":\"config_body_must_not_be_empty\"}");
    }
    if (request->content_len > OttoDeskConfigStore::kMaxConfigBytes) {
        httpd_resp_set_status(request, "413 Payload Too Large");
        httpd_resp_set_type(request, "application/json");
        return httpd_resp_sendstr(request, "{\"error\":\"config_size_must_not_exceed_3072_bytes\"}");
    }

    std::string json;
    json.resize(request->content_len);
    size_t received = 0;
    while (received < request->content_len) {
        const int count = httpd_req_recv(
            request, json.data() + received, request->content_len - received);
        if (count <= 0) {
            httpd_resp_set_status(request, "400 Bad Request");
            return httpd_resp_sendstr(request, "{\"error\":\"incomplete_request_body\"}");
        }
        received += static_cast<size_t>(count);
    }

    const esp_err_t err = OttoDeskConfigStore::Save(json);
    if (err != ESP_OK) {
        httpd_resp_set_status(request, "400 Bad Request");
        httpd_resp_set_type(request, "application/json");
        const std::string response =
            std::string("{\"error\":\"config_rejected\",\"reason\":\"") +
            esp_err_to_name(err) + "\"}";
        return httpd_resp_send(request, response.c_str(), response.size());
    }

    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, "{\"ok\":true,\"hardware_activation\":false}");
}
}  // namespace

esp_err_t OttoDeskConfigServer::Start() {
    if (server != nullptr) return ESP_OK;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = kPort;
    config.max_uri_handlers = 3;
    config.max_open_sockets = 3;
    config.stack_size = 8192;
    config.lru_purge_enable = true;

    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server start failed: %s", esp_err_to_name(err));
        server = nullptr;
        return err;
    }

    httpd_uri_t root = {};
    root.uri = "/";
    root.method = HTTP_GET;
    root.handler = RootHandler;

    httpd_uri_t get_config = {};
    get_config.uri = "/api/config";
    get_config.method = HTTP_GET;
    get_config.handler = GetConfigHandler;

    httpd_uri_t save_config = {};
    save_config.uri = "/api/config";
    save_config.method = HTTP_POST;
    save_config.handler = SaveConfigHandler;

    if ((err = httpd_register_uri_handler(server, &root)) != ESP_OK ||
        (err = httpd_register_uri_handler(server, &get_config)) != ESP_OK ||
        (err = httpd_register_uri_handler(server, &save_config)) != ESP_OK) {
        ESP_LOGE(TAG, "HTTP route registration failed: %s", esp_err_to_name(err));
        httpd_stop(server);
        server = nullptr;
        return err;
    }

    ESP_LOGI(TAG, "Safe configuration page available at http://192.168.4.1:%u/", kPort);
    return ESP_OK;
}

void OttoDeskConfigServer::Stop() {
    if (server != nullptr) {
        httpd_stop(server);
        server = nullptr;
        ESP_LOGI(TAG, "Configuration server stopped");
    }
}
