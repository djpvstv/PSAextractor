// MSVC flags std::getenv as "unsafe" (C4996) and suggests _dupenv_s, but
// _dupenv_s is Windows-only and getenv is portable + fine for read-only
// env lookups.
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "psa/overrides.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace psax {

namespace {

// ------------------------------------------------------------------
// Hand-rolled JSON-ish parser. Strict JSON syntax (no comments, no
// trailing commas). Errors are shaped "<file>:<line>:<col>: <msg>" so
// modders see exactly where their file broke. Every key is validated
// against the small closed set of allowed fields (name/description/
// format/args) so a typo like "nmae" is rejected instead of silently
// dropped.
// ------------------------------------------------------------------
class Parser {
public:
    Parser(std::string src, std::filesystem::path file)
        : src_(std::move(src)), file_(std::move(file)) {}

    OverrideMap parse() {
        skip_ws();
        if (pos_ >= src_.size()) return {};  // empty file = no overrides
        expect('{');
        OverrideMap out;
        skip_ws();
        if (peek() == '}') { advance(); expect_eof(); return out; }
        for (;;) {
            skip_ws();
            const std::size_t key_pos = pos_;
            std::string key = parse_string();
            const std::uint32_t cid = parse_cmd_id(key, key_pos);
            if (out.count(cid)) {
                pos_ = key_pos;
                error("duplicate cmd_id key '" + key + "' in file");
            }
            skip_ws(); expect(':'); skip_ws();
            out.emplace(cid, parse_command_override());
            skip_ws();
            if (peek() == ',') { advance(); continue; }
            expect('}');
            break;
        }
        expect_eof();
        return out;
    }

private:
    [[noreturn]] void error(const std::string& msg) const {
        std::size_t line = 1, col = 1;
        for (std::size_t i = 0; i < pos_ && i < src_.size(); ++i) {
            if (src_[i] == '\n') { ++line; col = 1; }
            else                 { ++col; }
        }
        std::ostringstream os;
        os << file_.string() << ':' << line << ':' << col << ": " << msg;
        throw std::runtime_error(os.str());
    }

    char peek() {
        if (pos_ >= src_.size()) error("unexpected end of file");
        return src_[pos_];
    }
    void advance() { ++pos_; }

    void expect(char c) {
        if (pos_ >= src_.size())
            error(std::string("expected '") + c + "' but reached end of file");
        if (src_[pos_] != c) {
            error(std::string("expected '") + c + "' but got '"
                  + src_[pos_] + "'");
        }
        ++pos_;
    }

    void expect_eof() {
        skip_ws();
        if (pos_ < src_.size())
            error("unexpected trailing content after top-level object");
    }

    void skip_ws() {
        while (pos_ < src_.size()) {
            const unsigned char c = static_cast<unsigned char>(src_[pos_]);
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos_;
            else break;
        }
    }

    std::string parse_string() {
        if (pos_ >= src_.size() || src_[pos_] != '"')
            error("expected a JSON string (opening '\"')");
        ++pos_;
        std::string out;
        while (pos_ < src_.size() && src_[pos_] != '"') {
            const char c = src_[pos_];
            if (c == '\\') {
                if (pos_ + 1 >= src_.size()) error("unterminated escape");
                const char esc = src_[pos_ + 1];
                switch (esc) {
                    case '"':  out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/':  out += '/';  break;
                    case 'n':  out += '\n'; break;
                    case 'r':  out += '\r'; break;
                    case 't':  out += '\t'; break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    default:
                        pos_ += 1;
                        error(std::string("unsupported escape \\") + esc
                              + " (allowed: \\\" \\\\ \\/ \\n \\r \\t \\b \\f)");
                }
                pos_ += 2;
            } else if (static_cast<unsigned char>(c) < 0x20) {
                error("unescaped control character in string");
            } else {
                out += c;
                ++pos_;
            }
        }
        if (pos_ >= src_.size()) error("unterminated string");
        ++pos_;  // closing quote
        return out;
    }

    std::uint32_t parse_cmd_id(const std::string& key, std::size_t key_pos) {
        std::string_view sv = key;
        if (sv.size() >= 2 && sv[0] == '0' && (sv[1] == 'x' || sv[1] == 'X'))
            sv.remove_prefix(2);
        if (sv.size() != 8) {
            pos_ = key_pos;
            error("cmd_id key must be exactly 8 hex digits (optionally "
                  "0x-prefixed), got '" + key + "'");
        }
        std::uint32_t val = 0;
        for (char c : sv) {
            int digit;
            if      (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = 10 + (c - 'a');
            else if (c >= 'A' && c <= 'F') digit = 10 + (c - 'A');
            else {
                pos_ = key_pos;
                error(std::string("cmd_id key contains non-hex char '")
                      + c + "' in '" + key + "'");
            }
            val = (val << 4) | static_cast<std::uint32_t>(digit);
        }
        return val;
    }

    CommandOverride parse_command_override() {
        expect('{');
        CommandOverride co;
        skip_ws();
        if (peek() == '}') { advance(); return co; }
        for (;;) {
            skip_ws();
            const std::size_t field_pos = pos_;
            std::string field = parse_string();
            skip_ws(); expect(':'); skip_ws();
            if      (field == "name")        co.name        = parse_string();
            else if (field == "description") co.description = parse_string();
            else if (field == "format")      co.format      = parse_string();
            else if (field == "args")        co.args        = parse_arg_list();
            else {
                pos_ = field_pos;
                error("unknown field '" + field
                      + "' (expected one of: name, description, format, args)");
            }
            skip_ws();
            if (peek() == ',') { advance(); continue; }
            expect('}');
            break;
        }
        return co;
    }

    std::vector<ArgOverride> parse_arg_list() {
        expect('[');
        std::vector<ArgOverride> out;
        skip_ws();
        if (peek() == ']') { advance(); return out; }
        for (;;) {
            skip_ws();
            out.push_back(parse_arg_override());
            skip_ws();
            if (peek() == ',') { advance(); continue; }
            expect(']');
            break;
        }
        return out;
    }

    ArgOverride parse_arg_override() {
        expect('{');
        ArgOverride ao;
        skip_ws();
        if (peek() == '}') { advance(); return ao; }
        for (;;) {
            skip_ws();
            const std::size_t field_pos = pos_;
            std::string field = parse_string();
            skip_ws(); expect(':'); skip_ws();
            if      (field == "name")        ao.name        = parse_string();
            else if (field == "description") ao.description = parse_string();
            else {
                pos_ = field_pos;
                error("unknown field '" + field
                      + "' in args entry (expected one of: name, description)");
            }
            skip_ws();
            if (peek() == ',') { advance(); continue; }
            expect('}');
            break;
        }
        return ao;
    }

    std::string           src_;
    std::filesystem::path file_;
    std::size_t           pos_ = 0;
};

// ------------------------------------------------------------------
// Active-overrides singleton. Populated once at startup by main()
// after we've located and parsed every dir. The map is stable across
// its lifetime (no mutations after set), so string_view / c_str()
// pointers into it are safe to return from the accessors below.
// ------------------------------------------------------------------
OverrideMap& active_map() {
    static OverrideMap m;
    return m;
}

const CommandOverride* find_cmd(std::uint32_t cmd_id) {
    const auto& m = active_map();
    auto it = m.find(cmd_id);
    return it == m.end() ? nullptr : &it->second;
}

const ArgOverride* find_arg(std::uint32_t cmd_id, std::uint32_t idx) {
    const auto* co = find_cmd(cmd_id);
    if (!co || idx >= co->args.size()) return nullptr;
    return &co->args[idx];
}

// Platform-specific env lookup that treats empty/missing identically.
std::string env_or_empty(const char* name) {
    if (const char* v = std::getenv(name)) return v;
    return {};
}

} // namespace

// ============================================================================
// Public API
// ============================================================================

OverrideMap parse_override_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        throw std::runtime_error("could not open override file: " + p.string());
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    std::string src = buf.str();
    // Strip UTF-8 BOM if present.
    if (src.size() >= 3 && static_cast<unsigned char>(src[0]) == 0xEF
                        && static_cast<unsigned char>(src[1]) == 0xBB
                        && static_cast<unsigned char>(src[2]) == 0xBF) {
        src.erase(0, 3);
    }
    return Parser(std::move(src), p).parse();
}

OverrideMap merge_overrides(OverrideMap base, const OverrideMap& top) {
    for (const auto& [cid, over] : top) {
        auto& into = base[cid];
        if (over.name)        into.name        = over.name;
        if (over.description) into.description = over.description;
        if (over.format)      into.format      = over.format;
        if (over.args.size() > into.args.size()) into.args.resize(over.args.size());
        for (std::size_t i = 0; i < over.args.size(); ++i) {
            if (over.args[i].name)        into.args[i].name        = over.args[i].name;
            if (over.args[i].description) into.args[i].description = over.args[i].description;
        }
    }
    return base;
}

OverrideMap load_override_dir(const std::filesystem::path& dir) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec))
        return {};

    std::vector<std::filesystem::path> files;
    for (const auto& ent : std::filesystem::directory_iterator(dir, ec)) {
        if (!ent.is_regular_file()) continue;
        auto ext = ent.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) {
                           return static_cast<char>(std::tolower(c));
                       });
        if (ext == ".json") files.push_back(ent.path());
    }
    std::sort(files.begin(), files.end());

    OverrideMap acc;
    for (const auto& f : files) {
        acc = merge_overrides(std::move(acc), parse_override_file(f));
    }
    return acc;
}

void set_active_overrides(OverrideMap m) {
    active_map() = std::move(m);
}

const char* override_command_name(std::uint32_t cmd_id) {
    const auto* co = find_cmd(cmd_id);
    return (co && co->name) ? co->name->c_str() : nullptr;
}

const char* override_command_description(std::uint32_t cmd_id) {
    const auto* co = find_cmd(cmd_id);
    return (co && co->description) ? co->description->c_str() : nullptr;
}

const char* override_command_format(std::uint32_t cmd_id) {
    const auto* co = find_cmd(cmd_id);
    return (co && co->format) ? co->format->c_str() : nullptr;
}

const char* override_command_arg_name(std::uint32_t cmd_id,
                                      std::uint32_t arg_idx) {
    const auto* ao = find_arg(cmd_id, arg_idx);
    return (ao && ao->name) ? ao->name->c_str() : nullptr;
}

const char* override_command_arg_description(std::uint32_t cmd_id,
                                             std::uint32_t arg_idx) {
    const auto* ao = find_arg(cmd_id, arg_idx);
    return (ao && ao->description) ? ao->description->c_str() : nullptr;
}

// ---- Path discovery -------------------------------------------------------

std::filesystem::path project_overrides_dir() {
    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    if (ec) return "overrides";  // best-effort relative path
    return cwd / "overrides";
}

std::filesystem::path user_overrides_dir() {
#if defined(_WIN32)
    std::string appdata = env_or_empty("APPDATA");
    if (appdata.empty()) {
        std::string userprofile = env_or_empty("USERPROFILE");
        if (userprofile.empty()) return {};
        appdata = userprofile + "\\AppData\\Roaming";
    }
    return std::filesystem::path(appdata) / "psax" / "overrides";
#elif defined(__APPLE__)
    const std::string home = env_or_empty("HOME");
    if (home.empty()) return {};
    return std::filesystem::path(home)
         / "Library" / "Application Support" / "psax" / "overrides";
#else
    std::string base = env_or_empty("XDG_CONFIG_HOME");
    if (base.empty()) {
        const std::string home = env_or_empty("HOME");
        if (home.empty()) return {};
        base = home + "/.config";
    }
    return std::filesystem::path(base) / "psax" / "overrides";
#endif
}

// ---- init-overrides -------------------------------------------------------

namespace {

constexpr const char* kReadmeBody =
"psax overrides\n"
"==============\n"
"\n"
"Drop any number of `*.json` files into this directory to extend or override\n"
"psax's built-in command table. Files are loaded in alphabetical order and\n"
"deep-merged, so later files overlay earlier ones per (cmd_id, field).\n"
"\n"
"If both the project directory (`./overrides/`) AND the per-user directory\n"
"exist, the per-user directory loads first and the project directory\n"
"overrides it. This lets you keep personal defaults globally and per-project\n"
"tweaks locally.\n"
"\n"
"See example.json in this directory for the file format. Malformed JSON is a\n"
"hard error at startup with a `file:line:col` message.\n";

constexpr const char* kExampleBody =
"{\n"
"  \"0xC0DE0100\": {\n"
"    \"name\": \"MyCustomHitboxSfx\",\n"
"    \"description\": \"PM/P+ hitbox SFX override (renamed via overrides).\",\n"
"    \"format\": \"hitbox={0} -> sfx={1}\",\n"
"    \"args\": [\n"
"      { \"name\": \"Hitbox ID\", \"description\": \"which hitbox slot\" },\n"
"      { \"name\": \"SFX ID\" }\n"
"    ]\n"
"  }\n"
"}\n";

// Try to create `dir` (and parents) and drop README + example inside on
// first creation. Returns InitOverridesResult with `created=false` if `dir`
// already existed (files not touched). Returns std::nullopt if the create
// or write failed (unwritable parent, permissions).
std::optional<InitOverridesResult> try_init(const std::filesystem::path& dir,
                                            bool used_fallback) {
    std::error_code ec;

    const bool already = std::filesystem::exists(dir, ec)
                      && std::filesystem::is_directory(dir, ec);
    if (already) return InitOverridesResult{dir, used_fallback, false};

    std::filesystem::create_directories(dir, ec);
    if (ec) return std::nullopt;

    // Sanity-check that we can actually write here before declaring victory.
    auto write = [&](const std::filesystem::path& p, const char* body) {
        std::ofstream f(p, std::ios::binary);
        if (!f) return false;
        f << body;
        return static_cast<bool>(f);
    };
    if (!write(dir / "README.md", kReadmeBody)) return std::nullopt;
    if (!write(dir / "example.json", kExampleBody)) return std::nullopt;

    return InitOverridesResult{dir, used_fallback, true};
}

} // namespace

InitOverridesResult init_overrides() {
    if (auto r = try_init(project_overrides_dir(), false)) return *r;
    if (auto r = try_init(user_overrides_dir(),    true))  return *r;
    throw std::runtime_error(
        "could not create overrides directory in either the project dir or "
        "the per-user config dir (both writes failed)");
}

std::vector<std::filesystem::path> active_overrides_paths() {
    std::vector<std::filesystem::path> out;
    std::error_code ec;
    // Load order = apply order: user first, project last (project wins).
    for (const auto& d : {user_overrides_dir(), project_overrides_dir()}) {
        if (d.empty()) continue;
        if (std::filesystem::exists(d, ec)
            && std::filesystem::is_directory(d, ec)) {
            out.push_back(d);
        }
    }
    return out;
}

} // namespace psax
