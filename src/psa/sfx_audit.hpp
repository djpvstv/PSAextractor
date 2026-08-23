#pragma once

#include "psa/event.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace psax {

class MiscSection;

// One result group from a sound-effect audit: filtered events for one
// (subaction_id, tab) pair, plus the resolved animation name.
struct SfxAuditEntry {
    std::size_t subaction_id = 0;
    const char* tab_label    = "";   // "Main" / "GFX" / "SFX" / "Other"
    std::string anim_name;
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
//   SoundEffect / SoundEffectTransient / OffensiveCollision /
//   SpecialOffensiveCollision / BasicVariableSet-to-RA-Basic[8..10].
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

// Walk all 4 tabs of every subaction in `ms`. Returns one entry per
// (subaction, tab) that has at least one event after filter_sfx_events().
std::vector<SfxAuditEntry> audit_sfx(const MiscSection& ms,
                                     const SfxAuditOptions& opt = {});

} // namespace psax
