#pragma once

#include <cstdint>
#include <vector>

namespace psax {

// Human-readable name for a specific argument of a specific command.
// Source of truth: PSAC's Parameters.txt (falling back to BrawlCrate's
// Parameters.txt for cmd_ids PSAC doesn't cover). Returns nullptr if the
// command has no schema entry or arg_idx is out of range.
//
// Example: command_arg_name(0x00020100, 0) -> "Frames"
//          command_arg_name(0x11150300, 1) -> "Instant"
const char* command_arg_name(std::uint32_t cmd_id, std::uint32_t arg_idx);

// Long-form description of the same argument, if available. Type-hint
// suffixes (e.g. "*GFXID:4") are stripped. Returns nullptr for no
// description or empty string.
const char* command_arg_description(std::uint32_t cmd_id, std::uint32_t arg_idx);

// Every cmd_id that has a per-argument schema entry. Used by the overrides
// dumper. Order is insertion order in the static table (numeric).
std::vector<std::uint32_t> all_schema_command_ids();

// Slot count stored in the schema for this cmd_id. May differ from
// arg_count_of(cmd_id) if BrawlCrate lists extra params we haven't verified.
// Returns 0 if the cmd_id has no schema entry.
std::uint32_t schema_arg_slot_count(std::uint32_t cmd_id);

} // namespace psax
