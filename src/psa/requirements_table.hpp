#pragma once

#include <cstdint>

namespace psax {

// Mask that separates the negation bit from the requirement ID.
// A raw Requirement arg with bit 31 set means "NOT <requirement>".
// Example: 0x80000003 = "Not On Ground"; 0x00000003 = "On Ground".
constexpr uint32_t kRequirementNegateBit = 0x80000000u;
constexpr uint32_t kRequirementIdMask    = 0x7FFFFFFFu;

// CamelCase name for a requirement ID (with negation bit already stripped),
// or nullptr if the ID has no PSAC-known name. Callers should format
// unknowns as `req(0x%X)`.
const char* requirement_name(uint32_t id);

} // namespace psax
