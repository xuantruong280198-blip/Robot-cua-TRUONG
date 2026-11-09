#include "otto_desk_servo.h"

#include <algorithm>
#include <cstdint>

namespace {
constexpr ledc_mode_t kSpeedMode = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t kTimer = LEDC_TIMER_0;
constexpr ledc_timer_bit_t kResolution = LEDC_TIMER_14_BIT;
constexpr uint32_t kPwmFrequencyHz = 50;
constexpr uint32_t kDutyMaximum = (1U << 14) - 1U;
constexpr uint32_t kPulseMinUs = 500;
constexpr uint32_t kPulseMaxUs = 2500;
constexpr uint32_t kPeriodUs = 1000000U / kPwmFrequencyHz;
}

OttoDeskServoAxis::OttoDeskServoAxis(
    gpio_num_t gpio, ledc_channel_t channel,
    bool board_pinout_validated, bool external_power_verified)
    : gpio_(gpio), channel_(channel) {
    // Fail closed: no PWM on a guessed pin or from the ESP32 3V3 rail.
    if (!board_pinout_validated || !external_power_verified ||
        gpio < GPIO_NUM_0 || gpio >= GPIO_NUM_MAX ||
        gpio == GPIO_NUM_0 || gpio == GPIO_NUM_3 ||
        gpio == GPIO_NUM_19 || gpio == GPIO_NUM_20 ||
        gpio == GPIO_NUM_43 || gpio == GPIO_NUM_44 ||
        gpio == GPIO_NUM_45 || gpio == GPIO_NUM_46) {
        return;
    }
}

OttoDeskServoAxis::~OttoDeskServoAxis() {
    if (ready_) {
        ledc_stop(kSpeedMode, channel_, 0);
    }
}

esp_err_t OttoDeskServoAxis::Initialize() {
    if (gpio_ < GPIO_NUM_0 || gpio_ >= GPIO_NUM_MAX) return ESP_ERR_INVALID_ARG;
    if (ready_) return ESP_OK;

    if (!timer_initialized_) {
        ledc_timer_config_t timer = {};
        timer.speed_mode = kSpeedMode;
        timer.duty_resolution = kResolution;
        timer.timer_num = kTimer;
        timer.freq_hz = kPwmFrequencyHz;
        timer.clk_cfg = LEDC_AUTO_CLK;
        esp_err_t err = ledc_timer_config(&timer);
        if (err != ESP_OK) return err;
        timer_initialized_ = true;
    }

    ledc_channel_config_t channel = {};
    channel.gpio_num = gpio_;
    channel.speed_mode = kSpeedMode;
    channel.channel = channel_;
    channel.intr_type = LEDC_INTR_DISABLE;
    channel.timer_sel = kTimer;
    channel.duty = 0;
    channel.hpoint = 0;
    esp_err_t err = ledc_channel_config(&channel);
    if (err != ESP_OK) return err;
    ready_ = true;
    return ESP_OK;
}

esp_err_t OttoDeskServoAxis::SetAngle(int angle_deg) {
    if (!ready_) return ESP_ERR_INVALID_STATE;
    const int angle = std::clamp(angle_deg, 0, 180);
    const uint32_t pulse_us = kPulseMinUs +
        (static_cast<uint32_t>(angle) * (kPulseMaxUs - kPulseMinUs)) / 180U;
    const uint32_t duty = (pulse_us * kDutyMaximum) / kPeriodUs;
    esp_err_t err = ledc_set_duty(kSpeedMode, channel_, duty);
    if (err != ESP_OK) return err;
    return ledc_update_duty(kSpeedMode, channel_);
}
