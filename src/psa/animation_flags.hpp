#pragma once

#include <cstdint>
#include <string>

namespace psax {

// Decoded view of the `flags` u32 stored in each SubActionFlags entry.
// Bit layout is verified against PSAC's SubaAnimFlagsForm.cs:
//   bits 24..31 : InTransition frame count (integer)
//   bits 16..23 : 8 boolean flags (NoOutTransition .. Unknown7)
//   bits  0..15 : TBD (semantics not yet identified)
struct AnimationFlags {
    uint8_t  in_transition;

    bool no_out_transition;
    bool loop;
    bool moves_character;
    bool unknown3;
    bool unknown4;
    bool unknown5;
    bool transition_out_from_start;
    bool unknown7;

    uint16_t low16;   // raw bits 0..15, exposed until we identify them
};

AnimationFlags decode_animation_flags(uint32_t raw);

// Compact human-readable form. Only mentions set flags; unset flags are
// omitted. Examples:
//   0x06060000  ->  "In=6, Loop, MovesCharacter"
//   0x00010000  ->  "NoOutTransition"
//   0x00000000  ->  "-"
// Includes a trailing "low16=0xNNNN" if bits 0..15 are non-zero.
std::string format_animation_flags(uint32_t raw);

} // namespace psax
