#pragma once

#include "psa/event.hpp"
#include "psa/subroutine_scan.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace psax {

class MiscSection;

// One result group from a variable-usage audit: filtered events for one
// location. Mirrors SfxAuditEntry so downstream printing is uniform.
struct VarAuditEntry {
    enum LocationKind { InSubAction, InSubroutine };
    LocationKind kind = InSubAction;

    // InSubAction:
    std::size_t subaction_id = 0;
    const char* tab_label    = "";
    std::string anim_name;

    // InSubroutine:
    uint32_t subroutine_stored_ptr = 0;
    std::vector<SubroutineCallSite> subroutine_callers;

    std::vector<Event> events;
};

// Optional filter: only include events that touch this specific variable.
// If has_target is false, every event with any Variable arg is included.
struct VarAuditOptions {
    uint32_t target_var_raw = 0;      // e.g. 0x20000008 for RA-Basic[8]
    bool     has_target     = false;
};

struct VarAuditFailure {
    std::size_t subaction_id = 0;
    const char* tab_label    = "";
    uint32_t    stored_ptr   = 0;
    std::string reason;
};

struct VarAuditReport {
    std::vector<VarAuditEntry>   entries;
    std::vector<VarAuditFailure> failures;
};

// True if `e` carries at least one Variable-typed arg (type 5). When
// opt.has_target is set, the arg's raw value must also match.
bool event_touches_variable(const Event& e, const VarAuditOptions& opt = {});

// Apply the variable filter to one event list.
std::vector<Event> filter_var_events(const std::vector<Event>& events,
                                     const VarAuditOptions& opt = {});

// Walk every SubAction tab + every reachable subroutine, keep events that
// touch variables (optionally a specific one). Fault-tolerant.
VarAuditReport audit_variables(const MiscSection& ms,
                               const VarAuditOptions& opt = {});

// Parse either a raw hex form ("0x20000008") or the DSL pretty form
// ("RA-Basic[8]", "IC-Basic[20003]", "RA-Float[5]", "RA-Bit[10]") into a
// raw variable descriptor. Returns false on malformed input.
bool parse_variable_descriptor(const char* s, uint32_t& out);

} // namespace psax
