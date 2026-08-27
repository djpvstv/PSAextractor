#include "psa/subroutine_scan.hpp"

#include "psa/character_root.hpp"
#include "psa/event_decoder.hpp"
#include "psa/misc_section.hpp"
#include "psa/subaction_table.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace psax {

namespace {

// Command IDs whose args carry a pointer to another event list.
// SubRoutine / Goto: first arg is the target.
// ConcurrentInfiniteLoop: has 2 args; the target is the first Pointer-typed
// one we find (position unverified — treated defensively).
constexpr uint32_t kCmdSubRoutine             = 0x00070100u;
constexpr uint32_t kCmdGoto                   = 0x00090100u;
constexpr uint32_t kCmdConcurrentInfiniteLoop = 0x0D000200u;

// Heuristic: does the u32 at `resolved_off` look like the start of a real
// PSA event list? Real cmd_ids always have their low byte = 0 (the last
// nibble encodes command-family details, but the very last byte is 0), and
// no observed real command has more than ~16 args in its byte-2 arg-count.
// Values like 0x0001326C (grab throw-parameter table) or 0xFFFFFFFF fail
// both checks.
//
// Grab / Catch subactions call `SubRoutine(ptr)` where `ptr` addresses a
// throw-parameter block, NOT an event list. We use this check to filter
// those out during subroutine discovery so they don't produce noisy
// "failed to decode" reports.
bool looks_like_event_list_start(const uint8_t* misc_data, std::size_t misc_size,
                                 std::size_t resolved_off) {
    if (resolved_off + 4 > misc_size) return false;
    const uint32_t cmd =
          (uint32_t(misc_data[resolved_off    ]) << 24)
        | (uint32_t(misc_data[resolved_off + 1]) << 16)
        | (uint32_t(misc_data[resolved_off + 2]) <<  8)
        | (uint32_t(misc_data[resolved_off + 3]));
    if (cmd == 0)             return false;  // starts with terminator = empty/invalid
    if ((cmd & 0xFFu) != 0u)  return false;  // real cmd_ids have low byte = 0
    if (((cmd >> 8) & 0xFFu) > 32u) return false;  // arg_count sanity cap
    return true;
}

// Return true if the arg carries a MISC-offset pointer we can resolve.
bool arg_looks_like_event_ptr(const Arg& a) {
    // Both Pointer (type 2) and Value (type 0) have been observed carrying
    // event-list offsets in various PSA commands. We'll trust Pointer strictly;
    // Value we accept only for the SubRoutine/Goto family where the arg
    // convention is well established.
    return a.type == ArgType::Pointer;
}

uint32_t take_first_arg_ptr(const Event& e) {
    if (e.args.empty()) return 0u;
    // For SubRoutine / Goto, PSAC always uses Pointer or Value here.
    if (e.args[0].type == ArgType::Pointer || e.args[0].type == ArgType::Value) {
        return e.args[0].raw_value;
    }
    return 0u;
}

uint32_t take_first_pointer_arg(const Event& e) {
    for (const auto& a : e.args) {
        if (arg_looks_like_event_ptr(a)) return a.raw_value;
    }
    return 0u;
}

} // namespace

uint32_t event_subroutine_target(const Event& e) {
    switch (e.cmd_id) {
        case kCmdSubRoutine:             return take_first_arg_ptr(e);
        case kCmdGoto:                   return take_first_arg_ptr(e);
        case kCmdConcurrentInfiniteLoop: return take_first_pointer_arg(e);
        default:                         return 0u;
    }
}

std::vector<DiscoveredSubroutine> collect_subroutines(const MiscSection& ms) {
    std::vector<DiscoveredSubroutine> out;
    const auto root = load_character_root(ms);

    const std::size_t count = subaction_main_count(
        root.fields[CharacterRoot::SubActionMain],
        root.fields[CharacterRoot::SubActionGFX]);
    if (count == 0) return out;

    struct TabDesc { const char* label; CharacterRoot::Field field; };
    const TabDesc tabs[] = {
        {"Main",  CharacterRoot::SubActionMain},
        {"GFX",   CharacterRoot::SubActionGFX},
        {"SFX",   CharacterRoot::SubActionSFX},
        {"Other", CharacterRoot::SubActionOther},
    };

    EventDecoder dec(ms.data(), ms.size());

    // First, collect every subaction event-list pointer across all 4 tabs.
    // A subroutine target that happens to equal one of these is really just a
    // cross-reference into an existing subaction (via Goto or Concurrent-
    // InfiniteLoop, most often) — NOT a new subroutine we should discover.
    std::unordered_set<uint32_t> subaction_ptrs;
    for (const auto& tab : tabs) {
        const auto table = read_subaction_table(ms, root.fields[tab.field], count);
        for (uint32_t p : table) {
            if (p != 0u && p != 0xFFFFFFFFu) subaction_ptrs.insert(p);
        }
    }

    // Map stored_ptr -> index in `out`, for deduping and finding.
    std::unordered_map<uint32_t, std::size_t> index_by_ptr;

    auto ensure_slot = [&](uint32_t stored_ptr) -> DiscoveredSubroutine& {
        auto it = index_by_ptr.find(stored_ptr);
        if (it != index_by_ptr.end()) return out[it->second];
        DiscoveredSubroutine ds;
        ds.stored_ptr      = stored_ptr;
        ds.resolved_offset = resolve_misc_ptr(stored_ptr);
        out.push_back(std::move(ds));
        index_by_ptr[stored_ptr] = out.size() - 1;
        return out.back();
    };

    // BFS queue: subroutine stored pointers still to process.
    std::queue<uint32_t> to_visit;

    // Seed: every subaction event list, recording its callers into any
    // subroutine it invokes directly.
    auto scan_events = [&](const std::vector<Event>& events,
                           auto&& add_callsite) {
        for (std::size_t idx = 0; idx < events.size(); ++idx) {
            const uint32_t tgt = event_subroutine_target(events[idx]);
            if (tgt == 0u || tgt == 0xFFFFFFFFu) continue;
            // Skip cross-references into existing subactions.
            if (subaction_ptrs.count(tgt) > 0) continue;
            // Skip targets that don't look like event lists (throw-parameter
            // tables, grab data, other non-code structures).
            if (!looks_like_event_list_start(ms.data(), ms.size(),
                                             resolve_misc_ptr(tgt))) continue;
            auto& sub = ensure_slot(tgt);
            add_callsite(sub, idx);
            if (sub.events.empty() && sub.decode_error.empty()) {
                to_visit.push(tgt);
            }
        }
    };

    for (std::size_t i = 0; i < count; ++i) {
        for (const auto& tab : tabs) {
            const auto table = read_subaction_table(
                ms, root.fields[tab.field], i + 1);
            if (i >= table.size()) continue;
            const uint32_t stored = table[i];
            if (stored == 0u || stored == 0xFFFFFFFFu) continue;

            std::vector<Event> events;
            try {
                events = dec.decode(resolve_misc_ptr(stored));
            } catch (const std::exception&) {
                continue;    // ignore bad subaction event lists silently
            }
            scan_events(events, [&](DiscoveredSubroutine& sub, std::size_t idx) {
                SubroutineCallSite cs;
                cs.kind          = SubroutineCallSite::FromSubAction;
                cs.subaction_id  = i;
                cs.tab_label     = tab.label;
                cs.event_index   = idx;
                sub.callers.push_back(cs);
            });
        }
    }

    // Drain BFS. Each subroutine's own events may invoke further subroutines.
    while (!to_visit.empty()) {
        const uint32_t cur = to_visit.front(); to_visit.pop();
        auto& sub = out[index_by_ptr[cur]];
        if (!sub.events.empty()) continue;   // already decoded

        try {
            sub.events = dec.decode(sub.resolved_offset);
        } catch (const std::exception& ex) {
            sub.decode_error = ex.what();
            continue;
        }
        scan_events(sub.events, [&](DiscoveredSubroutine& child, std::size_t idx) {
            SubroutineCallSite cs;
            cs.kind               = SubroutineCallSite::FromSubroutine;
            cs.caller_stored_ptr  = cur;
            cs.event_index        = idx;
            child.callers.push_back(cs);
        });
    }

    // Deterministic ordering: by resolved offset (ascending).
    std::sort(out.begin(), out.end(),
              [](const DiscoveredSubroutine& a, const DiscoveredSubroutine& b) {
                  return a.resolved_offset < b.resolved_offset;
              });
    return out;
}

} // namespace psax
