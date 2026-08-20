#include "psa/event.hpp"

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

} // namespace psax
