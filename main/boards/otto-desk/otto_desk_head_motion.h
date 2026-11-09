#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace otto_desk {

// Pure motion planning for the two-axis desk head. This module deliberately
// has no ESP-IDF, PWM, GPIO, or servo-driver dependency.
struct HeadAxisLimits {
    int min_deg = 0;
    int center_deg = 90;
    int max_deg = 180;
    float speed_deg_per_sec = 0.0F;
};


struct HeadPoseOffset {
    int pan_deg = 0;
    int tilt_deg = 0;
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

    // Resolve XiaoZhi emotion/status tokens into small relative head offsets.
    // Tilt sign depends on bracket mounting; calibrate before enabling outputs.
    static HeadPoseOffset PoseForEmotion(const char* emotion) {
        if (emotion == nullptr) return {};
        if (std::strcmp(emotion, "listening") == 0) return {0, -5};
        if (std::strcmp(emotion, "thinking") == 0) return {10, -7};
        if (std::strcmp(emotion, "speaking") == 0) return {0, 3};
        if (std::strcmp(emotion, "happy") == 0) return {-7, -5};
        if (std::strcmp(emotion, "sad") == 0) return {0, 10};
        if (std::strcmp(emotion, "surprised") == 0) return {0, -12};
        if (std::strcmp(emotion, "angry") == 0) return {7, 2};
        return {};
    }

    static HeadPoseOffset TargetForEmotion(const char* emotion,
                                           const HeadAxisLimits& pan,
                                           const HeadAxisLimits& tilt) {
        const HeadPoseOffset offset = PoseForEmotion(emotion);
        return {
            ClampTarget(pan.center_deg + offset.pan_deg, pan),
            ClampTarget(tilt.center_deg + offset.tilt_deg, tilt),
        };
    }

    // Invalid limits fail closed to the physical 0..180 degree envelope.
    static int ClampTarget(int target_deg, const HeadAxisLimits& limits) {
        if (!IsValid(limits)) return std::clamp(target_deg, 0, 180);
        return std::clamp(target_deg, limits.min_deg, limits.max_deg);
    }

    // Zero speed means an explicitly instantaneous move. Very slow speeds
    // saturate rather than overflowing the millisecond duration type.
    static uint32_t DurationMs(int from_deg, int to_deg,
                               const HeadAxisLimits& limits) {
        if (!IsValid(limits) || limits.speed_deg_per_sec <= 0.0F) return 0;
        const int delta = std::abs(ClampTarget(to_deg, limits) -
                                   ClampTarget(from_deg, limits));
        if (delta == 0) return 0;
        const double duration =
            std::ceil((static_cast<double>(delta) * 1000.0) /
                      static_cast<double>(limits.speed_deg_per_sec));
        if (duration >= static_cast<double>(
                            std::numeric_limits<uint32_t>::max())) {
            return std::numeric_limits<uint32_t>::max();
        }
        return static_cast<uint32_t>(duration);
    }

    // Ease-out cubic gives a quick initial response and a soft arrival.
    // Non-finite progress safely holds the clamped starting position.
    static float Interpolate(float progress, int from_deg, int to_deg,
                             const HeadAxisLimits& limits) {
        const int from = ClampTarget(from_deg, limits);
        const int to = ClampTarget(to_deg, limits);
        if (!IsValid(limits) || !std::isfinite(progress)) {
            return static_cast<float>(from);
        }
        const float t = std::clamp(progress, 0.0F, 1.0F);
        const float inverse = 1.0F - t;
        const float eased = 1.0F - inverse * inverse * inverse;
        return static_cast<float>(from) +
               static_cast<float>(to - from) * eased;
    }
};

}  // namespace otto_desk
