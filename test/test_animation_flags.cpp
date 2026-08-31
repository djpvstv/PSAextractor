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

// Verify against real SubActionFlags data across 4 PACs that our decoding
// matches exactly what PSAC's "Animation Flags" panel displays. All 13
// values below have been visually confirmed by the user against PSAC.
TEST_CASE("Known subactions decode to expected animation flags (PSAC verified)") {
    struct C {
        const char* pac;
        std::size_t id;
        const char* expected;
        const char* anim;   // for CAPTURE context on failure
    };
    const C cases[] = {
        // FitMario
        {"FitMario.pac",    0x00, "In=6",                              "Wait1"},
        {"FitMario.pac",    0x0A, "In=6, Loop, MovesCharacter",        "WalkSlow"},
        {"FitMario.pac",    0x0E, "-",                                 "Dash"},
        {"FitMario.pac",    0x11, "NoOutTransition",                   "Turn"},
        // FitMythra
        {"FitMythra.pac",   0x00, "In=9",                              "Wait1"},
        {"FitMythra.pac",   0x0A, "In=6, Loop, MovesCharacter",        "WalkSlow"},
        {"FitMythra.pac",   0x76, "Unknown3, Unknown5",                "ThrownB"},
        // FitChief
        {"FitChief.pac",    0x00, "In=6",                              "Wait1"},
        {"FitChief.pac",    0x0A, "In=6, Loop, MovesCharacter",        "WalkSlow"},
        {"FitChief.pac",    0x76, "Unknown3, Unknown5",                "ThrownB"},
        // FitGreninja
        {"FitGreninja.pac", 0x00, "In=6",                              "Wait1"},
        {"FitGreninja.pac", 0x0A, "In=6, Loop, MovesCharacter",        "WalkSlow"},
        {"FitGreninja.pac", 0x76, "Unknown3, Unknown5",                "ThrownB"},
        {"FitGreninja.pac", 0x1D6, "In=21",                            "SpecialHiEnd"},
    };
    for (const auto& c : cases) {
        CAPTURE(c.pac);
        CAPTURE(c.id);
        CAPTURE(c.anim);
        auto pac = psax::PacFile::load(sample(c.pac));
        auto misc = pac.find_misc_data();
        REQUIRE(misc);
        psax::MiscSection ms(pac.entry_data(*misc), misc->length);
        auto root = psax::load_character_root(ms);
        auto flags = psax::read_subaction_flags(
            ms, root.fields[psax::CharacterRoot::SubActionFlags], c.id + 1);
        REQUIRE(flags.size() > c.id);
        CHECK(psax::format_animation_flags(flags[c.id].flags) == c.expected);
    }
}
