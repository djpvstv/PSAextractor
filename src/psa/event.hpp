#pragma once

#include "psa/arg.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace psax {

struct Event {
    uint32_t cmd_id  = 0;
    uint32_t args_ptr = 0;   // stored form, still needs +32 to resolve
    std::vector<Arg> args;

    // PSAC-style raw form, e.g. "E=00020100:1-000927C0" or "E=64000000:".
    std::string to_raw_string() const;

    // DSL-friendly form, e.g. "AsynchronousTimer(10)" or "AllowInterrupt()".
    // Uses the command_table for the name; falls back to "Unknown_XXXXXXXX".
    std::string to_pretty_string() const;
};

} // namespace psax
