#include <doctest/doctest.h>

#include "psa/argument_schema.hpp"
#include "psa/command_table.hpp"
#include "psa/overrides.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <set>
#include <sstream>
#include <string>

namespace {

namespace fs = std::filesystem;

// RAII scratch dir: unique per test so parallel runs don't collide, and we
// always tear it down so a failing test doesn't leave state behind.
class ScratchDir {
public:
    ScratchDir() {
        std::mt19937_64 rng(std::random_device{}());
        path_ = fs::temp_directory_path()
              / ("psax_overrides_test_" + std::to_string(rng()));
        fs::create_directories(path_);
    }
    ~ScratchDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    const fs::path& path() const { return path_; }
    fs::path file(const char* name) const { return path_ / name; }
private:
    fs::path path_;
};

// Clear any active overrides so tests don't leak state between each other.
struct OverridesGuard {
    ~OverridesGuard() { psax::set_active_overrides({}); }
};

void write_file(const fs::path& p, const std::string& body) {
    std::ofstream f(p, std::ios::binary);
    f << body;
}

} // namespace

TEST_CASE("overrides parser: rename + description of a real command") {
    ScratchDir d;
    write_file(d.file("mod.json"), R"({
        "0xC0DE0100": {
            "name": "MyRenamedHitboxSfx",
            "description": "renamed via overrides",
            "args": [
                { "name": "Hitbox Slot" }
            ]
        }
    })");

    auto m = psax::parse_override_file(d.file("mod.json"));
    REQUIRE(m.count(0xC0DE0100u));
    const auto& e = m[0xC0DE0100u];
    REQUIRE(e.name);
    CHECK(*e.name == "MyRenamedHitboxSfx");
    REQUIRE(e.description);
    CHECK(*e.description == "renamed via overrides");
    REQUIRE(e.args.size() == 1u);
    REQUIRE(e.args[0].name);
    CHECK(*e.args[0].name == "Hitbox Slot");
}

TEST_CASE("overrides parser: cmd_id accepts hex with or without 0x prefix, "
          "case-insensitive") {
    ScratchDir d;
    write_file(d.file("m.json"),
               R"({ "C0DE0100": { "name": "no-prefix" } })");
    auto m = psax::parse_override_file(d.file("m.json"));
    CHECK(m.count(0xC0DE0100u));

    write_file(d.file("m.json"),
               R"({ "0xc0de0100": { "name": "lower" } })");
    auto m2 = psax::parse_override_file(d.file("m.json"));
    CHECK(m2.count(0xC0DE0100u));
}

TEST_CASE("overrides parser: descriptive errors with file:line:col + reason") {
    ScratchDir d;
    OverridesGuard g;

    SUBCASE("unknown field is rejected with allowed-set hint") {
        write_file(d.file("bad.json"), R"({
    "0xC0DE0100": {
        "nmae": "typo"
    }
})");
        try {
            psax::parse_override_file(d.file("bad.json"));
            FAIL("expected throw");
        } catch (const std::exception& e) {
            const std::string msg = e.what();
            CHECK(msg.find("bad.json") != std::string::npos);
            CHECK(msg.find(":3:9") != std::string::npos);
            CHECK(msg.find("unknown field 'nmae'") != std::string::npos);
            CHECK(msg.find("name, description, format, args") != std::string::npos);
        }
    }

    SUBCASE("wrong-length cmd_id key") {
        write_file(d.file("bad.json"), R"({
    "0xABC": {}
})");
        try {
            psax::parse_override_file(d.file("bad.json"));
            FAIL("expected throw");
        } catch (const std::exception& e) {
            const std::string msg = e.what();
            CHECK(msg.find("cmd_id key must be exactly 8 hex digits") != std::string::npos);
            CHECK(msg.find("0xABC") != std::string::npos);
        }
    }

    SUBCASE("non-hex character in cmd_id") {
        write_file(d.file("bad.json"), R"({
    "0xZZZZZZZZ": {}
})");
        try {
            psax::parse_override_file(d.file("bad.json"));
            FAIL("expected throw");
        } catch (const std::exception& e) {
            const std::string msg = e.what();
            CHECK(msg.find("non-hex char 'Z'") != std::string::npos);
        }
    }

    SUBCASE("duplicate cmd_id key") {
        write_file(d.file("bad.json"), R"({
    "0xC0DE0100": { "name": "first" },
    "0xC0DE0100": { "name": "again" }
})");
        try {
            psax::parse_override_file(d.file("bad.json"));
            FAIL("expected throw");
        } catch (const std::exception& e) {
            const std::string msg = e.what();
            CHECK(msg.find("duplicate cmd_id") != std::string::npos);
        }
    }

    SUBCASE("missing colon after field") {
        write_file(d.file("bad.json"), R"({
    "0xC0DE0100": {
        "name" "no-colon"
    }
})");
        try {
            psax::parse_override_file(d.file("bad.json"));
            FAIL("expected throw");
        } catch (const std::exception& e) {
            const std::string msg = e.what();
            CHECK(msg.find("expected ':'") != std::string::npos);
        }
    }
}

TEST_CASE("overrides load_override_dir: alphabetical merge order (later wins)") {
    ScratchDir d;
    write_file(d.file("01-base.json"),
               R"({ "0xC0DE0100": { "name": "base", "description": "kept from base" } })");
    write_file(d.file("02-top.json"),
               R"({ "0xC0DE0100": { "name": "top" } })");
    // Non-json file should be ignored.
    write_file(d.file("notes.txt"),
               R"({ "0xC0DE0100": { "name": "ignored" } })");

    const auto m = psax::load_override_dir(d.path());
    REQUIRE(m.count(0xC0DE0100u));
    const auto& e = m.at(0xC0DE0100u);
    REQUIRE(e.name);
    CHECK(*e.name == "top");                          // 02- overrides 01-
    REQUIRE(e.description);
    CHECK(*e.description == "kept from base");        // 01- survives (deep merge)
}

TEST_CASE("overrides load_override_dir: missing dir returns empty map, no error") {
    ScratchDir d;
    const auto m = psax::load_override_dir(d.path() / "does-not-exist");
    CHECK(m.empty());
}

TEST_CASE("overrides apply: command_name/description consult overrides first") {
    OverridesGuard g;

    // Sanity-check the built-in fallback before we install any override.
    CHECK(std::string(psax::command_name(0xC0DE0100u))
          == "ChangeHitboxSoundEffectCustom");

    psax::OverrideMap m;
    psax::CommandOverride co;
    co.name        = std::string("MyRenamed");
    co.description = std::string("my custom description");
    m.emplace(0xC0DE0100u, std::move(co));
    psax::set_active_overrides(std::move(m));

    CHECK(std::string(psax::command_name(0xC0DE0100u))        == "MyRenamed");
    CHECK(std::string(psax::command_description(0xC0DE0100u)) == "my custom description");

    // Unrelated commands still hit the built-in table.
    CHECK(std::string(psax::command_name(0x00020100u)) == "AsynchronousTimer");
}

TEST_CASE("overrides apply: partial overrides fall through per-field") {
    OverridesGuard g;

    psax::OverrideMap m;
    psax::CommandOverride co;
    co.name = std::string("PartialRename");  // description NOT set
    m.emplace(0xC0DE0100u, std::move(co));
    psax::set_active_overrides(std::move(m));

    CHECK(std::string(psax::command_name(0xC0DE0100u)) == "PartialRename");
    // description not overridden -> falls through to built-in.
    const char* d = psax::command_description(0xC0DE0100u);
    REQUIRE(d != nullptr);
    CHECK(std::string(d).find("hitbox") != std::string::npos);
}

TEST_CASE("overrides apply: command_arg_name overlays argument_schema entries") {
    OverridesGuard g;

    // Baseline: AsynchronousTimer's arg 0 is "Frames" per PSAC.
    REQUIRE(psax::command_arg_name(0x00020100u, 0u));
    CHECK(std::string(psax::command_arg_name(0x00020100u, 0u)) == "Frames");

    psax::OverrideMap m;
    psax::CommandOverride co;
    co.args.resize(1);
    co.args[0].name = std::string("Duration");
    m.emplace(0x00020100u, std::move(co));
    psax::set_active_overrides(std::move(m));

    CHECK(std::string(psax::command_arg_name(0x00020100u, 0u)) == "Duration");
    // Out-of-range still returns nullptr since we defer to schema layer.
    CHECK(psax::command_arg_name(0x00020100u, 5u) == nullptr);
}

TEST_CASE("overrides merge_overrides: deep field-level merge") {
    psax::OverrideMap base;
    {
        psax::CommandOverride co;
        co.name        = std::string("base-name");
        co.description = std::string("base-desc");
        co.args.resize(2);
        co.args[0].name = std::string("arg0-base");
        co.args[1].name = std::string("arg1-base");
        base.emplace(0x11111111u, std::move(co));
    }

    psax::OverrideMap top;
    {
        psax::CommandOverride co;
        co.format = std::string("{0}!");
        co.args.resize(1);
        co.args[0].description = std::string("arg0-top-desc");
        top.emplace(0x11111111u, std::move(co));
    }

    const auto merged = psax::merge_overrides(std::move(base), top);
    const auto& e = merged.at(0x11111111u);
    REQUIRE(e.name);        CHECK(*e.name == "base-name");
    REQUIRE(e.description); CHECK(*e.description == "base-desc");
    REQUIRE(e.format);      CHECK(*e.format == "{0}!");        // added by top
    REQUIRE(e.args.size() == 2u);
    REQUIRE(e.args[0].name);        CHECK(*e.args[0].name == "arg0-base");
    REQUIRE(e.args[0].description); CHECK(*e.args[0].description == "arg0-top-desc");
    REQUIRE(e.args[1].name);        CHECK(*e.args[1].name == "arg1-base");
}

TEST_CASE("overrides parser: empty args array and empty override object are OK") {
    ScratchDir d;
    write_file(d.file("m.json"), R"({
        "0xC0DE0100": {},
        "0x00020100": { "args": [] }
    })");
    auto m = psax::parse_override_file(d.file("m.json"));
    CHECK(m.size() == 2u);
    CHECK(m[0xC0DE0100u].args.empty());
    CHECK(m[0x00020100u].args.empty());
}

TEST_CASE("overrides dump_builtin_command_table: covers union of both tables") {
    std::ostringstream ss;
    psax::dump_builtin_command_table(ss);
    const std::string dump = ss.str();

    // Sanity: must be non-empty and start with a JSON object.
    REQUIRE(!dump.empty());
    CHECK(dump.front() == '{');

    // Every cmd_id from either table should appear as a hex key. Sample a
    // few we care about.
    for (const auto cid : {0x00020100u, 0x11150300u, 0xC0DE0100u}) {
        char key[16];
        std::snprintf(key, sizeof(key), "\"0x%08X\"", cid);
        CHECK_MESSAGE(dump.find(key) != std::string::npos,
                      "dump missing key ", key);
    }

    // Names + arg names we know should be present verbatim in the dump.
    CHECK(dump.find("\"TerminateGraphicEffect\"") != std::string::npos);
    CHECK(dump.find("\"Instant\"")                != std::string::npos);
    CHECK(dump.find("\"AsynchronousTimer\"")      != std::string::npos);
    CHECK(dump.find("\"Frames\"")                 != std::string::npos);
}

TEST_CASE("overrides dump_builtin_command_table: round-trips through the "
          "parser and reproduces every built-in lookup") {
    OverridesGuard g;  // make sure the dump reflects raw built-in, not overrides

    // Snapshot the built-in table BEFORE any override is installed. Values
    // are copied into std::strings so they stay valid across map churn -
    // the pointers returned by command_arg_name() would otherwise dangle
    // the moment we install a different active override map.
    struct Snap {
        std::optional<std::string>                             name, desc, fmt;
        std::vector<std::pair<std::optional<std::string>,
                              std::optional<std::string>>>     args;
    };
    auto snap_ptr = [](const char* p) -> std::optional<std::string> {
        if (!p) return std::nullopt;
        return std::string(p);
    };

    std::set<std::uint32_t> ids;
    for (auto id : psax::all_named_command_ids())  ids.insert(id);
    for (auto id : psax::all_schema_command_ids()) ids.insert(id);

    std::unordered_map<std::uint32_t, Snap> before;
    before.reserve(ids.size());
    for (auto cid : ids) {
        Snap s;
        s.name = snap_ptr(psax::command_name(cid));
        s.desc = snap_ptr(psax::command_description(cid));
        s.fmt  = snap_ptr(psax::command_format(cid));
        const auto slots = psax::schema_arg_slot_count(cid);
        s.args.reserve(slots);
        for (std::uint32_t i = 0; i < slots; ++i) {
            s.args.push_back({snap_ptr(psax::command_arg_name(cid, i)),
                              snap_ptr(psax::command_arg_description(cid, i))});
        }
        before.emplace(cid, std::move(s));
    }

    // Dump -> temp file -> parse (exercises the file loader too).
    std::ostringstream ss;
    psax::dump_builtin_command_table(ss);
    ScratchDir d;
    write_file(d.file("dump.json"), ss.str());
    const auto parsed = psax::parse_override_file(d.file("dump.json"));

    // Apply the parsed dump as active overrides. From here on, accessors
    // return pointers into the active map, but we only compare against
    // the string-owning snapshot, so no dangling.
    psax::set_active_overrides(parsed);

    std::size_t checked = 0;
    for (auto cid : ids) {
        const auto& snap = before.at(cid);
        if (snap.name) {
            const char* got = psax::command_name(cid);
            REQUIRE_MESSAGE(got != nullptr, "post-round-trip name missing for cid ", cid);
            CHECK_MESSAGE(std::string(got) == *snap.name,
                          "name mismatch after round-trip for cid ", cid);
        }
        if (snap.desc) {
            const char* got = psax::command_description(cid);
            REQUIRE_MESSAGE(got != nullptr, "post-round-trip desc missing for cid ", cid);
            CHECK_MESSAGE(std::string(got) == *snap.desc,
                          "desc mismatch after round-trip for cid ", cid);
        }
        if (snap.fmt) {
            const char* got = psax::command_format(cid);
            REQUIRE_MESSAGE(got != nullptr, "post-round-trip fmt missing for cid ", cid);
            CHECK_MESSAGE(std::string(got) == *snap.fmt,
                          "fmt mismatch after round-trip for cid ", cid);
        }
        for (std::uint32_t i = 0; i < snap.args.size(); ++i) {
            const auto& [n_base, d_base] = snap.args[i];
            if (n_base) {
                const char* got = psax::command_arg_name(cid, i);
                REQUIRE_MESSAGE(got != nullptr,
                                "post-round-trip arg name missing for cid ", cid);
                CHECK_MESSAGE(std::string(got) == *n_base,
                              "arg name mismatch after round-trip for cid ", cid);
            }
            if (d_base) {
                const char* got = psax::command_arg_description(cid, i);
                REQUIRE_MESSAGE(got != nullptr,
                                "post-round-trip arg desc missing for cid ", cid);
                CHECK_MESSAGE(std::string(got) == *d_base,
                              "arg desc mismatch after round-trip for cid ", cid);
            }
        }
        ++checked;
    }
    CHECK(checked > 300);  // sanity: we walked the whole table
}

TEST_CASE("overrides init_overrides: writes 00_builtin.json, refuses "
          "second run without --clean, --clean regenerates") {
    OverridesGuard g;

    // Point init at a scratch dir explicitly instead of relying on the
    // exe-relative default (which would write into the test binary's
    // build output directory).
    ScratchDir d;
    psax::InitOverridesOptions opts;
    opts.dir_override = d.path() / "overrides";

    // First run: creates dir + writes dump.
    const auto r1 = psax::init_overrides(opts);
    CHECK(r1.created);
    CHECK(r1.wrote_dump);
    const auto dump_path = r1.path / "00_builtin.json";
    REQUIRE(std::filesystem::exists(dump_path));
    const auto sz1 = std::filesystem::file_size(dump_path);
    CHECK(sz1 > 1000u);   // ~50-100KB expected

    // Second run without --clean: refuses to touch the file.
    // Corrupt the file first so we can detect any accidental rewrite.
    {
        std::ofstream f(dump_path, std::ios::binary);
        f << "SENTINEL";
    }
    const auto r2 = psax::init_overrides(opts);
    CHECK_FALSE(r2.created);        // dir already there
    CHECK_FALSE(r2.wrote_dump);     // key assertion - file left alone
    std::ifstream in(dump_path, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(in)), {});
    CHECK(content == "SENTINEL");

    // Third run with --clean: overwrites.
    opts.clean = true;
    const auto r3 = psax::init_overrides(opts);
    CHECK(r3.wrote_dump);
    CHECK(std::filesystem::file_size(dump_path) > 1000u);
}

TEST_CASE("overrides parser: UTF-8 BOM is stripped") {
    ScratchDir d;
    const std::string body =
        std::string("\xEF\xBB\xBF") +
        R"({ "0xC0DE0100": { "name": "bom-ok" } })";
    write_file(d.file("m.json"), body);
    auto m = psax::parse_override_file(d.file("m.json"));
    REQUIRE(m.count(0xC0DE0100u));
    CHECK(*m[0xC0DE0100u].name == "bom-ok");
}
