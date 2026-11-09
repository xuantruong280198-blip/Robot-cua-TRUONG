#include "otto_desk_gpio_safety.h"

#include <cassert>

int main() {
    using otto_desk::IsSafeEsp32S3Gpio;
    assert(!IsSafeEsp32S3Gpio(-1));
    assert(!IsSafeEsp32S3Gpio(49));
    for (int gpio : {0, 3, 19, 20, 26, 27, 28, 29, 30, 31, 32,
                     33, 34, 35, 36, 37, 43, 44, 45, 46}) {
        assert(!IsSafeEsp32S3Gpio(gpio));
    }
    for (int gpio : {1, 2, 4, 5, 18, 21, 38, 39, 40, 41, 42, 47, 48}) {
        assert(IsSafeEsp32S3Gpio(gpio));
    }
    return 0;
}
