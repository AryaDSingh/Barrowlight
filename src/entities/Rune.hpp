#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <string_view>
#include "entities/Talent.hpp"

namespace engine {
struct RuneDefinition { const char* id; const char* name; const char* description; };
inline constexpr std::array<RuneDefinition, 4> kRunes{{
    {"chain", "Chain", "Single-target projectiles: bounce to one other visible enemy within 3 tiles, at 50% damage. Walls block the bounce."},
    {"widen", "Widen", "Area attacks: +1 radius, +50% mana cost (rounded up; at least +1)."},
    {"venom", "Venom", "Damaging hits: 80% direct damage, then poison for 2 damage over each of 3 enemy turns. No existing on-hit effect allowed."},
    {"swift_passage", "Swift Passage", "Pure movement: +2 tiles and +2 cooldown turns. Does not support attacks with a retreat."},
}};
inline const RuneDefinition* findRune(std::string_view id) {
    for (const auto& rune : kRunes) if (id == rune.id) return &rune;
    return nullptr;
}
struct RuneInstance {
    std::string definitionId;
    std::uint64_t instanceId = 0;
    std::string talentId; // empty means in the rune bag; one owner/attachment per instance
};
inline std::string runeUnavailableReason(const Talent& talent, std::string_view runeId) {
    if (!findRune(runeId)) return "Unknown rune.";
    if (talent.id.empty()) return "This talent has no support slot.";
    if (runeId == "chain" && (!(talent.tags & ProjectileTag) || !(talent.tags & DamagingTag) || talent.shape != EffectShape::SingleTarget))
        return "Chain needs a damaging single-target projectile.";
    if (runeId == "widen" && (!(talent.tags & AreaTag) || !(talent.tags & DamagingTag)))
        return "Widen needs a damaging area talent.";
    if (runeId == "venom" && (!(talent.tags & DamagingTag) || talent.onHitEffect))
        return "Venom needs damage with no existing on-hit effect.";
    if (runeId == "swift_passage" && !(talent.tags & PureMovementTag))
        return "Swift Passage needs pure movement (such as Blink).";
    return {};
}
inline Talent resolveEffectiveTalent(const Talent& base, std::string_view runeId) {
    Talent result = base;
    result.scalingCooldown = base.cooldownTurns;
    if (runeId.empty() || !runeUnavailableReason(base, runeId).empty()) return result;
    if (runeId == "chain") result.chain = true;
    else if (runeId == "widen") { ++result.areaRadius; result.manaCost += std::max(1, (base.manaCost + 1) / 2); }
    else if (runeId == "venom") {
        result.damagePercent = 80;
        result.onHitEffect = StatusEffectInstance{StatusEffectType::Poison, 3, 2};
        result.onHitChance = 1.f;
    } else if (runeId == "swift_passage") { result.moveDistance += 2; result.cooldownTurns += 2; }
    return result;
}
}
