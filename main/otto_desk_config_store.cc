#include "otto_desk_config_store.h"

#include <cJSON.h>
#include <nvs.h>
#include <nvs_flash.h>

#include <cctype>

namespace {
constexpr char kNamespace[] = "ottdesk";
constexpr char kConfigKey[] = "config";

const cJSON* Get(const cJSON* object, const char* key) {
    return cJSON_IsObject(object) ? cJSON_GetObjectItemCaseSensitive(object, key) : nullptr;
}

bool IsFalse(const cJSON* object, const char* key) {
    const cJSON* value = Get(object, key);
    return cJSON_IsBool(value) && cJSON_IsFalse(value);
}

bool AllObjectValuesNull(const cJSON* object) {
    if (!cJSON_IsObject(object)) return false;
    for (const cJSON* item = object->child; item != nullptr; item = item->next) {
        if (!cJSON_IsNull(item)) return false;
    }
    return true;
}

bool IsSafeConfig(const cJSON* root) {
    if (!cJSON_IsObject(root)) return false;

    const cJSON* version = Get(root, "schema_version");
    if (!cJSON_IsNumber(version) || version->valuedouble != 1) return false;

    const cJSON* board = Get(root, "board");
    if (!IsFalse(board, "camera_enabled") ||
        !IsFalse(board, "onboard_codec_enabled") ||
        !IsFalse(board, "onboard_io_expander_enabled")) return false;

    const cJSON* display = Get(root, "display");
    if (!IsFalse(display, "enabled") ||
        !AllObjectValuesNull(Get(display, "gpio"))) return false;

    const cJSON* audio = Get(root, "audio");
    const cJSON* microphone = Get(audio, "microphone");
    const cJSON* amplifier = Get(audio, "amplifier");
    if (!IsFalse(microphone, "enabled") ||
        !AllObjectValuesNull(Get(microphone, "i2s")) ||
        !IsFalse(amplifier, "enabled") ||
        !AllObjectValuesNull(Get(amplifier, "i2s"))) return false;

    const cJSON* servos = Get(root, "servos");
    const cJSON* pan = Get(servos, "pan");
    const cJSON* tilt = Get(servos, "tilt");
    if (!IsFalse(pan, "enabled") || !cJSON_IsNull(Get(pan, "gpio")) ||
        !IsFalse(tilt, "enabled") || !cJSON_IsNull(Get(tilt, "gpio")) ||
        !IsFalse(servos, "external_power_verified")) return false;

    const cJSON* pir = Get(root, "pir");
    if (!IsFalse(pir, "enabled") || !cJSON_IsNull(Get(pir, "gpio"))) return false;

    return true;
}
}  // namespace

esp_err_t OttoDeskConfigStore::ValidateSafeJson(const std::string& json) {
    if (json.empty() || json.size() > kMaxConfigBytes) return ESP_ERR_INVALID_SIZE;

    const char* parse_end = nullptr;
    cJSON* root = cJSON_ParseWithLengthOpts(
        json.c_str(), json.size(), &parse_end, false);
    if (root == nullptr || parse_end == nullptr) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    const char* const input_end = json.c_str() + json.size();
    while (parse_end < input_end &&
           std::isspace(static_cast<unsigned char>(*parse_end))) {
        ++parse_end;
    }
    if (parse_end != input_end) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    const bool safe = IsSafeConfig(root);
    cJSON_Delete(root);
    return safe ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t OttoDeskConfigStore::Load(std::string& json) {
    json.clear();
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    size_t length = 0;
    err = nvs_get_str(handle, kConfigKey, nullptr, &length);
    if (err == ESP_OK) {
        if (length == 0 || length > kMaxConfigBytes + 1) {
            err = ESP_ERR_INVALID_SIZE;
        } else {
            json.resize(length);
            err = nvs_get_str(handle, kConfigKey, json.data(), &length);
            if (err == ESP_OK && !json.empty() && json.back() == '\0') json.pop_back();
        }
    }
    nvs_close(handle);

    if (err == ESP_OK) {
        err = ValidateSafeJson(json);
        if (err != ESP_OK) json.clear();
    }
    return err;
}

esp_err_t OttoDeskConfigStore::Save(const std::string& json) {
    esp_err_t err = ValidateSafeJson(json);
    if (err != ESP_OK) return err;

    nvs_handle_t handle = 0;
    err = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_str(handle, kConfigKey, json.c_str());
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t OttoDeskConfigStore::Clear() {
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_erase_key(handle, kConfigKey);
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}
