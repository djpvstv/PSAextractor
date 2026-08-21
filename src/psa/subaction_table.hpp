#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace psax {

class MiscSection;

// The SubAction<Main|GFX|SFX|Other> arrays are flat lists of u32 event_list_ptr
// values, one per SubAction ID. Array length equals PSAC's SubAction dropdown
// range (typically 0 to 0x1DD, so 478 entries in FitMario).
//
// This changed after our 4-byte-vs-8-byte confusion: PSAC's "Sub Action N"
// index is directly `array[N]`, and the u32 there is the stored event list
// pointer (needs `resolve_misc_ptr` = +32).

// Number of SubActionMain entries, inferred from the gap between the
// SubActionMain and SubActionGFX pointers in the character root struct.
// Returns 0 if either offset is 0 or the gap isn't a multiple of 4.
std::size_t subaction_main_count(uint32_t sub_action_main_stored,
                                 uint32_t sub_action_gfx_stored);

// Read `count` u32 event-list pointers from the array at (stored) offset.
// Empty entries (value 0) are still returned; caller filters.
std::vector<uint32_t>
read_subaction_table(const MiscSection& ms,
                     uint32_t table_start_stored,
                     std::size_t count);

} // namespace psax
