#include "psa/event.hpp"

#include "psa/argument_schema.hpp"
#include "psa/command_table.hpp"

#include <cstdio>
#include <cstring>

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

namespace {

// Expand a `{N}` template using the args as substitution values. Missing
// placeholders (index >= args.size()) render as `{?}`.
std::string apply_format(const char* fmt, const std::vector<Arg>& args) {
    std::string out;
    while (*fmt) {
        if (fmt[0] == '{') {
            // Find closing brace, parse an unsigned decimal index in between.
            const char* p = fmt + 1;
            std::size_t idx = 0;
            bool ok = false;
            while (*p >= '0' && *p <= '9') {
                idx = idx * 10 + std::size_t(*p - '0');
                ++p;
                ok = true;
            }
            if (ok && *p == '}') {
                if (idx < args.size()) out += args[idx].to_pretty_string();
                else                   out += "{?}";
                fmt = p + 1;
                continue;
            }
            // Not a well-formed placeholder - pass the '{' through literally.
        }
        out += *fmt++;
    }
    return out;
}

} // namespace

std::string Event::to_pretty_string() const {
    // Legacy mode: bare "LegacyName(args)" — matches historical psax output.
    // Dotted mode: "module.method(args)", or "method(args)" when the top
    // byte has no assigned module (0x00 flow, 0x01 loopRest). Unknown
    // cmd_ids fall back to "unknown_XXXXXXXX(args)" so grep-ability holds.
    const char* name = command_name(cmd_id);
    char head[32];
    std::string out;
    if (is_legacy_display_mode()) {
        if (name) {
            out = name;
        } else {
            std::snprintf(head, sizeof(head), "Unknown_%08X", cmd_id);
            out = head;
        }
    } else {
        if (const char* mod = command_module_shorthand(cmd_id)) {
            out = mod;
            out += '.';
        }
        if (name) {
            out += name;
        } else {
            std::snprintf(head, sizeof(head), "unknown_%08X", cmd_id);
            out += head;
        }
    }
    out += '(';
    if (const char* fmt = command_format(cmd_id)) {
        // Explicit per-command format string wins (handles reordering / equals-signs).
        out += apply_format(fmt, args);
    } else {
        // Default: comma-separated args. If we have named-arg metadata for this
        // command (from BrawlCrate's Parameters.txt), render as "name=value".
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i > 0) out += ", ";
            if (const char* aname = command_arg_name(cmd_id,
                                                    static_cast<uint32_t>(i))) {
                out += aname;
                out += '=';
            }
            out += args[i].to_pretty_string();
        }
    }
    out += ')';
    return out;
}

} // namespace psax
