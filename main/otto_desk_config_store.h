#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "esp_err.h"


struct OttoDeskServoConfig {
    std::optional<int> gpio;
    int min_deg = 0;
    int center_deg = 90;
    int max_deg = 180;
    float speed_deg_per_sec = 0.0F;
    bool enabled = false;
};

struct OttoDeskGpioPolicy {
    bool allow_unassigned = true;
    bool reject_duplicate_assignments = true;
    bool reject_bootstrap_conflicts = true;
    bool reject_flash_psram_conflicts = true;
    bool reject_usb_uart_conflicts = true;
    bool require_board_pinout_validation = true;
};

struct OttoDeskRuntimeConfig {
    int display_width = 240;
    int display_height = 240;
    std::optional<int> display_rotation;
    std::optional<int> display_sclk;
    std::optional<int> display_mosi;
    std::optional<int> display_cs;
    std::optional<int> display_dc;
    std::optional<int> display_reset;
    std::optional<int> display_backlight;
    bool display_enabled = false;

    std::optional<int> microphone_ws;
    std::optional<int> microphone_sck;
    std::optional<int> microphone_sd;
    bool microphone_enabled = false;

    std::optional<int> amplifier_din;
    std::optional<int> amplifier_bclk;
    std::optional<int> amplifier_lrclk;
    bool amplifier_enabled = false;

    OttoDeskServoConfig pan;
    OttoDeskServoConfig tilt;
    bool servo_external_power_verified = false;

    std::optional<int> pir_gpio;
    bool pir_enabled = false;

    OttoDeskGpioPolicy gpio_policy;

    // Runtime activation is permitted only after the JSON explicitly confirms
    // carrier-board signal routing; each peripheral still has its own enable
    // and safety checks before its driver is initialized.
    bool hardware_activation_permitted = false;
};

// Bounded NVS storage for the Otto Desk JSON configuration.
// Validation enforces the fixed schema, safe/unique GPIO assignments, and
// explicit routing acknowledgement before runtime drivers may be activated.
class OttoDeskConfigStore {
public:
    static constexpr size_t kMaxConfigBytes = 3072;

    static esp_err_t Load(std::string& json);
    static esp_err_t Save(const std::string& json);
    static esp_err_t Clear();
    static esp_err_t ValidateSafeJson(const std::string& json);
    static esp_err_t Decode(const std::string& json, OttoDeskRuntimeConfig& config);
};
