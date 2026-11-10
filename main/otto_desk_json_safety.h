#pragma once

#include <cstddef>
#include <string>

namespace otto_desk {

// Reject excessive or mismatched container nesting before invoking a
// recursive JSON parser. Braces inside quoted strings are ignored.
inline bool HasSafeJsonNesting(const std::string& json,
                               std::size_t max_depth = 32) {
    std::string expected_closers;
    expected_closers.reserve(json.size() < max_depth ? json.size() : max_depth);
    bool in_string = false;
    bool escaped = false;

    for (const char ch : json) {
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }

        if (ch == '"') {
            in_string = true;
        } else if (ch == '{' || ch == '[') {
            if (expected_closers.size() >= max_depth) return false;
            expected_closers.push_back(ch == '{' ? '}' : ']');
        } else if (ch == '}' || ch == ']') {
            if (expected_closers.empty() || expected_closers.back() != ch) {
                return false;
            }
            expected_closers.pop_back();
        }
    }

    return expected_closers.empty() && !in_string;
}

}  // namespace otto_desk
