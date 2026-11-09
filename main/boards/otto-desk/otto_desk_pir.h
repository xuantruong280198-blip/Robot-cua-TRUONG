#pragma once

#include <driver/gpio.h>
#include <esp_err.h>

// HC-SR501 digital input. It stays inactive unless the caller has validated
// the exact 40-pin header mapping; no pin is assumed by this driver.
class OttoDeskPirSensor {
public:
    OttoDeskPirSensor(gpio_num_t gpio, bool board_pinout_validated);
    esp_err_t Initialize();
    bool IsMotionDetected() const;
    bool IsReady() const { return ready_; }

private:
    gpio_num_t gpio_;
    bool permitted_ = false;
    bool ready_ = false;
};
