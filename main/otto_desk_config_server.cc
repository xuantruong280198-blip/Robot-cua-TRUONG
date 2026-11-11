#include "otto_desk_config_server.h"

#include "otto_desk_config_store.h"

#include <esp_err.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_app_desc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs.h>

#include <cstdio>

#include <string>
#include <strings.h>

namespace {
constexpr char TAG[] = "OttoDeskConfig";
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
textarea{box-sizing:border-box;width:100%;height:62vh;background:#0b1220;color:#d1fae5;border:1px solid #475569;border-radius:8px;padding:12px;font:13px monospace}table input.gpio-number{box-sizing:border-box;width:88px;max-width:100%;padding:7px;background:#0b1220;color:#fff;border:1px solid #64748b;border-radius:6px;font:inherit}
button{padding:10px 16px;margin:8px 8px 8px 0;border:0;border-radius:7px;font-weight:600}
small{color:#cbd5e1}#status{white-space:pre-wrap}
</style>
<p><a href="/" style="color:#93c5fd">← Quay lại cấu hình Wi-Fi XiaoZhi</a></p><h2>Otto Desk AI - Custom GPIO / JSON</h2>
<p>API này chỉ hoạt động trong chế độ Wi-Fi cấu hình. Nhập GPIO theo đúng dây đã đấu. Mặc định mọi thiết bị đều tắt. Chỉ đặt board.peripheral_routing_verified=true sau khi đã đối chiếu pinout thực tế; servo còn yêu cầu servos.external_power_verified=true. Thiết bị chỉ chạy khi đã xác nhận pinout và bật đúng mục trong JSON. Bảng dưới đây là danh sách tín hiệu, không phải sơ đồ chân đã xác minh.</p>
<section style="overflow-x:auto;margin:16px 0"><table style="width:100%;border-collapse:collapse;background:#1f2937"><thead><tr><th style="text-align:left;padding:9px">Thiết bị</th><th style="text-align:left;padding:9px">Tín hiệu</th><th style="text-align:left;padding:9px">GPIO</th><th style="text-align:left;padding:9px">Trạng thái</th></tr></thead><tbody>
<tr><td>ST7789</td><td>SCLK</td><td><input class="gpio-number" type="number" min="0" max="48" step="1" inputmode="numeric" placeholder="GPIO" data-gpio-path="display.gpio.sclk" aria-label="Nhập số GPIO cho display.gpio.sclk"></td><td data-gpio-state data-enabled-path="display.enabled">Đang tắt</td></tr>
<tr><td>ST7789</td><td>MOSI</td><td><input class="gpio-number" type="number" min="0" max="48" step="1" inputmode="numeric" placeholder="GPIO" data-gpio-path="display.gpio.mosi" aria-label="Nhập số GPIO cho display.gpio.mosi"></td><td data-gpio-state data-enabled-path="display.enabled">Đang tắt · chưa xác nhận pinout</td></tr>
<tr><td>ST7789</td><td>CS</td><td><input class="gpio-number" type="number" min="0" max="48" step="1" inputmode="numeric" placeholder="GPIO" data-gpio-path="display.gpio.cs" aria-label="Nhập số GPIO cho display.gpio.cs"></td><td data-gpio-state data-enabled-path="display.enabled">Đang tắt · chưa xác nhận pinout</td></tr>
<tr><td>ST7789</td><td>DC</td><td><input class="gpio-number" type="number" min="0" max="48" step="1" inputmode="numeric" placeholder="GPIO" data-gpio-path="display.gpio.dc" aria-label="Nhập số GPIO cho display.gpio.dc"></td><td data-gpio-state data-enabled-path="display.enabled">Đang tắt · chưa xác nhận pinout</td></tr>
<tr><td>ST7789</td><td>RESET</td><td><input class="gpio-number" type="number" min="0" max="48" step="1" inputmode="numeric" placeholder="GPIO" data-gpio-path="display.gpio.reset" aria-label="Nhập số GPIO cho display.gpio.reset"></td><td data-gpio-state data-enabled-path="display.enabled">Đang tắt · chưa xác nhận pinout</td></tr>
<tr><td>ST7789</td><td>BACKLIGHT</td><td><input class="gpio-number" type="number" min="0" max="48" step="1" inputmode="numeric" placeholder="GPIO" data-gpio-path="display.gpio.backlight" aria-label="Nhập số GPIO cho display.gpio.backlight"></td><td data-gpio-state data-enabled-path="display.enabled">Đang tắt · chưa xác nhận pinout</td></tr>
<tr><td>INMP441</td><td>WS</td><td><input class="gpio-number" type="number" min="0" max="48" step="1" inputmode="numeric" placeholder="GPIO" data-gpio-path="audio.microphone.i2s.ws" aria-label="Nhập số GPIO cho audio.microphone.i2s.ws"></td><td data-gpio-state data-enabled-path="audio.microphone.enabled">Đang tắt · chưa xác nhận pinout</td></tr>
<tr><td>INMP441</td><td>SCK</td><td><input class="gpio-number" type="number" min="0" max="48" step="1" inputmode="numeric" placeholder="GPIO" data-gpio-path="audio.microphone.i2s.sck" aria-label="Nhập số GPIO cho audio.microphone.i2s.sck"></td><td data-gpio-state data-enabled-path="audio.microphone.enabled">Đang tắt · chưa xác nhận pinout</td></tr>
<tr><td>INMP441</td><td>SD</td><td><input class="gpio-number" type="number" min="0" max="48" step="1" inputmode="numeric" placeholder="GPIO" data-gpio-path="audio.microphone.i2s.sd" aria-label="Nhập số GPIO cho audio.microphone.i2s.sd"></td><td data-gpio-state data-enabled-path="audio.microphone.enabled">Đang tắt · chưa xác nhận pinout</td></tr>
<tr><td>MAX98357A</td><td>DIN</td><td><input class="gpio-number" type="number" min="0" max="48" step="1" inputmode="numeric" placeholder="GPIO" data-gpio-path="audio.amplifier.i2s.din" aria-label="Nhập số GPIO cho audio.amplifier.i2s.din"></td><td data-gpio-state data-enabled-path="audio.amplifier.enabled">Đang tắt · chưa xác nhận pinout</td></tr>
<tr><td>MAX98357A</td><td>BCLK</td><td><input class="gpio-number" type="number" min="0" max="48" step="1" inputmode="numeric" placeholder="GPIO" data-gpio-path="audio.amplifier.i2s.bclk" aria-label="Nhập số GPIO cho audio.amplifier.i2s.bclk"></td><td data-gpio-state data-enabled-path="audio.amplifier.enabled">Đang tắt · chưa xác nhận pinout</td></tr>
<tr><td>MAX98357A</td><td>LRCLK</td><td><input class="gpio-number" type="number" min="0" max="48" step="1" inputmode="numeric" placeholder="GPIO" data-gpio-path="audio.amplifier.i2s.lrclk" aria-label="Nhập số GPIO cho audio.amplifier.i2s.lrclk"></td><td data-gpio-state data-enabled-path="audio.amplifier.enabled">Đang tắt · chưa xác nhận pinout</td></tr>
<tr><td>Servo đầu ngang</td><td>PAN signal</td><td><input class="gpio-number" type="number" min="0" max="48" step="1" inputmode="numeric" placeholder="GPIO" data-gpio-path="servos.pan.gpio" aria-label="Nhập số GPIO cho servos.pan.gpio"></td><td data-gpio-state data-enabled-path="servos.pan.enabled" data-servo-power="true">Đang tắt · chưa xác nhận pinout</td></tr>
<tr><td>Servo đầu dọc</td><td>TILT signal</td><td><input class="gpio-number" type="number" min="0" max="48" step="1" inputmode="numeric" placeholder="GPIO" data-gpio-path="servos.tilt.gpio" aria-label="Nhập số GPIO cho servos.tilt.gpio"></td><td data-gpio-state data-enabled-path="servos.tilt.enabled" data-servo-power="true">Đang tắt · chưa xác nhận pinout</td></tr>
<tr><td>PIR HC-SR501</td><td>OUT</td><td><input class="gpio-number" type="number" min="0" max="48" step="1" inputmode="numeric" placeholder="GPIO" data-gpio-path="pir.gpio" aria-label="Nhập số GPIO cho pir.gpio"></td><td data-gpio-state data-enabled-path="pir.enabled">Đang tắt · chưa xác nhận pinout</td></tr>
</tbody></table></section>
<p><strong>Trạng thái thiết bị</strong> được cập nhật theo JSON đang xem. Cấu hình đã lưu chỉ được áp dụng sau khi khởi động lại; servo chỉ được khởi tạo khi đã xác nhận nguồn ngoài riêng.</p>
<section style="margin:22px 0;padding:16px;border:1px solid #475569;border-radius:10px;background:#1f2937">
<h2>System · Thông tin thiết bị</h2><p><button onclick="loadSystemInfo()">Làm mới thông tin</button><button onclick="rebootDevice()" style="background:#991b1b;color:white">Khởi động lại ESP32</button></p>
<pre id="systemInfo" style="white-space:pre-wrap;overflow-wrap:anywhere;color:#d1fae5">Nhấn “Làm mới thông tin” để đọc trạng thái thiết bị.</pre>
</section>
<textarea id="json" spellcheck="false" placeholder="Dán nội dung config/default.json tại đây..."></textarea><br>
<button onclick="loadConfig()">Tải cấu hình đã lưu</button><button onclick="saveConfig()">Kiểm tra và lưu</button><button onclick="exportConfig()">Xuất JSON</button><label style="display:inline-block;padding:10px 16px;margin:8px;background:#334155;border-radius:7px;cursor:pointer">Nhập JSON<input id="file" type="file" accept=".json,application/json" hidden onchange="importConfig(event)"></label>
<p id="status">Chưa tải cấu hình.</p>
<script>
const box=document.getElementById('json'),status=document.getElementById('status');
let activeConfig=null;
function configValue(config,path){return path.split('.').reduce((value,key)=>value&&typeof value==='object'?value[key]:undefined,config);}
function setConfigValue(config,path,value){const keys=path.split('.');const last=keys.pop();const parent=keys.reduce((node,key)=>node[key],config);parent[last]=value;}
function renderGpioTable(config){activeConfig=config;document.querySelectorAll('input[data-gpio-path]').forEach(input=>{const value=configValue(config,input.dataset.gpioPath);input.value=Number.isInteger(value)?String(value):'';});const verified=configValue(config,'board.peripheral_routing_verified')===true;document.querySelectorAll('[data-gpio-state]').forEach(cell=>{const enabled=configValue(config,cell.dataset.enabledPath)===true;const needsServoPower=cell.dataset.servoPower==='true';const servoPower=configValue(config,'servos.external_power_verified')===true;if(!verified)cell.textContent='Khóa: chưa xác nhận pinout';else if(!enabled)cell.textContent='Pinout đã xác nhận · thiết bị đang tắt';else if(needsServoPower&&!servoPower)cell.textContent='Khóa: chưa xác nhận nguồn servo';else cell.textContent='Được bật trong JSON · chờ khởi động lại';});}
document.querySelector('table').addEventListener('input',event=>{const input=event.target;if(!input.matches('input[data-gpio-path]')||!activeConfig)return;const raw=input.value;const value=raw===''?null:Number(raw);if(value!==null&&(!Number.isInteger(value)||value<0||value>48)){input.setCustomValidity('Nhập GPIO nguyên từ 0 đến 48.');input.reportValidity();return;}input.setCustomValidity('');setConfigValue(activeConfig,input.dataset.gpioPath,value);box.value=JSON.stringify(activeConfig,null,2);status.textContent='Đã cập nhật '+input.dataset.gpioPath+' trong bản nháp. Nhấn Kiểm tra và lưu để lưu vào ESP32.';});
async function loadConfig(){status.textContent='Đang tải...';try{const r=await fetch('/api/config');const t=await r.text();if(!r.ok)throw Error(t);const config=JSON.parse(t);box.value=JSON.stringify(config,null,2);renderGpioTable(config);status.textContent=r.headers.get('X-Otto-Config-Source')==='default'?'Đã tải cấu hình mặc định an toàn (chưa có cấu hình lưu).':'Đã tải cấu hình đã lưu.';}catch(e){status.textContent='Chưa có cấu hình lưu hoặc lỗi: '+e.message;}}
async function saveConfig(){status.textContent='Đang kiểm tra và lưu...';try{const config=JSON.parse(box.value);const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:box.value});const t=await r.text();if(!r.ok)throw Error(t);renderGpioTable(config);status.textContent='Đã lưu JSON. Khởi động lại thiết bị để áp dụng cấu hình phần cứng.';}catch(e){status.textContent='Không lưu được: '+e.message;}}
async function importConfig(event){const file=event.target.files&&event.target.files[0];if(!file)return;if(file.size>3072){status.textContent='File vượt quá giới hạn 3072 byte.';event.target.value='';return;}try{const text=await file.text();const config=JSON.parse(text);box.value=text;renderGpioTable(config);status.textContent='Đã nhập '+file.name+'. Giá trị hiển thị chỉ là bản nháp; nhấn Kiểm tra và lưu để firmware xác thực.';}catch(e){status.textContent='File JSON không hợp lệ: '+e.message;}event.target.value='';}
function exportConfig(){try{JSON.parse(box.value);const blob=new Blob([box.value],{type:'application/json'});const url=URL.createObjectURL(blob);const a=document.createElement('a');a.href=url;a.download='otto-desk-config.json';a.click();URL.revokeObjectURL(url);status.textContent='Đã xuất otto-desk-config.json.';}catch(e){status.textContent='Không thể xuất: JSON chưa hợp lệ.';}}
async function loadSystemInfo(){const out=document.getElementById('systemInfo');out.textContent='Đang đọc...';try{const r=await fetch('/api/config',{cache:'no-store'});if(!r.ok)throw Error(await r.text());const h=r.headers;out.textContent='Firmware: '+(h.get('X-Otto-Firmware')||'không rõ')+'\nChip: '+(h.get('X-Otto-Chip')||'không rõ')+'\nCPU cores: '+(h.get('X-Otto-Cores')||'không rõ')+'\nChip revision: '+(h.get('X-Otto-Revision')||'không rõ')+'\nFlash: '+(h.get('X-Otto-Flash-MB')||'0')+' MB\nHeap còn trống: '+(h.get('X-Otto-Free-Heap')||'0')+' bytes';}catch(e){out.textContent='Không đọc được thông tin: '+e.message;}}
async function rebootDevice(){if(!confirm('Khởi động lại ESP32 ngay bây giờ?'))return;const out=document.getElementById('systemInfo');out.textContent='Đang gửi lệnh khởi động lại...';try{const r=await fetch('/api/config',{method:'POST',headers:{'X-Otto-Action':'reboot'}});if(!r.ok)throw Error(await r.text());out.textContent='Đã nhận lệnh. ESP32 sẽ khởi động lại; hãy kết nối lại Wi-Fi cấu hình sau vài giây.';}catch(e){out.textContent='Gửi lệnh thất bại: '+e.message;}}

</script></html>)html";
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    return httpd_resp_send(request, kPage, HTTPD_RESP_USE_STRLEN);
}

void SetSystemInfoHeaders(httpd_req_t* request) {
    esp_chip_info_t chip_info = {};
    esp_chip_info(&chip_info);
    uint32_t flash_size = 0;
    const esp_err_t flash_err = esp_flash_get_size(nullptr, &flash_size);
    const esp_app_desc_t* app = esp_app_get_description();
    char cores[8] = {};
    char revision[8] = {};
    char flash_mb[8] = {};
    char free_heap[16] = {};

    httpd_resp_set_hdr(request, "X-Otto-Firmware",
                       app != nullptr ? app->version : "unknown");
    httpd_resp_set_hdr(request, "X-Otto-Chip", CONFIG_IDF_TARGET);
    snprintf(cores, sizeof(cores), "%d", chip_info.cores);
    httpd_resp_set_hdr(request, "X-Otto-Cores", cores);
    snprintf(revision, sizeof(revision), "%d", chip_info.revision);
    httpd_resp_set_hdr(request, "X-Otto-Revision", revision);
    snprintf(flash_mb, sizeof(flash_mb), "%u",
             flash_err == ESP_OK ? static_cast<unsigned>(flash_size / (1024U * 1024U)) : 0U);
    httpd_resp_set_hdr(request, "X-Otto-Flash-MB", flash_mb);
    snprintf(free_heap, sizeof(free_heap), "%u", static_cast<unsigned>(esp_get_free_heap_size()));
    httpd_resp_set_hdr(request, "X-Otto-Free-Heap", free_heap);
}

esp_err_t GetConfigHandler(httpd_req_t* request) {
    std::string json;
    const esp_err_t err = OttoDeskConfigStore::Load(json);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // First-use experience: return the same fail-closed schema as config/default.json.
        // Every peripheral remains disabled and every GPIO remains null.
        httpd_resp_set_type(request, "application/json");
        httpd_resp_set_hdr(request, "Cache-Control", "no-store");
        SetSystemInfoHeaders(request);
        httpd_resp_set_hdr(request, "X-Otto-Config-Source", "default");
        return httpd_resp_sendstr(request, kDefaultConfig);
    }
    if (err != ESP_OK) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return httpd_resp_sendstr(request, "{\"error\":\"stored_config_unavailable\"}");
    }
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    SetSystemInfoHeaders(request);
    httpd_resp_set_hdr(request, "X-Otto-Config-Source", "saved");
    return httpd_resp_send(request, json.c_str(), json.size());
}

esp_err_t SaveConfigHandler(httpd_req_t* request) {
    char action[24] = {};
    if (httpd_req_get_hdr_value_str(request, "X-Otto-Action", action, sizeof(action)) == ESP_OK &&
        strcasecmp(action, "reboot") == 0) {
        httpd_resp_set_type(request, "application/json");
        const esp_err_t send_err =
            httpd_resp_sendstr(request, "{\"ok\":true,\"restarting\":true}");
        if (send_err == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();
        }
        return send_err;
    }

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

    OttoDeskRuntimeConfig decoded;
    const esp_err_t decode_err = OttoDeskConfigStore::Decode(json, decoded);
    if (decode_err != ESP_OK) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        httpd_resp_set_type(request, "application/json");
        return httpd_resp_sendstr(request, "{\"error\":\"saved_config_decode_failed\"}");
    }
    const std::string response = std::string("{\"ok\":true,\"hardware_activation\":") +
        (decoded.hardware_activation_permitted ? "true" : "false") +
        ",\"apply_after_reboot\":true}";
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, response.c_str(), response.size());
}
}  // namespace

esp_err_t OttoDeskConfigServer::RegisterRoutes(httpd_handle_t server) {

    httpd_uri_t root = {};
    root.uri = "/gpio";
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

    esp_err_t err = httpd_register_uri_handler(server, &root);
    if (err == ESP_OK) err = httpd_register_uri_handler(server, &get_config);
    if (err == ESP_OK) err = httpd_register_uri_handler(server, &save_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO route registration failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Custom GPIO page registered at http://192.168.4.1/gpio");
    return ESP_OK;
}

extern "C" esp_err_t otto_desk_register_config_routes(httpd_handle_t server) {
    return OttoDeskConfigServer::RegisterRoutes(server);
}
