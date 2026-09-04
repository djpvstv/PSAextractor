#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace psax {

// The MISC section is the payload of an ARC MiscData entry - it holds the
// entire PSA moveset. The first 32 bytes are a header. Field semantics for
// two of the five u32s (word1, word2) are still being reverse-engineered.
struct MiscHeader {
    static constexpr std::size_t kHeaderSize = 0x20;

    uint32_t file_size = 0;   // matches ARC entry length; CONFIRMED
    uint32_t word1     = 0;   // large offset (0x1EA64 for FitMario); role TBD
    uint32_t word2     = 0;   // medium offset (0x13E0 for FitMario); role TBD
    uint32_t word3     = 0;   // # of data-table entries ("exports") - CONFIRMED via pattern
    uint32_t word4     = 0;   // # of external-sub entries ("imports")

    // Convenience aliases with our best-guess semantics.
    uint32_t data_table_count()     const { return word3; }
    uint32_t external_sub_count()   const { return word4; }
};

// One entry in either the data table or the external subroutine table.
// Layout: (u32 data_ref, u32 name_offset_relative_to_string_pool).
struct TableEntry {
    static constexpr std::size_t kEntrySize = 0x08;
    uint32_t data_ref = 0;   // MISC-relative offset to the referenced code / data
    uint32_t name_rel = 0;   // offset from string pool start to a NUL-terminated name
};

class MiscSection {
public:
    MiscSection(const uint8_t* data, std::size_t size);

    const MiscHeader& header() const { return header_; }

    // MISC offset where the string pool begins (found by heuristic scan).
    std::size_t string_pool_start() const { return string_pool_start_; }

    // MISC offset where the ext sub table begins (= string_pool_start - N*8).
    std::size_t external_sub_table_start() const { return ext_table_start_; }

    // MISC offset where the data table begins (immediately before ext table).
    std::size_t data_table_start() const { return data_table_start_; }

    const std::vector<TableEntry>& data_table() const { return data_table_; }
    const std::vector<TableEntry>& external_subs() const { return external_subs_; }

    // Resolve a string_pool-relative offset to a NUL-terminated name.
    std::string_view name_at(uint32_t pool_relative_offset) const;

    const uint8_t* data() const { return data_; }
    std::size_t size() const { return size_; }

private:
    void parse_header();
    void locate_string_pool();
    void parse_tables();

    const uint8_t* data_;
    std::size_t size_;
    MiscHeader header_;
    std::size_t string_pool_start_ = 0;
    std::size_t ext_table_start_   = 0;
    std::size_t data_table_start_  = 0;
    std::vector<TableEntry> data_table_;
    std::vector<TableEntry> external_subs_;
};

} // namespace psax
