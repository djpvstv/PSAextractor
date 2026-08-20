#pragma once

#include <cstdint>
#include <string>

namespace psax {

// PSA argument types. The numeric value is what's stored in the file's
// arg-descriptor (first u32 of each 8-byte arg entry).
enum class ArgType : uint32_t {
    Value       = 0,   // signed integer
    Scalar      = 1,   // fixed-point number (raw / 60000 = display)
    Pointer     = 2,   // MISC-relative offset (stored form, needs +32)
    Boolean     = 3,   // 0 = false, non-zero = true
    Variable    = 5,   // packed (memory class + data type + index)
    Requirement = 6,   // requirement condition ID
};

const char* arg_type_name(ArgType t);

// One event argument as read from disk. Interpretation of raw_value depends
// on type. Callers that need typed access should build a formatter on top.
struct Arg {
    ArgType  type;
    uint32_t raw_value;

    // PSAC-style compact form: "1-000927C0" (type in decimal, value in 8-digit hex).
    std::string to_raw_string() const;
};

} // namespace psax
