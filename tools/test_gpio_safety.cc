#include "otto_desk_gpio_safety.h"

#include <cassert>
#include <initializer_list>

int main() {
    using otto_desk::IsSafeEsp32S3Gpio;
    assert(!IsSafeEsp32S3Gpio(-1));
    assert(!IsSafeEsp32S3Gpio(49));
    for (int gpio : {0, 3, 19, 20, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
                     33, 34, 35, 36, 37, 43, 44, 45, 46}) {
        assert(!IsSafeEsp32S3Gpio(gpio));
    }
    for (int gpio : {1, 2, 4, 5, 18, 21, 38, 39, 40, 41, 42, 47, 48}) {
        assert(IsSafeEsp32S3Gpio(gpio));
    }

    using otto_desk::IsValidI2sPinAssignment;
    static_assert(IsValidI2sPinAssignment(4, 5, 6, 7, 8, 9, 16000, 24000));
    static_assert(!IsValidI2sPinAssignment(4, 5, 6, 7, 8, 9, 0, 24000));
    static_assert(!IsValidI2sPinAssignment(4, 5, 6, 7, 8, 9, 16000, -1));
    static_assert(!IsValidI2sPinAssignment(4, 5, 6, 7, 8, 8, 16000, 24000));
    static_assert(!IsValidI2sPinAssignment(4, 5, 6, 7, 8, 43, 16000, 24000));
    static_assert(!IsValidI2sPinAssignment(4, 5, 6, 7, 8, 26, 16000, 24000));
    assert(IsValidI2sPinAssignment(40, 41, 42, 38, 39, 47, 16000, 24000));
    return 0;
}
