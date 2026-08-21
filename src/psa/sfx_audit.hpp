#pragma once

#include "psa/event.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace psax {

class MiscSection;

// One result group from a sound-effect audit: all filtered events for one
// (subaction_id, tab) pair, plus the resolved animation name.
struct SfxAuditEntry {
    std::size_t subaction_id = 0;
    const char* tab_label    = "";   // "Main" / "GFX" / "SFX" / "Other"
    std::string anim_name;           // resolved via SubActionFlags[i].anim_name_ptr
    std::vector<Event> events;       // filtered per event_is_sfx_relevant()
};

// Predicate: does this event contribute to sound-effect creation?
// True for:
//   - SoundEffect (0x0A000100), SoundEffectTransient (0x0A030100)
//   - OffensiveCollision (0x06000D00), SpecialOffensiveCollision (0x06010200)
//   - BasicVariableSet (0x12000200) whose target variable is RA-Basic[8..10]
//     (character-hitbox custom sound registers)
bool event_is_sfx_relevant(const Event& e);

// Walk all 4 tabs of every subaction in `ms`. Returns one entry per
// (subaction, tab) that contains at least one relevant event.
std::vector<SfxAuditEntry> audit_sfx(const MiscSection& ms);

} // namespace psax
