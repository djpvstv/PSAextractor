#include "psa/variable_audit.hpp"

#include "psa/arg.hpp"
#include "psa/character_root.hpp"
#include "psa/event_decoder.hpp"
#include "psa/misc_section.hpp"
#include "psa/subaction_flags.hpp"
#include "psa/subaction_table.hpp"
#include "psa/subroutine_scan.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <exception>

namespace psax {

bool event_touches_variable(const Event& e, const VarAuditOptions& opt) {
    for (const auto& a : e.args) {
        if (a.type != ArgType::Variable) continue;
        if (!opt.has_target) return true;
        if (a.raw_value == opt.target_var_raw) return true;
    }
    return false;
}

std::vector<Event> filter_var_events(const std::vector<Event>& events,
                                     const VarAuditOptions& opt) {
    std::vector<Event> out;
    for (const auto& e : events) {
        if (event_touches_variable(e, opt)) out.push_back(e);
    }
    return out;
}

VarAuditReport audit_variables(const MiscSection& ms,
                               const VarAuditOptions& opt) {
    VarAuditReport report;
    const auto root = load_character_root(ms);

    const std::size_t count = subaction_main_count(
        root.fields[CharacterRoot::SubActionMain],
        root.fields[CharacterRoot::SubActionGFX]);
    if (count == 0) return report;

    const auto flags = read_subaction_flags(
        ms, root.fields[CharacterRoot::SubActionFlags], count);

    struct TabDesc { const char* label; CharacterRoot::Field field; };
    const TabDesc tabs[] = {
        {"Main",  CharacterRoot::SubActionMain},
        {"GFX",   CharacterRoot::SubActionGFX},
        {"SFX",   CharacterRoot::SubActionSFX},
        {"Other", CharacterRoot::SubActionOther},
    };

    EventDecoder dec(ms.data(), ms.size());

    // SubAction pass.
    for (std::size_t i = 0; i < count; ++i) {
        for (const auto& tab : tabs) {
            const auto table = read_subaction_table(
                ms, root.fields[tab.field], i + 1);
            if (i >= table.size()) continue;
            const uint32_t stored = table[i];
            if (stored == 0u || stored == 0xFFFFFFFFu) continue;

            std::vector<Event> events;
            try {
                events = dec.decode(resolve_misc_ptr(stored));
            } catch (const std::exception& ex) {
                VarAuditFailure f;
                f.subaction_id = i;
                f.tab_label    = tab.label;
                f.stored_ptr   = stored;
                f.reason       = ex.what();
                report.failures.push_back(std::move(f));
                continue;
            }

            const auto relevant = filter_var_events(events, opt);
            if (relevant.empty()) continue;

            VarAuditEntry entry;
            entry.kind         = VarAuditEntry::InSubAction;
            entry.subaction_id = i;
            entry.tab_label    = tab.label;
            entry.anim_name    = (i < flags.size())
                                    ? subaction_anim_name(ms, flags[i])
                                    : std::string{};
            entry.events       = std::move(relevant);
            report.entries.push_back(std::move(entry));
        }
    }

    // Subroutine pass. collect_subroutines is fault-tolerant internally.
    const auto subs = collect_subroutines(ms);
    for (const auto& s : subs) {
        if (!s.decode_error.empty()) {
            VarAuditFailure f;
            f.tab_label  = "Subroutine";
            f.stored_ptr = s.stored_ptr;
            f.reason     = s.decode_error;
            report.failures.push_back(std::move(f));
            continue;
        }
        const auto relevant = filter_var_events(s.events, opt);
        if (relevant.empty()) continue;

        VarAuditEntry entry;
        entry.kind                   = VarAuditEntry::InSubroutine;
        entry.subroutine_stored_ptr  = s.stored_ptr;
        entry.subroutine_callers     = s.callers;
        entry.events                 = std::move(relevant);
        report.entries.push_back(std::move(entry));
    }

    return report;
}

namespace {
bool starts_with(const char*& s, const char* prefix) {
    const std::size_t n = std::strlen(prefix);
    if (std::strncmp(s, prefix, n) != 0) return false;
    s += n;
    return true;
}
} // namespace

bool parse_variable_descriptor(const char* s, uint32_t& out) {
    if (!s || !*s) return false;

    // Raw hex form: 0xXXXXXXXX
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        char* end = nullptr;
        const unsigned long v = std::strtoul(s + 2, &end, 16);
        if (end == s + 2 || *end != '\0') return false;
        out = static_cast<uint32_t>(v);
        return true;
    }

    // Pretty form: MC-DT[N]
    uint8_t mc = 0;
    if      (starts_with(s, "IC-")) mc = 0;
    else if (starts_with(s, "LA-")) mc = 1;
    else if (starts_with(s, "RA-")) mc = 2;
    else return false;

    uint8_t dt = 0;
    if      (starts_with(s, "Basic")) dt = 0;
    else if (starts_with(s, "Float")) dt = 1;
    else if (starts_with(s, "Bit"))   dt = 2;
    else return false;

    if (*s++ != '[') return false;
    char* end = nullptr;
    const unsigned long idx = std::strtoul(s, &end, 10);
    if (end == s || *end != ']' || *(end + 1) != '\0') return false;
    if (idx > 0x00FFFFFFu) return false;

    out = (static_cast<uint32_t>(mc) << 28)
        | (static_cast<uint32_t>(dt) << 24)
        | (static_cast<uint32_t>(idx) & 0x00FFFFFFu);
    return true;
}

} // namespace psax
