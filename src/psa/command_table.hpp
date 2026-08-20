#pragma once

#include <cstdint>
#include <optional>

namespace psax {

struct CommandInfo {
    uint32_t    id;
    const char* name;
    uint32_t    arg_count;
};

// Lookup for known PSA command IDs. Very small starter set - grows as we
// verify each command against PSAC ground truth.
class CommandTable {
public:
    std::optional<CommandInfo> lookup(uint32_t cmd_id) const;
};

} // namespace psax
