#include <ostream>
#include <string>

#include <doctest/doctest.h>

#include <unordered_set>

#include "pac/pac_file.hpp"
#include "psa/character_root.hpp"
#include "psa/event.hpp"
#include "psa/misc_section.hpp"
#include "psa/subaction_table.hpp"
#include "psa/subroutine_scan.hpp"

namespace {
std::string sample(const char* name) {
    return std::string(PSAX_TEST_PAC_DIR) + "/" + name;
}
}

TEST_CASE("event_subroutine_target: recognises SubRoutine, Goto, ConcurrentInfiniteLoop") {
    // SubRoutine + Pointer(0x18E78) -> 0x18E78 (verified against user's SpecialHi1)
    psax::Event sub;
    sub.cmd_id = 0x00070100u;
    sub.args.push_back({psax::ArgType::Pointer, 0x18E78u});
    CHECK(psax::event_subroutine_target(sub) == 0x18E78u);

    // Goto + Pointer(0x1234) -> 0x1234
    psax::Event gt;
    gt.cmd_id = 0x00090100u;
    gt.args.push_back({psax::ArgType::Pointer, 0x1234u});
    CHECK(psax::event_subroutine_target(gt) == 0x1234u);

    // ConcurrentInfiniteLoop: find first pointer-typed arg
    psax::Event cil;
    cil.cmd_id = 0x0D000200u;
    cil.args.push_back({psax::ArgType::Value,   0x42u});
    cil.args.push_back({psax::ArgType::Pointer, 0xABCDu});
    CHECK(psax::event_subroutine_target(cil) == 0xABCDu);

    // Not a subroutine-invoking command
    psax::Event other;
    other.cmd_id = 0x00020100u;   // Async Timer
    other.args.push_back({psax::ArgType::Scalar, 0x927C0u});
    CHECK(psax::event_subroutine_target(other) == 0u);

    // SubRoutine with wrong arg type (not Pointer/Value) - treat as no target.
    psax::Event bad_type;
    bad_type.cmd_id = 0x00070100u;
    bad_type.args.push_back({psax::ArgType::Variable, 0x22000008u});
    CHECK(psax::event_subroutine_target(bad_type) == 0u);
}

TEST_CASE("collect_subroutines: no discovered subroutine overlaps a SubAction entry") {
    // Goto / ConcurrentInfiniteLoop / SubRoutine sometimes point back at an
    // existing SubAction<X>[i] entry point. Those must NOT be reported as
    // subroutines - they'd be dupes of what the subaction scan already covers.
    for (const char* name : {"FitMario.pac", "FitGanon.pac", "FitWolf.pac",
                             "FitKirby.pac"}) {
        CAPTURE(name);
        auto pac = psax::PacFile::load(sample(name));
        auto misc = pac.find_misc_data();
        REQUIRE(misc);
        psax::MiscSection ms(pac.entry_data(*misc), misc->length);

        // Collect every subaction event-list pointer for cross-check.
        auto root = psax::load_character_root(ms);
        const std::size_t count = psax::subaction_main_count(
            root.fields[psax::CharacterRoot::SubActionMain],
            root.fields[psax::CharacterRoot::SubActionGFX]);
        std::unordered_set<uint32_t> subaction_ptrs;
        const psax::CharacterRoot::Field tab_fields[] = {
            psax::CharacterRoot::SubActionMain,
            psax::CharacterRoot::SubActionGFX,
            psax::CharacterRoot::SubActionSFX,
            psax::CharacterRoot::SubActionOther,
        };
        for (auto f : tab_fields) {
            auto t = psax::read_subaction_table(ms, root.fields[f], count);
            for (uint32_t p : t) {
                if (p != 0u && p != 0xFFFFFFFFu) subaction_ptrs.insert(p);
            }
        }

        const auto subs = psax::collect_subroutines(ms);
        for (const auto& s : subs) {
            CAPTURE(s.stored_ptr);
            CHECK(subaction_ptrs.count(s.stored_ptr) == 0u);
        }
    }
}

TEST_CASE("collect_subroutines: FitMario finds zero real subroutines") {
    // Mario's 169 SubRoutine/Goto calls all target either existing subactions
    // (correctly deduped) or throw-parameter blocks / garbage (correctly
    // rejected by the event-list-start heuristic). Zero is the right answer.
    auto pac = psax::PacFile::load(sample("FitMario.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);
    const auto subs = psax::collect_subroutines(ms);
    CHECK(subs.empty());
}

TEST_CASE("collect_subroutines: FitKirby finds real subroutines with callers") {
    // Kirby's copy-ability logic uses real subroutines. Assert we find some,
    // each has a caller, and results are sorted by offset.
    auto pac = psax::PacFile::load(sample("FitKirby.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);

    const auto subs = psax::collect_subroutines(ms);
    CHECK(subs.size() >= 1u);

    for (const auto& s : subs) {
        CAPTURE(s.stored_ptr);
        CHECK(!s.callers.empty());
        // Every discovered subroutine decoded cleanly (heuristic did its job).
        CHECK(s.decode_error.empty());
    }
    for (std::size_t i = 1; i < subs.size(); ++i) {
        CHECK(subs[i - 1].resolved_offset < subs[i].resolved_offset);
    }
}
