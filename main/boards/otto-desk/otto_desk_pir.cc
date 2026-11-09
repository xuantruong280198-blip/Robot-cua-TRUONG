#include "otto_desk_pir.h"
#include "otto_desk_gpio_safety.h"

OttoDeskPirSensor::OttoDeskPirSensor(gpio_num_t gpio, bool board_pinout_validated)
    : gpio_(gpio), permitted_(board_pinout_validated &&
                              otto_desk::IsSafeEsp32S3Gpio(static_cast<int>(gpio))) {
}

esp_err_t OttoDeskPirSensor::Initialize() {
    if (!permitted_) return ESP_ERR_INVALID_STATE;
    if (ready_) return ESP_OK;

    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << static_cast<unsigned>(gpio_);
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    const esp_err_t err = gpio_config(&config);
    if (err == ESP_OK) ready_ = true;
    return err;
}

bool OttoDeskPirSensor::IsMotionDetected() const {
    return ready_ && gpio_get_level(gpio_) == 1;
}
