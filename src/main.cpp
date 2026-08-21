#include "pac/pac_file.hpp"
#include "psa/character_root.hpp"
#include "psa/command_table.hpp"
#include "psa/event_decoder.hpp"
#include "psa/misc_section.hpp"
#include "psa/subaction_table.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

namespace {

void print_usage() {
    std::fprintf(stderr,
        "usage:\n"
        "  psax <pac>                            PAC + MISC summary\n"
        "  psax <pac> --list-subactions          list non-empty SubActionMain slots\n"
        "  psax <pac> --subaction <id> [tab]     decode events for a subaction\n"
        "                                        tab = main | gfx | sfx | other\n"
        "                                        (default: all four)\n"
        "  psax <pac> --events <hex-off>         decode events at a MISC stored offset\n");
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
    auto root = psax::load_character_root(ms);
    std::printf("\n--- Character Root (from data_table[0], resolved) ---\n");
    for (std::size_t i = 0; i < psax::CharacterRoot::kFieldCount; ++i) {
        auto f = static_cast<psax::CharacterRoot::Field>(i);
        std::printf("  %-24s 0x%X\n",
                    psax::CharacterRoot::name_of(f), root.fields[i]);
    }
}

void list_subactions(const psax::PacFile& pac) {
    auto misc = pac.find_misc_data();
    if (!misc) { std::fprintf(stderr, "no MISC section\n"); return; }
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);
    auto root = psax::load_character_root(ms);
    const std::size_t count = psax::subaction_main_count(
        root.fields[psax::CharacterRoot::SubActionMain],
        root.fields[psax::CharacterRoot::SubActionGFX]);
    auto sat  = psax::read_subaction_table(
        ms, root.fields[psax::CharacterRoot::SubActionMain], count);

    std::printf("SubActionMain (%zu total, non-empty only):\n\n", count);
    std::printf("  %-6s %-14s %s\n", "id", "event_list_ptr", "resolved");
    int shown = 0;
    for (std::size_t i = 0; i < sat.size(); ++i) {
        if (sat[i] == 0) continue;
        std::printf("  0x%-4zX 0x%-12X 0x%zX\n",
                    i, sat[i], psax::resolve_misc_ptr(sat[i]));
        ++shown;
    }
    std::printf("\n  (%d non-empty of %zu)\n", shown, count);
}

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

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { print_usage(); return 2; }
    const char* path = argv[1];
    try {
        auto pac = psax::PacFile::load(path);
        std::printf("file: %s (%zu bytes)\n\n", path, pac.size());

        if (argc == 2) {
            print_summary(pac);
        } else if (std::strcmp(argv[2], "--list-subactions") == 0) {
            list_subactions(pac);
        } else if (argc >= 4 && std::strcmp(argv[2], "--events") == 0) {
            decode_events_at(pac, parse_hex(argv[3]));
        } else if (argc >= 4 && std::strcmp(argv[2], "--subaction") == 0) {
            const char* tab = (argc >= 5) ? argv[4] : nullptr;
            decode_subaction(pac, parse_index(argv[3]), tab);
        } else {
            print_usage();
            return 2;
        }
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
