#pragma once

// Conservative ESP32-S3 module policy for this N16R8 target.
// GPIO26-37 are withheld because octal flash/PSRAM routing can occupy them.
// GPIO0/3/45/46 are strapping pins; 19/20 are native USB; 43/44 are UART0.
// The visible 40-pin header label order is confirmed from the user's photo;
// peripheral wiring and board-specific reserved routes still require audit.
namespace otto_desk {

constexpr bool IsSafeEsp32S3Gpio(int gpio) {
    if (gpio < 0 || gpio > 48) return false;
    // ESP32-S3 has no GPIO22-25; reject holes in the GPIO number space.
    if (gpio >= 22 && gpio <= 25) return false;
    if (gpio == 0 || gpio == 3 || gpio == 19 || gpio == 20) return false;
    if (gpio >= 26 && gpio <= 37) return false;
    if (gpio == 43 || gpio == 44 || gpio == 45 || gpio == 46) return false;
    return true;
}

// Validate the six independent I2S signals before allocating driver resources.
// The mic and amplifier use separate I2S controllers in this implementation,
// so their clock/data pins must all be distinct.
constexpr bool IsValidI2sPinAssignment(int mic_ws, int mic_sck, int mic_sd,
                                       int amp_bclk, int amp_lrclk, int amp_din,
                                       int input_sample_rate, int output_sample_rate) {
    const int pins[] = {mic_ws, mic_sck, mic_sd, amp_bclk, amp_lrclk, amp_din};
    if (input_sample_rate <= 0 || output_sample_rate <= 0) return false;
    for (int i = 0; i < 6; ++i) {
        if (!IsSafeEsp32S3Gpio(pins[i])) return false;
        for (int j = 0; j < i; ++j) {
            if (pins[i] == pins[j]) return false;
        }
    }
    return true;
}

}  // namespace otto_desk
