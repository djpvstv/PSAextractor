#pragma once

#include "psa/event.hpp"
#include "psa/subroutine_scan.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace psax {

class MiscSection;

// One result group from a sound-effect audit: filtered events for one
// location. Subactions and subroutines share this type — check `kind`.
struct SfxAuditEntry {
    enum LocationKind { InSubAction, InSubroutine };
    LocationKind kind = InSubAction;

    // For InSubAction:
    std::size_t subaction_id = 0;
    const char* tab_label    = "";   // "Main" / "GFX" / "SFX" / "Other"
    std::string anim_name;

    // For InSubroutine:
    uint32_t subroutine_stored_ptr = 0;
    std::vector<SubroutineCallSite> subroutine_callers;

    std::vector<Event> events;       // filtered per filter_sfx_events()
};

// Optional inclusive range filter applied to SoundEffect / SoundEffectTransient
// sound IDs (the first arg's raw value). Defaults let everything through.
// Collisions and RA-Basic writes are unaffected.
struct SfxAuditOptions {
    uint32_t min_sound_id = 0u;
    uint32_t max_sound_id = 0xFFFFFFFFu;
};

// True if `e` is one of the 5 command IDs the SFX audit ever considers:
//   SoundEffect (0x0A000100) / SoundEffectTransient (0x0A030100) /
//   OffensiveCollision (0x06000D00) / SpecialOffensiveCollision (0x06150F00) /
//   BasicVariableSet (0x12000200) targeting RA-Basic[8..10].
// This is CONTEXT-FREE; it doesn't apply the "collisions only count when
// paired with an RA-Basic write" rule — that's filter_sfx_events().
bool event_is_sfx_candidate(const Event& e);

// Apply the audit's per-subaction filter to one event list. Rules:
//   - SoundEffect / SoundEffectTransient  -> included if sound ID is within
//     [opt.min_sound_id, opt.max_sound_id] (inclusive on both sides).
//   - BasicVariableSet targeting RA-Basic[8..10]  -> always included.
//   - OffensiveCollision / SpecialOffensiveCollision  -> included ONLY if
//     the same event list contains at least one RA-Basic[8..10] write.
std::vector<Event> filter_sfx_events(const std::vector<Event>& events,
                                     const SfxAuditOptions& opt = {});

// One record of a subaction/tab whose event stream failed to decode
// (usually because the file's structure differs from a normal fighter PAC
// and the event_list_ptr didn't point at a valid event list).
struct SfxAuditFailure {
    std::size_t subaction_id = 0;
    const char* tab_label    = "";
    uint32_t    stored_ptr   = 0;
    std::string reason;      // exception message from EventDecoder
};

// Composite return type so callers know how much coverage they got.
struct SfxAuditReport {
    std::vector<SfxAuditEntry>   entries;
    std::vector<SfxAuditFailure> failures;
};

// Walk all 4 tabs of every subaction in `ms` and every reachable subroutine.
// Any location whose event stream decode throws is captured in `failures`
// and skipped rather than aborting the audit — special/article files often
// have layouts where not every SubAction<X> slot is a valid event list.
// Subactions are added first (subaction-major); subroutines follow, sorted
// by resolved MISC offset.
SfxAuditReport audit_sfx(const MiscSection& ms,
                         const SfxAuditOptions& opt = {});

} // namespace psax
