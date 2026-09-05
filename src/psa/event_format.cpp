#include "psa/event_format.hpp"

namespace psax {

namespace {

enum class BlockKind { Neutral, Opener, Extender, Middle, Closer };

// Classify by cmd_id. The cases here mirror the block-related entries in
// command_table.cpp - if a new block-shaped command lands, add it here too.
BlockKind classify(std::uint32_t cmd_id) {
    switch (cmd_id) {
        // Openers - indent AFTER printing.
        case 0x00040100u:  // SetLoop
        case 0x000A0100u:  // If
        case 0x000A0200u:  // IfValue
        case 0x000A0400u:  // IfComparison
        case 0x00100200u:  // Switch
            return BlockKind::Opener;

        // Extenders - extend the prior If's condition. Print at outer level,
        // no depth change (the opener already pushed).
        case 0x000B0100u:  // And
        case 0x000B0200u:  // AndValue
        case 0x000B0400u:  // AndComparison
        case 0x000C0100u:  // Or
        case 0x000C0200u:  // OrValue
        case 0x000C0400u:  // OrComparison
            return BlockKind::Extender;

        // Middles - close one branch and open the next at the same depth.
        // Print at outer, depth stays the same (close and open cancel).
        case 0x000D0100u:  // ElseIf
        case 0x000D0200u:  // ElseIfValue
        case 0x000D0400u:  // ElseIfComparison
        case 0x000E0000u:  // Else
        case 0x00110100u:  // Case
        case 0x00120000u:  // DefaultCase
            return BlockKind::Middle;

        // Closers - pop first, then print at the new (outer) depth.
        case 0x00050000u:  // ExecuteLoop
        case 0x000F0000u:  // EndIf
        case 0x00130000u:  // EndSwitch
            return BlockKind::Closer;

        default:
            return BlockKind::Neutral;
    }
}

// 2 spaces per level keeps deep nesting readable without pushing the raw
// column too far right.
constexpr std::size_t kSpacesPerLevel = 2;

std::string spaces(std::size_t levels) {
    return std::string(levels * kSpacesPerLevel, ' ');
}

} // namespace

std::string BlockIndentTracker::indent_for(const Event& e) {
    const BlockKind kind = classify(e.cmd_id);
    switch (kind) {
        case BlockKind::Opener: {
            std::string out = spaces(depth_);
            ++depth_;
            return out;
        }
        case BlockKind::Extender:
        case BlockKind::Middle: {
            // Render at outer level. Clamp for defensive robustness: a
            // malformed stream with an Else that lacks a matching If
            // shouldn't produce negative indent.
            const std::size_t d = depth_ > 0 ? depth_ - 1 : 0;
            return spaces(d);
        }
        case BlockKind::Closer: {
            if (depth_ > 0) --depth_;
            return spaces(depth_);
        }
        case BlockKind::Neutral:
        default:
            return spaces(depth_);
    }
}

} // namespace psax
