#pragma once

#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace ota_version {

// Parse bounded, numeric dotted versions. Malformed server-provided versions
// are rejected instead of allowing std::stoi exceptions to escape OTA checks.
inline bool Parse(std::string_view text, std::vector<uint32_t>& parts) {
    parts.clear();
    if (text.empty() || text.size() > 64) return false;

    size_t start = 0;
    while (start < text.size()) {
        if (parts.size() >= 8) return false;
        const size_t end = text.find('.', start);
        const size_t stop = end == std::string_view::npos ? text.size() : end;
        if (stop == start) return false;

        uint32_t value = 0;
        for (size_t i = start; i < stop; ++i) {
            const char ch = text[i];
            if (ch < '0' || ch > '9') return false;
            const uint32_t digit = static_cast<uint32_t>(ch - '0');
            if (value > (std::numeric_limits<uint32_t>::max() - digit) / 10U) {
                return false;
            }
            value = value * 10U + digit;
        }
        parts.push_back(value);
        if (end == std::string_view::npos) break;
        start = end + 1;
        if (start == text.size()) return false;
    }
    return !parts.empty();
}

// Missing trailing components are zero: 1.2 and 1.2.0 are equivalent.
inline bool IsNewer(std::string_view current, std::string_view candidate) {
    std::vector<uint32_t> current_parts;
    std::vector<uint32_t> candidate_parts;
    if (!Parse(current, current_parts) || !Parse(candidate, candidate_parts)) {
        return false;
    }

    const size_t count = current_parts.size() > candidate_parts.size()
        ? current_parts.size() : candidate_parts.size();
    for (size_t i = 0; i < count; ++i) {
        const uint32_t lhs = i < current_parts.size() ? current_parts[i] : 0U;
        const uint32_t rhs = i < candidate_parts.size() ? candidate_parts[i] : 0U;
        if (rhs != lhs) return rhs > lhs;
    }
    return false;
}

}  // namespace ota_version
