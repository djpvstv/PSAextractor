#pragma once

#include <cstdint>

namespace psax {

// Arg count is encoded in bits 8..15 of the command ID (byte 2 of the u32,
// counting from MSB). Empirically confirmed against PSAC's event stream:
//   0x00020100 (Async Timer, 1 arg)      -> (>> 8) & 0xFF = 0x01
//   0x120A0100 (Bit Var Set, 1 arg)      -> 0x01
//   0x64000000 (Allow Interrupt, 0 args) -> 0x00
//   0x12000200 (Basic Var Set, 2 args)   -> 0x02   (per PSA guide)
inline uint32_t arg_count_of(uint32_t cmd_id) {
    return (cmd_id >> 8) & 0xFFu;
}

// Human-readable name for a command ID, or nullptr if unknown.
// Table is small and curated; add entries as we verify each command against
// PSAC ground truth. Arg counts are derived from cmd_id, not stored here.
const char* command_name(uint32_t cmd_id);

} // namespace psax
