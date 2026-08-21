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

} // namespace

bool event_is_sfx_relevant(const Event& e) {
    switch (e.cmd_id) {
        case 0x0A000100u:   // SoundEffect
        case 0x0A030100u:   // SoundEffectTransient
        case 0x06000D00u:   // OffensiveCollision
        case 0x06010200u:   // SpecialOffensiveCollision
            return true;
        case 0x12000200u:   // BasicVariableSet: only if writing RA-Basic[8..10]
            return e.args.size() >= 2u && is_ra_basic_8_to_10(e.args[1]);
        default:
            return false;
    }
}

std::vector<SfxAuditEntry> audit_sfx(const MiscSection& ms) {
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

    // Iterate subaction-major so all 4 tabs for one subaction stay together.
    for (std::size_t i = 0; i < count; ++i) {
        for (const auto& tab : tabs) {
            const auto table = read_subaction_table(
                ms, root.fields[tab.field], i + 1);
            if (i >= table.size() || table[i] == 0) continue;

            const auto events = dec.decode(resolve_misc_ptr(table[i]));
            std::vector<Event> relevant;
            for (const auto& e : events) {
                if (event_is_sfx_relevant(e)) relevant.push_back(e);
            }
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
