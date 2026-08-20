#include <ostream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "pac/pac_file.hpp"
#include "psa/event_decoder.hpp"

namespace {
std::string sample(const char* name) {
    return std::string(PSAX_TEST_PAC_DIR) + "/" + name;
}
}

// Ground truth from PSAC (user's copy-paste of RunBrake subaction, Main tab):
//   E=00020100:1-000927C0,E=120A0100:5-22000010,E=120B0100:5-22000012,
//   E=00020100:1-000BE6E0,E=64000000:
//
// Notes on locating the event list:
//   * The SubActionMain entry for this subaction stores event_list_ptr = 0x108D0.
//   * Applying the +32 convention: actual events start at 0x108D0 + 32 = 0x108F0.
//   * Args for the 4 non-empty events sit at 0x108D0..0x108EF (immediately before
//     the event list), each referenced by an event's args_ptr in stored form.
TEST_CASE("FitMario RunBrake: 5 events decode exactly as PSAC displays them") {
    auto pac = psax::PacFile::load(sample("FitMario.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);

    psax::EventDecoder dec(pac.entry_data(*misc), misc->length);

    // The stored event_list_ptr for RunBrake is 0x108D0; resolve it.
    const std::size_t event_list_offset = psax::resolve_misc_ptr(0x108D0u);
    CHECK(event_list_offset == 0x108F0u);

    auto events = dec.decode(event_list_offset);
    REQUIRE(events.size() == 5u);

    // Concatenate raw-string form and compare to PSAC verbatim.
    std::string joined;
    for (std::size_t i = 0; i < events.size(); ++i) {
        if (i > 0) joined += ',';
        joined += events[i].to_raw_string();
    }
    const std::string expected =
        "E=00020100:1-000927C0,E=120A0100:5-22000010,E=120B0100:5-22000012,"
        "E=00020100:1-000BE6E0,E=64000000:";
    CHECK(joined == expected);
}

TEST_CASE("Individual RunBrake events match PSAC field-by-field") {
    auto pac = psax::PacFile::load(sample("FitMario.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    psax::EventDecoder dec(pac.entry_data(*misc), misc->length);
    auto events = dec.decode(psax::resolve_misc_ptr(0x108D0u));
    REQUIRE(events.size() == 5u);

    // Event 0: Asynchronous Timer, Scalar 0x927C0 (= 10.0 frames * 60000).
    CHECK(events[0].cmd_id == 0x00020100u);
    REQUIRE(events[0].args.size() == 1u);
    CHECK(events[0].args[0].type == psax::ArgType::Scalar);
    CHECK(events[0].args[0].raw_value == 0x000927C0u);

    // Event 1: Bit Variable Set, Variable descriptor 0x22000010.
    CHECK(events[1].cmd_id == 0x120A0100u);
    REQUIRE(events[1].args.size() == 1u);
    CHECK(events[1].args[0].type == psax::ArgType::Variable);
    CHECK(events[1].args[0].raw_value == 0x22000010u);

    // Event 2: Bit Variable Clear, Variable descriptor 0x22000012.
    CHECK(events[2].cmd_id == 0x120B0100u);
    REQUIRE(events[2].args.size() == 1u);
    CHECK(events[2].args[0].type == psax::ArgType::Variable);
    CHECK(events[2].args[0].raw_value == 0x22000012u);

    // Event 3: Asynchronous Timer, Scalar 0xBE6E0 (= 13.0 frames * 60000).
    CHECK(events[3].cmd_id == 0x00020100u);
    REQUIRE(events[3].args.size() == 1u);
    CHECK(events[3].args[0].type == psax::ArgType::Scalar);
    CHECK(events[3].args[0].raw_value == 0x000BE6E0u);

    // Event 4: Allow Interrupt, no args.
    CHECK(events[4].cmd_id == 0x64000000u);
    CHECK(events[4].args.empty());
}
