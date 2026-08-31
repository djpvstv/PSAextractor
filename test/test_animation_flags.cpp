#include <ostream>
#include <string>

#include <doctest/doctest.h>

#include "pac/pac_file.hpp"
#include "psa/animation_flags.hpp"
#include "psa/character_root.hpp"
#include "psa/misc_section.hpp"
#include "psa/subaction_flags.hpp"

namespace {
std::string sample(const char* name) {
    return std::string(PSAX_TEST_PAC_DIR) + "/" + name;
}
}

TEST_CASE("decode_animation_flags: bit layout verified against PSAC source") {
    // Bits 24-31: InTransition; bits 16-23: 8 boolean flags starting at bit 16.
    // See psa/animation_flags.hpp for the mapping.

    // All zero -> everything off, in_transition=0.
    auto z = psax::decode_animation_flags(0x00000000u);
    CHECK(z.in_transition == 0u);
    CHECK_FALSE(z.no_out_transition);
    CHECK_FALSE(z.loop);
    CHECK_FALSE(z.moves_character);
    CHECK(z.low16 == 0u);

    // Bit 16 only -> NoOutTransition
    auto nx = psax::decode_animation_flags(0x00010000u);
    CHECK(nx.no_out_transition);
    CHECK_FALSE(nx.loop);
    CHECK(nx.in_transition == 0u);

    // Bit 17 only -> Loop
    auto lp = psax::decode_animation_flags(0x00020000u);
    CHECK(lp.loop);
    CHECK_FALSE(lp.no_out_transition);

    // Combined: Loop + MovesCharacter with InTransition=6
    auto ws = psax::decode_animation_flags(0x06060000u);
    CHECK(ws.in_transition == 6u);
    CHECK(ws.loop);
    CHECK(ws.moves_character);
    CHECK_FALSE(ws.no_out_transition);
    CHECK_FALSE(ws.unknown7);

    // High bits 20-23 test
    auto hi = psax::decode_animation_flags(0x00F00000u);
    CHECK(hi.unknown4);
    CHECK(hi.unknown5);
    CHECK(hi.transition_out_from_start);
    CHECK(hi.unknown7);
    CHECK_FALSE(hi.loop);

    // Low 16 bits preserved raw
    auto lo = psax::decode_animation_flags(0x0000ABCDu);
    CHECK(lo.low16 == 0xABCDu);
    CHECK(lo.in_transition == 0u);
}

TEST_CASE("format_animation_flags: human-readable string form") {
    CHECK(psax::format_animation_flags(0x00000000u) == "-");
    CHECK(psax::format_animation_flags(0x00010000u) == "NoOutTransition");
    CHECK(psax::format_animation_flags(0x06000000u) == "In=6");
    CHECK(psax::format_animation_flags(0x06060000u) == "In=6, Loop, MovesCharacter");
    CHECK(psax::format_animation_flags(0x00000ABCu) == "low16=0xABC");
}

// Verify against real FitMario SubActionFlags data that our decoding matches
// the values PSAC would display.
TEST_CASE("FitMario known subactions decode to expected animation flags") {
    auto pac = psax::PacFile::load(sample("FitMario.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);
    auto root = psax::load_character_root(ms);
    auto flags = psax::read_subaction_flags(
        ms, root.fields[psax::CharacterRoot::SubActionFlags], 0x20u);

    // Values measured from earlier probes:
    //   Wait1 (0x00) -> 0x06000000 : In=6
    //   WalkSlow (0x0A) -> 0x06060000 : In=6, Loop, MovesCharacter
    //   Dash (0x0E) -> 0x00000000 : none
    //   Turn (0x11) -> 0x00010000 : NoOutTransition
    REQUIRE(flags.size() >= 0x12u);
    CHECK(psax::format_animation_flags(flags[0x00].flags) == "In=6");
    CHECK(psax::format_animation_flags(flags[0x0A].flags) == "In=6, Loop, MovesCharacter");
    CHECK(psax::format_animation_flags(flags[0x0E].flags) == "-");
    CHECK(psax::format_animation_flags(flags[0x11].flags) == "NoOutTransition");
}
