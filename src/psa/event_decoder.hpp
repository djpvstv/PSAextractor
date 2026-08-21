#pragma once

#include "psa/event.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace psax {

// Convert a stored PSA pointer (as read from within MISC data) to an actual
// MISC-relative offset. All in-MISC pointers use the convention
// `actual = stored + 32` (they are relative to end-of-MISC-header).
inline std::size_t resolve_misc_ptr(uint32_t stored) {
    return static_cast<std::size_t>(stored) + 32u;
}

class EventDecoder {
public:
    EventDecoder(const uint8_t* misc_data, std::size_t misc_size);

    // Decode events from a RESOLVED MISC offset (already includes the +32).
    // Reads (cmd_id, args_ptr) pairs until a (0, 0) terminator. Arg count
    // is derived from the cmd_id itself, so unknown commands still decode
    // their args correctly.
    std::vector<Event> decode(std::size_t event_list_offset) const;

private:
    const uint8_t* data_;
    std::size_t    size_;
};

} // namespace psax
