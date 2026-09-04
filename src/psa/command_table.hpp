#pragma once

#include <cstdint>
#include <vector>

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
// Arg counts are derived from cmd_id, not stored here.
const char* command_name(uint32_t cmd_id);

// Longer PSAC-style description, or nullptr if unknown.
const char* command_description(uint32_t cmd_id);

// Optional per-command display-format string. Placeholders `{N}` are replaced
// with each decoded arg's pretty-string. If nullptr, the caller should fall
// back to a default of "arg0, arg1, ...". Examples:
//
//   BasicVariableSet -> "{1} = {0}"     // wire order (value, var); display (var = value)
//   BitVariableSet   -> "{0} = true"
//   BitVariableClear -> "{0} = false"
//
// Only commands with non-default argument display need entries. Add sparingly
// as we verify each layout against PSAC.
const char* command_format(uint32_t cmd_id);

// Every cmd_id that has a curated name entry. Used by the overrides dumper
// to serialize the built-in table; extensions and modders should get the
// same list via `psax init-overrides`. Order is insertion order in the
// static table (roughly numeric).
std::vector<std::uint32_t> all_named_command_ids();

} // namespace psax
