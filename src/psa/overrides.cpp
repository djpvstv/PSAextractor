// MSVC flags std::getenv as "unsafe" (C4996) and suggests _dupenv_s, but
// _dupenv_s is Windows-only and getenv is portable + fine for read-only
// env lookups.
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#elif defined(__APPLE__)
#  include <mach-o/dyld.h>
#else
#  include <unistd.h>
#endif

#include "psa/overrides.hpp"

#include "psa/argument_schema.hpp"
#include "psa/command_table.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <vector>

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

// ---- Built-in table dumper ------------------------------------------------

namespace {

// Emit a JSON string literal. Escapes the same set of chars our parser
// accepts: `"`, `\`, and the C0 control chars via \n \r \t \b \f, with
// everything else \uXXXX-escaped so we never emit raw control bytes.
void emit_json_string(std::ostream& out, const char* s) {
    out << '"';
    if (!s) { out << '"'; return; }
    for (const char* p = s; *p; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        switch (c) {
            case '"':  out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n";  break;
            case '\r': out << "\\r";  break;
            case '\t': out << "\\t";  break;
            case '\b': out << "\\b";  break;
            case '\f': out << "\\f";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out << buf;
                } else {
                    out.put(static_cast<char>(c));
                }
        }
    }
    out << '"';
}

} // namespace

void dump_builtin_command_table(std::ostream& out) {
    // Collect the union of every cmd_id we know about across both tables.
    // std::set gives us sorted iteration + de-duplication for free.
    std::set<std::uint32_t> ids;
    for (auto id : all_named_command_ids())  ids.insert(id);
    for (auto id : all_schema_command_ids()) ids.insert(id);

    out << "{\n";
    bool first = true;
    for (auto cid : ids) {
        if (!first) out << ",\n";
        first = false;

        char idbuf[16];
        std::snprintf(idbuf, sizeof(idbuf), "0x%08X", cid);
        out << "    \"" << idbuf << "\": {";

        // We emit each set of fields on one line for compactness - the
        // file is 383 entries and gets unwieldy fast if every field is on
        // its own line. Modders can reformat with jq if they prefer.
        bool need_comma = false;
        auto sep = [&]() {
            if (need_comma) out << ",";
            out << "\n        ";
            need_comma = true;
        };

        if (const char* n = command_name(cid)) {
            sep(); out << "\"name\": ";        emit_json_string(out, n);
        }
        if (const char* d = command_description(cid)) {
            sep(); out << "\"description\": "; emit_json_string(out, d);
        }
        if (const char* f = command_format(cid)) {
            sep(); out << "\"format\": ";      emit_json_string(out, f);
        }

        // Emit `args` if the schema knows about any slot for this cmd_id.
        // We honor the schema slot count (may exceed arg_count_of when
        // BrawlCrate lists more than the cmd_id byte encodes); anything
        // extra is still useful metadata for the extension.
        const std::uint32_t slots = schema_arg_slot_count(cid);
        if (slots > 0) {
            sep();
            out << "\"args\": [";
            for (std::uint32_t i = 0; i < slots; ++i) {
                if (i > 0) out << ", ";
                out << "{";
                const char* an = command_arg_name(cid, i);
                const char* ad = command_arg_description(cid, i);
                bool inner_comma = false;
                if (an) {
                    out << "\"name\": ";
                    emit_json_string(out, an);
                    inner_comma = true;
                }
                if (ad) {
                    if (inner_comma) out << ", ";
                    out << "\"description\": ";
                    emit_json_string(out, ad);
                }
                out << "}";
            }
            out << "]";
        }

        if (need_comma) out << "\n    ";
        out << "}";
    }
    out << "\n}\n";
}

// ---- Path discovery -------------------------------------------------------

namespace {

// Locate the running executable's full path. std::filesystem has no
// portable way to do this - each OS exposes its own mechanism.
std::filesystem::path running_exe_path() {
#if defined(_WIN32)
    // GetModuleFileNameW writes UTF-16 and truncates without null-terminating
    // on overflow; keep doubling the buffer until it fits.
    std::vector<wchar_t> buf(MAX_PATH);
    for (;;) {
        const DWORD n = ::GetModuleFileNameW(nullptr, buf.data(),
                                             static_cast<DWORD>(buf.size()));
        if (n == 0) return {};
        if (n < buf.size()) return std::filesystem::path(buf.data());
        buf.resize(buf.size() * 2);
        if (buf.size() > (1u << 16)) return {};  // pathological, give up
    }
#elif defined(__APPLE__)
    std::uint32_t sz = 0;
    _NSGetExecutablePath(nullptr, &sz);  // sets sz to required length
    std::vector<char> buf(sz);
    if (_NSGetExecutablePath(buf.data(), &sz) != 0) return {};
    std::error_code ec;
    auto canonical = std::filesystem::canonical(buf.data(), ec);
    return ec ? std::filesystem::path(buf.data()) : canonical;
#else
    // Linux (and other /proc-based unices). Read the symlink at
    // /proc/self/exe; length is not knowable in advance, so use a loop.
    std::vector<char> buf(4096);
    for (;;) {
        const ssize_t n = ::readlink("/proc/self/exe", buf.data(), buf.size());
        if (n < 0) return {};
        if (static_cast<std::size_t>(n) < buf.size()) {
            return std::filesystem::path(std::string(buf.data(),
                                                     static_cast<std::size_t>(n)));
        }
        buf.resize(buf.size() * 2);
        if (buf.size() > (1u << 16)) return {};
    }
#endif
}

} // namespace

std::filesystem::path exe_overrides_dir() {
    const auto p = running_exe_path();
    if (p.empty()) return {};
    return p.parent_path() / "overrides";
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
"This directory lives next to the psax executable (portable-app model - move\n"
"the exe and its overrides move with it). If a per-user config directory\n"
"ALSO exists, it loads first and this directory overrides it. That lets you\n"
"keep personal defaults globally and per-install tweaks locally.\n"
"\n"
"`00_builtin.json`\n"
"-----------------\n"
"`init-overrides` writes this file with a snapshot of psax's built-in command\n"
"table (names, descriptions, formats, per-arg names + descriptions). The `00_`\n"
"prefix guarantees it loads first, so any other file you add layers on top of\n"
"it via the deep merge. Editing 00_builtin.json in place also works.\n"
"\n"
"To re-sync 00_builtin.json after a psax upgrade adds new commands, run\n"
"`psax init-overrides --clean` (it will refuse without --clean if the file\n"
"already exists).\n"
"\n"
"Malformed JSON is a hard error at startup with a `file:line:col` message.\n";

constexpr const char* kBuiltinFilename = "00_builtin.json";

// Try to create/populate `dir`. Returns the result on success, or nullopt
// if we couldn't create the dir OR couldn't write files into it.
std::optional<InitOverridesResult> try_init(const std::filesystem::path& dir,
                                            bool used_fallback,
                                            const InitOverridesOptions& opts) {
    std::error_code ec;

    const bool dir_existed = std::filesystem::exists(dir, ec)
                          && std::filesystem::is_directory(dir, ec);
    if (!dir_existed) {
        std::filesystem::create_directories(dir, ec);
        if (ec) return std::nullopt;
    }

    // README is a no-op if already present - the user might have edited it.
    const auto readme = dir / "README.md";
    if (!std::filesystem::exists(readme, ec)) {
        std::ofstream f(readme, std::ios::binary);
        if (!f) return std::nullopt;
        f << kReadmeBody;
        if (!f) return std::nullopt;
    }

    // Refuse to touch 00_builtin.json if it exists unless the caller passed
    // --clean. This is the safety valve: user edits to the seed survive
    // re-invocations of init-overrides.
    const auto dump_path = dir / kBuiltinFilename;
    const bool dump_existed = std::filesystem::exists(dump_path, ec);
    bool wrote_dump = false;
    if (!dump_existed || opts.clean) {
        std::ofstream f(dump_path, std::ios::binary);
        if (!f) return std::nullopt;
        dump_builtin_command_table(f);
        if (!f) return std::nullopt;
        wrote_dump = true;
    }

    return InitOverridesResult{dir, used_fallback, !dir_existed, wrote_dump};
}

} // namespace

InitOverridesResult init_overrides(const InitOverridesOptions& opts) {
    // Explicit dir wins - used by tests to point init at a scratch dir.
    if (!opts.dir_override.empty()) {
        if (auto r = try_init(opts.dir_override, false, opts)) return *r;
        throw std::runtime_error(
            "could not create overrides directory at requested path: "
            + opts.dir_override.string());
    }
    if (auto exe = exe_overrides_dir(); !exe.empty()) {
        if (auto r = try_init(exe, false, opts)) return *r;
    }
    if (auto r = try_init(user_overrides_dir(), true, opts)) return *r;
    throw std::runtime_error(
        "could not create overrides directory next to the executable or in "
        "the per-user config dir (both writes failed)");
}

std::vector<std::filesystem::path> active_overrides_paths() {
    std::vector<std::filesystem::path> out;
    std::error_code ec;
    // Load order = apply order: user first, exe last (exe wins).
    for (const auto& d : {user_overrides_dir(), exe_overrides_dir()}) {
        if (d.empty()) continue;
        if (std::filesystem::exists(d, ec)
            && std::filesystem::is_directory(d, ec)) {
            out.push_back(d);
        }
    }
    return out;
}

} // namespace psax
