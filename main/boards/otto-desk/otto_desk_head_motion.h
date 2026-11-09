#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace otto_desk {

// Pure motion planning for the two-axis desk head. This module deliberately
// has no ESP-IDF, PWM, GPIO, or servo-driver dependency.
struct HeadAxisLimits {
    int min_deg = 0;
    int center_deg = 90;
    int max_deg = 180;
    float speed_deg_per_sec = 0.0F;
};

class HeadMotionPlanner {
public:
    static bool IsValid(const HeadAxisLimits& limits) {
        return limits.min_deg >= 0 && limits.max_deg <= 180 &&
               limits.min_deg < limits.center_deg &&
               limits.center_deg < limits.max_deg &&
               std::isfinite(limits.speed_deg_per_sec) &&
               limits.speed_deg_per_sec >= 0.0F &&
               limits.speed_deg_per_sec <= 360.0F;
    }

    static int ClampTarget(int target_deg, const HeadAxisLimits& limits) {
        return std::clamp(target_deg, limits.min_deg, limits.max_deg);
    }

    // Zero speed means an explicitly instantaneous move. A positive speed
    // produces a duration rounded up to whole milliseconds.
    static uint32_t DurationMs(int from_deg, int to_deg,
                               const HeadAxisLimits& limits) {
        if (limits.speed_deg_per_sec <= 0.0F) return 0;
        const int delta = std::abs(ClampTarget(to_deg, limits) -
                                   ClampTarget(from_deg, limits));
        if (delta == 0) return 0;
        return static_cast<uint32_t>(
            std::ceil((static_cast<double>(delta) * 1000.0) /
                      static_cast<double>(limits.speed_deg_per_sec)));
    }

    // Ease-out cubic gives a quick initial response and a soft arrival.
    static float Interpolate(float progress, int from_deg, int to_deg,
                             const HeadAxisLimits& limits) {
        const float t = std::clamp(progress, 0.0F, 1.0F);
        const float inverse = 1.0F - t;
        const float eased = 1.0F - inverse * inverse * inverse;
        const int from = ClampTarget(from_deg, limits);
        const int to = ClampTarget(to_deg, limits);
        return static_cast<float>(from) +
               static_cast<float>(to - from) * eased;
    }
};

}  // namespace otto_desk
