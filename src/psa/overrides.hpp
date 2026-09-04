#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace psax {

// One argument's override. Missing fields fall through to lower layers
// (earlier files, then the built-in schema).
struct ArgOverride {
    std::optional<std::string> name;
    std::optional<std::string> description;
};

// One command's override. `args` is indexed by argument position; entries with
// no fields set are placeholders that skip that arg (letting a later position
// be overridden without restating earlier ones).
struct CommandOverride {
    std::optional<std::string> name;
    std::optional<std::string> description;
    std::optional<std::string> format;
    std::vector<ArgOverride>   args;
};

using OverrideMap = std::unordered_map<std::uint32_t, CommandOverride>;

// Parse one .json file. Throws std::runtime_error with a message shaped as
// "<path>:<line>:<col>: <problem>" on any malformation, including unknown
// fields.
OverrideMap parse_override_file(const std::filesystem::path& p);

// Load every *.json in `dir` in alphabetical order, deep-merging so later
// files overlay earlier ones per (cmd_id, field). Missing dir returns empty
// map without error. A malformed file rethrows the parser error.
OverrideMap load_override_dir(const std::filesystem::path& dir);

// Field-level overlay: `top`'s set fields win, `base`'s survive where `top`
// has no value. Arg vectors are extended to max(base, top) length.
OverrideMap merge_overrides(OverrideMap base, const OverrideMap& top);

// Install the process-global map consulted by command_name(), etc.
// Passing an empty map clears any active overrides.
void set_active_overrides(OverrideMap m);

// Lookups the accessors in command_table / argument_schema consult first.
// Return nullptr if the active map has no matching entry / field.
const char* override_command_name(std::uint32_t cmd_id);
const char* override_command_description(std::uint32_t cmd_id);
const char* override_command_format(std::uint32_t cmd_id);
const char* override_command_arg_name(std::uint32_t cmd_id,
                                      std::uint32_t arg_idx);
const char* override_command_arg_description(std::uint32_t cmd_id,
                                             std::uint32_t arg_idx);

// Conventional locations. project = "<cwd>/overrides"; user = a
// platform-specific per-user config dir ("<APPDATA>/psax/overrides" on
// Windows, "<XDG_CONFIG_HOME|~/.config>/psax/overrides" on Linux,
// "~/Library/Application Support/psax/overrides" on macOS).
std::filesystem::path project_overrides_dir();
std::filesystem::path user_overrides_dir();

// Result of `psax init-overrides`. `path` is where files were created;
// `used_fallback` is true if the project dir wasn't writable and we fell
// back to the user dir. `created` is false if the target dir already
// existed (nothing new was written).
struct InitOverridesResult {
    std::filesystem::path path;
    bool used_fallback;
    bool created;
};

// Try to create project_overrides_dir(); if that fails (unwritable parent,
// permissions), fall back to user_overrides_dir(). Populates a README.md
// and example.json in the target dir on first creation. Throws on total
// failure (neither dir usable).
InitOverridesResult init_overrides();

// For `psax where-overrides` - every directory that would be consulted by
// auto-load, in the order they'd be applied. Only includes dirs that
// currently exist.
std::vector<std::filesystem::path> active_overrides_paths();

} // namespace psax
