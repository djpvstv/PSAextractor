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

    // DSL-friendly formatted form. Examples per type:
    //   Value    -> "42"       (signed int)
    //   Scalar   -> "10.0"     (raw / 60000)
    //   Pointer  -> "@0x1E9C0" (stored form)
    //   Boolean  -> "true" / "false"
    //   Variable -> "RA-Bit[16]"
    //   Requirement -> "req(0x%X)"  until we have a requirement table
    std::string to_pretty_string() const;
};

// Decoded Variable descriptor. See variable_from_raw().
struct VariableRef {
    enum class MemClass : uint8_t { IC = 0, LA = 1, RA = 2, Unknown = 0xFF };
    enum class DataType : uint8_t { Basic = 0, Float = 1, Bit = 2, Unknown = 0xFF };

    MemClass mem_class;
    DataType data_type;
    uint32_t index;

    // "RA-Bit[16]" style; "Var(0x%X)" if unknown class/type.
    std::string to_string() const;
};

VariableRef variable_from_raw(uint32_t raw);

} // namespace psax
