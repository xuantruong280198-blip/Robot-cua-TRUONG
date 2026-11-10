#pragma once

namespace otto_desk {

struct DisplayRotation {
    bool swap_xy;
    bool mirror_x;
    bool mirror_y;
};

// Convert the validated JSON rotation into the LVGL/ESP-LCD transform flags.
// Unsupported values deliberately fall back to the native orientation.
constexpr DisplayRotation GetDisplayRotation(int degrees) {
    switch (degrees) {
        case 90: return {true, true, false};
        case 180: return {false, true, true};
        case 270: return {true, false, true};
        case 0:
        default: return {false, false, false};
    }
}

}  // namespace otto_desk
