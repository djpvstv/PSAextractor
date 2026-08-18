#include "psa/misc_section.hpp"

#include "util/binary_reader.hpp"

#include <stdexcept>
#include <string_view>

namespace psax {

MiscSection::MiscSection(const uint8_t* data, std::size_t size)
    : data_(data), size_(size) {
    parse_header();
    parse_tables();
}

void MiscSection::parse_header() {
    if (size_ < MiscHeader::kHeaderSize) {
        throw std::runtime_error("MISC section too small to hold header");
    }
    BinaryReader r(data_, size_);
    header_.file_size         = r.read_u32_be();
    header_.data_table_offset = r.read_u32_be();
    header_.extern_sub_offset = r.read_u32_be();
    header_.data_table_count  = r.read_u32_be();
    header_.extern_sub_count  = r.read_u32_be();
    // 12 bytes reserved.

    // Sanity: the header's own file_size must not exceed the buffer we were given.
    if (header_.file_size > size_) {
        throw std::runtime_error("MISC header file_size exceeds buffer size");
    }
}

void MiscSection::parse_tables() {
    // Data table: dataTableCount entries at dataTableOffset.
    const std::size_t data_end =
        std::size_t(header_.data_table_offset) +
        std::size_t(header_.data_table_count) * DataTableEntry::kEntrySize;
    if (data_end > size_) {
        throw std::runtime_error("MISC data table extends past buffer");
    }
    data_table_.reserve(header_.data_table_count);
    for (uint32_t i = 0; i < header_.data_table_count; ++i) {
        const std::size_t off = header_.data_table_offset + i * DataTableEntry::kEntrySize;
        BinaryReader r(data_ + off, DataTableEntry::kEntrySize);
        DataTableEntry e;
        e.data_offset = r.read_u32_be();
        e.name_offset = r.read_u32_be();
        data_table_.push_back(e);
    }

    // External subroutine table.
    const std::size_t ext_end =
        std::size_t(header_.extern_sub_offset) +
        std::size_t(header_.extern_sub_count) * ExternalSubEntry::kEntrySize;
    if (ext_end > size_) {
        throw std::runtime_error("MISC external-sub table extends past buffer");
    }
    external_subs_.reserve(header_.extern_sub_count);
    for (uint32_t i = 0; i < header_.extern_sub_count; ++i) {
        const std::size_t off = header_.extern_sub_offset + i * ExternalSubEntry::kEntrySize;
        BinaryReader r(data_ + off, ExternalSubEntry::kEntrySize);
        ExternalSubEntry e;
        e.data_offset = r.read_u32_be();
        e.name_offset = r.read_u32_be();
        external_subs_.push_back(e);
    }
}

std::string_view MiscSection::string_at(uint32_t misc_offset) const {
    if (misc_offset >= size_) {
        throw std::out_of_range("MiscSection::string_at: offset past buffer");
    }
    const char* start = reinterpret_cast<const char*>(data_ + misc_offset);
    std::size_t max_len = size_ - misc_offset;
    std::size_t len = 0;
    while (len < max_len && start[len] != '\0') ++len;
    return std::string_view(start, len);
}

} // namespace psax
