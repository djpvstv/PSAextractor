#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <ostream>
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

// Conventional locations. exe = "<dir-containing-psax-executable>/overrides"
// (portable-app model - overrides travel with the binary). user = a
// platform-specific per-user config dir ("<APPDATA>/psax/overrides" on
// Windows, "<XDG_CONFIG_HOME|~/.config>/psax/overrides" on Linux,
// "~/Library/Application Support/psax/overrides" on macOS). Both are
// consulted at runtime; exe wins on conflict via deep merge.
//
// exe_overrides_dir() returns an empty path if the running binary's
// location can't be determined (very rare - happens if the exe was
// deleted mid-run, or on unsupported platforms).
std::filesystem::path exe_overrides_dir();
std::filesystem::path user_overrides_dir();

// Serialize every cmd_id known to the built-in tables (union of the name
// table + arg-schema table) to `out` in override-file JSON format, ready
// to be re-read by parse_override_file. This is what `init-overrides`
// writes as the "00_builtin.json" seed.
void dump_builtin_command_table(std::ostream& out);

struct InitOverridesOptions {
    // If false (default), an existing 00_builtin.json is left untouched
    // and `wrote_dump` in the result is false. If true, the dump is
    // regenerated even if the file already exists - used to re-sync after
    // a psax version bump adds new commands.
    bool clean = false;

    // If non-empty, use this exact directory instead of exe/user defaults.
    // Primarily for tests that want to point init at a scratch dir; also
    // usable if a caller wants explicit placement.
    std::filesystem::path dir_override;
};

// Result of `psax init-overrides`. `path` is where files live;
// `used_fallback` is true if the exe dir wasn't writable and we fell
// back to the user dir. `created` is true if the target dir was newly
// created. `wrote_dump` is true if 00_builtin.json was (re)written this
// invocation.
struct InitOverridesResult {
    std::filesystem::path path;
    bool used_fallback;
    bool created;
    bool wrote_dump;
};

// Try to create exe_overrides_dir(); if that fails (unwritable parent,
// permissions, or exe dir unknown), fall back to user_overrides_dir().
// Writes README.md and 00_builtin.json into the target dir. Throws on
// total failure (neither dir usable). Refuses to overwrite an existing
// 00_builtin.json unless `opts.clean` is set.
InitOverridesResult init_overrides(const InitOverridesOptions& opts = {});

// For `psax where-overrides` - every directory that would be consulted by
// auto-load, in the order they'd be applied. Only includes dirs that
// currently exist.
std::vector<std::filesystem::path> active_overrides_paths();

} // namespace psax
