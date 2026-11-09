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
    return 0;
}
