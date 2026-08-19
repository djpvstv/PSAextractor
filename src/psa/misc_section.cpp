#include "psa/misc_section.hpp"

#include "util/binary_reader.hpp"

#include <stdexcept>

namespace psax {

MiscSection::MiscSection(const uint8_t* data, std::size_t size)
    : data_(data), size_(size) {
    parse_header();
    locate_string_pool();
    parse_tables();
}

void MiscSection::parse_header() {
    if (size_ < MiscHeader::kHeaderSize) {
        throw std::runtime_error("MISC section too small to hold header");
    }
    BinaryReader r(data_, size_);
    header_.file_size = r.read_u32_be();
    header_.word1     = r.read_u32_be();
    header_.word2     = r.read_u32_be();
    header_.word3     = r.read_u32_be();
    header_.word4     = r.read_u32_be();

    if (header_.file_size > size_) {
        throw std::runtime_error("MISC file_size exceeds buffer size");
    }
}

// Confirmed empirically against PSAC screenshots of FitMario.pac:
//   Data Table name at pool offset 0 is "data", which sits at MISC 0x23C84.
//   Ext sub entry 0 is "effectAnimCmd_BatSwing4Common".
// Formula: STRPOOL = word1 + word2*4 + 32 + word3*8 + word4*8
// The +32 gap is an unaccounted-for structure between the lookup table and the
// data table — possibly 8 extra lookup entries or a small sub-header. TBD.
void MiscSection::locate_string_pool() {
    string_pool_start_ =
        std::size_t(header_.word1)
      + std::size_t(header_.word2) * 4
      + 32
      + std::size_t(header_.word3) * 8
      + std::size_t(header_.word4) * 8;
    if (string_pool_start_ > size_) {
        throw std::runtime_error("MISC: derived string_pool_start past buffer");
    }
}

void MiscSection::parse_tables() {
    const uint32_t ext_ct  = header_.external_sub_count();
    const uint32_t data_ct = header_.data_table_count();

    // Extern sub table sits immediately before the string pool.
    ext_table_start_  = string_pool_start_ - std::size_t(ext_ct) * TableEntry::kEntrySize;
    // Data table sits immediately before the extern sub table.
    data_table_start_ = ext_table_start_   - std::size_t(data_ct) * TableEntry::kEntrySize;

    if (data_table_start_ < MiscHeader::kHeaderSize) {
        throw std::runtime_error("MISC: derived table region underflows header");
    }

    auto read_pair = [&](std::size_t off) {
        BinaryReader r(data_ + off, TableEntry::kEntrySize);
        TableEntry e;
        e.data_ref = r.read_u32_be();
        e.name_rel = r.read_u32_be();
        return e;
    };

    data_table_.reserve(data_ct);
    for (uint32_t i = 0; i < data_ct; ++i) {
        data_table_.push_back(read_pair(data_table_start_ + i * TableEntry::kEntrySize));
    }
    external_subs_.reserve(ext_ct);
    for (uint32_t i = 0; i < ext_ct; ++i) {
        external_subs_.push_back(read_pair(ext_table_start_ + i * TableEntry::kEntrySize));
    }
}

std::string_view MiscSection::name_at(uint32_t pool_relative_offset) const {
    const std::size_t base = string_pool_start_ + pool_relative_offset;
    if (base >= size_) {
        throw std::out_of_range("MiscSection::name_at: offset past buffer");
    }
    const char* start = reinterpret_cast<const char*>(data_ + base);
    std::size_t max_len = size_ - base;
    std::size_t len = 0;
    while (len < max_len && start[len] != '\0') ++len;
    return std::string_view(start, len);
}

} // namespace psax
