#pragma once

#include <utility>
#include <algorithm>

#include "entities/Actor.hpp"

namespace engine {

// The player-controlled Actor. Always has ai() == nullptr -- decisions
// come from input (a later prompt's InputHandler), never from a strategy
// object.
//
// As of Prompt 15, the talent kit is passed in rather than hardcoded --
// previously this constructor built TalentSet(spellbladeTalents())
// itself, coupling Player to one specific class. Which kit a given
// Player actually knows is now PlayerClassFactory's job
// (talentSetForClass), the same "factory decides, the class itself
// stays generic" shape MonsterFactory already established for monsters.
// Progression adds talents to that initial kit as levels are earned.
//
// As of Prompt 20: level_/xp_ track character progression -- level_
// starts at 1, capped at 10; xp_ is progress toward the *next* level
// specifically (resets on level-up), not a cumulative lifetime total,
// so "45/60 XP" reads directly as "progress within this level" for the
// HUD rather than needing a separate cumulative-vs-incremental
// conversion. The actual leveling curve and level-up effects live in
// PlayerLeveling.hpp (logic), not here -- Player stays plain data plus
// simple accessors, the same separation this project has kept between
// data and behavior everywhere else (Stats/AttributeFormulas, Talent/
// TalentEffects).
//
// Tree ownership and point balances are permanent progression; TalentSet owns ranks/cooldowns.
class Player : public Actor {
public:
    Player(Position position, Stats stats, TalentSet talents)
        : Actor("Player", '@', position, stats, nullptr, std::move(talents)), baseStats_(stats) {}

    // Permanent progression changes base stats; combat changes current hp/mana in stats().
    Stats& baseStats() { return baseStats_; }
    const Stats& baseStats() const { return baseStats_; }
    Stats effectiveStats() const {
        Stats result = baseStats_;
        for (int i = 0; i < kEquipmentSlotCount; ++i) {
            const auto* item = inventory().equipped(static_cast<EquipmentSlot>(i));
            if (!item) continue;
            const auto b = item->bonuses();
            result.strength += b.strength; result.dexterity += b.dexterity;
            result.intelligence += b.intelligence;
            result.maxHp += b.maxHp; result.maxMana += b.maxMana;
        }
        result.maxHp += result.maxHp * (talents().passiveValue(PassiveKind::IronSkin) + talents().passiveValue(PassiveKind::Wellspring)) / 100;
        result.maxMana += result.maxMana * (talents().passiveValue(PassiveKind::Devotion) + talents().passiveValue(PassiveKind::Wellspring)) / 100;
        if (const int balance = talents().passiveValue(PassiveKind::Balance)) {
            result.strength += balance; result.dexterity += balance; result.intelligence += balance;
        }
        result.hp = std::clamp(stats().hp, 0, result.maxHp);
        result.mana = std::clamp(stats().mana, 0, result.maxMana);
        return result;
    }
    void refreshEquipmentStats() { stats() = effectiveStats(); }
    // Deeper bases ask for Strength, Dexterity or Intelligence to wear.
    bool meetsRequirements(const ItemDefinition& d) const {
        return stats().strength >= d.reqStrength && stats().dexterity >= d.reqDexterity && stats().intelligence >= d.reqIntelligence;
    }
    bool equip(std::size_t index, std::optional<EquipmentSlot> target={}) {
        if (index < inventory().items().size() && inventory().items()[index]->definition() &&
            !meetsRequirements(*inventory().items()[index]->definition())) return false;
        if (!inventory().equip(index,target)) return false;
        refreshEquipmentStats(); return true;
    }
    bool unequip(EquipmentSlot slot) {
        if (!inventory().unequip(slot)) return false;
        refreshEquipmentStats(); return true;
    }

    bool bloodRelic=false, animationRelic=false;
    // Cloth's ward: a shield over your life. Transient: it refills on load.
    int ward=0, wardRest=0;
    // Ward from spells (Arcane Shroud): soaks hits before the gear's ward and
    // lasts until spent, up to your maximum mana.
    int spellWard=0;
    // Ascendancy (entities/Ascendancy.hpp): the chosen ascendancy's id
    // (empty until the first trial), unspent ascendancy points, and the
    // trials' sigils held and trials cleared (bit n-1 = trial n).
    std::string ascendancy;
    // What the player carries for light (0 none, 1 torch, 2 lantern) and
    // whether it is lit. A doused light hides you in the dark.
    int lightSource=1;
    bool lightLit=true;
    bool bloodMagicUnlocked=false; // offered blood at the Blood Altar
    int patron=0, favor=0;         // the god you are sworn to (Patron) and its favor
    int ascendancyPoints=0, trialKeys=0, trialsCleared=0;
    std::vector<int> deathlessSpentFloors;
    int& level() { return level_; }
    int level() const { return level_; }

    int& xp() { return xp_; }
    int xp() const { return xp_; }

    struct TreeAccess { std::string id; bool specialized=false; };
    std::vector<TreeAccess>& trees() { return trees_; }
    const std::vector<TreeAccess>& trees() const { return trees_; }
    int& treePoints() { return treePoints_; }
    int treePoints() const { return treePoints_; }
    int& abilityPoints() { return abilityPoints_; }
    int abilityPoints() const { return abilityPoints_; }
    // How many attribute points this character has earned (2 per
    // level, see PlayerLeveling.hpp) but not yet spent. Decremented as
    // each point is allocated to Strength/Dexterity/Intelligence via
    // the AttributeAllocation screen -- see
    // Application::offerAttributeAllocationIfPending().
    int& unspentAttributePoints() { return unspentAttributePoints_; }
    int unspentAttributePoints() const { return unspentAttributePoints_; }

private:
    Stats baseStats_;
    int level_ = 1;
    int xp_ = 0;
    std::vector<TreeAccess> trees_;
    int treePoints_ = 1, abilityPoints_ = 4;
    int unspentAttributePoints_ = 0;
};

} // namespace engine
