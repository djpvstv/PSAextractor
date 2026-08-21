#include "psa/arg.hpp"

#include <cstdio>
#include <cstdint>

namespace psax {

const char* arg_type_name(ArgType t) {
    switch (t) {
        case ArgType::Value:       return "Value";
        case ArgType::Scalar:      return "Scalar";
        case ArgType::Pointer:     return "Pointer";
        case ArgType::Boolean:     return "Boolean";
        case ArgType::Type4:       return "Type4";
        case ArgType::Variable:    return "Variable";
        case ArgType::Requirement: return "Requirement";
    }
    return "Unknown";
}

std::string Arg::to_raw_string() const {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%u-%08X",
                  static_cast<unsigned>(type), raw_value);
    return std::string(buf);
}

// Bit layout, verified against user-supplied PSAC examples:
//   0x20000008 = RA-Basic[8]        -> mc=2 (RA), dt=0 (Basic), idx=8
//   0x10000009 = LA-Basic[9]        -> mc=1 (LA), dt=0 (Basic), idx=9
//   0x00004E23 = IC-Basic[20003]    -> mc=0 (IC), dt=0 (Basic), idx=20003
//   0x21000008 = RA-Float[8]        -> mc=2 (RA), dt=1 (Float), idx=8
//   0x22000008 = RA-Bit[8]          -> mc=2 (RA), dt=2 (Bit),   idx=8
//
// So:
//   bits 28..31 = memory class (0=IC, 1=LA, 2=RA)
//   bits 24..27 = data type    (0=Basic, 1=Float, 2=Bit)
//   bits  0..23 = index
VariableRef variable_from_raw(uint32_t raw) {
    VariableRef v;
    const uint8_t mc = static_cast<uint8_t>((raw >> 28) & 0xFu);
    const uint8_t dt = static_cast<uint8_t>((raw >> 24) & 0xFu);
    v.mem_class = (mc <= 2) ? static_cast<VariableRef::MemClass>(mc)
                            : VariableRef::MemClass::Unknown;
    v.data_type = (dt <= 2) ? static_cast<VariableRef::DataType>(dt)
                            : VariableRef::DataType::Unknown;
    v.index     = raw & 0x00FFFFFFu;
    return v;
}

std::string VariableRef::to_string() const {
    const char* mc = nullptr;
    switch (mem_class) {
        case MemClass::IC: mc = "IC"; break;
        case MemClass::LA: mc = "LA"; break;
        case MemClass::RA: mc = "RA"; break;
        default: break;
    }
    const char* dt = nullptr;
    switch (data_type) {
        case DataType::Basic: dt = "Basic"; break;
        case DataType::Float: dt = "Float"; break;
        case DataType::Bit:   dt = "Bit";   break;
        default: break;
    }
    char buf[64];
    if (mc && dt) {
        std::snprintf(buf, sizeof(buf), "%s-%s[%u]", mc, dt, index);
    } else {
        // Fallback: raw hex descriptor (memory-class in upper nibble, data-type in next).
        std::snprintf(buf, sizeof(buf), "Var(0x%08X)",
                      (static_cast<uint32_t>((mem_class == MemClass::Unknown ? 0xF : uint8_t(mem_class))) << 28)
                    | (static_cast<uint32_t>((data_type == DataType::Unknown ? 0xF : uint8_t(data_type))) << 24)
                    | (index & 0xFFFFFFu));
    }
    return buf;
}

std::string Arg::to_pretty_string() const {
    char buf[64];
    switch (type) {
        // Non-scalar values are always displayed in hex with `0x` prefix, per
        // user convention (matches how PSAC shows integer/pointer args).
        case ArgType::Value:
        case ArgType::Pointer:
        case ArgType::Type4: {
            std::snprintf(buf, sizeof(buf), "0x%X", raw_value);
            return buf;
        }
        case ArgType::Scalar: {
            // Fixed-point: raw / 60000 = display. Signed decimal float.
            const double f = static_cast<double>(static_cast<int32_t>(raw_value)) / 60000.0;
            std::snprintf(buf, sizeof(buf), "%g", f);
            return buf;
        }
        case ArgType::Boolean: {
            return raw_value ? "true" : "false";
        }
        case ArgType::Variable: {
            // Variable index (inside brackets) is decimal, e.g. RA-Basic[38544].
            return variable_from_raw(raw_value).to_string();
        }
        case ArgType::Requirement: {
            std::snprintf(buf, sizeof(buf), "req(0x%X)", raw_value);
            return buf;
        }
    }
    std::snprintf(buf, sizeof(buf), "arg%u(0x%X)",
                  static_cast<unsigned>(type), raw_value);
    return buf;
}

} // namespace psax
