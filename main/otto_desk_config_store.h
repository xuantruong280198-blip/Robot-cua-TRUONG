#pragma once

#include <string>

#include "esp_err.h"

// Bounded NVS storage for the Otto Desk JSON configuration.
// This layer deliberately accepts only the conservative, hardware-disabled
// configuration until a board-specific pin map and runtime drivers are ready.
class OttoDeskConfigStore {
public:
    static constexpr size_t kMaxConfigBytes = 3072;

    static esp_err_t Load(std::string& json);
    static esp_err_t Save(const std::string& json);
    static esp_err_t Clear();
    static esp_err_t ValidateSafeJson(const std::string& json);
};
