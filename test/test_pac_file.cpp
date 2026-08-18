#include <ostream>
#include <cstdint>
#include <string>

#include <doctest/doctest.h>

#include "pac/pac_file.hpp"

namespace {
std::string sample(const char* name) {
    return std::string(PSAX_TEST_PAC_DIR) + "/" + name;
}
}

TEST_CASE("PacFile loads each sample PAC and reports non-zero size") {
    for (const char* name : { "FitMario.pac", "FitGanon.pac", "FitWolf.pac",
                              "FitKirby.pac", "Fighter.pac" }) {
        CAPTURE(name);
        auto pac = psax::PacFile::load(sample(name));
        CHECK(pac.size() > 0);
    }
}

TEST_CASE("ARC header parses correctly") {
    struct Case { const char* file; const char* name; uint16_t nodes; };
    const Case cases[] = {
        {"FitMario.pac", "FitMario", 2},
        {"FitGanon.pac", "FitGanon", 2},
        {"FitWolf.pac",  "FitWolf",  2},
        {"FitKirby.pac", "FitKirby", 2},
        {"Fighter.pac",  "Fighter",  4},
    };
    for (const auto& c : cases) {
        CAPTURE(c.file);
        auto pac = psax::PacFile::load(sample(c.file));
        CHECK(pac.header().name == c.name);
        CHECK(pac.header().version == 0x0101);
        CHECK(pac.header().node_count == c.nodes);
    }
}

TEST_CASE("First ARC entry is MiscData for character PACs") {
    for (const char* name : { "FitMario.pac", "FitGanon.pac",
                              "FitWolf.pac",  "FitKirby.pac" }) {
        CAPTURE(name);
        auto pac = psax::PacFile::load(sample(name));
        REQUIRE(!pac.entries().empty());
        CHECK(pac.entries()[0].file_type == psax::ArcFileType::MiscData);
        CHECK(pac.entries()[0].file_index == 0);
        CHECK(pac.entries()[0].redirect_index == -1);
        CHECK(pac.entries()[0].data_offset == 0x60u);
    }
}

// BrawlCrate reports FitWolf's MiscData "Uncompressed Size (Bytes) 171106".
TEST_CASE("FitWolf MiscData length matches BrawlCrate (171106)") {
    auto pac = psax::PacFile::load(sample("FitWolf.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    CHECK(misc->length == 171106u);
    CHECK(misc->data_offset == 0x60u);
}

TEST_CASE("All parsed entries fit within the file") {
    for (const char* name : { "FitMario.pac", "FitGanon.pac", "FitWolf.pac",
                              "FitKirby.pac", "Fighter.pac" }) {
        CAPTURE(name);
        auto pac = psax::PacFile::load(sample(name));
        REQUIRE(pac.entries().size() == pac.header().node_count);
        for (const auto& e : pac.entries()) {
            CAPTURE(psax::arc_file_type_name(e.file_type));
            CAPTURE(e.file_index);
            CAPTURE(e.data_offset);
            CAPTURE(e.length);
            CHECK(e.data_offset + e.length <= pac.size());
        }
    }
}
