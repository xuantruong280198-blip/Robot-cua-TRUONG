#include "otto_desk_config_store.h"
#include "boards/otto-desk/otto_desk_gpio_safety.h"
#include "otto_desk_json_safety.h"

#include <cJSON.h>
#include <nvs.h>
#include <nvs_flash.h>

#include <cctype>
#include <cstring>
#include <initializer_list>
#include <optional>

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

bool IsSafeNullableGpio(const cJSON* object, const char* key) {
    const cJSON* value = Get(object, key);
    if (cJSON_IsNull(value)) return true;
    return cJSON_IsNumber(value) &&
           value->valuedouble == static_cast<double>(value->valueint) &&
           otto_desk::IsSafeEsp32S3Gpio(value->valueint);
}

bool HasSafeGpioKeys(const cJSON* object, std::initializer_list<const char*> keys) {
    if (!HasExactKeys(object, keys)) return false;
    for (const char* key : keys) {
        if (!IsSafeNullableGpio(object, key)) return false;
    }
    return true;
}

bool IsGpioKey(const char* key) {
    static constexpr const char* kKeys[] = {
        "gpio", "ws", "sck", "sd", "din", "bclk", "lrclk",
        "sclk", "mosi", "miso", "cs", "dc", "reset", "backlight"
    };
    if (key == nullptr) return false;
    for (const char* candidate : kKeys) {
        if (std::strcmp(key, candidate) == 0) return true;
    }
    return false;
}

bool HasSafeUniqueGpioAssignments(const cJSON* node, bool (&used)[49]) {
    if (!cJSON_IsObject(node) && !cJSON_IsArray(node)) return true;
    for (const cJSON* item = node->child; item != nullptr; item = item->next) {
        if (cJSON_IsObject(node) && IsGpioKey(item->string) &&
            !cJSON_IsObject(item) && !cJSON_IsArray(item) && !cJSON_IsNull(item)) {
            if (!cJSON_IsNumber(item) ||
                item->valuedouble != static_cast<double>(item->valueint) ||
                !otto_desk::IsSafeEsp32S3Gpio(item->valueint) ||
                used[item->valueint]) return false;
            used[item->valueint] = true;
        }
        if (!HasSafeUniqueGpioAssignments(item, used)) return false;
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


int ReadInt(const cJSON* object, const char* key, int fallback = 0) {
    const cJSON* value = Get(object, key);
    return cJSON_IsNumber(value) ? value->valueint : fallback;
}

float ReadFloat(const cJSON* object, const char* key, float fallback = 0.0F) {
    const cJSON* value = Get(object, key);
    return cJSON_IsNumber(value) ? static_cast<float>(value->valuedouble) : fallback;
}

bool ReadBool(const cJSON* object, const char* key) {
    return cJSON_IsTrue(Get(object, key));
}

std::optional<int> ReadOptionalInt(const cJSON* object, const char* key) {
    const cJSON* value = Get(object, key);
    if (cJSON_IsNull(value) || value == nullptr) return std::nullopt;
    if (!cJSON_IsNumber(value)) return std::nullopt;
    return value->valueint;
}

OttoDeskServoConfig ReadServo(const cJSON* servo) {
    OttoDeskServoConfig result;
    result.gpio = ReadOptionalInt(servo, "gpio");
    result.min_deg = ReadInt(servo, "min_deg");
    result.center_deg = ReadInt(servo, "center_deg", 90);
    result.max_deg = ReadInt(servo, "max_deg", 180);
    result.speed_deg_per_sec = ReadFloat(servo, "speed_deg_per_sec");
    result.enabled = ReadBool(servo, "enabled");
    return result;
}

bool IsSafeServo(const cJSON* servo, bool allow_enabled) {
    const bool enabled = ReadBool(servo, "enabled");
    const cJSON* gpio = Get(servo, "gpio");
    return HasExactKeys(servo, {"gpio", "min_deg", "center_deg", "max_deg",
                                "speed_deg_per_sec", "enabled"}) &&
           IsSafeNullableGpio(servo, "gpio") &&
           (!enabled || (allow_enabled && cJSON_IsNumber(gpio))) &&
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

    bool used_gpios[49] = {};
    if (!HasSafeUniqueGpioAssignments(root, used_gpios)) return false;

    if (!IsNumber(root, "schema_version", 1) ||
        !IsString(root, "project", "Robot-cua-TRUONG")) return false;

    const cJSON* firmware = Get(root, "firmware_base");
    if (!HasExactKeys(firmware, {"repository", "ref"}) ||
        !IsString(firmware, "repository", "https://github.com/78/xiaozhi-esp32") ||
        !IsString(firmware, "ref", "v2.5.0")) return false;

    const cJSON* board = Get(root, "board");
    const bool routing_verified = cJSON_IsTrue(Get(board, "peripheral_routing_verified"));
    if (!HasExactKeys(board, {"manufacturer", "product_family", "pcb_revision", "profile",
                              "upstream_reference_profile", "chip", "flash_mb", "psram_mb",
                              "camera_enabled", "onboard_codec_enabled",
                              "onboard_io_expander_enabled", "header_pin_count",
                              "header_labels_confirmed", "peripheral_routing_verified"}) ||
        !IsString(board, "manufacturer", "Waveshare") ||
        !IsString(board, "product_family", "ESP32-S3-CAM-N16R8") ||
        !IsNumber(board, "header_pin_count", 40) ||
        !cJSON_IsTrue(Get(board, "header_labels_confirmed")) ||
        !cJSON_IsBool(Get(board, "peripheral_routing_verified")) ||
        !IsNullableString(board, "pcb_revision") ||
        !IsString(board, "profile", "otto-esp32-s3-cam-40pin") ||
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
        (!routing_verified && !IsFalse(display, "enabled")) ||
        !HasSafeGpioKeys(Get(display, "gpio"),
                     {"sclk", "mosi", "cs", "dc", "reset", "backlight"})) return false;
    if (ReadBool(display, "enabled")) {
        const cJSON* pins = Get(display, "gpio");
        if (!cJSON_IsNumber(Get(pins, "sclk")) || !cJSON_IsNumber(Get(pins, "mosi")) ||
            !cJSON_IsNumber(Get(pins, "dc")) || !cJSON_IsNumber(Get(pins, "reset")) ||
            !cJSON_IsNumber(Get(pins, "backlight"))) return false;
    }

    const cJSON* audio = Get(root, "audio");
    if (!HasExactKeys(audio, {"microphone", "amplifier"})) return false;
    const cJSON* microphone = Get(audio, "microphone");
    const cJSON* mic_i2s = Get(microphone, "i2s");
    if (!HasExactKeys(microphone, {"type", "i2s", "enabled"}) ||
        !IsString(microphone, "type", "inmp441") ||
        (!routing_verified && !IsFalse(microphone, "enabled")) ||
        !HasSafeGpioKeys(mic_i2s, {"ws", "sck", "sd"}) ||
        (ReadBool(microphone, "enabled") &&
         (!cJSON_IsNumber(Get(mic_i2s, "ws")) || !cJSON_IsNumber(Get(mic_i2s, "sck")) ||
          !cJSON_IsNumber(Get(mic_i2s, "sd"))))) return false;

    const cJSON* amplifier = Get(audio, "amplifier");
    const cJSON* amp_i2s = Get(amplifier, "i2s");
    const cJSON* impedance = Get(amplifier, "speaker_impedance_ohm");
    if (ReadBool(microphone, "enabled") != ReadBool(amplifier, "enabled")) return false;
    if (!HasExactKeys(amplifier, {"type", "i2s", "enabled", "speaker_impedance_ohm"}) ||
        !IsString(amplifier, "type", "max98357a") ||
        (!routing_verified && !IsFalse(amplifier, "enabled")) ||
        !HasSafeGpioKeys(amp_i2s, {"din", "bclk", "lrclk"}) ||
        (ReadBool(amplifier, "enabled") &&
         (!cJSON_IsNumber(Get(amp_i2s, "din")) || !cJSON_IsNumber(Get(amp_i2s, "bclk")) ||
          !cJSON_IsNumber(Get(amp_i2s, "lrclk")))) ||
        !(cJSON_IsNull(impedance) ||
          (cJSON_IsNumber(impedance) && impedance->valuedouble > 0))) return false;

    const cJSON* servos = Get(root, "servos");
    const bool external_power_verified = cJSON_IsTrue(Get(servos, "external_power_verified"));
    if (!HasExactKeys(servos, {"pan", "tilt", "external_power_verified"}) ||
        !cJSON_IsBool(Get(servos, "external_power_verified")) ||
        !IsSafeServo(Get(servos, "pan"), routing_verified && external_power_verified) ||
        !IsSafeServo(Get(servos, "tilt"), routing_verified && external_power_verified) ||
        (!routing_verified && external_power_verified)) return false;

    const cJSON* pir = Get(root, "pir");
    if (!HasExactKeys(pir, {"type", "gpio", "enabled"}) ||
        !IsString(pir, "type", "hc-sr501") ||
        !IsSafeNullableGpio(pir, "gpio") ||
        (!routing_verified && !IsFalse(pir, "enabled")) ||
        (ReadBool(pir, "enabled") && !cJSON_IsNumber(Get(pir, "gpio")))) return false;

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
    if (!otto_desk::HasSafeJsonNesting(json)) return ESP_ERR_INVALID_ARG;

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


esp_err_t OttoDeskConfigStore::Decode(const std::string& json,
                                      OttoDeskRuntimeConfig& config) {
    config = OttoDeskRuntimeConfig{};
    esp_err_t err = ValidateSafeJson(json);
    if (err != ESP_OK) return err;

    cJSON* root = cJSON_ParseWithLength(json.c_str(), json.size());
    if (root == nullptr) return ESP_ERR_INVALID_ARG;

    const cJSON* display = Get(root, "display");
    const cJSON* display_gpio = Get(display, "gpio");
    config.display_width = ReadInt(display, "width", 240);
    config.display_height = ReadInt(display, "height", 240);
    const cJSON* rotation = Get(display, "rotation");
    if (cJSON_IsNumber(rotation)) config.display_rotation = rotation->valueint;
    config.display_sclk = ReadOptionalInt(display_gpio, "sclk");
    config.display_mosi = ReadOptionalInt(display_gpio, "mosi");
    config.display_cs = ReadOptionalInt(display_gpio, "cs");
    config.display_dc = ReadOptionalInt(display_gpio, "dc");
    config.display_reset = ReadOptionalInt(display_gpio, "reset");
    config.display_backlight = ReadOptionalInt(display_gpio, "backlight");
    config.display_enabled = ReadBool(display, "enabled");

    const cJSON* audio = Get(root, "audio");
    const cJSON* microphone = Get(audio, "microphone");
    const cJSON* mic_i2s = Get(microphone, "i2s");
    config.microphone_ws = ReadOptionalInt(mic_i2s, "ws");
    config.microphone_sck = ReadOptionalInt(mic_i2s, "sck");
    config.microphone_sd = ReadOptionalInt(mic_i2s, "sd");
    config.microphone_enabled = ReadBool(microphone, "enabled");

    const cJSON* amplifier = Get(audio, "amplifier");
    const cJSON* amp_i2s = Get(amplifier, "i2s");
    config.amplifier_din = ReadOptionalInt(amp_i2s, "din");
    config.amplifier_bclk = ReadOptionalInt(amp_i2s, "bclk");
    config.amplifier_lrclk = ReadOptionalInt(amp_i2s, "lrclk");
    config.amplifier_enabled = ReadBool(amplifier, "enabled");

    const cJSON* servos = Get(root, "servos");
    config.pan = ReadServo(Get(servos, "pan"));
    config.tilt = ReadServo(Get(servos, "tilt"));
    config.servo_external_power_verified = ReadBool(servos, "external_power_verified");

    const cJSON* pir = Get(root, "pir");
    config.pir_gpio = ReadOptionalInt(pir, "gpio");
    config.pir_enabled = ReadBool(pir, "enabled");

    const cJSON* policy = Get(root, "gpio_policy");
    config.gpio_policy.allow_unassigned = ReadBool(policy, "allow_unassigned");
    config.gpio_policy.reject_duplicate_assignments =
        ReadBool(policy, "reject_duplicate_assignments");
    config.gpio_policy.reject_bootstrap_conflicts =
        ReadBool(policy, "reject_bootstrap_conflicts");
    config.gpio_policy.reject_flash_psram_conflicts =
        ReadBool(policy, "reject_flash_psram_conflicts");
    config.gpio_policy.reject_usb_uart_conflicts =
        ReadBool(policy, "reject_usb_uart_conflicts");
    config.gpio_policy.require_board_pinout_validation =
        ReadBool(policy, "require_board_pinout_validation");

    // Hardware remains off by default. Activation requires the explicit
    // board-routing acknowledgement plus complete, unique, safe GPIO entries.
    const cJSON* board = Get(root, "board");
    config.hardware_activation_permitted =
        cJSON_IsTrue(Get(board, "peripheral_routing_verified"));
    cJSON_Delete(root);
    return ESP_OK;
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
