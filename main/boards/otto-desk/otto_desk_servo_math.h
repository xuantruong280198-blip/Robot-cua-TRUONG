#pragma once
#include <algorithm>
#include <cstdint>
namespace otto_desk {
constexpr uint32_t kServoPulseMinUs = 500;
constexpr uint32_t kServoPulseMaxUs = 2500;
constexpr uint32_t kServoPeriodUs = 20000;
constexpr uint32_t kServoDutyMaximum = (1U << 14) - 1U;
constexpr uint32_t ServoPulseUsForAngle(int angle_deg) {
    const int bounded = std::clamp(angle_deg, 0, 180);
    return kServoPulseMinUs + (static_cast<uint32_t>(bounded) *
           (kServoPulseMaxUs - kServoPulseMinUs)) / 180U;
}
constexpr int ClampServoAngle(int angle_deg, int min_deg, int max_deg) {
    return (min_deg >= 0 && min_deg < max_deg && max_deg <= 180)
        ? std::clamp(angle_deg, min_deg, max_deg)
        : std::clamp(angle_deg, 0, 180);
}
constexpr uint32_t ServoPulseUsForAngle(int angle_deg, int min_deg, int max_deg) {
    return ServoPulseUsForAngle(ClampServoAngle(angle_deg, min_deg, max_deg));
}
constexpr uint32_t ServoDutyForAngle(int angle_deg) {
    return (ServoPulseUsForAngle(angle_deg) * kServoDutyMaximum) / kServoPeriodUs;
}
constexpr uint32_t ServoDutyForAngle(int angle_deg, int min_deg, int max_deg) {
    return (ServoPulseUsForAngle(angle_deg, min_deg, max_deg) * kServoDutyMaximum) /
           kServoPeriodUs;
}
}  // namespace otto_desk
