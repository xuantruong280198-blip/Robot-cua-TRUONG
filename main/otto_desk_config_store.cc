#include "otto_desk_config_store.h"

#include <cJSON.h>
#include <nvs.h>
#include <nvs_flash.h>

#include <cctype>
#include <cstring>
#include <initializer_list>

namespace {
constexpr char kNamespace[] = "ottdesk";
constexpr char kConfigKey[] = "config";

const cJSON* Get(const cJSON* object, const char* key) {
    return cJSON_IsObject(object) ? cJSON_GetObjectItemCaseSensitive(object, key) : nullptr;
}

bool HasExactKeys(const cJSON* object, std::initializer_list<const char*> keys) {
    if (!cJSON_IsObject(object)) return false;

    size_t count = 0;
    for (const cJSON* item = object->child; item != nullptr; item = item->next) {
        if (item->string == nullptr) return false;
        bool known = false;
        for (const char* key : keys) {
            if (std::strcmp(item->string, key) == 0) {
                known = true;
                break;
            }
        }
        if (!known) return false;
        ++count;
    }
    return count == keys.size();
}

bool IsFalse(const cJSON* object, const char* key) {
    const cJSON* value = Get(object, key);
    return cJSON_IsBool(value) && cJSON_IsFalse(value);
}

bool IsString(const cJSON* object, const char* key, const char* expected) {
    const cJSON* value = Get(object, key);
    return cJSON_IsString(value) && value->valuestring != nullptr &&
           std::strcmp(value->valuestring, expected) == 0;
}

bool IsNumber(const cJSON* object, const char* key, double expected) {
    const cJSON* value = Get(object, key);
    return cJSON_IsNumber(value) && value->valuedouble == expected;
}

bool IsNullableString(const cJSON* object, const char* key) {
    const cJSON* value = Get(object, key);
    return cJSON_IsNull(value) ||
           (cJSON_IsString(value) && value->valuestring != nullptr);
}

bool IsNull(const cJSON* object, const char* key) {
    return cJSON_IsNull(Get(object, key));
}

bool HasNullKeys(const cJSON* object, std::initializer_list<const char*> keys) {
    if (!HasExactKeys(object, keys)) return false;
    for (const char* key : keys) {
        if (!IsNull(object, key)) return false;
    }
    return true;
}

bool IsBoundedNumber(const cJSON* object, const char* key,
                     double minimum, double maximum, bool allow_minimum = true) {
    const cJSON* value = Get(object, key);
    if (!cJSON_IsNumber(value)) return false;
    return (allow_minimum ? value->valuedouble >= minimum : value->valuedouble > minimum) &&
           value->valuedouble <= maximum;
}

bool IsBoundedInteger(const cJSON* object, const char* key,
                      int minimum, int maximum) {
    const cJSON* value = Get(object, key);
    return cJSON_IsNumber(value) &&
           value->valuedouble == static_cast<double>(value->valueint) &&
           value->valueint >= minimum && value->valueint <= maximum;
}

bool HasNoDuplicateKeys(const cJSON* object) {
    if (!cJSON_IsObject(object) && !cJSON_IsArray(object)) return true;
    for (const cJSON* item = object->child; item != nullptr; item = item->next) {
        if (cJSON_IsObject(object)) {
            for (const cJSON* other = item->next; other != nullptr; other = other->next) {
                if (item->string != nullptr && other->string != nullptr &&
                    std::strcmp(item->string, other->string) == 0) return false;
            }
        }
        if (!HasNoDuplicateKeys(item)) return false;
    }
    return true;
}

bool IsSafeServo(const cJSON* servo) {
    return HasExactKeys(servo, {"gpio", "min_deg", "center_deg", "max_deg",
                                "speed_deg_per_sec", "enabled"}) &&
           IsNull(servo, "gpio") && IsFalse(servo, "enabled") &&
           IsBoundedInteger(servo, "min_deg", 0, 180) &&
           IsBoundedInteger(servo, "center_deg", 0, 180) &&
           IsBoundedInteger(servo, "max_deg", 0, 180) &&
           IsBoundedNumber(servo, "speed_deg_per_sec", 0, 360, false) &&
           Get(servo, "min_deg")->valuedouble < Get(servo, "center_deg")->valuedouble &&
           Get(servo, "center_deg")->valuedouble < Get(servo, "max_deg")->valuedouble;
}

bool IsSafeConfig(const cJSON* root) {
    if (!HasExactKeys(root, {"schema_version", "project", "firmware_base", "board",
                             "display", "audio", "servos", "pir", "gpio_policy"}) ||
        !HasNoDuplicateKeys(root)) return false;

    if (!IsNumber(root, "schema_version", 1) ||
        !IsString(root, "project", "Robot-cua-TRUONG")) return false;

    const cJSON* firmware = Get(root, "firmware_base");
    if (!HasExactKeys(firmware, {"repository", "ref"}) ||
        !IsString(firmware, "repository", "https://github.com/78/xiaozhi-esp32") ||
        !IsString(firmware, "ref", "v2.5.0")) return false;

    const cJSON* board = Get(root, "board");
    if (!HasExactKeys(board, {"manufacturer", "product_family", "pcb_revision", "profile",
                              "upstream_reference_profile", "chip", "flash_mb", "psram_mb",
                              "camera_enabled", "onboard_codec_enabled",
                              "onboard_io_expander_enabled"}) ||
        !IsString(board, "manufacturer", "Waveshare") ||
        !IsString(board, "product_family", "ESP32-S3-CAM-OVxxxx") ||
        !IsNullableString(board, "pcb_revision") ||
        !IsString(board, "profile", "otto-waveshare-esp32-s3-cam") ||
        !IsString(board, "upstream_reference_profile", "waveshare/esp32-s3-cam") ||
        !IsString(board, "chip", "esp32s3") ||
        !IsNumber(board, "flash_mb", 16) || !IsNumber(board, "psram_mb", 8) ||
        !IsFalse(board, "camera_enabled") ||
        !IsFalse(board, "onboard_codec_enabled") ||
        !IsFalse(board, "onboard_io_expander_enabled")) return false;

    const cJSON* display = Get(root, "display");
    if (!HasExactKeys(display, {"type", "width", "height", "rotation", "gpio", "enabled"}) ||
        !IsString(display, "type", "st7789") ||
        !IsNumber(display, "width", 240) || !IsNumber(display, "height", 240) ||
        !(IsNull(display, "rotation") || IsNumber(display, "rotation", 0) ||
          IsNumber(display, "rotation", 90) || IsNumber(display, "rotation", 180) ||
          IsNumber(display, "rotation", 270)) ||
        !IsFalse(display, "enabled") ||
        !HasNullKeys(Get(display, "gpio"),
                     {"sclk", "mosi", "cs", "dc", "reset", "backlight"})) return false;

    const cJSON* audio = Get(root, "audio");
    if (!HasExactKeys(audio, {"microphone", "amplifier"})) return false;
    const cJSON* microphone = Get(audio, "microphone");
    const cJSON* mic_i2s = Get(microphone, "i2s");
    if (!HasExactKeys(microphone, {"type", "i2s", "enabled"}) ||
        !IsString(microphone, "type", "inmp441") ||
        !IsFalse(microphone, "enabled") ||
        !HasNullKeys(mic_i2s, {"ws", "sck", "sd"})) return false;

    const cJSON* amplifier = Get(audio, "amplifier");
    const cJSON* amp_i2s = Get(amplifier, "i2s");
    const cJSON* impedance = Get(amplifier, "speaker_impedance_ohm");
    if (!HasExactKeys(amplifier, {"type", "i2s", "enabled", "speaker_impedance_ohm"}) ||
        !IsString(amplifier, "type", "max98357a") ||
        !IsFalse(amplifier, "enabled") ||
        !HasNullKeys(amp_i2s, {"din", "bclk", "lrclk"}) ||
        !(cJSON_IsNull(impedance) ||
          (cJSON_IsNumber(impedance) && impedance->valuedouble > 0))) return false;

    const cJSON* servos = Get(root, "servos");
    if (!HasExactKeys(servos, {"pan", "tilt", "external_power_verified"}) ||
        !IsSafeServo(Get(servos, "pan")) ||
        !IsSafeServo(Get(servos, "tilt")) ||
        !IsFalse(servos, "external_power_verified")) return false;

    const cJSON* pir = Get(root, "pir");
    if (!HasExactKeys(pir, {"type", "gpio", "enabled"}) ||
        !IsString(pir, "type", "hc-sr501") ||
        !IsNull(pir, "gpio") || !IsFalse(pir, "enabled")) return false;

    const cJSON* policy = Get(root, "gpio_policy");
    if (!HasExactKeys(policy, {"allow_unassigned", "reject_duplicate_assignments",
                               "reject_bootstrap_conflicts", "reject_flash_psram_conflicts",
                               "reject_usb_uart_conflicts", "require_board_pinout_validation"})) {
        return false;
    }
    // Keep the safety policy fail-closed. A JSON import must not be able to
    // disable duplicate-pin, boot-strap, flash/PSRAM, USB/UART or pinout checks.
    if (!cJSON_IsTrue(Get(policy, "allow_unassigned")) ||
        !cJSON_IsTrue(Get(policy, "reject_duplicate_assignments")) ||
        !cJSON_IsTrue(Get(policy, "reject_bootstrap_conflicts")) ||
        !cJSON_IsTrue(Get(policy, "reject_flash_psram_conflicts")) ||
        !cJSON_IsTrue(Get(policy, "reject_usb_uart_conflicts")) ||
        !cJSON_IsTrue(Get(policy, "require_board_pinout_validation"))) {
        return false;
    }
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
