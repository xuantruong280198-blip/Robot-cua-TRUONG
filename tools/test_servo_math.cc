#include "otto_desk_servo_math.h"
#include <cassert>
using namespace otto_desk;
int main() {
    static_assert(ServoPulseUsForAngle(0) == 500);
    static_assert(ServoPulseUsForAngle(90) == 1500);
    static_assert(ServoPulseUsForAngle(180) == 2500);
    static_assert(ServoPulseUsForAngle(-30) == 500);
    static_assert(ServoPulseUsForAngle(240) == 2500);
    static_assert(ServoDutyForAngle(0) == (500U * kServoDutyMaximum) / 20000U);
    static_assert(ServoDutyForAngle(90) == (1500U * kServoDutyMaximum) / 20000U);
    static_assert(ServoDutyForAngle(180) == (2500U * kServoDutyMaximum) / 20000U);
    assert(ServoDutyForAngle(-1) == ServoDutyForAngle(0));
    assert(ServoDutyForAngle(181) == ServoDutyForAngle(180));
    // Per-axis mechanical limits must be enforced before PWM conversion.
    static_assert(ClampServoAngle(0, 20, 160) == 20);
    static_assert(ClampServoAngle(90, 20, 160) == 90);
    static_assert(ClampServoAngle(180, 20, 160) == 160);
    static_assert(ServoPulseUsForAngle(0, 20, 160) == 722);
    static_assert(ServoPulseUsForAngle(180, 20, 160) == 2277);
    static_assert(ServoDutyForAngle(0, 20, 160) ==
                  (722U * kServoDutyMaximum) / kServoPeriodUs);
    static_assert(ClampServoAngle(180, 0, 180) == 180);
    // Invalid limits do not create an out-of-range pulse.
    static_assert(ClampServoAngle(180, 160, 20) == 180);
    return 0;
}
