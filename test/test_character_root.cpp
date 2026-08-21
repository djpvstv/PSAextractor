#include <cstdint>
#include <ostream>
#include <string>

#include <doctest/doctest.h>

#include "pac/pac_file.hpp"
#include "psa/character_root.hpp"
#include "psa/event_decoder.hpp"
#include "psa/misc_section.hpp"
#include "psa/subaction_table.hpp"

namespace {
std::string sample(const char* name) {
    return std::string(PSAX_TEST_PAC_DIR) + "/" + name;
}
}

// PSAC's Data > Data Offset tab, verbatim for FitMario.pac.
// These are the STORED pointer values (raw u32s from the file).
TEST_CASE("FitMario CharacterRoot fields match PSAC Data Offset tab") {
    auto pac = psax::PacFile::load(sample("FitMario.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);
    auto root = psax::load_character_root(ms);

    using F = psax::CharacterRoot;
    CHECK(root.fields[F::SubActionFlags]      == 0x1B3E0u);
    CHECK(root.fields[F::ModelVisibility]     == 0x1E17Cu);
    CHECK(root.fields[F::Attributes]          == 0x0u);
    CHECK(root.fields[F::SSEAttributes]       == 0x2E4u);
    CHECK(root.fields[F::MiscSectionOffset]   == 0x1E974u);
    CHECK(root.fields[F::CommonActionFlags]   == 0x5C8u);
    CHECK(root.fields[F::SpecialActionFlags]  == 0x19570u);
    CHECK(root.fields[F::ExtraActionFlags]    == 0x16E8u);
    CHECK(root.fields[F::ActionInterrupts]    == 0x1E744u);
    CHECK(root.fields[F::EntrySpecials]       == 0x195E0u);
    CHECK(root.fields[F::ExitSpecials]        == 0x195FCu);
    CHECK(root.fields[F::ActionPre]           == 0x19618u);
    CHECK(root.fields[F::SubActionMain]       == 0x1C2D0u);
    CHECK(root.fields[F::SubActionGFX]        == 0x1CA48u);
    CHECK(root.fields[F::SubActionSFX]        == 0x1D1C0u);
    CHECK(root.fields[F::SubActionOther]      == 0x1D938u);
    CHECK(root.fields[F::EntryArticle]        == 0x1E59Cu);
    CHECK(root.fields[F::DataFlags1]          == 0x1u);
    CHECK(root.fields[F::DataFlags2]          == 0x735Fu);
    CHECK(root.fields[F::DataFlags3]          == 0xFFFFFFFFu);
}

// SubActionMain entries are 4 bytes each (u32 event_list_ptr), NOT 8 bytes.
// PSAC's "Sub Action N" is `array[N]` directly. Verified for FitMario:
// PSAC says "Sub Action 0x10 -> RunBrake at 0x108D0", and array[0x10] = 0x108D0.
TEST_CASE("FitMario SubActionMain: array length matches PSAC dropdown range") {
    auto pac = psax::PacFile::load(sample("FitMario.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);
    auto root = psax::load_character_root(ms);

    const std::size_t count = psax::subaction_main_count(
        root.fields[psax::CharacterRoot::SubActionMain],
        root.fields[psax::CharacterRoot::SubActionGFX]);
    // PSAC dropdown for FitMario shows 0..0x1DD, so 478 entries.
    CHECK(count == 478u);
}

TEST_CASE("FitMario SubActionMain[0x10] is RunBrake (event_list_ptr = 0x108D0)") {
    auto pac = psax::PacFile::load(sample("FitMario.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);
    auto root = psax::load_character_root(ms);

    const std::size_t count = psax::subaction_main_count(
        root.fields[psax::CharacterRoot::SubActionMain],
        root.fields[psax::CharacterRoot::SubActionGFX]);
    auto sat = psax::read_subaction_table(
        ms, root.fields[psax::CharacterRoot::SubActionMain], count);
    REQUIRE(sat.size() == count);
    CHECK(sat[0x10] == 0x108D0u);
}

// TODO(anim-names): animation names for each SubAction live in a separate
// parallel structure not yet identified. The SubActionMain array only holds
// event_list_ptrs. Non-blocking for event decoding.
