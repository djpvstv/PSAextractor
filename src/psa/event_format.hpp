#pragma once

#include "psa/event.hpp"

#include <cstddef>
#include <string>

namespace psax {

// Stateful indent tracker for block-structured event streams. Feed events
// in stream order; each call returns the indent-prefix string (spaces) to
// prepend to that event's rendering, and updates internal depth so nested
// blocks (If/EndIf, SetLoop/ExecuteLoop, Switch/EndSwitch) show at the
// right level. Extenders (And/Or) and middles (Else/ElseIf/Case) render at
// the OUTER indent so the block structure stays visible.
//
// Reset (or use a fresh instance) at the start of each event list -
// depth doesn't carry across streams.
class BlockIndentTracker {
public:
    std::string indent_for(const Event& e);
    void        reset() { depth_ = 0; }
    std::size_t depth() const { return depth_; }
private:
    std::size_t depth_ = 0;
};

} // namespace psax
