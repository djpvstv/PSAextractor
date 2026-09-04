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

// Human-readable name for a command ID. Returns the DOTTED-syntax method
// name by default (e.g. "terminate" for 0x11150300 TerminateGraphicEffect),
// or the historical PSAC name if --legacy is active. nullptr if unknown.
// Overridable via the runtime overrides file (overrides target the method
// name, not the legacy form).
const char* command_name(uint32_t cmd_id);

// Historical PSAC display name for a command, ignoring the --legacy toggle.
// Used by --legacy mode itself and by the dumper. nullptr if unknown.
const char* command_legacy_name(uint32_t cmd_id);

// Dotted-syntax method name for a command (the part after "module.").
// nullptr if unknown. Consults overrides.
const char* command_method_name(uint32_t cmd_id);

// Module shorthand for the top byte of the cmd_id (e.g. "work" for 0x12).
// nullptr when the top byte has no assigned module — currently 0x00 (flow
// control) and 0x01 (loopRest) render as bare "method(args)" per your
// preference for uncluttered flow display.
const char* command_module_shorthand(std::uint32_t cmd_id);

// Long game-symbol module name for the top byte (e.g. "soWorkManageModuleImpl"
// for 0x12). Kept for future tooltip/verbose output. nullptr if unknown.
const char* command_module_long_name(std::uint32_t cmd_id);

// Global toggle: on -> command_name returns legacy_name, Event::to_pretty_string
// uses bare "LegacyName(...)"; off -> "module.method(...)" dotted form.
// Set once from main() after arg parsing.
void set_legacy_display_mode(bool on);
bool is_legacy_display_mode();

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
