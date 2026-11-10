#include "otto_desk_head_motion.h"

#include <cmath>
#include <cstdio>
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
    Check(std::strcmp(otto_desk::SpeakingNodEmotion(true), "neutral") == 0,
          "first speaking nod phase relaxes toward neutral");
    Check(std::strcmp(otto_desk::SpeakingNodEmotion(false), "speaking") == 0,
          "second speaking nod phase returns to speaking pose");

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

    const HeadAxisLimits pan{30, 90, 150, 60.0F};
    const HeadAxisLimits tilt{40, 90, 140, 45.0F};
    const auto neutral = HeadMotionPlanner::TargetForEmotion("neutral", pan, tilt);
    Check(neutral.pan_deg == 90 && neutral.tilt_deg == 90,
          "neutral emotion returns calibrated center");
    const auto sad = HeadMotionPlanner::TargetForEmotion("sad", pan, tilt);
    Check(sad.pan_deg == 90 && sad.tilt_deg == 100,
          "sad emotion requests gentle tilt offset");
    const auto thinking = HeadMotionPlanner::TargetForEmotion("thinking", pan, tilt);
    Check(thinking.pan_deg == 100 && thinking.tilt_deg == 83,
          "thinking emotion requests pan and tilt offsets");
    const auto listening = HeadMotionPlanner::TargetForEmotion("listening", pan, tilt);
    Check(listening.pan_deg == 90 && listening.tilt_deg == 85,
          "listening emotion leans subtly");
    const auto unknown = HeadMotionPlanner::TargetForEmotion("unmapped-token", pan, tilt);
    Check(unknown.pan_deg == 90 && unknown.tilt_deg == 90,
          "unknown emotion falls back to neutral");
    const auto bounded = HeadMotionPlanner::TargetForEmotion(
        "sad", pan, {40, 135, 140, 45.0F});
    Check(bounded.tilt_deg == 140,
          "emotion offsets obey configured axis limits");
    const auto null_emotion = HeadMotionPlanner::TargetForEmotion(nullptr, pan, tilt);
    Check(null_emotion.pan_deg == 90 && null_emotion.tilt_deg == 90,
          "null emotion falls back to neutral");


    // Stateful emotion sequence supports smooth movement and mid-flight retargeting.
    otto_desk::HeadMotionSequence sequence(pan, tilt);
    Check(sequence.IsValid(), "valid motion sequence accepted");
    sequence.SetEmotion("sad");
    auto frame = sequence.Current();
    Check(frame.moving, "emotion request starts motion");
    frame = sequence.Advance(100);
    Check(frame.moving && frame.tilt_deg > 90.0F && frame.tilt_deg < 100.0F,
          "tilt moves smoothly before target");
    sequence.SetEmotion("neutral");
    Check(sequence.Current().moving, "new emotion retargets current pose");
    frame = sequence.Advance(5000);
    Check(!frame.moving && std::fabs(frame.pan_deg - 90.0F) < 0.001F &&
              std::fabs(frame.tilt_deg - 90.0F) < 0.001F,
          "sequence arrives exactly at neutral pose");
    otto_desk::HeadMotionSequence invalid_sequence(
        {90, 90, 90, 50.0F}, tilt);
    Check(!invalid_sequence.IsValid(), "invalid axis disables sequence");
    frame = invalid_sequence.Advance(1000);
    Check(!frame.moving && frame.pan_deg == 90.0F && frame.tilt_deg == 90.0F,
          "invalid sequence fails closed");

    if (failures != 0) {
        std::fprintf(stderr, "%d head motion test(s) failed\n", failures);
        return 1;
    }
    std::puts("All Otto Desk head motion tests passed.");
    return 0;
}
