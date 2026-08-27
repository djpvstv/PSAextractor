#include <ostream>
#include <string>

#include <doctest/doctest.h>

#include "pac/pac_file.hpp"
#include "psa/event.hpp"
#include "psa/misc_section.hpp"
#include "psa/variable_audit.hpp"

namespace {
std::string sample(const char* name) {
    return std::string(PSAX_TEST_PAC_DIR) + "/" + name;
}

psax::Event ev(uint32_t cmd) { psax::Event e; e.cmd_id = cmd; return e; }
psax::Event ev_var(uint32_t cmd, psax::ArgType t, uint32_t val) {
    psax::Event e; e.cmd_id = cmd;
    e.args.push_back({t, val});
    return e;
}
}

TEST_CASE("event_touches_variable: any Variable arg matches when no target set") {
    // Event with a Variable arg -> touches variables
    auto e1 = ev_var(0x120A0100u, psax::ArgType::Variable, 0x22000008u);
    CHECK(psax::event_touches_variable(e1));

    // Event with only non-Variable args -> doesn't touch variables
    auto e2 = ev_var(0x00020100u, psax::ArgType::Scalar, 0x927C0u);
    CHECK_FALSE(psax::event_touches_variable(e2));

    // Event with no args at all
    auto e3 = ev(0x64000000u);
    CHECK_FALSE(psax::event_touches_variable(e3));
}

TEST_CASE("event_touches_variable: target filter matches exact raw only") {
    auto e = ev_var(0x120A0100u, psax::ArgType::Variable, 0x22000008u);

    psax::VarAuditOptions match;
    match.has_target = true; match.target_var_raw = 0x22000008u;
    CHECK(psax::event_touches_variable(e, match));

    psax::VarAuditOptions miss;
    miss.has_target = true; miss.target_var_raw = 0x22000009u;
    CHECK_FALSE(psax::event_touches_variable(e, miss));
}

TEST_CASE("parse_variable_descriptor: DSL form") {
    struct C { const char* s; uint32_t expected; };
    const C cases[] = {
        {"RA-Basic[8]",      0x20000008u},
        {"RA-Basic[9]",      0x20000009u},
        {"LA-Basic[9]",      0x10000009u},
        {"IC-Basic[20003]",  0x00004E23u},
        {"RA-Float[8]",      0x21000008u},
        {"RA-Bit[8]",        0x22000008u},
        {"RA-Bit[16]",       0x22000010u},
    };
    for (const auto& c : cases) {
        CAPTURE(c.s);
        uint32_t got = 0;
        REQUIRE(psax::parse_variable_descriptor(c.s, got));
        CHECK(got == c.expected);
    }
}

TEST_CASE("parse_variable_descriptor: raw hex form") {
    uint32_t got = 0;
    REQUIRE(psax::parse_variable_descriptor("0x20000008", got));
    CHECK(got == 0x20000008u);
    REQUIRE(psax::parse_variable_descriptor("0X22000010", got));
    CHECK(got == 0x22000010u);
}

TEST_CASE("parse_variable_descriptor: rejects malformed input") {
    uint32_t got = 0;
    CHECK_FALSE(psax::parse_variable_descriptor("", got));
    CHECK_FALSE(psax::parse_variable_descriptor("not a var", got));
    CHECK_FALSE(psax::parse_variable_descriptor("XX-Basic[8]", got));
    CHECK_FALSE(psax::parse_variable_descriptor("RA-Foo[8]", got));
    CHECK_FALSE(psax::parse_variable_descriptor("RA-Basic[]", got));
    CHECK_FALSE(psax::parse_variable_descriptor("RA-Basic[8", got));
    CHECK_FALSE(psax::parse_variable_descriptor("RA-Basic[8]extra", got));
    CHECK_FALSE(psax::parse_variable_descriptor("0xZZ", got));
}

TEST_CASE("audit_variables: FitMario returns many locations without a target") {
    auto pac = psax::PacFile::load(sample("FitMario.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);
    const auto report = psax::audit_variables(ms);

    // Mario touches variables constantly (RA-Bit flags for interrupts,
    // Basic vars for state, etc.). Empirically ~50 locations for FitMario.
    CHECK(report.entries.size() >= 20u);

    // Every event in every entry must actually touch a variable.
    for (const auto& r : report.entries) {
        REQUIRE(!r.events.empty());
        for (const auto& e : r.events) {
            CAPTURE(e.cmd_id);
            CHECK(psax::event_touches_variable(e));
        }
    }
}

TEST_CASE("audit_variables: RA-Bit[16] filter returns only that variable") {
    auto pac = psax::PacFile::load(sample("FitMario.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);

    psax::VarAuditOptions opt;
    opt.has_target = true;
    REQUIRE(psax::parse_variable_descriptor("RA-Bit[16]", opt.target_var_raw));
    CHECK(opt.target_var_raw == 0x22000010u);

    const auto report = psax::audit_variables(ms, opt);
    // RunBrake (SubAction 0x10) writes RA-Bit[16] so at minimum there's one hit.
    CHECK(report.entries.size() >= 1u);

    // Every event must reference RA-Bit[16] exactly.
    for (const auto& r : report.entries) {
        for (const auto& e : r.events) {
            bool found = false;
            for (const auto& a : e.args) {
                if (a.type == psax::ArgType::Variable && a.raw_value == 0x22000010u) {
                    found = true; break;
                }
            }
            CHECK(found);
        }
    }
}
