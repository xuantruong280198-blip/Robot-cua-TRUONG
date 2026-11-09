#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace otto_desk {

constexpr int16_t Inmp441SlotToPcm16(int32_t slot) {
    // The signed 24-bit microphone sample is left-aligned in a 32-bit slot.
    // Extracting the upper 16 bits preserves the signed two's-complement value.
    return static_cast<int16_t>(static_cast<uint32_t>(slot) >> 16);
}

constexpr int16_t ScalePcm16(int16_t sample, int volume_percent) {
    const int bounded_volume = std::clamp(volume_percent, 0, 100);
    const int32_t scaled = static_cast<int32_t>(sample) * bounded_volume / 100;
    return static_cast<int16_t>(std::clamp(
        scaled, static_cast<int32_t>(std::numeric_limits<int16_t>::min()),
        static_cast<int32_t>(std::numeric_limits<int16_t>::max())));
}

}  // namespace otto_desk
