#pragma once

#include <cstdint>

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

} // namespace psax
