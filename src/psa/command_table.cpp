#include "psa/command_table.hpp"

namespace psax {

namespace {
// Verified against PSAC's RunBrake subaction decoding.
constexpr CommandInfo kCommands[] = {
    {0x00020100u, "AsynchronousTimer", 1u},
    {0x120A0100u, "BitVariableSet",    1u},
    {0x120B0100u, "BitVariableClear",  1u},
    {0x64000000u, "AllowInterrupt",    0u},
};
} // namespace

std::optional<CommandInfo> CommandTable::lookup(uint32_t cmd_id) const {
    for (const auto& c : kCommands) {
        if (c.id == cmd_id) return c;
    }
    return std::nullopt;
}

} // namespace psax
