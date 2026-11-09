#include "otto_desk_head_motion.h"

#include <type_traits>

// Compile-time contract checks: these run in every firmware build and guard
// the planner's public API without enabling any physical servo output.
namespace {
static_assert(std::is_class_v<otto_desk::HeadMotionPlanner>);
static_assert(std::is_aggregate_v<otto_desk::HeadAxisLimits>);
}  // namespace
