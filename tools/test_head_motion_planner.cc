#include "otto_desk_head_motion.h"

#include <cmath>
#include <cstdint>
#include <limits>

using otto_desk::HeadAxisLimits;
using otto_desk::HeadMotionPlanner;

namespace {
int failures = 0;

void Check(bool condition, const char* name) {
    if (!condition) {
        ++failures;
        // Keep output deterministic and useful in CI logs.
        std::fprintf(stderr, "FAIL: %s\n", name);
    }
}
}  // namespace

int main() {
    const HeadAxisLimits normal{20, 90, 160, 70.0F};
    Check(HeadMotionPlanner::IsValid(normal), "valid limits accepted");
    Check(!HeadMotionPlanner::IsValid({20, 20, 160, 70.0F}),
          "center must be strictly inside limits");
    Check(!HeadMotionPlanner::IsValid({20, 90, 160,
                                       std::numeric_limits<float>::infinity()}),
          "non-finite speed rejected");
    Check(HeadMotionPlanner::ClampTarget(-10, normal) == 20,
          "target clamps to minimum");
    Check(HeadMotionPlanner::ClampTarget(999, normal) == 160,
          "target clamps to maximum");

    Check(HeadMotionPlanner::DurationMs(20, 90, normal) == 1000,
          "duration rounds up to milliseconds");
    Check(HeadMotionPlanner::DurationMs(90, 90, normal) == 0,
          "zero distance has zero duration");
    Check(HeadMotionPlanner::DurationMs(20, 160, {20, 90, 160, 0.0F}) == 0,
          "zero speed is instantaneous");
    Check(HeadMotionPlanner::DurationMs(20, 160, {20, 90, 160, 1.0e-30F}) ==
              std::numeric_limits<uint32_t>::max(),
          "very slow speed saturates duration");
    Check(HeadMotionPlanner::DurationMs(20, 160, {90, 90, 90, 50.0F}) == 0,
          "invalid limits never schedule motion");

    Check(std::fabs(HeadMotionPlanner::Interpolate(0.0F, 20, 160, normal) -
                    20.0F) < 0.001F,
          "interpolation starts at source");
    Check(std::fabs(HeadMotionPlanner::Interpolate(1.0F, 20, 160, normal) -
                    160.0F) < 0.001F,
          "interpolation ends at target");
    Check(std::fabs(HeadMotionPlanner::Interpolate(0.5F, 20, 160, normal) -
                    142.5F) < 0.001F,
          "ease-out cubic midpoint");
    Check(std::fabs(HeadMotionPlanner::Interpolate(
                        std::numeric_limits<float>::quiet_NaN(), 20, 160, normal) -
                    20.0F) < 0.001F,
          "non-finite progress holds source");
    Check(std::fabs(HeadMotionPlanner::Interpolate(0.5F, 20, 160,
                                                   {90, 90, 90, 50.0F}) -
                    20.0F) < 0.001F,
          "invalid limits hold safe clamped source");

    if (failures != 0) {
        std::fprintf(stderr, "%d head motion test(s) failed\n", failures);
        return 1;
    }
    std::puts("All Otto Desk head motion tests passed.");
    return 0;
}
