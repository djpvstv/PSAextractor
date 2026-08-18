#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace psax {

// The MISC section is the payload of an ARC MiscData entry. It holds the
// entire PSA moveset: actions, subactions, subroutines, articles, plus the
// data table (this file's exports) and external subroutine table (imports
// resolved against Fighter.pac at runtime).
//
// The section starts with a 32-byte header. All offsets are relative to the
// start of the MISC section (not the outer PAC file).

struct MiscHeader {
    static constexpr std::size_t kHeaderSize = 0x20;

    uint32_t file_size            = 0; // matches ARC entry length
    uint32_t data_table_offset    = 0; // where the data table (exports) starts
    uint32_t extern_sub_offset    = 0; // where the external-subroutine table starts
    uint32_t data_table_count     = 0; // # of exported entries
    uint32_t extern_sub_count     = 0; // # of external references (0 for Fighter.pac)
    // 12 bytes reserved / padding follow.
};

// One entry in the data table: a named export.
struct DataTableEntry {
    static constexpr std::size_t kEntrySize = 0x08;
    uint32_t data_offset = 0;   // MISC-relative offset to the exported data
    uint32_t name_offset = 0;   // MISC-relative offset to a NUL-terminated name string
};

// One entry in the external-subroutine table: an import from Fighter.pac.
struct ExternalSubEntry {
    static constexpr std::size_t kEntrySize = 0x08;
    uint32_t data_offset = 0;   // MISC-relative offset where this import is REFERENCED
    uint32_t name_offset = 0;   // MISC-relative offset to the imported symbol name
};

class MiscSection {
public:
    // `data` must point to the MISC section start; `size` must be at least
    // the MISC file_size. `data` is not copied — MiscSection is a view.
    MiscSection(const uint8_t* data, std::size_t size);

    const MiscHeader& header() const { return header_; }
    const std::vector<DataTableEntry>& data_table() const { return data_table_; }
    const std::vector<ExternalSubEntry>& external_subs() const { return external_subs_; }

    // Resolve a MISC-relative offset to a NUL-terminated string.
    // Throws if the offset is out of range.
    std::string_view string_at(uint32_t misc_offset) const;

    const uint8_t* data() const { return data_; }
    std::size_t size() const { return size_; }

private:
    void parse_header();
    void parse_tables();

    const uint8_t* data_;
    std::size_t size_;
    MiscHeader header_;
    std::vector<DataTableEntry> data_table_;
    std::vector<ExternalSubEntry> external_subs_;
};

} // namespace psax
