#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace psax {

class MiscSection;

// Fixed-layout struct that each character PAC exports as its "data" symbol.
// Field names + order taken from PSAC's Data > Data Offset tab. The struct is
// 27 offset u32s + 4 flag u32s = 124 bytes.
//
// All offset fields are stored PSA pointers (need `resolve_misc_ptr` to reach
// the actual MISC offset). A value of 0 means "unused".
struct CharacterRoot {
    // Byte offset within the struct is index * 4. Ordering matches PSAC.
    enum Field : std::size_t {
        SubActionFlags        = 0,
        ModelVisibility       = 1,
        Attributes            = 2,
        SSEAttributes         = 3,
        MiscSectionOffset     = 4,
        CommonActionFlags     = 5,
        SpecialActionFlags    = 6,
        ExtraActionFlags      = 7,
        ActionInterrupts      = 8,
        EntrySpecials         = 9,
        ExitSpecials          = 10,
        ActionPre             = 11,
        SubActionMain         = 12,
        SubActionGFX          = 13,
        SubActionSFX          = 14,
        SubActionOther        = 15,
        BoneFloats1           = 16,
        BoneFloats2           = 17,
        BoneReferences        = 18,
        HandBones             = 19,
        EntryActionOverride   = 20,
        ExitActionOverride    = 21,
        ExtraActionInterrupts = 22,
        BoneFloats3           = 23,
        Unknown24             = 24,
        StaticArticles        = 25,
        EntryArticle          = 26,
        // The 4 DataFlags words follow (indices 27..30).
        DataFlags0            = 27,
        DataFlags1            = 28,
        DataFlags2            = 29,
        DataFlags3            = 30,
        kFieldCount           = 31,
    };

    static constexpr std::size_t kStructBytes = kFieldCount * 4;

    uint32_t fields[kFieldCount];

    const char* field_name(Field f) const;
    static const char* name_of(Field f);
};

// Load the character root struct from a MISC section. The "data" export in
// the data table is assumed to point (via +32 resolution) at this struct.
// Throws if there is no "data" export or the struct doesn't fit.
CharacterRoot load_character_root(const MiscSection& ms);

} // namespace psax
