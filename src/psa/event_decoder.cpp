#include "psa/event_decoder.hpp"

#include "util/binary_reader.hpp"

#include <stdexcept>

namespace psax {

EventDecoder::EventDecoder(const uint8_t* data, std::size_t size)
    : data_(data), size_(size) {}

std::vector<Event> EventDecoder::decode(std::size_t event_list_offset) const {
    std::vector<Event> events;
    std::size_t off = event_list_offset;

    while (true) {
        if (off + 8 > size_) {
            throw std::runtime_error("EventDecoder: event stream past end of buffer");
        }
        BinaryReader r(data_ + off, 8);
        const uint32_t cmd_id   = r.read_u32_be();
        const uint32_t args_ptr = r.read_u32_be();
        off += 8;

        if (cmd_id == 0 && args_ptr == 0) break;  // terminator

        Event e;
        e.cmd_id   = cmd_id;
        e.args_ptr = args_ptr;

        const auto info = table_.lookup(cmd_id);
        const uint32_t n_args = info ? info->arg_count : 0u;

        if (n_args > 0) {
            const std::size_t args_start = resolve_misc_ptr(args_ptr);
            if (args_start + std::size_t(n_args) * 8u > size_) {
                throw std::runtime_error("EventDecoder: args past end of buffer");
            }
            BinaryReader ar(data_ + args_start, std::size_t(n_args) * 8u);
            e.args.reserve(n_args);
            for (uint32_t i = 0; i < n_args; ++i) {
                Arg a;
                a.type      = static_cast<ArgType>(ar.read_u32_be());
                a.raw_value = ar.read_u32_be();
                e.args.push_back(a);
            }
        }
        events.push_back(std::move(e));
    }
    return events;
}

} // namespace psax
