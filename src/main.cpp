#include "pac/pac_file.hpp"
#include "psa/character_root.hpp"
#include "psa/command_table.hpp"
#include "psa/event_decoder.hpp"
#include "psa/misc_section.hpp"
#include "psa/animation_flags.hpp"
#include "psa/overrides.hpp"
#include "psa/sfx_audit.hpp"
#include "psa/subaction_flags.hpp"
#include "psa/subaction_table.hpp"
#include "psa/subroutine_scan.hpp"
#include "psa/variable_audit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

namespace {

std::string PSAX_VERSION {"0.2.1"};

void print_usage() {
    std::fprintf(stderr,
        "version: %s\n"
        "usage:\n"
        "  psax <pac>                                     PAC + MISC summary\n"
        "  psax <pac> --list-subactions [--with-body]     list every subaction with content in any tab\n"
        "                                                 --with-body: also decode & print each tab\n"
        "  psax <pac> --subaction <id> [tab]              decode events for a subaction\n"
        "                                                 tab = main | gfx | sfx | other\n"
        "                                                 (default: all four)\n"
        "  psax <pac> --events <hex-off>                  decode events at a MISC stored offset\n"
        "  psax <pac> --audit-sfx [--min N] [--max N]     list SFX-relevant events across all subactions\n"
        "                                                 min/max gate SoundEffect ID inclusively\n"
        "                                                 (decimal or 0x-prefixed hex)\n"
        "  psax <pac> --list-subroutines [--with-body]    discover every subroutine reachable from any\n"
        "                                                 SubAction (via SubRoutine/Goto/ConcurrentLoop)\n"
        "                                                 and show its callers. optionally print each\n"
        "                                                 subroutine's decoded events\n"
        "  psax <pac> --audit-var                         list every event that touches any variable\n"
        "                                                 (get/set), grouped by subaction+tab or\n"
        "                                                 subroutine\n"
        "  psax <pac> --find-var <descriptor>             same as --audit-var, filtered to one variable.\n"
        "                                                 descriptor: DSL form like 'RA-Basic[8]' or\n"
        "                                                 raw hex like '0x20000008'\n"
        "  psax init-overrides                            create an overrides/ dir + README + example.json\n"
        "                                                 next to the current dir; falls back to the\n"
        "                                                 per-user config dir if the current dir isn't\n"
        "                                                 writable. no PAC required\n"
        "  psax where-overrides                           print every override dir that would be applied,\n"
        "                                                 in load order. no PAC required\n",
    PSAX_VERSION.c_str());
}

// Parse a non-negative integer written in decimal or, if 0x-prefixed, hex.
bool parse_uint_dec_or_hex(const char* s, uint32_t& out) {
    if (!s || !*s) return false;
    char* end = nullptr;
    const unsigned long v = std::strtoul(s, &end, 0);   // base 0 auto-detects 0x
    if (end == s || *end != '\0') return false;
    out = static_cast<uint32_t>(v);
    return true;
}

// Parse trailing --min / --max flags starting at argv[start]. Returns false
// on malformed input; the error was already reported to stderr.
bool parse_audit_sfx_options(int argc, char** argv, int start,
                             psax::SfxAuditOptions& out) {
    for (int i = start; i < argc; ++i) {
        const bool has_next = (i + 1) < argc;
        if (std::strcmp(argv[i], "--min") == 0 && has_next) {
            if (!parse_uint_dec_or_hex(argv[++i], out.min_sound_id)) {
                std::fprintf(stderr, "bad --min value: %s\n", argv[i]);
                return false;
            }
        } else if (std::strcmp(argv[i], "--max") == 0 && has_next) {
            if (!parse_uint_dec_or_hex(argv[++i], out.max_sound_id)) {
                std::fprintf(stderr, "bad --max value: %s\n", argv[i]);
                return false;
            }
        } else {
            std::fprintf(stderr, "unrecognized option: %s\n", argv[i]);
            return false;
        }
    }
    return true;
}

// Shared printer for VarAuditEntry - mirrors the SFX audit output style.
void print_var_entry(const psax::VarAuditEntry& r) {
    if (r.kind == psax::VarAuditEntry::InSubAction) {
        const char* name = r.anim_name.empty() ? "<unnamed>" : r.anim_name.c_str();
        std::printf("Subaction 0x%zX - %s - %s\n",
                    r.subaction_id, r.tab_label, name);
    } else {
        std::printf("Subroutine 0x%X", r.subroutine_stored_ptr);
        if (!r.subroutine_callers.empty()) {
            std::printf("  (called from ");
            const std::size_t n = r.subroutine_callers.size() < 3
                                    ? r.subroutine_callers.size() : 3;
            for (std::size_t i = 0; i < n; ++i) {
                const auto& c = r.subroutine_callers[i];
                if (i > 0) std::printf(", ");
                if (c.kind == psax::SubroutineCallSite::FromSubAction) {
                    std::printf("SubAction 0x%zX %s",
                                c.subaction_id, c.tab_label);
                } else {
                    std::printf("Subroutine 0x%X", c.caller_stored_ptr);
                }
            }
            if (r.subroutine_callers.size() > n) {
                std::printf(", +%zu more", r.subroutine_callers.size() - n);
            }
            std::printf(")");
        }
        std::printf("\n");
    }
    for (const auto& ev : r.events) {
        std::printf("  %s\n", ev.to_pretty_string().c_str());
    }
    std::printf("\n");
}

void audit_var_cli(const psax::PacFile& pac, const psax::VarAuditOptions& opt) {
    auto misc = pac.find_misc_data();
    if (!misc) { std::fprintf(stderr, "no MISC section\n"); return; }
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);
    const auto report = psax::audit_variables(ms, opt);

    for (const auto& r : report.entries) print_var_entry(r);

    std::size_t sa = 0, sr = 0;
    for (const auto& r : report.entries) {
        (r.kind == psax::VarAuditEntry::InSubAction ? sa : sr) += 1;
    }
    if (opt.has_target) {
        std::printf("(%zu locations = %zu subaction-tabs + %zu subroutines; "
                    "filtered to variable 0x%08X)\n",
                    report.entries.size(), sa, sr, opt.target_var_raw);
    } else {
        std::printf("(%zu locations = %zu subaction-tabs + %zu subroutines)\n",
                    report.entries.size(), sa, sr);
    }

    if (!report.failures.empty()) {
        std::fprintf(stderr, "\n%zu locations failed to decode:\n",
                     report.failures.size());
        const std::size_t detail = report.failures.size() < 10
                                        ? report.failures.size() : 10;
        for (std::size_t i = 0; i < detail; ++i) {
            const auto& f = report.failures[i];
            std::fprintf(stderr, "  subaction 0x%zX %s at stored 0x%X: %s\n",
                         f.subaction_id, f.tab_label, f.stored_ptr, f.reason.c_str());
        }
        if (report.failures.size() > detail) {
            std::fprintf(stderr, "  ... and %zu more\n",
                         report.failures.size() - detail);
        }
    }
}

void list_subroutines_cli(const psax::PacFile& pac, bool with_body) {
    auto misc = pac.find_misc_data();
    if (!misc) { std::fprintf(stderr, "no MISC section\n"); return; }
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);
    const auto subs = psax::collect_subroutines(ms);

    for (const auto& s : subs) {
        std::printf("Subroutine 0x%X  (resolved 0x%zX)  %zu events%s\n",
                    s.stored_ptr, s.resolved_offset, s.events.size(),
                    s.decode_error.empty() ? "" : "  [decode failed]");
        if (!s.decode_error.empty()) {
            std::printf("  ! %s\n", s.decode_error.c_str());
        }
        std::printf("  called by:\n");
        for (const auto& c : s.callers) {
            if (c.kind == psax::SubroutineCallSite::FromSubAction) {
                std::printf("    SubAction 0x%zX %-5s [event %zu]\n",
                            c.subaction_id, c.tab_label, c.event_index);
            } else {
                std::printf("    Subroutine 0x%X     [event %zu]\n",
                            c.caller_stored_ptr, c.event_index);
            }
        }
        if (with_body) {
            for (std::size_t i = 0; i < s.events.size(); ++i) {
                std::printf("  [%2zu] %-40s # %s\n",
                            i, s.events[i].to_pretty_string().c_str(),
                            s.events[i].to_raw_string().c_str());
            }
        }
        std::printf("\n");
    }

    std::size_t failed = 0;
    for (const auto& s : subs) if (!s.decode_error.empty()) ++failed;
    std::printf("(%zu subroutines discovered; %zu failed to decode)\n",
                subs.size(), failed);
}

void audit_sfx_cli(const psax::PacFile& pac, const psax::SfxAuditOptions& opt) {
    auto misc = pac.find_misc_data();
    if (!misc) { std::fprintf(stderr, "no MISC section\n"); return; }
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);

    const auto report = psax::audit_sfx(ms, opt);
    for (const auto& r : report.entries) {
        if (r.kind == psax::SfxAuditEntry::InSubAction) {
            const char* name = r.anim_name.empty() ? "<unnamed>" : r.anim_name.c_str();
            std::printf("Subaction 0x%zX - %s - %s\n",
                        r.subaction_id, r.tab_label, name);
        } else {
            std::printf("Subroutine 0x%X", r.subroutine_stored_ptr);
            if (!r.subroutine_callers.empty()) {
                std::printf("  (called from ");
                // Show up to 3 callers to keep the line readable.
                const std::size_t n = r.subroutine_callers.size() < 3
                                        ? r.subroutine_callers.size() : 3;
                for (std::size_t i = 0; i < n; ++i) {
                    const auto& c = r.subroutine_callers[i];
                    if (i > 0) std::printf(", ");
                    if (c.kind == psax::SubroutineCallSite::FromSubAction) {
                        std::printf("SubAction 0x%zX %s",
                                    c.subaction_id, c.tab_label);
                    } else {
                        std::printf("Subroutine 0x%X", c.caller_stored_ptr);
                    }
                }
                if (r.subroutine_callers.size() > n) {
                    std::printf(", +%zu more", r.subroutine_callers.size() - n);
                }
                std::printf(")");
            }
            std::printf("\n");
        }
        for (const auto& ev : r.events) {
            std::printf("  %s\n", ev.to_pretty_string().c_str());
        }
        std::printf("\n");
    }

    // Count subactions vs subroutines separately in the summary.
    std::size_t sa = 0, sr = 0;
    for (const auto& r : report.entries) {
        (r.kind == psax::SfxAuditEntry::InSubAction ? sa : sr) += 1;
    }
    const bool has_min = opt.min_sound_id != 0u;
    const bool has_max = opt.max_sound_id != 0xFFFFFFFFu;
    if (has_min || has_max) {
        std::printf("(%zu locations = %zu subaction-tabs + %zu subroutines; "
                    "SoundEffect filtered to id in [0x%X, 0x%X])\n",
                    report.entries.size(), sa, sr,
                    opt.min_sound_id, opt.max_sound_id);
    } else {
        std::printf("(%zu locations = %zu subaction-tabs + %zu subroutines)\n",
                    report.entries.size(), sa, sr);
    }

    if (!report.failures.empty()) {
        std::fprintf(stderr, "\n%zu subaction-tabs failed to decode:\n", report.failures.size());
        // Show the first 10 in detail so the user can drill in with --subaction / --events;
        // list only IDs for the rest so the output stays manageable.
        const std::size_t detail = report.failures.size() < 10 ? report.failures.size() : 10;
        for (std::size_t i = 0; i < detail; ++i) {
            const auto& f = report.failures[i];
            std::fprintf(stderr, "  subaction 0x%zX %s at stored 0x%X: %s\n",
                         f.subaction_id, f.tab_label, f.stored_ptr, f.reason.c_str());
        }
        if (report.failures.size() > detail) {
            std::fprintf(stderr, "  ... and %zu more\n",
                         report.failures.size() - detail);
        }
    }
}

void print_summary(const psax::PacFile& pac) {
    std::printf("--- ARC header ---\n");
    std::printf("  name:       %s\n", pac.header().name.c_str());
    std::printf("  version:    0x%04X\n", pac.header().version);
    std::printf("  node_count: %u\n\n", pac.header().node_count);

    std::printf("--- ARC entries ---\n");
    for (std::size_t i = 0; i < pac.entries().size(); ++i) {
        const auto& e = pac.entries()[i];
        std::printf("  [%zu] type=%s (0x%04X) index=%u length=%u bytes  data@0x%zX\n",
                    i, psax::arc_file_type_name(e.file_type),
                    static_cast<uint16_t>(e.file_type),
                    e.file_index, e.length, e.data_offset);
    }

    auto misc = pac.find_misc_data();
    if (!misc) return;
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);

    std::printf("\n--- MiscData (PSA) ---\n");
    std::printf("  size:            %u bytes\n", misc->length);
    std::printf("  data_table:      %zu entries\n", ms.data_table().size());
    std::printf("  external_subs:   %zu entries\n", ms.external_subs().size());

    if (ms.data_table().empty()) return;
    psax::CharacterRoot root{};
    try {
        root = psax::load_character_root(ms);
    } catch (const std::exception& ex) {
        std::printf("\n--- Character Root ---\n  (not available: %s)\n", ex.what());
        return;
    }
    std::printf("\n--- Character Root (from data_table entry 'data', resolved) ---\n");
    for (std::size_t i = 0; i < psax::CharacterRoot::kFieldCount; ++i) {
        auto f = static_cast<psax::CharacterRoot::Field>(i);
        std::printf("  %-24s 0x%X\n",
                    psax::CharacterRoot::name_of(f), root.fields[i]);
    }
}

// Forward-declared; the real body lives after decode_subaction_tab so it can
// reuse kTabs / decode_subaction_tab.
void list_subactions(const psax::PacFile& pac, bool with_body);

void decode_events_at(const psax::PacFile& pac, uint32_t stored_offset) {
    auto misc = pac.find_misc_data();
    if (!misc) { std::fprintf(stderr, "no MISC section\n"); return; }
    psax::EventDecoder dec(pac.entry_data(*misc), misc->length);

    const std::size_t resolved = psax::resolve_misc_ptr(stored_offset);
    std::printf("Event stream at stored 0x%X (resolved 0x%zX):\n\n",
                stored_offset, resolved);
    auto events = dec.decode(resolved);
    for (std::size_t i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        std::printf("  [%2zu] %-40s # %s\n",
                    i, e.to_pretty_string().c_str(), e.to_raw_string().c_str());
    }
    std::printf("\n  (%zu events)\n", events.size());
}

// Which SubAction<Tab> table to read; also indexes into the character root.
struct TabInfo {
    const char* label;
    psax::CharacterRoot::Field root_field;
};
constexpr TabInfo kTabs[] = {
    {"Main",  psax::CharacterRoot::SubActionMain},
    {"GFX",   psax::CharacterRoot::SubActionGFX},
    {"SFX",   psax::CharacterRoot::SubActionSFX},
    {"Other", psax::CharacterRoot::SubActionOther},
};

// Returns -1 if `s` doesn't match any tab keyword.
int tab_index_from_name(const char* s) {
    if (!s) return -1;
    if (std::strcmp(s, "main")  == 0) return 0;
    if (std::strcmp(s, "gfx")   == 0) return 1;
    if (std::strcmp(s, "sfx")   == 0) return 2;
    if (std::strcmp(s, "other") == 0) return 3;
    return -1;
}

// Decode one tab's events for a given subaction id. Prints a header + events.
// Returns true if there were events to decode, false if the entry was empty.
bool decode_subaction_tab(const psax::PacFile& pac,
                          const psax::MiscSection& ms,
                          const psax::CharacterRoot& root,
                          std::size_t id,
                          const TabInfo& tab) {
    auto sat = psax::read_subaction_table(
        ms, root.fields[tab.root_field], id + 1);
    if (id >= sat.size() || sat[id] == 0) {
        std::printf("  --- %-5s: (empty)\n\n", tab.label);
        return false;
    }
    const uint32_t stored = sat[id];
    std::printf("  --- %-5s: event_list_ptr=0x%X (resolved 0x%zX) ---\n",
                tab.label, stored, psax::resolve_misc_ptr(stored));

    psax::EventDecoder dec(pac.entry_data(*pac.find_misc_data()), ms.size());
    auto events = dec.decode(psax::resolve_misc_ptr(stored));
    for (std::size_t i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        std::printf("    [%2zu] %-40s # %s\n",
                    i, e.to_pretty_string().c_str(), e.to_raw_string().c_str());
    }
    std::printf("    (%zu events)\n\n", events.size());
    return true;
}

// Non-empty across ALL four tabs, not just Main. When `with_body` is true,
// each populated tab is decoded (same output as `--subaction <id>`).
void list_subactions(const psax::PacFile& pac, bool with_body) {
    auto misc = pac.find_misc_data();
    if (!misc) { std::fprintf(stderr, "no MISC section\n"); return; }
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);
    auto root = psax::load_character_root(ms);
    const std::size_t count = psax::subaction_main_count(
        root.fields[psax::CharacterRoot::SubActionMain],
        root.fields[psax::CharacterRoot::SubActionGFX]);

    // Read all four tables + anim names once.
    std::vector<uint32_t> tabs_data[4];
    for (std::size_t t = 0; t < 4; ++t) {
        tabs_data[t] = psax::read_subaction_table(
            ms, root.fields[kTabs[t].root_field], count);
    }
    const auto flags = psax::read_subaction_flags(
        ms, root.fields[psax::CharacterRoot::SubActionFlags], count);

    auto populated = [](uint32_t v) { return v != 0u && v != 0xFFFFFFFFu; };

    if (!with_body) {
        std::printf("SubActions (%zu total, non-empty in any tab):\n\n", count);
        std::printf("  %-6s  %-28s  %-9s %-9s %-9s %-9s  %s\n",
                    "id", "anim", "Main", "GFX", "SFX", "Other", "flags");
    }

    std::size_t shown = 0;
    for (std::size_t i = 0; i < count; ++i) {
        bool any = false;
        for (std::size_t t = 0; t < 4; ++t) {
            if (populated(tabs_data[t][i])) { any = true; break; }
        }
        if (!any) continue;
        ++shown;

        const std::string anim = (i < flags.size())
            ? psax::subaction_anim_name(ms, flags[i]) : std::string{};

        if (!with_body) {
            // Show the stored event_list_ptr per tab; "-" if the tab is empty.
            char cell[4][12] = {"-", "-", "-", "-"};
            for (std::size_t t = 0; t < 4; ++t) {
                if (populated(tabs_data[t][i])) {
                    std::snprintf(cell[t], sizeof(cell[t]), "0x%X", tabs_data[t][i]);
                }
            }
            const std::string flag_str = (i < flags.size())
                ? psax::format_animation_flags(flags[i].flags) : std::string("-");
            std::printf("  0x%-4zX  %-28s  %-9s %-9s %-9s %-9s  %s\n",
                        i, anim.empty() ? "<unnamed>" : anim.c_str(),
                        cell[0], cell[1], cell[2], cell[3],
                        flag_str.c_str());
        } else {
            std::printf("=== SubAction 0x%zX%s%s ===\n\n",
                        i,
                        anim.empty() ? "" : " - ",
                        anim.c_str());
            for (const auto& t : kTabs) {
                decode_subaction_tab(pac, ms, root, i, t);
            }
        }
    }

    std::printf("\n(%zu non-empty of %zu)\n", shown, count);
}

// If tab_filter is nullptr, decode all four tabs; otherwise decode just the named one.
void decode_subaction(const psax::PacFile& pac, std::size_t id, const char* tab_filter) {
    auto misc = pac.find_misc_data();
    if (!misc) { std::fprintf(stderr, "no MISC section\n"); return; }
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);
    auto root = psax::load_character_root(ms);

    std::printf("SubAction 0x%zX", id);
    if (tab_filter) std::printf("  [%s tab only]", tab_filter);
    std::printf("\n\n");

    if (tab_filter) {
        const int idx = tab_index_from_name(tab_filter);
        if (idx < 0) {
            std::fprintf(stderr, "unknown tab '%s' (want main|gfx|sfx|other)\n",
                         tab_filter);
            return;
        }
        decode_subaction_tab(pac, ms, root, id, kTabs[idx]);
    } else {
        for (const auto& t : kTabs) {
            decode_subaction_tab(pac, ms, root, id, t);
        }
    }
}

uint32_t parse_hex(const char* s) {
    if (!s) return 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    return static_cast<uint32_t>(std::strtoul(s, nullptr, 16));
}

std::size_t parse_index(const char* s) {
    if (!s) return 0;
    int base = 10;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { s += 2; base = 16; }
    return static_cast<std::size_t>(std::strtoul(s, nullptr, base));
}

// Bare subcommands don't take a PAC and are inspection/setup commands, so
// they intentionally skip the auto-load of override files. A malformed
// override in the user's dirs otherwise couldn't be diagnosed without
// running the tool successfully.
int handle_init_overrides() {
    const auto r = psax::init_overrides();
    if (r.created) {
        std::printf("Created overrides directory: %s\n", r.path.string().c_str());
        std::printf("  wrote README.md and example.json\n");
    } else {
        std::printf("Overrides directory already exists: %s\n",
                    r.path.string().c_str());
        std::printf("  (nothing written; drop *.json files here)\n");
    }
    if (r.used_fallback) {
        std::printf("  (project dir wasn't writable; used per-user fallback)\n");
    }
    return 0;
}

int handle_where_overrides() {
    const auto paths = psax::active_overrides_paths();
    if (paths.empty()) {
        std::printf(
            "No override directories exist yet.\n"
            "  project (checked): %s\n"
            "  user    (checked): %s\n"
            "Run 'psax init-overrides' to create one.\n",
            psax::project_overrides_dir().string().c_str(),
            psax::user_overrides_dir().string().c_str());
        return 0;
    }
    std::printf("Override directories that will be applied (in load order; "
                "later wins):\n");
    for (const auto& p : paths) {
        std::printf("  %s\n", p.string().c_str());
    }
    return 0;
}

// Silent on success; a malformed file throws with a "<path>:<line>:<col>:
// <msg>" message that the caller's try/catch surfaces as a hard error.
void auto_load_overrides() {
    auto acc = psax::load_override_dir(psax::user_overrides_dir());
    acc = psax::merge_overrides(std::move(acc),
                                psax::load_override_dir(psax::project_overrides_dir()));
    psax::set_active_overrides(std::move(acc));
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { print_usage(); return 2; }

    // Version flag: works standalone (no PAC file required).
    if (std::strcmp(argv[1], "--v") == 0 ||
        std::strcmp(argv[1], "--version") == 0) {
        std::printf("psax %s\n", PSAX_VERSION.c_str());
        return 0;
    }

    try {
        // Bare subcommands (no PAC required) - inspection/setup only.
        if (std::strcmp(argv[1], "init-overrides") == 0)  return handle_init_overrides();
        if (std::strcmp(argv[1], "where-overrides") == 0) return handle_where_overrides();

        const char* path = argv[1];
        auto pac = psax::PacFile::load(path);
        // Silently apply any user-supplied overrides. Malformed files throw
        // before we've printed any per-run output, so a broken override
        // gives a clean single "error: ..." message with no half-run noise.
        auto_load_overrides();
        std::printf("file: %s (%zu bytes)\n\n", path, pac.size());

        if (argc == 2) {
            print_summary(pac);
        } else if (std::strcmp(argv[2], "--list-subactions") == 0) {
            const bool with_body = (argc >= 4)
                && std::strcmp(argv[3], "--with-body") == 0;
            list_subactions(pac, with_body);
        } else if (std::strcmp(argv[2], "--audit-sfx") == 0) {
            psax::SfxAuditOptions opt;
            if (!parse_audit_sfx_options(argc, argv, 3, opt)) return 2;
            audit_sfx_cli(pac, opt);
        } else if (std::strcmp(argv[2], "--list-subroutines") == 0) {
            const bool with_body = (argc >= 4)
                && std::strcmp(argv[3], "--with-body") == 0;
            list_subroutines_cli(pac, with_body);
        } else if (std::strcmp(argv[2], "--audit-var") == 0) {
            audit_var_cli(pac, {});
        } else if (argc >= 4 && std::strcmp(argv[2], "--find-var") == 0) {
            psax::VarAuditOptions opt;
            if (!psax::parse_variable_descriptor(argv[3], opt.target_var_raw)) {
                std::fprintf(stderr,
                    "bad variable descriptor: %s\n"
                    "  expected 'RA-Basic[8]' style or '0x20000008' hex\n",
                    argv[3]);
                return 2;
            }
            opt.has_target = true;
            audit_var_cli(pac, opt);
        } else if (argc >= 4 && std::strcmp(argv[2], "--events") == 0) {
            decode_events_at(pac, parse_hex(argv[3]));
        } else if (argc >= 4 && std::strcmp(argv[2], "--subaction") == 0) {
            const char* tab = (argc >= 5) ? argv[4] : nullptr;
            decode_subaction(pac, parse_index(argv[3]), tab);
        } else {
            print_usage();
            return 2;
        }
        std::printf("version: %s\n",PSAX_VERSION.c_str());
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
