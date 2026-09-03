#pragma once

#include <cstdint>

namespace psax {

// Human-readable name for a specific argument of a specific command, as
// sourced from BrawlCrate's Parameters.txt. Returns nullptr if the
// command has no schema entry or the arg_idx is out of range.
//
// Example: command_arg_name(0x00020100, 0) -> "Frames"
//          command_arg_name(0x0A000100, 0) -> "Sound Effect ID"
const char* command_arg_name(std::uint32_t cmd_id, std::uint32_t arg_idx);

} // namespace psax
