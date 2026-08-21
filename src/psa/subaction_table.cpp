#include "psa/subaction_table.hpp"

#include "psa/event_decoder.hpp"
#include "psa/misc_section.hpp"
#include "util/binary_reader.hpp"

#include <stdexcept>

namespace psax {

std::size_t subaction_main_count(uint32_t sub_action_main_stored,
                                 uint32_t sub_action_gfx_stored) {
    if (sub_action_main_stored == 0 || sub_action_gfx_stored == 0) return 0;
    if (sub_action_gfx_stored <= sub_action_main_stored) return 0;
    const uint32_t span = sub_action_gfx_stored - sub_action_main_stored;
    if (span % 4u != 0u) return 0;
    return span / 4u;
}

std::vector<uint32_t>
read_subaction_table(const MiscSection& ms,
                     uint32_t table_start_stored,
                     std::size_t count) {
    std::vector<uint32_t> out;
    if (count == 0 || table_start_stored == 0) return out;

    const std::size_t start = resolve_misc_ptr(table_start_stored);
    const std::size_t need  = count * 4u;
    if (start + need > ms.size()) {
        throw std::runtime_error("subaction table extends past MISC end");
    }
    out.reserve(count);
    BinaryReader r(ms.data() + start, need);
    for (std::size_t i = 0; i < count; ++i) {
        out.push_back(r.read_u32_be());
    }
    return out;
}

} // namespace psax
