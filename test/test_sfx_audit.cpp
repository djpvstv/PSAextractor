#include <ostream>
#include <string>

#include <doctest/doctest.h>

#include "pac/pac_file.hpp"
#include "psa/misc_section.hpp"
#include "psa/sfx_audit.hpp"
#include "psa/subaction_flags.hpp"
#include "psa/character_root.hpp"

namespace {
std::string sample(const char* name) {
    return std::string(PSAX_TEST_PAC_DIR) + "/" + name;
}
}

TEST_CASE("SubActionFlags: index 0x10 for FitMario resolves to 'RunBrake'") {
    auto pac = psax::PacFile::load(sample("FitMario.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);
    auto root = psax::load_character_root(ms);

    auto flags = psax::read_subaction_flags(
        ms, root.fields[psax::CharacterRoot::SubActionFlags], 32u);
    REQUIRE(flags.size() >= 0x11u);
    CHECK(psax::subaction_anim_name(ms, flags[0x10]) == "RunBrake");
}

TEST_CASE("event_is_sfx_relevant: matches SoundEffect / Collisions / RA-Basic[8-10] set") {
    // SoundEffect (any args)
    psax::Event snd;
    snd.cmd_id = 0x0A000100u;
    CHECK(psax::event_is_sfx_relevant(snd));

    // OffensiveCollision
    psax::Event hit;
    hit.cmd_id = 0x06000D00u;
    CHECK(psax::event_is_sfx_relevant(hit));

    // BasicVariableSet: RA-Basic[8] -> relevant
    psax::Event set_rb8;
    set_rb8.cmd_id = 0x12000200u;
    set_rb8.args.push_back({psax::ArgType::Value, 0x5459u});
    set_rb8.args.push_back({psax::ArgType::Variable, 0x20000008u});
    CHECK(psax::event_is_sfx_relevant(set_rb8));

    // BasicVariableSet: RA-Basic[10] -> relevant (boundary)
    psax::Event set_rb10 = set_rb8;
    set_rb10.args[1].raw_value = 0x2000000Au;
    CHECK(psax::event_is_sfx_relevant(set_rb10));

    // BasicVariableSet: RA-Basic[11] -> NOT relevant (just past range)
    psax::Event set_rb11 = set_rb8;
    set_rb11.args[1].raw_value = 0x2000000Bu;
    CHECK_FALSE(psax::event_is_sfx_relevant(set_rb11));

    // BasicVariableSet: LA-Basic[8] -> NOT relevant (wrong memory class)
    psax::Event set_la8 = set_rb8;
    set_la8.args[1].raw_value = 0x10000008u;
    CHECK_FALSE(psax::event_is_sfx_relevant(set_la8));

    // BasicVariableSet: RA-Bit[8] -> NOT relevant (wrong data type)
    psax::Event set_rb_bit = set_rb8;
    set_rb_bit.args[1].raw_value = 0x22000008u;
    CHECK_FALSE(psax::event_is_sfx_relevant(set_rb_bit));

    // Arbitrary other event
    psax::Event other;
    other.cmd_id = 0x00020100u;  // Async Timer
    CHECK_FALSE(psax::event_is_sfx_relevant(other));
}

TEST_CASE("audit_sfx: FitMario has a non-trivial number of SFX-relevant subaction-tabs") {
    auto pac = psax::PacFile::load(sample("FitMario.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);

    const auto results = psax::audit_sfx(ms);
    // Mario has many attacks and sound cues; expect at least a few dozen hits.
    CHECK(results.size() > 20u);

    // Every result should have at least one relevant event.
    for (const auto& r : results) {
        CAPTURE(r.subaction_id);
        CAPTURE(r.tab_label);
        CHECK(!r.events.empty());
        for (const auto& ev : r.events) {
            CHECK(psax::event_is_sfx_relevant(ev));
        }
    }
}
