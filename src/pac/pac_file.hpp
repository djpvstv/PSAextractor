#pragma once

#include "pac/arc_types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace psax {

// Parses a Brawl fighter .pac (an ARC archive) and exposes its entries.
// Header + entry parsing runs at load time; entry data is a view into the buffer.
class PacFile {
public:
    static PacFile load(const std::string& path);

    const ArcHeader& header() const { return header_; }
    const std::vector<ArcEntry>& entries() const { return entries_; }

    // First MiscData entry (holds the PSA moveset), or nullopt if not present.
    std::optional<ArcEntry> find_misc_data() const;

    // Raw pointer to an entry's data (length = entry.length).
    const uint8_t* entry_data(const ArcEntry& e) const;

    std::size_t size() const { return bytes_.size(); }
    const std::vector<uint8_t>& bytes() const { return bytes_; }

private:
    void parse_header();
    void parse_entries();

    std::vector<uint8_t> bytes_;
    ArcHeader header_;
    std::vector<ArcEntry> entries_;
};

} // namespace psax
