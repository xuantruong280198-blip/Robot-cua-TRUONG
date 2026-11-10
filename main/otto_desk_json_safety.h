#pragma once

#include <cstddef>
#include <string>

namespace otto_desk {

// Reject excessive or structurally unbalanced container depth before invoking
// a recursive JSON parser. Braces inside quoted strings are ignored; JSON
// syntax and matching bracket types are checked by the JSON parser afterward.
inline bool HasSafeJsonNesting(const std::string& json,
                               std::size_t max_depth = 32) {
    std::size_t depth = 0;
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
            if (depth == max_depth) return false;
            ++depth;
        } else if (ch == '}' || ch == ']') {
            if (depth == 0) return false;
            --depth;
        }
    }

    return depth == 0 && !in_string && !escaped;
}

}  // namespace otto_desk
