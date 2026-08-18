#include "pac/pac_file.hpp"

#include "util/binary_reader.hpp"

#include <stdexcept>

namespace psax {

namespace {
constexpr uint32_t kArcMagic  = 0x41524300u; // "ARC\0"
constexpr std::size_t kEntryAlign = 0x20;

std::size_t align_up(std::size_t v, std::size_t a) {
    return (v + (a - 1)) & ~(a - 1);
}
} // namespace

PacFile PacFile::load(const std::string& path) {
    PacFile p;
    p.bytes_ = load_file(path);
    p.parse_header();
    p.parse_entries();
    return p;
}

void PacFile::parse_header() {
    if (bytes_.size() < ArcHeader::kHeaderSize) {
        throw std::runtime_error("PAC file too small to contain ARC header");
    }
    BinaryReader r(bytes_.data(), bytes_.size());
    const uint32_t magic = r.read_u32_be();
    if (magic != kArcMagic) {
        throw std::runtime_error("PAC file: bad magic (expected 'ARC\\0')");
    }
    header_.version    = r.read_u16_be();
    header_.node_count = r.read_u16_be();
    auto name_view = r.read_cstring_at(0x10);
    header_.name.assign(name_view.begin(), name_view.end());
}

void PacFile::parse_entries() {
    entries_.clear();
    entries_.reserve(header_.node_count);

    std::size_t offset = ArcHeader::kHeaderSize;
    for (uint16_t i = 0; i < header_.node_count; ++i) {
        if (offset + ArcEntry::kEntryHeaderSize > bytes_.size()) {
            throw std::runtime_error("PAC file: entry header past end of file");
        }
        BinaryReader r(bytes_.data() + offset, ArcEntry::kEntryHeaderSize);
        ArcEntry e;
        e.file_type      = static_cast<ArcFileType>(r.read_u16_be());
        e.file_index     = r.read_u16_be();
        e.length         = r.read_u32_be();
        e.group_id       = r.read_u16_be();
        e.redirect_index = static_cast<int16_t>(r.read_u16_be());
        e.data_offset    = offset + ArcEntry::kEntryHeaderSize;

        if (e.data_offset + e.length > bytes_.size()) {
            throw std::runtime_error("PAC file: entry data past end of file");
        }
        entries_.push_back(e);
        offset = align_up(e.data_offset + e.length, kEntryAlign);
    }
}

std::optional<ArcEntry> PacFile::find_misc_data() const {
    for (const auto& e : entries_) {
        if (e.file_type == ArcFileType::MiscData) return e;
    }
    return std::nullopt;
}

const uint8_t* PacFile::entry_data(const ArcEntry& e) const {
    if (e.data_offset + e.length > bytes_.size()) {
        throw std::out_of_range("PacFile::entry_data: entry out of bounds");
    }
    return bytes_.data() + e.data_offset;
}

} // namespace psax
