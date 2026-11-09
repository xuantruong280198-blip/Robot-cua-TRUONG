#include <esp_log.h>
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "application.h"
#include "otto_desk_config_store.h"

#define TAG "main"

extern "C" void app_main(void)
{
    // Initialize NVS flash for WiFi configuration
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS flash to fix corruption");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Load and revalidate any saved Otto Desk configuration before board startup.
    // No saved config is a normal first-boot condition; peripherals remain disabled
    // until a verified board profile and runtime GPIO layer are available.
    std::string otto_desk_config;
    const esp_err_t config_ret = OttoDeskConfigStore::Load(otto_desk_config);
    if (config_ret == ESP_OK) {
        OttoDeskRuntimeConfig runtime_config;
        const esp_err_t decode_ret = OttoDeskConfigStore::Decode(otto_desk_config, runtime_config);
        if (decode_ret == ESP_OK) {
            ESP_LOGI(TAG,
                     "Decoded Otto Desk config: display=%s mic=%s amp=%s pan=%s tilt=%s pir=%s; hardware activation remains locked",
                     runtime_config.display_enabled ? "on" : "off",
                     runtime_config.microphone_enabled ? "on" : "off",
                     runtime_config.amplifier_enabled ? "on" : "off",
                     runtime_config.pan.enabled ? "on" : "off",
                     runtime_config.tilt.enabled ? "on" : "off",
                     runtime_config.pir_enabled ? "on" : "off");
        } else {
            ESP_LOGW(TAG, "Could not decode validated Otto Desk configuration: %s",
                     esp_err_to_name(decode_ret));
        }
        ESP_LOGI(TAG, "Validated saved Otto Desk configuration (%u bytes); no GPIO or peripheral drivers are started",
                 static_cast<unsigned>(otto_desk_config.size()));
    } else if (config_ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved Otto Desk configuration; retaining current board defaults");
    } else {
        ESP_LOGW(TAG, "Saved Otto Desk configuration rejected or unavailable: %s",
                 esp_err_to_name(config_ret));
    }

    // Initialize and run the application
    auto& app = Application::GetInstance();
    app.Initialize();
    app.Run();  // This function runs the main event loop and never returns
}
