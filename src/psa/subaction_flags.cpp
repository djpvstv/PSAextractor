#include "psa/subaction_flags.hpp"

#include "psa/event_decoder.hpp"
#include "psa/misc_section.hpp"
#include "util/binary_reader.hpp"

#include <stdexcept>

namespace psax {

std::vector<SubActionFlagsEntry> read_subaction_flags(
    const MiscSection& ms, uint32_t table_start_stored, std::size_t count) {
    std::vector<SubActionFlagsEntry> out;
    if (count == 0 || table_start_stored == 0) return out;

    const std::size_t start = resolve_misc_ptr(table_start_stored);
    const std::size_t need  = count * 8u;
    if (start + need > ms.size()) {
        throw std::runtime_error("SubActionFlags table extends past MISC end");
    }
    out.reserve(count);
    BinaryReader r(ms.data() + start, need);
    for (std::size_t i = 0; i < count; ++i) {
        SubActionFlagsEntry e;
        e.flags         = r.read_u32_be();
        e.anim_name_ptr = r.read_u32_be();
        out.push_back(e);
    }
    return out;
}

std::string subaction_anim_name(const MiscSection& ms,
                                const SubActionFlagsEntry& e) {
    if (e.anim_name_ptr == 0) return {};
    const std::size_t off = resolve_misc_ptr(e.anim_name_ptr);
    if (off >= ms.size()) return {};
    const char* start = reinterpret_cast<const char*>(ms.data() + off);
    std::size_t max_len = ms.size() - off;
    std::size_t len = 0;
    while (len < max_len && start[len] != '\0') ++len;
    return std::string(start, len);
}

} // namespace psax
