#include <doctest/doctest.h>

#include "psa/event.hpp"
#include "psa/event_format.hpp"

namespace {

psax::Event ev(std::uint32_t cmd_id) {
    psax::Event e;
    e.cmd_id = cmd_id;
    return e;
}

// Helper: build a stream, run the tracker, return one indent string per event.
std::vector<std::string> walk(const std::vector<psax::Event>& stream) {
    psax::BlockIndentTracker t;
    std::vector<std::string> out;
    out.reserve(stream.size());
    for (const auto& e : stream) out.push_back(t.indent_for(e));
    return out;
}

} // namespace

TEST_CASE("BlockIndentTracker: neutral stream stays at depth 0") {
    // Two non-block events -> both at depth 0 -> empty indent.
    auto out = walk({ev(0x00020100u), ev(0x00020100u)});   // AsynchronousTimer x2
    CHECK(out[0].empty());
    CHECK(out[1].empty());
}

TEST_CASE("BlockIndentTracker: If -> body -> EndIf indents the body only") {
    auto out = walk({
        ev(0x000A0100u),   // If (opener)
        ev(0x00020100u),   // AsyncTimer (body)
        ev(0x00020100u),   // AsyncTimer (body)
        ev(0x000F0000u),   // EndIf (closer)
        ev(0x00020100u),   // AsyncTimer (outer)
    });
    CHECK(out[0] == "");
    CHECK(out[1] == "  ");   // 1 level x 2 spaces
    CHECK(out[2] == "  ");
    CHECK(out[3] == "");     // EndIf popped, prints at outer
    CHECK(out[4] == "");
}

TEST_CASE("BlockIndentTracker: And/Or extenders print at outer level like the If") {
    auto out = walk({
        ev(0x000A0100u),   // If
        ev(0x000B0100u),   // And
        ev(0x000C0100u),   // Or
        ev(0x00020100u),   // body
        ev(0x000F0000u),   // EndIf
    });
    CHECK(out[0] == "");
    CHECK(out[1] == "");   // And renders at outer (== If's level)
    CHECK(out[2] == "");   // Or too
    CHECK(out[3] == "  "); // body indented
    CHECK(out[4] == "");
}

TEST_CASE("BlockIndentTracker: Else and ElseIf print at outer, body stays indented") {
    auto out = walk({
        ev(0x000A0100u),   // If
        ev(0x00020100u),   // body1
        ev(0x000E0000u),   // Else
        ev(0x00020100u),   // body2
        ev(0x000D0100u),   // ElseIf
        ev(0x00020100u),   // body3
        ev(0x000F0000u),   // EndIf
    });
    CHECK(out[0] == "");
    CHECK(out[1] == "  ");
    CHECK(out[2] == "");     // Else at outer
    CHECK(out[3] == "  ");   // else body still depth 1
    CHECK(out[4] == "");     // ElseIf at outer
    CHECK(out[5] == "  ");
    CHECK(out[6] == "");
}

TEST_CASE("BlockIndentTracker: nested If/EndIf indents cumulatively") {
    auto out = walk({
        ev(0x000A0100u),   // If      depth 0 -> 1
        ev(0x000A0100u),   // If      depth 1 -> 2
        ev(0x00020100u),   // body            depth 2
        ev(0x000F0000u),   // EndIf   depth 2 -> 1
        ev(0x00020100u),   // outer1          depth 1
        ev(0x000F0000u),   // EndIf   depth 1 -> 0
        ev(0x00020100u),   // outer0          depth 0
    });
    CHECK(out[0] == "");
    CHECK(out[1] == "  ");
    CHECK(out[2] == "    ");  // depth 2
    CHECK(out[3] == "  ");    // inner EndIf back to depth 1
    CHECK(out[4] == "  ");
    CHECK(out[5] == "");
    CHECK(out[6] == "");
}

TEST_CASE("BlockIndentTracker: SetLoop / ExecuteLoop indent loop body") {
    auto out = walk({
        ev(0x00040100u),   // SetLoop
        ev(0x00020100u),   // body
        ev(0x00060000u),   // LoopBreak (neutral inside)
        ev(0x00050000u),   // ExecuteLoop (closer)
        ev(0x00020100u),   // outer
    });
    CHECK(out[0] == "");
    CHECK(out[1] == "  ");
    CHECK(out[2] == "  ");   // LoopBreak treated as neutral -> stays inside
    CHECK(out[3] == "");     // ExecuteLoop closes
    CHECK(out[4] == "");
}

TEST_CASE("BlockIndentTracker: Switch/Case/DefaultCase/EndSwitch") {
    auto out = walk({
        ev(0x00100200u),   // Switch  depth 0 -> 1
        ev(0x00110100u),   // Case    print at 0, stays 1
        ev(0x00020100u),   // body    depth 1
        ev(0x00120000u),   // DefaultCase  print at 0, stays 1
        ev(0x00020100u),   // body    depth 1
        ev(0x00130000u),   // EndSwitch depth 1 -> 0
    });
    CHECK(out[0] == "");
    CHECK(out[1] == "");
    CHECK(out[2] == "  ");
    CHECK(out[3] == "");
    CHECK(out[4] == "  ");
    CHECK(out[5] == "");
}

TEST_CASE("BlockIndentTracker: stray EndIf without matching If clamps to 0") {
    // Defensive: a malformed stream shouldn't produce negative depth.
    auto out = walk({
        ev(0x000F0000u),   // EndIf with no If
        ev(0x00020100u),   // neutral
    });
    CHECK(out[0] == "");
    CHECK(out[1] == "");
}

TEST_CASE("BlockIndentTracker: reset() clears state between streams") {
    psax::BlockIndentTracker t;
    (void)t.indent_for(ev(0x000A0100u));   // If -> depth 1
    CHECK(t.depth() == 1u);
    t.reset();
    CHECK(t.depth() == 0u);
    CHECK(t.indent_for(ev(0x00020100u)).empty());
}
