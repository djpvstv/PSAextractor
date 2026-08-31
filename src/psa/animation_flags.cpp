#include "psa/animation_flags.hpp"

#include <cstdio>

namespace psax {

AnimationFlags decode_animation_flags(uint32_t raw) {
    AnimationFlags f{};
    f.in_transition            = static_cast<uint8_t>((raw >> 24) & 0xFFu);
    const uint8_t b1           = static_cast<uint8_t>((raw >> 16) & 0xFFu);
    f.no_out_transition        = (b1 & 0x01u) != 0;
    f.loop                     = (b1 & 0x02u) != 0;
    f.moves_character          = (b1 & 0x04u) != 0;
    f.unknown3                 = (b1 & 0x08u) != 0;
    f.unknown4                 = (b1 & 0x10u) != 0;
    f.unknown5                 = (b1 & 0x20u) != 0;
    f.transition_out_from_start= (b1 & 0x40u) != 0;
    f.unknown7                 = (b1 & 0x80u) != 0;
    f.low16                    = static_cast<uint16_t>(raw & 0xFFFFu);
    return f;
}

std::string format_animation_flags(uint32_t raw) {
    const auto f = decode_animation_flags(raw);
    std::string out;

    auto append = [&](const char* name) {
        if (!out.empty()) out += ", ";
        out += name;
    };
    char buf[32];

    if (f.in_transition != 0) {
        std::snprintf(buf, sizeof(buf), "In=%u", f.in_transition);
        append(buf);
    }
    if (f.no_out_transition)         append("NoOutTransition");
    if (f.loop)                      append("Loop");
    if (f.moves_character)           append("MovesCharacter");
    if (f.unknown3)                  append("Unknown3");
    if (f.unknown4)                  append("Unknown4");
    if (f.unknown5)                  append("Unknown5");
    if (f.transition_out_from_start) append("TransitionOutFromStart");
    if (f.unknown7)                  append("Unknown7");
    if (f.low16 != 0) {
        std::snprintf(buf, sizeof(buf), "low16=0x%X", f.low16);
        append(buf);
    }

    return out.empty() ? std::string("-") : out;
}

} // namespace psax
