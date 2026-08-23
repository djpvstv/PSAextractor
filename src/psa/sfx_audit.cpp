#include "psa/sfx_audit.hpp"

#include "psa/arg.hpp"
#include "psa/character_root.hpp"
#include "psa/event_decoder.hpp"
#include "psa/misc_section.hpp"
#include "psa/subaction_flags.hpp"
#include "psa/subaction_table.hpp"

namespace psax {

namespace {

bool is_ra_basic_8_to_10(const Arg& a) {
    if (a.type != ArgType::Variable) return false;
    const auto v = variable_from_raw(a.raw_value);
    return v.mem_class == VariableRef::MemClass::RA
        && v.data_type == VariableRef::DataType::Basic
        && v.index >= 8u && v.index <= 10u;
}

bool is_direct_sound(const Event& e) {
    return e.cmd_id == 0x0A000100u    // SoundEffect
        || e.cmd_id == 0x0A030100u;   // SoundEffectTransient
}

// Extract the sound ID from a SoundEffect / SoundEffectTransient event.
// Both commands take a single Value arg (the sound bank offset).
uint32_t direct_sound_id(const Event& e) {
    return e.args.empty() ? 0u : e.args[0].raw_value;
}

bool is_ra_basic_write(const Event& e) {
    return e.cmd_id == 0x12000200u    // BasicVariableSet
        && e.args.size() >= 2u
        && is_ra_basic_8_to_10(e.args[1]);
}

bool is_collision(const Event& e) {
    return e.cmd_id == 0x06000D00u    // OffensiveCollision          (13 args)
        || e.cmd_id == 0x06150F00u;   // SpecialOffensiveCollision   (15 args)
    // Note: 0x06010200 is ChangeHitboxDamage, NOT a collision creator.
}

} // namespace

bool event_is_sfx_candidate(const Event& e) {
    return is_direct_sound(e) || is_ra_basic_write(e) || is_collision(e);
}

std::vector<Event> filter_sfx_events(const std::vector<Event>& events,
                                     const SfxAuditOptions& opt) {
    // First pass: does this event list write RA-Basic[8..10] at all?
    bool has_custom_sound_var = false;
    for (const auto& e : events) {
        if (is_ra_basic_write(e)) { has_custom_sound_var = true; break; }
    }

    std::vector<Event> out;
    for (const auto& e : events) {
        if (is_direct_sound(e)) {
            const uint32_t id = direct_sound_id(e);
            if (id >= opt.min_sound_id && id <= opt.max_sound_id) out.push_back(e);
        } else if (is_ra_basic_write(e)) {
            out.push_back(e);
        } else if (is_collision(e) && has_custom_sound_var) {
            out.push_back(e);
        }
    }
    return out;
}

std::vector<SfxAuditEntry> audit_sfx(const MiscSection& ms,
                                     const SfxAuditOptions& opt) {
    std::vector<SfxAuditEntry> out;
    const auto root = load_character_root(ms);

    const std::size_t count = subaction_main_count(
        root.fields[CharacterRoot::SubActionMain],
        root.fields[CharacterRoot::SubActionGFX]);
    if (count == 0) return out;

    const auto flags = read_subaction_flags(
        ms, root.fields[CharacterRoot::SubActionFlags], count);

    struct TabDesc { const char* label; CharacterRoot::Field field; };
    const TabDesc tabs[] = {
        {"Main",  CharacterRoot::SubActionMain},
        {"GFX",   CharacterRoot::SubActionGFX},
        {"SFX",   CharacterRoot::SubActionSFX},
        {"Other", CharacterRoot::SubActionOther},
    };

    EventDecoder dec(ms.data(), ms.size());

    for (std::size_t i = 0; i < count; ++i) {
        for (const auto& tab : tabs) {
            const auto table = read_subaction_table(
                ms, root.fields[tab.field], i + 1);
            if (i >= table.size() || table[i] == 0) continue;

            const auto events   = dec.decode(resolve_misc_ptr(table[i]));
            const auto relevant = filter_sfx_events(events, opt);
            if (relevant.empty()) continue;

            SfxAuditEntry entry;
            entry.subaction_id = i;
            entry.tab_label    = tab.label;
            entry.anim_name    = (i < flags.size())
                                    ? subaction_anim_name(ms, flags[i])
                                    : std::string{};
            entry.events       = std::move(relevant);
            out.push_back(std::move(entry));
        }
    }
    return out;
}

} // namespace psax
