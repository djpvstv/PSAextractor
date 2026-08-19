#include <ostream>
#include <cstdint>
#include <string>

#include <doctest/doctest.h>

#include "pac/pac_file.hpp"
#include "psa/misc_section.hpp"

namespace {
std::string sample(const char* name) {
    return std::string(PSAX_TEST_PAC_DIR) + "/" + name;
}
}

TEST_CASE("MISC header: file_size matches ARC entry length") {
    for (const char* name : { "FitMario.pac", "FitGanon.pac", "FitWolf.pac",
                              "FitKirby.pac", "Fighter.pac" }) {
        CAPTURE(name);
        auto pac = psax::PacFile::load(sample(name));
        auto misc = pac.find_misc_data();
        REQUIRE(misc);
        psax::MiscSection ms(pac.entry_data(*misc), misc->length);
        CHECK(ms.header().file_size == misc->length);
    }
}

TEST_CASE("MISC header: reserved bytes 0x14..0x1F are zero") {
    for (const char* name : { "FitMario.pac", "FitGanon.pac", "FitWolf.pac",
                              "FitKirby.pac", "Fighter.pac" }) {
        CAPTURE(name);
        auto pac = psax::PacFile::load(sample(name));
        auto misc = pac.find_misc_data();
        REQUIRE(misc);
        const uint8_t* p = pac.entry_data(*misc);
        for (std::size_t i = 0x14; i < 0x20; ++i) {
            CAPTURE(i);
            CHECK(p[i] == 0);
        }
    }
}

TEST_CASE("MISC header: measured u32 values") {
    struct C { const char* file; uint32_t w1, w2, w3, w4; };
    const C cases[] = {
        {"FitMario.pac", 0x1EA64, 0x13E0,  1,   79},
        {"FitWolf.pac",  0x23854, 0x15DF,  2,   79},
        {"FitKirby.pac", 0x44EF4, 0x2B1C,  37,  80},
        {"Fighter.pac",  0x24334, 0x157A,  195, 0 },
    };
    for (const auto& c : cases) {
        CAPTURE(c.file);
        auto pac = psax::PacFile::load(sample(c.file));
        auto misc = pac.find_misc_data();
        REQUIRE(misc);
        psax::MiscSection ms(pac.entry_data(*misc), misc->length);
        CHECK(ms.header().word1 == c.w1);
        CHECK(ms.header().word2 == c.w2);
        CHECK(ms.header().word3 == c.w3);
        CHECK(ms.header().word4 == c.w4);
    }
}

// Confirmed empirically via PSAC. String pool location:
//   STRPOOL = word1 + word2*4 + 32 + word3*8 + word4*8
// The 32-byte gap after the lookup table is currently unexplained.
TEST_CASE("FitMario: derived layout matches PSAC ground truth") {
    auto pac = psax::PacFile::load(sample("FitMario.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);

    CHECK(ms.string_pool_start()        == 0x23C84u);
    CHECK(ms.external_sub_table_start() == 0x23A0Cu);
    CHECK(ms.data_table_start()         == 0x23A04u);
    CHECK(ms.data_table().size()        == 1u);
    CHECK(ms.external_subs().size()     == 79u);
}

TEST_CASE("FitMario: data table entry name is 'data' (per PSAC)") {
    auto pac = psax::PacFile::load(sample("FitMario.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);
    REQUIRE(ms.data_table().size() == 1u);
    CHECK(ms.name_at(ms.data_table()[0].name_rel) == "data");
}

TEST_CASE("FitMario: first 15 external subs match PSAC screenshot exactly") {
    // Verbatim from the PSAC "External Sub Routines" tab.
    const char* expected[] = {
        "effectAnimCmd_BatSwing4Common",
        "effectAnimCmd_HarisenSwing4HoldCommon",
        "effectAnimCmd_LipStickSwing4HoldCommon",
        "effectAnimCmd_ScopeAirFireCommon",
        "effectAnimCmd_ScopeAirRapidCommon",
        "effectAnimCmd_ScopeAirStartCommon",
        "effectAnimCmd_ScopeFireUpperCommon",
        "effectAnimCmd_ScopeRapidUpperCommon",
        "effectAnimCmd_ScopeStartUpperCommon",
        "effectAnimCmd_SmashThrowBCommon",
        "effectAnimCmd_SmashThrowFCommon",
        "effectAnimCmd_SmashThrowHiCommon",
        "effectAnimCmd_SmashThrowLwCommon",
        "effectAnimCmd_StarRodSwing4HoldCommon",
        "effectAnimCmd_SwordSwing4HoldCommon",
    };
    auto pac = psax::PacFile::load(sample("FitMario.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);
    REQUIRE(ms.external_subs().size() == 79u);
    for (std::size_t i = 0; i < sizeof(expected)/sizeof(expected[0]); ++i) {
        CAPTURE(i);
        auto n = ms.name_at(ms.external_subs()[i].name_rel);
        CHECK(std::string(n) == expected[i]);
    }
}

// Sanity across all sample PACs: every name_rel dereferences to a NUL-terminated
// identifier (letters, digits, underscore), at least 3 chars long.
TEST_CASE("All ext-sub names in all PACs look like real identifiers") {
    for (const char* name : { "FitMario.pac", "FitGanon.pac", "FitWolf.pac",
                              "FitKirby.pac" }) {
        CAPTURE(name);
        auto pac = psax::PacFile::load(sample(name));
        auto misc = pac.find_misc_data();
        REQUIRE(misc);
        psax::MiscSection ms(pac.entry_data(*misc), misc->length);
        for (std::size_t i = 0; i < ms.external_subs().size(); ++i) {
            CAPTURE(i);
            auto n = ms.name_at(ms.external_subs()[i].name_rel);
            REQUIRE(n.size() >= 3);
            for (char c : n) {
                const bool ok = (c >= '0' && c <= '9')
                             || (c >= 'A' && c <= 'Z')
                             || (c >= 'a' && c <= 'z')
                             || c == '_';
                CHECK(ok);
            }
        }
    }
}
