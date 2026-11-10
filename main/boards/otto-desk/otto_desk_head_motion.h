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


// The phase is toggled before this helper is called. Start the first 700 ms
// speaking interval by relaxing to neutral, then alternate back to speaking.
constexpr const char* SpeakingNodEmotion(bool phase) {
    return phase ? "neutral" : "speaking";
}

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


// Stateful, hardware-independent two-axis motion sequence. A board task calls
// Advance() periodically and forwards the resulting pose to guarded drivers.
// This class never touches GPIO, PWM, or servo power.
struct HeadMotionFrame {
    float pan_deg = 90.0F;
    float tilt_deg = 90.0F;
    bool moving = false;
};

class HeadMotionSequence {
public:
    HeadMotionSequence(HeadAxisLimits pan, HeadAxisLimits tilt)
        : pan_limits_(pan), tilt_limits_(tilt) {
        valid_ = HeadMotionPlanner::IsValid(pan_limits_) &&
                 HeadMotionPlanner::IsValid(tilt_limits_);
        if (valid_) {
            pan_ = pan_target_ = static_cast<float>(pan.center_deg);
            tilt_ = tilt_target_ = static_cast<float>(tilt.center_deg);
        }
    }

    bool IsValid() const { return valid_; }

    void SetEmotion(const char* emotion) {
        if (!valid_) return;
        const HeadPoseOffset target =
            HeadMotionPlanner::TargetForEmotion(emotion, pan_limits_, tilt_limits_);
        pan_start_ = pan_;
        tilt_start_ = tilt_;
        pan_target_ = static_cast<float>(target.pan_deg);
        tilt_target_ = static_cast<float>(target.tilt_deg);
        pan_duration_ms_ = HeadMotionPlanner::DurationMs(
            static_cast<int>(std::lround(pan_start_)), target.pan_deg, pan_limits_);
        tilt_duration_ms_ = HeadMotionPlanner::DurationMs(
            static_cast<int>(std::lround(tilt_start_)), target.tilt_deg, tilt_limits_);
        duration_ms_ = std::max(pan_duration_ms_, tilt_duration_ms_);
        elapsed_ms_ = 0;
        if (duration_ms_ == 0) {
            pan_ = pan_target_;
            tilt_ = tilt_target_;
        }
    }

    HeadMotionFrame Advance(uint32_t elapsed_ms) {
        if (!valid_) return {90.0F, 90.0F, false};
        if (duration_ms_ == 0 || elapsed_ms == 0) return Current();
        const uint64_t next = static_cast<uint64_t>(elapsed_ms_) + elapsed_ms;
        elapsed_ms_ = static_cast<uint32_t>(
            std::min<uint64_t>(next, duration_ms_));
        pan_ = InterpolateAxis(pan_start_, pan_target_, elapsed_ms_,
                               pan_duration_ms_);
        tilt_ = InterpolateAxis(tilt_start_, tilt_target_, elapsed_ms_,
                                tilt_duration_ms_);
        if (elapsed_ms_ >= duration_ms_) {
            pan_ = pan_target_;
            tilt_ = tilt_target_;
        }
        return Current();
    }

    HeadMotionFrame Current() const {
        return {pan_, tilt_, valid_ && duration_ms_ != 0 &&
                                  elapsed_ms_ < duration_ms_};
    }

private:
    static float InterpolateAxis(float from, float to, uint32_t elapsed,
                                 uint32_t duration) {
        if (duration == 0 || elapsed >= duration) return to;
        const float t = static_cast<float>(elapsed) /
                        static_cast<float>(duration);
        const float inverse = 1.0F - t;
        const float eased = 1.0F - inverse * inverse * inverse;
        return from + (to - from) * eased;
    }

    HeadAxisLimits pan_limits_;
    HeadAxisLimits tilt_limits_;
    float pan_ = 90.0F;
    float tilt_ = 90.0F;
    float pan_start_ = 90.0F;
    float tilt_start_ = 90.0F;
    float pan_target_ = 90.0F;
    float tilt_target_ = 90.0F;
    uint32_t pan_duration_ms_ = 0;
    uint32_t tilt_duration_ms_ = 0;
    uint32_t duration_ms_ = 0;
    uint32_t elapsed_ms_ = 0;
    bool valid_ = false;
};

}  // namespace otto_desk
