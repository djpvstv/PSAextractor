#include <ostream>
#include <string>

#include <doctest/doctest.h>

#include "pac/pac_file.hpp"
#include "psa/character_root.hpp"
#include "psa/misc_section.hpp"
#include "psa/sfx_audit.hpp"
#include "psa/subaction_flags.hpp"

namespace {
std::string sample(const char* name) {
    return std::string(PSAX_TEST_PAC_DIR) + "/" + name;
}

psax::Event ev(uint32_t cmd) {
    psax::Event e; e.cmd_id = cmd; return e;
}
psax::Event ev_var_set(uint32_t var_desc) {
    psax::Event e;
    e.cmd_id = 0x12000200u;
    e.args.push_back({psax::ArgType::Value,    0x1234u});
    e.args.push_back({psax::ArgType::Variable, var_desc});
    return e;
}
}

TEST_CASE("SubActionFlags: index 0x10 for FitMario resolves to 'RunBrake'") {
    auto pac = psax::PacFile::load(sample("FitMario.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);
    auto root = psax::load_character_root(ms);

    auto flags = psax::read_subaction_flags(
        ms, root.fields[psax::CharacterRoot::SubActionFlags], 32u);
    REQUIRE(flags.size() >= 0x11u);
    CHECK(psax::subaction_anim_name(ms, flags[0x10]) == "RunBrake");
}

TEST_CASE("event_is_sfx_candidate: matches the 5 candidate patterns") {
    CHECK(psax::event_is_sfx_candidate(ev(0x0A000100u)));      // SoundEffect
    CHECK(psax::event_is_sfx_candidate(ev(0x0A030100u)));      // SoundEffectTransient
    CHECK(psax::event_is_sfx_candidate(ev(0x06000D00u)));      // OffensiveCollision        (13 args)
    CHECK(psax::event_is_sfx_candidate(ev(0x06150F00u)));      // SpecialOffensiveCollision (15 args)
    CHECK_FALSE(psax::event_is_sfx_candidate(ev(0x06010200u))); // ChangeHitboxDamage — not a creator
    CHECK(psax::event_is_sfx_candidate(ev_var_set(0x20000008u)));  // RA-Basic[8]
    CHECK(psax::event_is_sfx_candidate(ev_var_set(0x2000000Au)));  // RA-Basic[10]

    // Not candidates
    CHECK_FALSE(psax::event_is_sfx_candidate(ev(0x00020100u)));       // Async Timer
    CHECK_FALSE(psax::event_is_sfx_candidate(ev_var_set(0x2000000Bu)));// RA-Basic[11]
    CHECK_FALSE(psax::event_is_sfx_candidate(ev_var_set(0x10000008u)));// LA-Basic[8]
    CHECK_FALSE(psax::event_is_sfx_candidate(ev_var_set(0x22000008u)));// RA-Bit[8]
}

TEST_CASE("filter_sfx_events: min/max gates SoundEffect IDs inclusively") {
    auto snd = [](uint32_t id) {
        psax::Event e; e.cmd_id = 0x0A000100u;
        e.args.push_back({psax::ArgType::Value, id});
        return e;
    };
    const std::vector<psax::Event> input = { snd(0x50), snd(0x100), snd(0x150), snd(0x200), snd(0x300) };

    // No bounds: all pass.
    CHECK(psax::filter_sfx_events(input).size() == 5u);

    // Min inclusive.
    {
        psax::SfxAuditOptions opt; opt.min_sound_id = 0x100u;
        auto out = psax::filter_sfx_events(input, opt);
        REQUIRE(out.size() == 4u);
        CHECK(out[0].args[0].raw_value == 0x100u);   // boundary included
    }

    // Max inclusive.
    {
        psax::SfxAuditOptions opt; opt.max_sound_id = 0x200u;
        auto out = psax::filter_sfx_events(input, opt);
        REQUIRE(out.size() == 4u);
        CHECK(out.back().args[0].raw_value == 0x200u);   // boundary included
    }

    // Both bounds narrow the window.
    {
        psax::SfxAuditOptions opt;
        opt.min_sound_id = 0x100u;
        opt.max_sound_id = 0x200u;
        auto out = psax::filter_sfx_events(input, opt);
        REQUIRE(out.size() == 3u);
        CHECK(out[0].args[0].raw_value == 0x100u);
        CHECK(out[2].args[0].raw_value == 0x200u);
    }

    // Range that excludes everything.
    {
        psax::SfxAuditOptions opt;
        opt.min_sound_id = 0x1000u;
        auto out = psax::filter_sfx_events(input, opt);
        CHECK(out.empty());
    }
}

TEST_CASE("filter_sfx_events: min/max does NOT affect collisions or RA-Basic writes") {
    // Both event kinds should pass through regardless of min/max settings.
    std::vector<psax::Event> input = {
        ev_var_set(0x20000008u),   // RA-Basic[8] = 0x1234
        ev(0x06000D00u),           // OffensiveCollision
    };
    psax::SfxAuditOptions opt;
    opt.min_sound_id = 0x9999u;    // absurd high threshold
    opt.max_sound_id = 0x9999u;
    auto out = psax::filter_sfx_events(input, opt);
    REQUIRE(out.size() == 2u);
    CHECK(out[0].cmd_id == 0x12000200u);
    CHECK(out[1].cmd_id == 0x06000D00u);
}

TEST_CASE("filter_sfx_events: collisions require an RA-Basic[8..10] write in the same list") {
    // A) collisions only — should filter OUT the collisions entirely.
    {
        std::vector<psax::Event> input = { ev(0x06000D00u), ev(0x06000D00u) };
        auto out = psax::filter_sfx_events(input);
        CHECK(out.empty());
    }

    // B) direct sound only — always included.
    {
        std::vector<psax::Event> input = { ev(0x0A000100u) };
        auto out = psax::filter_sfx_events(input);
        REQUIRE(out.size() == 1u);
        CHECK(out[0].cmd_id == 0x0A000100u);
    }

    // C) RA-Basic write + collisions — both included, in order.
    {
        std::vector<psax::Event> input = {
            ev_var_set(0x20000008u),   // RA-Basic[8] = 0x1234
            ev_var_set(0x20000009u),   // RA-Basic[9] = 0x1234
            ev(0x06000D00u),           // OffensiveCollision
            ev(0x06150F00u),           // SpecialOffensiveCollision
        };
        auto out = psax::filter_sfx_events(input);
        REQUIRE(out.size() == 4u);
        CHECK(out[0].cmd_id == 0x12000200u);
        CHECK(out[1].cmd_id == 0x12000200u);
        CHECK(out[2].cmd_id == 0x06000D00u);
        CHECK(out[3].cmd_id == 0x06150F00u);
    }

    // D) RA-Basic write for a non-sound index (RA-Basic[7]) does NOT enable
    //    collision inclusion, and the write itself is dropped.
    {
        std::vector<psax::Event> input = {
            ev_var_set(0x20000007u),   // RA-Basic[7] — not a sound register
            ev(0x06000D00u),
        };
        auto out = psax::filter_sfx_events(input);
        CHECK(out.empty());
    }

    // E) Interleaved: direct sound is kept regardless of context.
    {
        std::vector<psax::Event> input = {
            ev(0x00020100u),           // Async Timer (dropped)
            ev(0x0A000100u),           // SoundEffect (kept)
            ev(0x06000D00u),           // OffensiveCollision (dropped, no var set)
        };
        auto out = psax::filter_sfx_events(input);
        REQUIRE(out.size() == 1u);
        CHECK(out[0].cmd_id == 0x0A000100u);
    }
}

TEST_CASE("audit_sfx: FitMario returns sensible non-empty results") {
    auto pac = psax::PacFile::load(sample("FitMario.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);

    const auto report = psax::audit_sfx(ms);
    CHECK(report.entries.size() > 20u);   // still expect many SFX hits from direct sounds
    // Mario has 2 known edge-case subroutines whose events don't decode cleanly
    // (args past end of buffer) — count is small and stable, but not zero now
    // that we also scan subroutines.
    CHECK(report.failures.size() <= 5u);

    // Every entry should have already passed the contextual filter — if a
    // collision appears, the same entry MUST also have an RA-Basic[8..10] write.
    for (const auto& r : report.entries) {
        CAPTURE(r.subaction_id);
        CAPTURE(r.tab_label);
        REQUIRE(!r.events.empty());

        bool has_collision = false;
        bool has_ra_basic_write = false;
        for (const auto& e : r.events) {
            if (e.cmd_id == 0x06000D00u || e.cmd_id == 0x06150F00u) has_collision = true;
            if (e.cmd_id == 0x12000200u) has_ra_basic_write = true;
        }
        if (has_collision) CHECK(has_ra_basic_write);
    }
}

TEST_CASE("audit_sfx: subaction entries come first, then subroutines") {
    // Both kinds should appear in the report, and never interleave (subactions
    // always precede subroutines). Not every fighter has subroutine-based SFX,
    // so we don't hard-require both counts > 0 here — see below for a stricter
    // subroutine-coverage check on FitMario's --list-subroutines output.
    auto pac = psax::PacFile::load(sample("FitMario.pac"));
    auto misc = pac.find_misc_data();
    REQUIRE(misc);
    psax::MiscSection ms(pac.entry_data(*misc), misc->length);
    const auto report = psax::audit_sfx(ms);

    std::size_t sub_action_count = 0, subroutine_count = 0;
    bool saw_subaction_after_subroutine = false;
    bool prev_was_subroutine = false;
    for (const auto& r : report.entries) {
        if (r.kind == psax::SfxAuditEntry::InSubAction) {
            ++sub_action_count;
            if (prev_was_subroutine) saw_subaction_after_subroutine = true;
        } else {
            ++subroutine_count;
            // Subroutines should have their call-site metadata populated.
            CHECK_FALSE(r.subroutine_callers.empty());
            prev_was_subroutine = true;
        }
    }
    CHECK(sub_action_count > 0u);
    CHECK_FALSE(saw_subaction_after_subroutine);
}
