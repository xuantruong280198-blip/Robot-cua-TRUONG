#include "ota_version.h"

#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>

int main() {
    using ota_version::IsNewer;
    std::vector<uint32_t> parts;

    assert(ota_version::Parse("1.2.3", parts));
    assert((parts == std::vector<uint32_t>{1, 2, 3}));
    assert(!ota_version::Parse("", parts));
    assert(!ota_version::Parse("1..2", parts));
    assert(!ota_version::Parse("1.2.", parts));
    assert(!ota_version::Parse("v1.2.3", parts));
    assert(!ota_version::Parse("1.2-beta", parts));
    assert(!ota_version::Parse("4294967296", parts));
    assert(!ota_version::Parse("1.2.3.4.5.6.7.8.9", parts));
    assert(!ota_version::Parse("12345678901234567890123456789012345678901234567890123456789012345", parts));

    assert(IsNewer("1.2.3", "1.2.4"));
    assert(IsNewer("1.2", "1.3"));
    assert(IsNewer("1.2", "1.2.1"));
    assert(!IsNewer("1.2", "1.2.0"));
    assert(!IsNewer("1.2.0", "1.2"));
    assert(!IsNewer("1.2.3", "1.2.3"));
    assert(!IsNewer("2.0.0", "1.99.99"));
    assert(!IsNewer("broken", "9.9.9"));
    assert(!IsNewer("1.0.0", "broken"));
    assert(!IsNewer("1.0.0", "4294967296"));
    return 0;
}
