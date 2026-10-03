#pragma once

#include <map>
#include <string>

#include "entities/Item.hpp"
#include "entities/StatusEffects.hpp"
#include "entities/Talent.hpp"

namespace engine {

// Which assets/icons/<name>.png represents each talent, item and status
// effect (see assets/sprites/CREDITS.txt for the icon set). Talents are
// keyed by id; anything unlisted falls back to an icon chosen from what
// the talent does, so a new talent always gets something sensible.
inline std::string talentIcon(const Talent& talent) {
    static const std::map<std::string, std::string> byId{
        {"basic.attack", "broadsword"}, {"basic.cleanse", "aura"}, {"basic.light", "crystal-ball"}, {"basic.shove", "push"}, {"basic.pray", "aura"},
        {"warrior.cleave", "axe-swing"}, {"warrior.slam", "hammer-drop"}, {"warrior.whirlwind", "spinning-sword"},
        {"warrior.rallying_cry", "shouting"}, {"warrior.berserkers_fury", "wolverine-claws"},
        {"warrior.undying_rage", "life-support"},
        {"arcane.bolt", "magic-swirl"}, {"mage.arcane_bolt", "magic-swirl"}, {"spellblade.arcane", "sword-spin"},
        {"mage.arcane_storm", "lightning-storm"}, {"mage.arcane_focus", "third-eye"},
        {"arcane.mind_shatter", "psychic-waves"}, {"mage.mind_shatter", "psychic-waves"},
        {"arcane.blink", "teleport"}, {"mage.blink", "teleport"}, {"spellblade.blink", "teleport"},
        {"arcane.efficiency", "crystal-ball"}, {"mage.overload", "power-lightning"},
        {"fire.meteor", "meteor-impact"}, {"mage.meteor", "meteor-impact"},
        {"fire.ember_bolt", "fire-dash"}, {"spellblade.ember_bolt", "fire-dash"},
        {"fire.fireball", "fireball"}, {"spellblade.fireball", "fireball"}, {"fire.kindle", "burning-embers"},
        {"ice.shard", "ice-bolt"}, {"ice.nova", "frozen-orb"}, {"ice.frostbite", "snowflake-1"},
        {"ice.shatter", "shattered-glass"},
        {"lightning.bolt", "lightning-arc"}, {"lightning.chain", "chained-arrow-heads"},
        {"lightning.discharge", "lightning-shout"}, {"lightning.static_charge", "static"},
        {"bow.quick_shot", "high-shot"}, {"thief.quick_shot", "high-shot"},
        {"bow.piercing_shot", "arrow-cluster"}, {"thief.piercing_shot", "arrow-cluster"},
        {"bow.volley", "arrow-scope"}, {"thief.volley", "arrow-scope"},
        {"bow.marksmanship", "target-arrows"}, {"thief.steady_aim", "target-arrows"},
        {"thief.adrenaline", "run"}, {"acrobatics.vault_kick", "boot-kick"}, {"thief.vault_kick", "boot-kick"},
        {"acrobatics.leap", "jump-across"}, {"acrobatics.tumble", "dodging"}, {"acrobatics.footwork", "footprint"},
        {"stealth.conceal", "hood"}, {"stealth.ambush", "backstab"}, {"stealth.strike", "stiletto"},
        {"stealth.vanish_strike", "shadow-follower"},
        {"shield.bash", "shield-bounces"}, {"shield.guard", "checked-shield"}, {"shield.shockwave", "shield-reflect"},
        {"shield.training", "shield"},
        {"cloth.gather_mana", "crystal-ball"}, {"cloth.pulse", "radial-balance"}, {"cloth.spellweave", "spell-book"},
        {"cloth.ward", "magic-shield"},
        {"animation.raise", "tombstone"}, {"animation.army", "skull-crossed-bones"},
        {"animation.pact", "shattered-glass"}, {"animation.swap", "bone-knife"},
        {"spellblade.flame", "flaming-trident"}, {"spellblade.frost", "ice-spear"},
        {"spellblade.storm", "lightning-saber"}, {"spellblade.imbue", "glowing-hands"},
        {"spellblade.renewal", "healing"}, {"spellblade.rhythm", "sword-clash"},
        {"spellblade.execution", "decapitation"}, {"spellblade.power_strike", "sword-smithing"},
        {"spellblade.quick_strike", "quick-slash"}, {"spellblade.reckless_lunge", "sprint"},
        {"spellblade.immolate", "burning-passion"}, {"spellblade.strike", "relic-blade"},
        {"one_handed.quick_strike", "quick-slash"}, {"one_handed.parry", "bordered-shield"},
        {"one_handed.riposte", "sword-wound"}, {"one_handed.execution", "decapitation"},
        {"two_handed.cleave", "axe-swing"}, {"two_handed.fury", "wolverine-claws"},
        {"two_handed.bloodlust", "burning-passion"}, {"two_handed.whirlwind", "spinning-sword"},
        {"light_armour.sidestep", "run"}, {"light_armour.evasion", "leather-armor"},
        {"light_armour.precision", "third-eye"}, {"light_armour.parting_strike", "plain-dagger"},
        {"heavy_armour.shoulder_check", "breastplate"}, {"heavy_armour.brace", "visored-helm"},
        {"heavy_armour.resolve", "life-support"}, {"heavy_armour.second_wind", "heart-inside"},
        {"blood_magic.pact", "bleeding-heart"}, {"blood_magic.drain", "healing"},
        {"blood_magic.deathless", "life-support"}, {"blood_magic.wither", "tombstone"},
        {"shadow_archer.shot", "shadow-follower"}, {"shadow_archer.mark", "arrow-scope"},
        {"shadow_archer.unseen", "hood"}, {"shadow_archer.death", "skull-crossed-bones"},
        {"spellblade.release", "shattered-glass"},
        {"brawling.tackle", "charging-bull"}, {"brawling.grapple", "grab"}, {"brawling.hard_landing", "impact-point"},
        {"brawling.hurl", "human-cannonball"},
        {"whip.lash", "whip"}, {"whip.trip", "tripwire"}, {"whip.flay", "barbed-coil"}, {"whip.snare", "lasso"},
        {"shadow.bolt", "evil-moon"}, {"shadow.snuff", "smoking-orb"}, {"shadow.umbral", "night-sky"}, {"shadow.veil", "eclipse"},
        {"radiance.sear", "sunbeams"}, {"radiance.flare", "sun-radiations"}, {"radiance.inner_light", "candlebright"}, {"radiance.dawn", "sunrise"},
        {"alchemy.oil", "round-bottom-flask"}, {"alchemy.firebomb", "molotov"}, {"alchemy.brews", "bubbling-flask"}, {"alchemy.acid", "fizzing-flask"},
    };
    if (const auto it = byId.find(talent.id); it != byId.end()) return it->second;
    if (talent.name == "Second Wind") return "heart-inside";
    if (talent.passive) return "third-eye";
    if (talent.restoreHpPercent || talent.effectKind == TalentEffectKind::Heal) return "healing";
    if (talent.restoreMana) return "crystal-ball";
    if (talent.shape == EffectShape::Movement) return "sprint";
    if (talent.effectKind == TalentEffectKind::SelfBuff) return "aura";
    if (talent.tags & AreaTag) return "radial-balance";
    if (talent.projectile) return "magic-swirl";
    return "broadsword";
}

inline std::string itemIcon(const ItemDefinition& item) {
    switch (item.slot) {
        case EquipmentSlot::Weapon:
            switch (item.weaponKind) {
                case WeaponKind::TwoHanded: return "relic-blade";
                case WeaponKind::Bow: return "pocket-bow";
                case WeaponKind::Staff: return "wizard-staff";
                case WeaponKind::Whip: return "whip";
                default: return std::string(item.id).find("dagger") != std::string::npos ? "plain-dagger" : "broadsword";
            }
        case EquipmentSlot::Armour:
            return item.armourKind == ArmourKind::Heavy ? "breastplate"
                 : item.armourKind == ArmourKind::Light ? "leather-armor" : "robe";
        case EquipmentSlot::Charm: return "gem-pendant";
        case EquipmentSlot::OffHand:
            return item.weaponKind == WeaponKind::Shield ? "bordered-shield" : "crystal-wand";
        case EquipmentSlot::Head: return "visored-helm";
        case EquipmentSlot::Cloak: return "cape";
        case EquipmentSlot::Hands: return "gloves";
        case EquipmentSlot::Belt: return "belt-buckles";
        case EquipmentSlot::Feet: return "boots";
        case EquipmentSlot::Ring1: case EquipmentSlot::Ring2: return "ring";
    }
    return "knapsack";
}

// The faint silhouette shown in an empty equipment slot.
inline std::string slotIcon(EquipmentSlot slot) {
    switch (slot) {
        case EquipmentSlot::Weapon: return "broadsword";
        case EquipmentSlot::Armour: return "breastplate";
        case EquipmentSlot::Charm: return "gem-pendant";
        case EquipmentSlot::OffHand: return "bordered-shield";
        case EquipmentSlot::Head: return "visored-helm";
        case EquipmentSlot::Cloak: return "cape";
        case EquipmentSlot::Hands: return "gloves";
        case EquipmentSlot::Belt: return "belt-buckles";
        case EquipmentSlot::Feet: return "boots";
        case EquipmentSlot::Ring1: case EquipmentSlot::Ring2: return "ring";
    }
    return "knapsack";
}

inline std::string statusIcon(StatusEffectType type) {
    switch (type) {
        case StatusEffectType::Poison: return "bleeding-heart";
        case StatusEffectType::Stun: return "hammer-drop";
        case StatusEffectType::StunRecovery: return "life-support";
        case StatusEffectType::Empowered: return "glowing-hands";
        case StatusEffectType::Burn: return "burning-embers";
        case StatusEffectType::Chill: return "snowflake-1";
        case StatusEffectType::Shock: return "static";
        case StatusEffectType::Guard: return "checked-shield";
        case StatusEffectType::Evasion: return "dodging";
        case StatusEffectType::Concealed: return "hood";
        case StatusEffectType::Opening: return "footprint";
        case StatusEffectType::Marked: return "target-arrows";
        case StatusEffectType::FlameBlade: return "flaming-trident";
        case StatusEffectType::FrostBlade: return "ice-spear";
        case StatusEffectType::StormBlade: return "lightning-saber";
        case StatusEffectType::ArcaneBlade: return "sword-spin";
        case StatusEffectType::BattleRhythm: return "sword-clash";
        case StatusEffectType::BloodPact: return "bleeding-heart";
        case StatusEffectType::Wither: return "tombstone";
        case StatusEffectType::HuntersMark: return "arrow-scope";
        case StatusEffectType::UnseenReady: return "hood";
        case StatusEffectType::ManaDrain: return "psychic-waves";
        case StatusEffectType::Doom: return "skull-crossed-bones";
        case StatusEffectType::Smothered: return "shadow-follower";
        case StatusEffectType::Grappled: return "manacles";
        case StatusEffectType::Blinded: return "blindfold";
    }
    return "aura";
}

} // namespace engine
