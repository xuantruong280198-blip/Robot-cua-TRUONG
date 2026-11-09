#pragma once

// Conservative ESP32-S3 module policy for this N16R8 target.
// GPIO26-37 are withheld because octal flash/PSRAM routing can occupy them.
// GPIO0/3/45/46 are strapping pins; 19/20 are native USB; 43/44 are UART0.
// The exact carrier-board pin map must still be independently verified.
namespace otto_desk {

constexpr bool IsSafeEsp32S3Gpio(int gpio) {
    if (gpio < 0 || gpio > 48) return false;
    if (gpio == 0 || gpio == 3 || gpio == 19 || gpio == 20) return false;
    if (gpio >= 26 && gpio <= 37) return false;
    if (gpio == 43 || gpio == 44 || gpio == 45 || gpio == 46) return false;
    return true;
}

}  // namespace otto_desk
