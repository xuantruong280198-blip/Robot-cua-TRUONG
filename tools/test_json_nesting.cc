#include "otto_desk_json_safety.h"

#include <cassert>
#include <string>

int main() {
    using otto_desk::HasSafeJsonNesting;

    assert(HasSafeJsonNesting("{\"a\":[{\"b\":1}]}"));
    assert(HasSafeJsonNesting("{\"text\":\"[[[{{{ }}}]]] \"}"));
    assert(HasSafeJsonNesting("{\"text\":\"escaped quote: \\\" and brace {\"}"));
    assert(HasSafeJsonNesting("{\"a\":0}", 1));
    assert(!HasSafeJsonNesting("{\"a\":[0]}", 1));
    std::string too_deep = "{\"x\":";
    for (int i = 0; i < 33; ++i) too_deep.push_back('[');
    too_deep.push_back('0');
    for (int i = 0; i < 33; ++i) too_deep.push_back(']');
    too_deep.push_back('}');
    assert(!HasSafeJsonNesting(too_deep));
    assert(!HasSafeJsonNesting("{\"a\":[0}"));
    assert(!HasSafeJsonNesting("{\"a\":0"));
    assert(!HasSafeJsonNesting("{\"a\":\"unterminated}"));
    assert(!HasSafeJsonNesting("}"));
    assert(!HasSafeJsonNesting("{]"));
    return 0;
}
