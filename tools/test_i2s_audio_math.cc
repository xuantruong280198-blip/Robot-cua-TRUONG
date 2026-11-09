#include "otto_i2s_audio_math.h"

#include <cassert>
#include <cstdint>
#include <limits>

using namespace otto_desk;

int main() {
    static_assert(Inmp441SlotToPcm16(0x7FFF0000) == 32767);
    static_assert(Inmp441SlotToPcm16(0x80000000) == -32768);
    static_assert(Inmp441SlotToPcm16(0x00010000) == 1);
    static_assert(Inmp441SlotToPcm16(static_cast<int32_t>(0xFFFF0000U)) == -1);
    static_assert(ScalePcm16(1000, 100) == 1000);
    static_assert(ScalePcm16(1000, 50) == 500);
    static_assert(ScalePcm16(-1000, 50) == -500);
    static_assert(ScalePcm16(1000, 0) == 0);
    static_assert(ScalePcm16(1000, 200) == 1000);
    static_assert(ScalePcm16(-1000, -20) == 0);
    assert(ScalePcm16(std::numeric_limits<int16_t>::min(), 100) ==
           std::numeric_limits<int16_t>::min());
    assert(ScalePcm16(std::numeric_limits<int16_t>::max(), 100) ==
           std::numeric_limits<int16_t>::max());
    return 0;
}
