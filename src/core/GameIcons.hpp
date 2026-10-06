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
        {"fire.flame_wall", "campfire"},
        {"spellblade.cleave", "sword-spin"}, {"spellblade.ward", "magic-shield"}, {"spellblade.blink_strike", "teleport"},
        {"animation.spear", "bone-knife"}, {"animation.bone_armour", "broken-bone"}, {"animation.prison", "stone-tower"},
        {"blood_magic.boil", "bleeding-wound"}, {"blood_magic.transfusion", "heart-inside"}, {"blood_magic.rite", "sacrificial-dagger"},
        {"resonance.grave_light", "candlebright"}, {"resonance.boiling_blood", "burning-passion"}, {"resonance.bloodletter", "bloody-sword"},
        {"acrobatics.somersault", "jump-across"}, {"acrobatics.fleet", "sprint"}, {"acrobatics.untouchable", "dodging"},
        {"cloth.shroud", "magic-shield"}, {"cloth.surge", "crystal-ball"}, {"cloth.burst", "psychic-waves"},
        {"light_armour.feint", "quick-slash"}, {"light_armour.blur", "run"}, {"light_armour.perfect", "plain-dagger"},
        {"heavy_armour.fortify", "visored-helm"}, {"heavy_armour.juggernaut", "charging-bull"}, {"heavy_armour.unbreakable", "breastplate"},
        {"resonance.flowing_mana", "drop"}, {"resonance.arcane_bulwark", "shield-reflect"}, {"resonance.unstoppable", "shield-bounces"},
        {"alchemy.frost", "fizzing-flask"}, {"alchemy.volatile", "bubbling-flask"}, {"alchemy.greek_fire", "molotov"},
        {"traps.bear", "wolf-trap"}, {"traps.ambusher", "trap-mask"}, {"traps.net", "lasso"},
        {"hexes.enfeeble", "hourglass"}, {"hexes.echo", "cursed-star"}, {"hexes.doom", "death-zone"},
        {"resonance.incendiary", "fire-dash"}, {"resonance.wasting_curse", "dripping-goo"}, {"resonance.witchfire", "evil-book"},
        {"brawling.haymaker", "punch-blast"}, {"brawling.knockout", "fist"}, {"brawling.piledriver", "hammer-drop"},
        {"whip.disarm", "whip"}, {"whip.taskmaster", "manacles"}, {"whip.whirl", "barbed-coil"},
        {"skirmish.hit_and_run", "fire-dash"}, {"skirmish.slipstream", "wind-slap"}, {"skirmish.flying_kick", "boot-kick"},
        {"resonance.ghost_step", "shadow-follower"}, {"resonance.lightning_feet", "lightning-arc"}, {"resonance.tremor", "earth-crack"},
        {"spear.impale", "pierced-body"}, {"spear.skewer", "spear-hook"}, {"spear.throw", "thrown-spear"},
        {"mace.slam", "hammer-drop"}, {"mace.concussion", "broken-bone"}, {"mace.skullcracker", "spiked-mace"},
        {"crossbow.explosive", "time-bomb"}, {"crossbow.heavy_draw", "crossbow"}, {"crossbow.ballista", "heavy-arrow"},
        {"resonance.bonecrusher", "mace-head"}, {"resonance.storm_bolts", "power-lightning"}, {"resonance.bedrock", "stone-block"},
        {"earth.grasp", "earth-crack"}, {"earth.aftershock", "earth-spit"}, {"earth.boulder", "rolling-bomb"},
        {"tide.undertow", "big-wave"}, {"tide.tidecaller", "drop"}, {"tide.flood", "water-bolt"},
        {"venom.fester", "dripping-goo"}, {"venom.virulence", "virus"}, {"venom.blight", "plague-doctor-profile"},
        {"resonance.mire", "falling-rocks"}, {"resonance.foul_water", "poison-bottle"}, {"resonance.envenomed_blades", "bloody-sword"},
        {"bow.point_blank", "high-shot"}, {"bow.focus", "target-arrows"}, {"bow.rain", "arrow-cluster"},
        {"stealth.feign", "tombstone"}, {"stealth.phantom", "ninja-mask"}, {"stealth.assassinate", "backstab"},
        {"daggers.throw", "bone-knife"}, {"daggers.cut_deep", "bleeding-wound"}, {"daggers.eviscerate", "sacrificial-dagger"},
        {"resonance.fire_arrows", "flaming-trident"}, {"resonance.unseen_hand", "hood"}, {"resonance.assassins_edge", "stiletto"},
        {"shield.rush", "charging-bull"}, {"shield.bulwark", "checked-shield"}, {"shield.bastion", "stone-tower"},
        {"shadow.step", "shadow-follower"}, {"shadow.dread", "blindfold"}, {"shadow.devour", "evil-book"},
        {"radiance.holy_light", "healing"}, {"radiance.halo", "sun-radiations"}, {"radiance.judgement", "sunbeams"},
        {"resonance.twilight", "eclipse"}, {"resonance.templars_edge", "relic-blade"}, {"resonance.hallowed_guard", "magic-shield"},
        {"ice.rime_field", "snowflake-1"}, {"ice.hoarfrost", "shatter"}, {"ice.blizzard", "frozen-orb"},
        {"lightning.thunderclap", "lightning-shout"}, {"lightning.arc_flash", "static"}, {"lightning.tempest", "lightning-storm"},
        {"two_handed.leap_slam", "jump-across"}, {"two_handed.follow_through", "sprint"}, {"two_handed.blood_frenzy", "bloody-sword"},
        {"resonance.thermal_shock", "lightning-saber"}, {"resonance.cold_steel", "ice-spear"}, {"resonance.searing_edge", "flaming-trident"}, {"resonance.spellsword", "relic-blade"}, {"fire.kindling", "lantern-flame"}, {"fire.wildfire", "burning-embers"}, {"fire.firestorm", "flame-spin"},
        {"one_handed.pommel", "sword-clash"}, {"one_handed.exploit", "bleeding-wound"}, {"one_handed.blade_dance", "spinning-blades"},
        {"arcane.repulse", "wind-slap"}, {"arcane.afterimage", "shadow-follower"}, {"arcane.kinetic", "impact-point"}, {"arcane.torrent", "crystal-wand"},
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
        {"spear.thrust", "spear-feather"}, {"spear.brace", "spiked-fence"}, {"spear.long_reach", "stone-spear"}, {"spear.vault", "jump-across"},
        {"daggers.lacerate", "cut-palm"}, {"daggers.backstab", "sacrificial-dagger"}, {"daggers.hemorrhage", "bleeding-wound"}, {"daggers.whirl", "spinning-blades"},
        {"mace.crush", "mace-head"}, {"mace.stagger", "sands-of-time"}, {"mace.bonebreaker", "broken-bone"}, {"mace.shatter", "shatter"},
        {"crossbow.heavy", "heavy-arrow"}, {"crossbow.pierce", "pierced-body"}, {"crossbow.windlass", "target-shot"}, {"crossbow.pin", "pin"},
        {"earth.spike", "earth-spit"}, {"earth.pillar", "stone-tower"}, {"earth.stoneskin", "rock"}, {"earth.quake", "earth-crack"},
        {"tide.bolt", "water-bolt"}, {"tide.wave", "big-wave"}, {"tide.riptide", "drop"}, {"tide.maelstrom", "vortex"},
        {"hexes.misfortune", "cursed-star"}, {"hexes.link", "linked-rings"}, {"hexes.malediction", "evil-book"}, {"hexes.puppet", "puppet"},
        {"venom.bolt", "poison-bottle"}, {"venom.miasma", "poison-gas"}, {"venom.ruin", "dripping-goo"}, {"venom.plague", "plague-doctor-profile"},
        {"traps.snare", "wolf-trap"}, {"traps.tripwire", "tripwire"}, {"traps.trapper", "trap-mask"}, {"traps.rigged", "time-bomb"},
        {"skirmish.lunge", "sword-slice"}, {"skirmish.pass", "crossed-swords"}, {"skirmish.running_start", "sprint"}, {"skirmish.blitz", "wind-slap"},
        {"lamplighter.swing", "torch"}, {"lamplighter.hurl", "flame-spin"}, {"lamplighter.ward", "lantern-flame"}, {"lamplighter.bonfire", "campfire"},
        {"stormlance.thrust", "lightning-tear"}, {"stormlance.javelin", "thrown-spear"}, {"stormlance.static_edge", "zeus-sword"}, {"stormlance.vault", "thunder-struck"},
        {"hexblade.edge", "bloody-sword"}, {"hexblade.rend", "skull-slices"}, {"hexblade.lingering", "hourglass"}, {"hexblade.doom", "death-zone"},
        {"saboteur.caltrops", "caltrops"}, {"saboteur.smoke", "powder"}, {"saboteur.tricks", "ninja-mask"}, {"saboteur.booby", "rolling-bomb"},
        {"stonefist.fist", "fist"}, {"stonefist.slam", "punch-blast"}, {"stonefist.granite", "stone-block"}, {"stonefist.landslide", "falling-rocks"},
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
                case WeaponKind::Spear: return "spear-hook";
                case WeaponKind::Mace: return "spiked-mace";
                case WeaponKind::Crossbow: return "crossbow";
                case WeaponKind::Dagger: return "plain-dagger";
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
        case StatusEffectType::Bleed: return "bleeding-wound";
        case StatusEffectType::Sundered: return "cracked-shield";
        case StatusEffectType::Pinned: return "pin";
        case StatusEffectType::Braced: return "spiked-fence";
        case StatusEffectType::Misfortune: return "cursed-star";
        case StatusEffectType::Linked: return "linked-rings";
        case StatusEffectType::Puppeted: return "puppet";
        case StatusEffectType::Plague: return "virus";
        case StatusEffectType::Frenzy: return "bloody-sword";
        case StatusEffectType::Hasted: return "sprint";
        case StatusEffectType::Slowed: return "hourglass";
    }
    return "aura";
}

} // namespace engine
