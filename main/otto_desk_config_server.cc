#include "otto_desk_config_server.h"

#include "otto_desk_config_store.h"

#include <esp_err.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <nvs.h>

#include <string>

namespace {
constexpr char TAG[] = "OttoDeskConfig";
constexpr uint16_t kPort = 8080;
httpd_handle_t server = nullptr;

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
<p>API này chỉ hoạt động trong chế độ Wi-Fi cấu hình. Firmware hiện chỉ chấp nhận JSON có toàn bộ thiết bị và GPIO ở trạng thái tắt/chưa gán.</p>
<textarea id="json" spellcheck="false" placeholder="Dán nội dung config/default.json tại đây..."></textarea><br>
<button onclick="loadConfig()">Tải cấu hình đã lưu</button><button onclick="saveConfig()">Kiểm tra và lưu</button><button onclick="exportConfig()">Xuất JSON</button><label style="display:inline-block;padding:10px 16px;margin:8px;background:#334155;border-radius:7px;cursor:pointer">Nhập JSON<input id="file" type="file" accept=".json,application/json" hidden onchange="importConfig(event)"></label>
<p id="status">Chưa tải cấu hình.</p>
<script>
const box=document.getElementById('json'),status=document.getElementById('status');
async function loadConfig(){status.textContent='Đang tải...';try{const r=await fetch('/api/config');const t=await r.text();if(!r.ok)throw Error(t);box.value=JSON.stringify(JSON.parse(t),null,2);status.textContent='Đã tải cấu hình từ NVS.';}catch(e){status.textContent='Chưa có cấu hình lưu hoặc lỗi: '+e.message;}}
async function saveConfig(){status.textContent='Đang kiểm tra và lưu...';try{JSON.parse(box.value);const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:box.value});const t=await r.text();if(!r.ok)throw Error(t);status.textContent='Đã lưu JSON an toàn. Thiết bị ngoại vi vẫn đang tắt.';}catch(e){status.textContent='Không lưu được: '+e.message;}}\nasync function importConfig(event){const file=event.target.files&&event.target.files[0];if(!file)return;if(file.size>3072){status.textContent='File vượt quá giới hạn 3072 byte.';event.target.value='';return;}try{const text=await file.text();JSON.parse(text);box.value=text;status.textContent='Đã nhập '+file.name+'. Nhấn Kiểm tra và lưu để firmware xác thực.';}catch(e){status.textContent='File JSON không hợp lệ: '+e.message;}event.target.value='';}\nfunction exportConfig(){try{JSON.parse(box.value);const blob=new Blob([box.value],{type:'application/json'});const url=URL.createObjectURL(blob);const a=document.createElement('a');a.href=url;a.download='otto-desk-config.json';a.click();URL.revokeObjectURL(url);status.textContent='Đã xuất otto-desk-config.json.';}catch(e){status.textContent='Không thể xuất: JSON chưa hợp lệ.';}}
</script></html>)html";
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    return httpd_resp_send(request, kPage, HTTPD_RESP_USE_STRLEN);
}

esp_err_t GetConfigHandler(httpd_req_t* request) {
    std::string json;
    const esp_err_t err = OttoDeskConfigStore::Load(json);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        httpd_resp_set_status(request, "404 Not Found");
        httpd_resp_set_type(request, "application/json");
        return httpd_resp_sendstr(request, "{\"error\":\"no_saved_config\",\"hint\":\"paste config/default.json and save\"}");
    }
    if (err != ESP_OK) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return httpd_resp_sendstr(request, "{\"error\":\"stored_config_unavailable\"}");
    }
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, json.c_str(), json.size());
}

esp_err_t SaveConfigHandler(httpd_req_t* request) {
    if (request->content_len == 0 || request->content_len > OttoDeskConfigStore::kMaxConfigBytes) {
        httpd_resp_set_status(request, "413 Payload Too Large");
        return httpd_resp_sendstr(request, "{\"error\":\"config_size_must_be_1_to_3072_bytes\"}");
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
