#include "psa/event.hpp"

#include "psa/command_table.hpp"

#include <cstdio>

namespace psax {

std::string Event::to_raw_string() const {
    char head[32];
    std::snprintf(head, sizeof(head), "E=%08X:", cmd_id);
    std::string out(head);
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i > 0) out += ',';
        out += args[i].to_raw_string();
    }
    return out;
}

std::string Event::to_pretty_string() const {
    const char* name = command_name(cmd_id);
    char head[32];
    std::string out;
    if (name) {
        out = name;
    } else {
        std::snprintf(head, sizeof(head), "Unknown_%08X", cmd_id);
        out = head;
    }
    out += '(';
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i > 0) out += ", ";
        out += args[i].to_pretty_string();
    }
    out += ')';
    return out;
}

} // namespace psax
