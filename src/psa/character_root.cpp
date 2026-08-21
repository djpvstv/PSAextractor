#include "psa/character_root.hpp"

#include "psa/event_decoder.hpp"   // resolve_misc_ptr
#include "psa/misc_section.hpp"
#include "util/binary_reader.hpp"

#include <stdexcept>

namespace psax {

namespace {
constexpr const char* kFieldNames[CharacterRoot::kFieldCount] = {
    "SubActionFlags", "ModelVisibility", "Attributes", "SSEAttributes",
    "MiscSection", "CommonActionFlags", "SpecialActionFlags", "ExtraActionFlags",
    "ActionInterrupts", "EntrySpecials", "ExitSpecials", "ActionPre",
    "SubActionMain", "SubActionGFX", "SubActionSFX", "SubActionOther",
    "BoneFloats1", "BoneFloats2", "BoneReferences", "HandBones",
    "EntryActionOverride", "ExitActionOverride", "ExtraActionInterrupts",
    "BoneFloats3", "Unknown24", "StaticArticles", "EntryArticle",
    "DataFlags0", "DataFlags1", "DataFlags2", "DataFlags3",
};
} // namespace

const char* CharacterRoot::name_of(Field f) {
    return (f < kFieldCount) ? kFieldNames[f] : "?";
}

const char* CharacterRoot::field_name(Field f) const { return name_of(f); }

CharacterRoot load_character_root(const MiscSection& ms) {
    if (ms.data_table().empty()) {
        throw std::runtime_error("character root: MISC has no data table entries");
    }
    const uint32_t data_ref_stored = ms.data_table()[0].data_ref;
    const std::size_t root_off = resolve_misc_ptr(data_ref_stored);
    if (root_off + CharacterRoot::kStructBytes > ms.size()) {
        throw std::runtime_error("character root: struct extends past MISC end");
    }
    CharacterRoot r;
    BinaryReader br(ms.data() + root_off, CharacterRoot::kStructBytes);
    for (std::size_t i = 0; i < CharacterRoot::kFieldCount; ++i) {
        r.fields[i] = br.read_u32_be();
    }
    return r;
}

} // namespace psax
