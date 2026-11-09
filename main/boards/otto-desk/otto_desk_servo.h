#pragma once

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_err.h>

#include "otto_desk_head_motion.h"

// Hardware PWM output for one MG90S axis. Construction is refused unless
// the caller confirms both a validated board pin map and external servo power.
class OttoDeskServoAxis {
public:
    OttoDeskServoAxis(gpio_num_t gpio, ledc_channel_t channel,
                      bool board_pinout_validated, bool external_power_verified);
    ~OttoDeskServoAxis();

    esp_err_t Initialize();
    esp_err_t SetAngle(int angle_deg);
    bool IsReady() const { return ready_; }

private:
    gpio_num_t gpio_;
    ledc_channel_t channel_;
    bool permitted_ = false;
    bool ready_ = false;
    bool timer_initialized_ = false;
    ledc_timer_t timer_;
};
