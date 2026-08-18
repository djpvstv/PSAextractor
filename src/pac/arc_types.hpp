#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace psax {

// Nintendo ARC archive file-type enum (subset relevant to fighter PACs).
// Only MiscData (1) is confirmed by us against BrawlCrate; names 2-7 are the
// commonly-cited BrawlLib values; 8 is unverified — in fighter PACs it appears
// to be the nested ef_<char> effects archive.
enum class ArcFileType : uint16_t {
    None          = 0,
    MiscData      = 1,   // PSA moveset section — the one we care about
    ModelData     = 2,
    TextureData   = 3,
    AnimationData = 4,
    SceneData     = 5,
    Type6         = 6,
    EffectData    = 7,
    Type8         = 8,   // nested ARC in fighter PACs (ef_<char>)
};

const char* arc_file_type_name(ArcFileType t);

struct ArcHeader {
    static constexpr std::size_t kHeaderSize = 0x40;
    uint16_t version    = 0;
    uint16_t node_count = 0;
    std::string name;   // up to 32 bytes, NUL-trimmed
};

struct ArcEntry {
    static constexpr std::size_t kEntryHeaderSize = 0x20;
    ArcFileType file_type      = ArcFileType::None;
    uint16_t    file_index     = 0;
    uint32_t    length         = 0;   // data length in bytes (not counting header)
    uint16_t    group_id       = 0;
    int16_t     redirect_index = 0;   // -1 (0xFFFF) means "none"
    std::size_t data_offset    = 0;   // absolute file offset where data begins
};

} // namespace psax
