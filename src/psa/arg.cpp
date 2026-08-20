#include "psa/arg.hpp"

#include <cstdio>

namespace psax {

const char* arg_type_name(ArgType t) {
    switch (t) {
        case ArgType::Value:       return "Value";
        case ArgType::Scalar:      return "Scalar";
        case ArgType::Pointer:     return "Pointer";
        case ArgType::Boolean:     return "Boolean";
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

} // namespace psax
