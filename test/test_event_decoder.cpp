#include <ostream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "pac/pac_file.hpp"
#include "psa/command_table.hpp"
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

// Verbatim SpecialOffensiveCollision from a user-supplied SpecialHi1 stream:
//   E=06150F00:0-00460000,0-00000003,0-0000005A,0-00500064,0-00000000,
//              1-000493E0,1-00000000,1-000CD140,1-00000000,1-00000000,
//              1-0002BF20,1-00000000,0-32831C82,0-00000000,0-004FFFD3
// Built here as a synthetic Event (no MISC needed) and pushed through the
// pretty formatter to confirm the 15-arg command decodes cleanly.
TEST_CASE("SpecialOffensiveCollision (0x06150F00) decodes 15 args and names correctly") {
    psax::Event e;
    e.cmd_id = 0x06150F00u;
    // arg_count derived from cmd_id byte 2 = 0x0F = 15
    CHECK(psax::arg_count_of(e.cmd_id) == 15u);

    struct A { psax::ArgType t; uint32_t v; };
    const A args[] = {
        {psax::ArgType::Value,  0x00460000u}, {psax::ArgType::Value,  0x00000003u},
        {psax::ArgType::Value,  0x0000005Au}, {psax::ArgType::Value,  0x00500064u},
        {psax::ArgType::Value,  0x00000000u}, {psax::ArgType::Scalar, 0x000493E0u},
        {psax::ArgType::Scalar, 0x00000000u}, {psax::ArgType::Scalar, 0x000CD140u},
        {psax::ArgType::Scalar, 0x00000000u}, {psax::ArgType::Scalar, 0x00000000u},
        {psax::ArgType::Scalar, 0x0002BF20u}, {psax::ArgType::Scalar, 0x00000000u},
        {psax::ArgType::Value,  0x32831C82u}, {psax::ArgType::Value,  0x00000000u},
        {psax::ArgType::Value,  0x004FFFD3u},
    };
    for (const auto& a : args) e.args.push_back({a.t, a.v});
    REQUIRE(e.args.size() == 15u);

    // Full expected pretty form (Scalars as decimal, Values as 0x…).
    // Arg names come from BrawlCrate's Parameters.txt via command_arg_name().
    const std::string expected =
        "SpecialOffensiveCollision("
        "Bone ID/Hitbox ID=0x460000, Damage=0x3, Trajectory=0x5A, "
        "Weight Knockback/Knockback Growth=0x500064, Shield Damage/Base Knockback=0x0, "
        "Size=5, X Offset=0, Y Offset=14, Z Offset=0, Tripping Rate=0, "
        "Hitlag Multiplier=3, Directional Influence Multiplier=0, "
        "Flags=0x32831C82, Rehit Rate=0x0, Special Flags=0x4FFFD3)";
    CHECK(e.to_pretty_string() == expected);

    // Raw form round-trip.
    const std::string expected_raw =
        "E=06150F00:0-00460000,0-00000003,0-0000005A,0-00500064,0-00000000,"
        "1-000493E0,1-00000000,1-000CD140,1-00000000,1-00000000,"
        "1-0002BF20,1-00000000,0-32831C82,0-00000000,0-004FFFD3";
    CHECK(e.to_raw_string() == expected_raw);
}

TEST_CASE("RunBrake pretty-prints to DSL form matching PSAC semantics") {
    auto pac = psax::PacFile::load(sample("FitMario.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    psax::EventDecoder dec(pac.entry_data(*misc), misc->length);
    auto events = dec.decode(psax::resolve_misc_ptr(0x108D0u));
    REQUIRE(events.size() == 5u);

    CHECK(events[0].to_pretty_string() == "AsynchronousTimer(Frames=10)");
    CHECK(events[1].to_pretty_string() == "BitVariableSet(RA-Bit[16] = true)");
    CHECK(events[2].to_pretty_string() == "BitVariableClear(RA-Bit[18] = false)");
    CHECK(events[3].to_pretty_string() == "AsynchronousTimer(Frames=13)");
    CHECK(events[4].to_pretty_string() == "AllowInterrupt()");
}

TEST_CASE("Variable decoder unpacks memory-class + data-type + index") {
    // User-provided PSAC ground truth examples covering all classes/types:
    //   0x20000008 -> RA-Basic[8]        (RunBrake earlier: 0x22000010 -> RA-Bit[16])
    //   0x20000009 -> RA-Basic[9]
    //   0x10000009 -> LA-Basic[9]
    //   0x00004E23 -> IC-Basic[20003]
    //   0x21000008 -> RA-Float[8]
    //   0x22000008 -> RA-Bit[8]
    // Bit layout: [mc:4][dt:4][index:24]  (was [dt:4][mc:4][...] - swapped).
    struct C { uint32_t raw; const char* expected; };
    const C cases[] = {
        {0x20000008u, "RA-Basic[8]"},
        {0x20000009u, "RA-Basic[9]"},
        {0x10000009u, "LA-Basic[9]"},
        {0x00004E23u, "IC-Basic[20003]"},
        {0x21000008u, "RA-Float[8]"},
        {0x22000008u, "RA-Bit[8]"},
        {0x22000010u, "RA-Bit[16]"},   // RunBrake regression check
        {0x22000012u, "RA-Bit[18]"},
    };
    for (const auto& c : cases) {
        CAPTURE(c.raw);
        CHECK(psax::variable_from_raw(c.raw).to_string() == c.expected);
    }
}

TEST_CASE("BasicVariableSet renders with variable = value order (per PSAC)") {
    // Wire layout is (value, variable); PSAC re-orders to "variable = value".
    // Values (type 0), pointers (type 2), etc. render as hex with 0x prefix.
    // Variable indices render as decimal inside brackets.
    struct C { uint32_t arg0_type; uint32_t arg0_val; uint32_t arg1_type; uint32_t arg1_val; const char* expected; };
    const C cases[] = {
        // "Basic Variable Set: RA-Basic[8] = 0x5459"
        {0, 0x5459u, 5, 0x20000008u, "BasicVariableSet(RA-Basic[8] = 0x5459)"},
        {0, 0x5459u, 5, 0x10000009u, "BasicVariableSet(LA-Basic[9] = 0x5459)"},
        {0, 0x5459u, 5, 0x00004E23u, "BasicVariableSet(IC-Basic[20003] = 0x5459)"},
        {0, 0x5459u, 5, 0x21000008u, "BasicVariableSet(RA-Float[8] = 0x5459)"},
        {0, 0x5459u, 5, 0x22000008u, "BasicVariableSet(RA-Bit[8] = 0x5459)"},
        // 188460 / 60000 = 3.141 exactly (Scalar arg stays decimal)
        {1, 0x2E02Cu, 0, 0x3u, "BasicVariableSet(0x3 = 3.141)"},
        // Pointer arg: also hex-formatted like a Value.
        {2, 0x2E02Cu, 0, 0x3u, "BasicVariableSet(0x3 = 0x2E02C)"},
        {3, 0x1u,     0, 0x3u, "BasicVariableSet(0x3 = true)"},
        // Requirement ID 0 = "Character Exists?" per PSAC's Requirements.txt.
        {6, 0x0u,     0, 0x3u, "BasicVariableSet(0x3 = CharacterExists)"},
        // The specific example the user flagged: value 0x40AF5EDD, RA-Basic[15].
        {0, 0x40AF5EDDu, 5, 0x2000000Fu, "BasicVariableSet(RA-Basic[15] = 0x40AF5EDD)"},
    };
    for (const auto& c : cases) {
        CAPTURE(c.expected);
        psax::Event e;
        e.cmd_id = 0x12000200u;
        e.args.push_back({static_cast<psax::ArgType>(c.arg0_type), c.arg0_val});
        e.args.push_back({static_cast<psax::ArgType>(c.arg1_type), c.arg1_val});
        CHECK(e.to_pretty_string() == c.expected);
    }
}

TEST_CASE("Requirement arg decodes via PSAC's Requirements.txt with negation bit") {
    // User-supplied ground-truth examples:
    //   E=02010200:0-00000019,6-00000003  -> ChangeAction(0x19, OnGround)
    //   E=02010200:0-00000019,6-80000003  -> ChangeAction(0x19, !OnGround)
    psax::Event pos;
    pos.cmd_id = 0x02010200u;
    pos.args.push_back({psax::ArgType::Value,       0x19u});
    pos.args.push_back({psax::ArgType::Requirement, 0x00000003u});
    CHECK(pos.to_pretty_string() == "ChangeAction(Action=0x19, Requirement=OnGround)");

    psax::Event neg = pos;
    neg.args[1].raw_value = 0x80000003u;
    CHECK(neg.to_pretty_string() == "ChangeAction(Action=0x19, Requirement=!OnGround)");

    // Known and unknown IDs isolated at Arg level.
    CHECK(psax::Arg{psax::ArgType::Requirement, 0x00000000u}.to_pretty_string() == "CharacterExists");
    CHECK(psax::Arg{psax::ArgType::Requirement, 0x80000000u}.to_pretty_string() == "!CharacterExists");
    CHECK(psax::Arg{psax::ArgType::Requirement, 0x0000000Eu}.to_pretty_string() == "req(0xE)");   // PSAC placeholder
    CHECK(psax::Arg{psax::ArgType::Requirement, 0x8000000Eu}.to_pretty_string() == "!req(0xE)");
    CHECK(psax::Arg{psax::ArgType::Requirement, 0x00001234u}.to_pretty_string() == "req(0x1234)"); // way out of table
}

// TerminateGraphicEffect (0x11150300) arg names came from
// BrawlCrate as (Graphic, Boolean, Boolean). PSAC's
// Parameters.txt has the better names (Graphic ID, Instant, Boolean).
// This test locks in the PSAC-preferred names against real bytes from
// FitChief.pac's subroutine at stored offset 0x3087C.
TEST_CASE("FitChief subroutine 0x3087C: TerminateGraphicEffect uses PSAC arg names") {
    auto pac = psax::PacFile::load(sample("FitChief.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    psax::EventDecoder dec(pac.entry_data(*misc), misc->length);

    const auto events = dec.decode(psax::resolve_misc_ptr(0x3087Cu));
    REQUIRE(events.size() > 52u);

    const auto& e = events[52];
    CHECK(e.cmd_id == 0x11150300u);   // TerminateGraphicEffect

    // Structural: 3 args of the expected types + values from the raw bytes.
    REQUIRE(e.args.size() == 3u);
    CHECK(e.args[0].type == psax::ArgType::Value);
    CHECK(e.args[0].raw_value == 0x01FB0015u);
    CHECK(e.args[1].type == psax::ArgType::Boolean);
    CHECK(e.args[1].raw_value == 0x00000001u);   // true
    CHECK(e.args[2].type == psax::ArgType::Boolean);
    CHECK(e.args[2].raw_value == 0x00000001u);   // true

    // Pretty-print: PSAC names win - "Graphic ID" and "Instant", not
    // BrawlCrate's degraded "Graphic" and "Boolean".
    const std::string got = e.to_pretty_string();
    CHECK(got ==
        "TerminateGraphicEffect(Graphic ID=0x1FB0015, Instant=true, Boolean=true)");
}

TEST_CASE("BitVariableSet / Clear render with = true / = false suffix") {
    psax::Event set;
    set.cmd_id = 0x120A0100u;
    set.args.push_back({psax::ArgType::Variable, 0x22000010u});
    CHECK(set.to_pretty_string() == "BitVariableSet(RA-Bit[16] = true)");

    psax::Event clr;
    clr.cmd_id = 0x120B0100u;
    clr.args.push_back({psax::ArgType::Variable, 0x22000012u});
    CHECK(clr.to_pretty_string() == "BitVariableClear(RA-Bit[18] = false)");
}

TEST_CASE("Scalar decoder divides raw by 60000") {
    psax::Arg a1{psax::ArgType::Scalar, 0x000927C0u}; // 600000
    CHECK(a1.to_pretty_string() == "10");

    psax::Arg a2{psax::ArgType::Scalar, 0x000BE6E0u}; // 780000
    CHECK(a2.to_pretty_string() == "13");
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
