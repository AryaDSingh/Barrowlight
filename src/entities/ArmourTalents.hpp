#pragma once

#include <algorithm>

#include "entities/Actor.hpp"

namespace engine {
inline bool armourMatches(const Actor& actor, ArmourRequirement requirement) {
    const auto kind=actor.inventory().armourKind();
    switch (requirement) {
    case ArmourRequirement::Cloth: return kind==ArmourKind::Cloth || kind==ArmourKind::Unarmoured;
    case ArmourRequirement::Light: return kind==ArmourKind::Light;
    case ArmourRequirement::Heavy: return kind==ArmourKind::Heavy;
    default: return true;
    }
}
inline const char* armourRequirementText(ArmourRequirement requirement) {
    switch (requirement) {
    case ArmourRequirement::Cloth: return "Requires cloth or no armour.";
    case ArmourRequirement::Light: return "Requires light armour.";
    case ArmourRequirement::Heavy: return "Requires heavy armour.";
    default: return "";
    }
}
// Read equipment at the moment of use; no cached buffs can survive an armour swap.
inline int armourPassive(const Actor& actor, PassiveKind kind) {
    ArmourRequirement requirement=ArmourRequirement::None;
    switch (kind) {
    case PassiveKind::ClothWard: case PassiveKind::Spellweave: requirement=ArmourRequirement::Cloth; break;
    case PassiveKind::LightEvasion: case PassiveKind::LightPrecision: requirement=ArmourRequirement::Light; break;
    case PassiveKind::HeavyBrace: case PassiveKind::HeavyResolve: requirement=ArmourRequirement::Heavy; break;
    default: return 0;
    }
    return armourMatches(actor,requirement) ? actor.talents().passiveValue(kind) : 0;
}
// Gear defences. Armour takes a share off every hit (with diminishing
// returns), evasion is dodge, and ward is a shield that soaks hits and
// refills once you've been left alone a few turns (Application::tickWard).
inline int gearArmour(const Actor& actor) { return std::max(0, actor.inventory().defence(ArmourKind::Heavy) + actor.inventory().affixTotal(BonusStat::Armour)); }
inline int gearEvasion(const Actor& actor) { return actor.inventory().defence(ArmourKind::Light) + actor.inventory().affixTotal(BonusStat::Evasion); }
inline int gearWard(const Actor& actor) { return std::max(0, actor.inventory().defence(ArmourKind::Cloth) + actor.inventory().affixTotal(BonusStat::Ward)); }
// Armour takes a flat amount off every direct hit, attack or spell: a quarter
// of it, rounded up (a Chain Coat's 8 stops 2). A hit that lands always
// deals at least 1; Guard and ward can still stop it entirely.
inline int armourReduction(int armour) { return armour <= 0 ? 0 : (armour + 3) / 4; }
inline int afterArmour(int damage, int armour) { return damage <= 0 ? damage : std::max(1, damage - armourReduction(armour)); }
inline int armourDodgeBonus(const Actor& actor) {
    int bonus=actor.stats().maxMana>0 && actor.stats().mana*2>=actor.stats().maxMana ? armourPassive(actor,PassiveKind::ClothWard) : 0;
    if (actor.statusEffects().has(StatusEffectType::Opening)) bonus+=armourPassive(actor,PassiveKind::LightEvasion);
    return bonus+gearEvasion(actor);
}
inline int armourCritBonus(const Actor& actor) {
    return actor.statusEffects().has(StatusEffectType::Opening) ? armourPassive(actor,PassiveKind::LightPrecision) : 0;
}
inline int armourGuardBonus(const Actor& actor) {
    return actor.statusEffects().has(StatusEffectType::Opening) ? armourPassive(actor,PassiveKind::HeavyBrace) : 0;
}
// Ascendancy defences (entities/Ascendancy.hpp): Last Stand blunts hits at
// low life; Quick Hands adds dodge.
// Gear affixes join them: Warding blunts hits, Nimble adds dodge.
inline int ascendancyGuardBonus(const Actor& actor) {
    return (actor.talents().passiveValue(PassiveKind::LastStand,actor.stats()) && actor.stats().hp*3<=actor.stats().maxHp ? 3 : 0) +
           actor.inventory().affixTotal(BonusStat::Warding) + actor.talents().passiveValue(PassiveKind::Resilience,actor.stats()) +
           actor.talents().passiveValue(PassiveKind::Stoneskin,actor.stats()) + actor.statusEffects().magnitudeOf(StatusEffectType::Steadfast) + actor.statusEffects().magnitudeOf(StatusEffectType::Thornguard) + actor.statusEffects().magnitudeOf(StatusEffectType::GlacialGuard) + (actor.statusEffects().has(StatusEffectType::WinterMarch) ? 3 : 0) +
           (actor.talents().passiveValue(PassiveKind::HeatSink,actor.stats()) ? actor.statusEffects().magnitudeOf(StatusEffectType::Heat)/3 : 0) +
           (actor.talents().passiveValue(PassiveKind::HeatEngine,actor.stats()) ? actor.statusEffects().magnitudeOf(StatusEffectType::Heat)/2 : 0) +
           (actor.statusEffects().has(StatusEffectType::Opening) ? actor.talents().passiveValue(PassiveKind::EnGarde,actor.stats()) : 0);
}
inline int ascendancyDodgeBonus(const Actor& actor) {
    return actor.talents().passiveValue(PassiveKind::QuickHands,actor.stats()) + actor.inventory().affixTotal(BonusStat::Dodge) +
           actor.talents().passiveValue(PassiveKind::Versatility,actor.stats());
}
inline bool armourResistsStun(const Actor& actor) {
    const int chance=armourPassive(actor,PassiveKind::HeavyResolve);
    return chance>0 && rollChance(chance/100.f);
}
} // namespace engine
