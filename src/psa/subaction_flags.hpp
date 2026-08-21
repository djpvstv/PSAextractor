#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace psax {

class MiscSection;

// One entry in the SubActionFlags array (8 bytes: flags + anim_name_ptr).
// Verified against PSAC: entry [0x10] for FitMario resolves to "RunBrake",
// matching the animation shown in PSAC's Sub Actions tab.
struct SubActionFlagsEntry {
    uint32_t flags         = 0;   // raw flag bits (semantics TBD per-bit)
    uint32_t anim_name_ptr = 0;   // stored form; +32 to resolve
};

// Read `count` entries starting from `table_start_stored` (typical value comes
// from CharacterRoot.fields[SubActionFlags]). Empty entries (both fields zero)
// are still returned; caller filters.
std::vector<SubActionFlagsEntry> read_subaction_flags(
    const MiscSection& ms, uint32_t table_start_stored, std::size_t count);

// Resolve an entry's animation name, e.g. "RunBrake". Returns empty string
// if anim_name_ptr is 0 or the resolved offset is out of range.
std::string subaction_anim_name(const MiscSection& ms,
                                const SubActionFlagsEntry& e);

} // namespace psax
