#include <algorithm>
#include <filesystem>
#include <ostream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "pac/pac_file.hpp"
#include "psa/character_root.hpp"
#include "psa/misc_section.hpp"
#include "psa/sfx_audit.hpp"
#include "psa/subaction_flags.hpp"
#include "psa/subaction_table.hpp"
#include "psa/subroutine_scan.hpp"
#include "psa/variable_audit.hpp"

namespace {

std::vector<std::filesystem::path> discover_pacs() {
    namespace fs = std::filesystem;
    std::vector<fs::path> out;
    for (const auto& entry : fs::directory_iterator(PSAX_TEST_PAC_DIR)) {
        if (entry.is_regular_file() && entry.path().extension() == ".pac") {
            out.push_back(entry.path());
        }
    }
    // Deterministic order so test failures reproduce consistently.
    std::sort(out.begin(), out.end());
    return out;
}

// Fighter.pac has a different layout (no data-table entry named "data") — its
// character-level features aren't decoded yet. All other files should be
// treated as character-style fighters.
bool is_character_style(const std::string& fname) {
    return fname != "Fighter.pac";
}

} // namespace

TEST_CASE("regression sweep: every PAC opens and its ARC + MISC parse succeeds") {
    for (const auto& path : discover_pacs()) {
        const std::string fname = path.filename().string();
        CAPTURE(fname);
        auto pac = psax::PacFile::load(path.string());
        CHECK(pac.size() > 0);
        auto misc = pac.find_misc_data();
        REQUIRE(misc);
        CHECK_NOTHROW((void)psax::MiscSection(pac.entry_data(*misc), misc->length));
    }
}

TEST_CASE("regression sweep: character-style PACs run all top-level operations "
          "without throwing") {
    for (const auto& path : discover_pacs()) {
        const std::string fname = path.filename().string();
        if (!is_character_style(fname)) continue;
        CAPTURE(fname);

        auto pac = psax::PacFile::load(path.string());
        auto misc = pac.find_misc_data();
        REQUIRE(misc);
        psax::MiscSection ms(pac.entry_data(*misc), misc->length);

        CHECK_NOTHROW((void)psax::load_character_root(ms));
        CHECK_NOTHROW((void)psax::audit_sfx(ms));
        CHECK_NOTHROW((void)psax::audit_variables(ms));
        CHECK_NOTHROW((void)psax::collect_subroutines(ms));
    }
}

TEST_CASE("regression sweep: no subroutine decode failures on any PAC "
          "(heuristic filter is doing its job)") {
    for (const auto& path : discover_pacs()) {
        const std::string fname = path.filename().string();
        if (!is_character_style(fname)) continue;
        CAPTURE(fname);

        auto pac = psax::PacFile::load(path.string());
        auto misc = pac.find_misc_data();
        REQUIRE(misc);
        psax::MiscSection ms(pac.entry_data(*misc), misc->length);

        const auto subs = psax::collect_subroutines(ms);
        for (const auto& s : subs) {
            CAPTURE(s.stored_ptr);
            CHECK(s.decode_error.empty());
        }
    }
}

TEST_CASE("regression sweep: subactions with content are non-trivial "
          "(>= 50 populated across any tab)") {
    // Sanity: every real character file has plenty of populated subactions.
    for (const auto& path : discover_pacs()) {
        const std::string fname = path.filename().string();
        if (!is_character_style(fname)) continue;
        CAPTURE(fname);

        auto pac = psax::PacFile::load(path.string());
        auto misc = pac.find_misc_data();
        REQUIRE(misc);
        psax::MiscSection ms(pac.entry_data(*misc), misc->length);
        auto root = psax::load_character_root(ms);

        const std::size_t count = psax::subaction_main_count(
            root.fields[psax::CharacterRoot::SubActionMain],
            root.fields[psax::CharacterRoot::SubActionGFX]);

        std::size_t populated = 0;
        const psax::CharacterRoot::Field tab_fields[] = {
            psax::CharacterRoot::SubActionMain,
            psax::CharacterRoot::SubActionGFX,
            psax::CharacterRoot::SubActionSFX,
            psax::CharacterRoot::SubActionOther,
        };
        std::vector<std::vector<uint32_t>> tables;
        for (auto f : tab_fields) {
            tables.push_back(psax::read_subaction_table(ms, root.fields[f], count));
        }
        for (std::size_t i = 0; i < count; ++i) {
            for (const auto& t : tables) {
                if (i < t.size() && t[i] != 0u && t[i] != 0xFFFFFFFFu) {
                    ++populated; break;
                }
            }
        }
        CHECK(populated >= 50u);
    }
}
