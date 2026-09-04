#pragma once

#include "psa/event.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace psax {

class MiscSection;

// If `e` invokes a subroutine-like target (SubRoutine, Goto,
// ConcurrentInfiniteLoop), returns the stored MISC pointer arg the game
// would jump to. Returns 0 otherwise. The target still needs
// `resolve_misc_ptr` (+32) to reach the actual event list.
uint32_t event_subroutine_target(const Event& e);

// One place a discovered subroutine is invoked from.
struct SubroutineCallSite {
    enum Kind { FromSubAction, FromSubroutine };
    Kind kind;

    // FromSubAction: which subaction slot + which tab.
    std::size_t subaction_id = 0;
    const char* tab_label    = "";   // "Main" | "GFX" | "SFX" | "Other"

    // FromSubroutine: which subroutine (by stored ptr) invoked us.
    uint32_t caller_stored_ptr = 0;

    // Common: index of the calling event within its own event list.
    std::size_t event_index = 0;
};

struct DiscoveredSubroutine {
    uint32_t    stored_ptr      = 0;   // as it appears in caller args
    std::size_t resolved_offset = 0;   // actual MISC offset (stored + 32)
    std::vector<Event> events;         // decoded body (may be empty on error)
    std::vector<SubroutineCallSite> callers;
    std::string decode_error;          // populated if decoding failed
};

// Walk every SubAction Main/GFX/SFX/Other event list, follow all subroutine
// calls transitively, and return one record per unique subroutine offset.
// Fault-tolerant - bad event lists are recorded via `decode_error` and skipped.
std::vector<DiscoveredSubroutine> collect_subroutines(const MiscSection& ms);

} // namespace psax
