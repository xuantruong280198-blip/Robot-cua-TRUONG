#include "otto_desk_display_rotation.h"

#include <cassert>

int main() {
    using otto_desk::GetDisplayRotation;
    const auto r0 = GetDisplayRotation(0);
    assert(!r0.swap_xy && !r0.mirror_x && !r0.mirror_y);

    const auto r90 = GetDisplayRotation(90);
    assert(r90.swap_xy && r90.mirror_x && !r90.mirror_y);

    const auto r180 = GetDisplayRotation(180);
    assert(!r180.swap_xy && r180.mirror_x && r180.mirror_y);

    const auto r270 = GetDisplayRotation(270);
    assert(r270.swap_xy && !r270.mirror_x && r270.mirror_y);

    // Invalid values fail closed to the native orientation.
    const auto invalid = GetDisplayRotation(45);
    assert(!invalid.swap_xy && !invalid.mirror_x && !invalid.mirror_y);
    return 0;
}
