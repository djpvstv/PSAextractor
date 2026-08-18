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

psax::MiscSection open_misc(const char* pac_name) {
    static thread_local std::vector<uint8_t> keep_alive;
    auto pac = psax::PacFile::load(sample(pac_name));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    // Own a copy of the bytes so the MiscSection view stays valid past the PacFile.
    keep_alive.assign(pac.entry_data(*misc), pac.entry_data(*misc) + misc->length);
    return psax::MiscSection(keep_alive.data(), keep_alive.size());
}
} // namespace

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

TEST_CASE("MISC header: known counts") {
    // Values observed and cross-checked with our hex dumps.
    struct Case { const char* file; uint32_t data_ct; uint32_t ext_ct; };
    const Case cases[] = {
        {"FitMario.pac", 1,   79},
        {"FitWolf.pac",  2,   79},
        {"FitKirby.pac", 37,  80},
        {"Fighter.pac",  195, 0 },
    };
    for (const auto& c : cases) {
        CAPTURE(c.file);
        auto pac = psax::PacFile::load(sample(c.file));
        auto misc = pac.find_misc_data();
        REQUIRE(misc);
        psax::MiscSection ms(pac.entry_data(*misc), misc->length);
        CHECK(ms.header().data_table_count == c.data_ct);
        CHECK(ms.header().extern_sub_count == c.ext_ct);
    }
}

TEST_CASE("MISC header: reserved padding words are zero") {
    // Bytes 0x14..0x1F are all zero across every sample we looked at.
    for (const char* name : { "FitMario.pac", "FitWolf.pac", "FitKirby.pac",
                              "Fighter.pac" }) {
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

TEST_CASE("MISC tables fit within the section") {
    for (const char* name : { "FitMario.pac", "FitGanon.pac", "FitWolf.pac",
                              "FitKirby.pac", "Fighter.pac" }) {
        CAPTURE(name);
        auto pac = psax::PacFile::load(sample(name));
        auto misc = pac.find_misc_data();
        REQUIRE(misc);
        psax::MiscSection ms(pac.entry_data(*misc), misc->length);
        const auto& h = ms.header();
        CHECK(h.data_table_offset + h.data_table_count * 8u <= misc->length);
        CHECK(h.extern_sub_offset + h.extern_sub_count * 8u <= misc->length);
    }
}

TEST_CASE("Data table entries have valid name offsets") {
    for (const char* name : { "FitMario.pac", "FitWolf.pac", "FitKirby.pac",
                              "Fighter.pac" }) {
        CAPTURE(name);
        auto pac = psax::PacFile::load(sample(name));
        auto misc = pac.find_misc_data();
        REQUIRE(misc);
        psax::MiscSection ms(pac.entry_data(*misc), misc->length);
        for (std::size_t i = 0; i < ms.data_table().size(); ++i) {
            const auto& d = ms.data_table()[i];
            CAPTURE(i);
            CAPTURE(d.name_offset);
            CAPTURE(d.data_offset);
            // Name must point inside the section (or be zero if unnamed —
            // but we haven't seen zero yet; leave it strict for now).
            CHECK(d.name_offset > 0);
            CHECK(d.name_offset < misc->length);
            // First char of a name should be printable ASCII (all PSA names
            // observed are alphanumeric + underscore).
            uint8_t c = pac.entry_data(*misc)[d.name_offset];
            CHECK((c >= 0x20 && c < 0x7F));
        }
    }
}

TEST_CASE("External sub entries have valid name offsets") {
    for (const char* name : { "FitMario.pac", "FitWolf.pac", "FitKirby.pac" }) {
        CAPTURE(name);
        auto pac = psax::PacFile::load(sample(name));
        auto misc = pac.find_misc_data();
        REQUIRE(misc);
        psax::MiscSection ms(pac.entry_data(*misc), misc->length);
        for (std::size_t i = 0; i < ms.external_subs().size(); ++i) {
            const auto& e = ms.external_subs()[i];
            CAPTURE(i);
            CAPTURE(e.name_offset);
            CHECK(e.name_offset < misc->length);
            uint8_t c = pac.entry_data(*misc)[e.name_offset];
            CHECK((c >= 0x20 && c < 0x7F));
        }
    }
}
