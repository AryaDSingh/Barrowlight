#pragma once

#include "entities/Item.hpp"
#include "entities/MonsterType.hpp"
#include "entities/Talent.hpp"

namespace engine {

// What a hit sounds like: the weapon's edge, or the element of the spell.
// Each family has several recorded variants in assets/sounds/sfx/.
enum class HitSound { Slash, Pierce, Blunt, Fire, Frost, Lightning, Water, Arcane, Shadow, Light, Earth, Rot, Blood };

inline const char* hitFamily(HitSound s) {
    switch (s) {
        case HitSound::Slash: return "slash";
        case HitSound::Pierce: return "pierce";
        case HitSound::Blunt: return "blunt";
        case HitSound::Fire: return "fire";
        case HitSound::Frost: return "frost";
        case HitSound::Lightning: return "lightning";
        case HitSound::Water: return "water";
        case HitSound::Arcane: return "arcane";
        case HitSound::Shadow: return "shadow";
        case HitSound::Light: return "light";
        case HitSound::Earth: return "earth";
        case HitSound::Rot: return "rot";
        case HitSound::Blood: return "blood";
    }
    return "blunt";
}

// The layer a critical hit adds on top: a wet spray where blades bite into
// something that bleeds, a bone crunch from blunt blows or bloodless foes,
// and each element's own heavier version.
inline const char* critFamily(HitSound s, bool targetBleeds) {
    switch (s) {
        case HitSound::Slash: case HitSound::Pierce: case HitSound::Blood:
            return targetBleeds ? "crit_gore" : "crit_bone";
        case HitSound::Blunt: case HitSound::Earth: return "crit_bone";
        case HitSound::Fire: return "crit_fire";
        case HitSound::Frost: return "crit_frost";
        case HitSound::Lightning: return "crit_storm";
        default: return "crit_magic";
    }
}

// A weapon's own edge.
inline HitSound weaponSound(WeaponKind w) {
    switch (w) {
        case WeaponKind::OneHanded: case WeaponKind::TwoHanded: case WeaponKind::Dagger: case WeaponKind::Whip: return HitSound::Slash;
        case WeaponKind::Bow: case WeaponKind::Spear: case WeaponKind::Crossbow: return HitSound::Pierce;
        default: return HitSound::Blunt; // maces, shields, staves and bare fists
    }
}

// A talent sounds like its element when it has one, otherwise like the weapon in hand.
inline HitSound talentSound(const Talent& t, WeaponKind weapon) {
    switch (t.tree) {
        case TalentTree::Fire: case TalentTree::Lamplighter: return HitSound::Fire;
        case TalentTree::Ice: return HitSound::Frost;
        case TalentTree::Lightning: case TalentTree::Stormlance: case TalentTree::Tempest: return HitSound::Lightning;
        case TalentTree::Tide: return HitSound::Water;
        case TalentTree::Arcane: case TalentTree::Cloth: return HitSound::Arcane;
        case TalentTree::Shadow: case TalentTree::Hexes: case TalentTree::Animation: case TalentTree::Bonewright: return HitSound::Shadow;
        case TalentTree::Radiance: return HitSound::Light;
        case TalentTree::Earth: return HitSound::Earth;
        case TalentTree::Venom: return HitSound::Rot;
        case TalentTree::BloodMagic: return HitSound::Blood;
        default: break;
    }
    if (t.onHitEffect) switch (t.onHitEffect->type) {
        case StatusEffectType::Burn: return HitSound::Fire;
        case StatusEffectType::Chill: return HitSound::Frost;
        case StatusEffectType::Shock: return HitSound::Lightning;
        case StatusEffectType::Poison: case StatusEffectType::Plague: return HitSound::Rot;
        default: break;
    }
    if (t.spellstrike) return HitSound::Arcane;
    if (t.tree == TalentTree::Bow || t.tree == TalentTree::Crossbow || t.tree == TalentTree::ShadowArcher || t.projectile) return HitSound::Pierce;
    if (t.tree == TalentTree::Brawling || t.tree == TalentTree::Stonefist || t.tree == TalentTree::Mace || t.tree == TalentTree::Shield) return HitSound::Blunt;
    return weaponSound(weapon);
}

// A monster's blow, by what it is and how it fights.
inline HitSound monsterSound(MonsterType type, bool magic) {
    switch (type) {
        case MonsterType::Archer: case MonsterType::SkeletonArcher: case MonsterType::GoblinSlinger: return HitSound::Pierce;
        case MonsterType::Ogre: case MonsterType::GoblinBulwark: case MonsterType::SkeletonGuard: case MonsterType::CryptSentinel: return HitSound::Blunt;
        case MonsterType::OrcFirebrand: case MonsterType::Torchbearer: return HitSound::Fire;
        case MonsterType::FrostAcolyte: return HitSound::Frost;
        case MonsterType::DrownedChorister: return HitSound::Lightning;
        case MonsterType::Gloomstalker: case MonsterType::CryptShade: return HitSound::Shadow;
        case MonsterType::DrownedOne: case MonsterType::DeepLurker: case MonsterType::TheSleeper: return HitSound::Water;
        case MonsterType::Spider: return HitSound::Rot;
        case MonsterType::Mimic: return HitSound::Pierce;
        case MonsterType::OrcSmith: case MonsterType::Forgemaster: return HitSound::Blunt;
        case MonsterType::SlagGolem: return HitSound::Earth;
        case MonsterType::Slagling: case MonsterType::BellowsImp: return HitSound::Fire;
        default: return magic ? HitSound::Arcane : HitSound::Slash;
    }
}

// What a monster sounds like when it spots you, is hurt, or dies:
// assets/sounds/sfx/voice_<kind>_<alert|hurt|death>_<n>.ogg.
inline const char* monsterVoice(MonsterType type) {
    switch (type) {
        case MonsterType::Ogre: case MonsterType::GoblinWarlord: case MonsterType::OrcFirebrand: case MonsterType::Mimic:
        case MonsterType::OrcSmith: case MonsterType::SlagGolem: case MonsterType::Forgemaster: return "brute";
        case MonsterType::Slagling: return "drowned"; // it bubbles
        case MonsterType::BellowsImp: return "goblin";
        case MonsterType::Gloomstalker: return "beast";
        case MonsterType::Spider: return "spider";
        case MonsterType::Skeleton: case MonsterType::SkeletonArcher: case MonsterType::SkeletonGuard: case MonsterType::CryptSentinel: return "bones";
        case MonsterType::CryptShade: case MonsterType::Lich: case MonsterType::DrownedChorister: return "spirit";
        case MonsterType::DrownedOne: case MonsterType::DeepLurker: case MonsterType::TheSleeper: return "drowned";
        case MonsterType::Archer: case MonsterType::Bonecaller: case MonsterType::GraveMender: case MonsterType::FrostAcolyte:
        case MonsterType::OssuaryWarden: case MonsterType::Torchbearer: return "human";
        default: return "goblin";
    }
}

} // namespace engine
