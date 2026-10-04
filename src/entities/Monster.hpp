#pragma once

#include <memory>
#include <string>
#include <utility>

#include "entities/Actor.hpp"
#include "entities/EnemyTactics.hpp"
#include "entities/EnemyIntent.hpp"
#include "entities/MonsterTier.hpp"
#include "entities/MonsterType.hpp"

namespace engine {

// A non-player Actor. Always constructed with an AIBehavior -- there is
// no default here on purpose, unlike Player. What makes different monster
// types feel different is which Stats, which AIBehavior (parameterized
// with its own numbers), and -- as of Prompt 10 -- which talents get
// plugged in here, not a subclass per monster type. Most monster types
// don't need talents at all (a plain melee/ranged attack doesn't need
// cooldown tracking); Shaman and Bomber do, reusing the same TalentSet
// the player uses for exactly the same reason: it's already generic,
// nothing about it is player-specific.
//
// Remembers its own `type_` (Prompt 12) purely so save/load can
// reconstruct a matching Monster via MonsterFactory::createMonster() --
// nothing about normal gameplay reads it.
class Monster : public Actor {
public:
    Monster(MonsterType type, std::string name, char glyph, Position position, Stats stats,
            std::unique_ptr<AIBehavior> ai, TalentSet talents = TalentSet{})
        : Actor(std::move(name), glyph, position, stats, std::move(ai), std::move(talents)),
          type_(type) { lastObservedHp=stats.hp; tactics.home=position; tactics.lastKnown=position; tactics.concealed=enemyAmbusher(type); }

    int lastObservedHp=0; // transient damage observation; rebuilt on load
    EnemyTactics tactics;
    bool allied=false;
    int summonRank=1, summonIntelligence=0, remainingLife=0; // 0 permanent; positive temporary
    bool vaultGuard=false;
    // Champions of the rare deep-floor events (see Landmark.hpp's
    // championName); each drops a unique item when it falls. 0 = none.
    int eventChampion=0;
    // Out of sight, an alerted monster is glimpsed only on the turn it is
    // alerted (glimpseTurns), then only when you can actually see it.
    // Transient: not saved.
    bool wasAlerted=false;
    Roam roam=Roam::None;
    // Path of Exile events (ApplicationRifts.cpp); saved, format 38.
    int essence=0;         // Essence: 0 none, else its power
    bool corrupted=false;  // an essence monster that was corrupted
    int rift=0;            // 1 riftborn, 2 the Rift-keeper
    int glimpseTurns=0;
    // Boss tricks with light and surfaces (Application::bossSurfaceAction).
    // Transient: a reload simply restarts the count.
    int bossTimer=0;
    bool flooded=false;
    int recoveryActions=0;
    int summonsCommitted=0;
    MonsterType type() const { return type_; }
    std::optional<EnemyIntent>& intent() { return intent_; }
    const std::optional<EnemyIntent>& intent() const { return intent_; }

    // Which Elite/Nightmare tier this monster was created at (Prompt
    // 22) -- Base by default, set explicitly by MonsterFactory
    // ::createMonster() via setTier(), the same "constructor stays
    // simple, a setter attaches the extra field afterward" pattern
    // Actor::setXpReward() already established. Not a constructor
    // parameter specifically so every existing test that constructs a
    // Monster directly (there are many, across nearly every test file)
    // keeps compiling completely unchanged. Purely for rendering (the
    // Elite/Nightmare border, see Application) -- gameplay-affecting
    // numbers (hp, damage) are already baked into stats()/the
    // AIBehavior at creation time and don't need this to be read back.
    MonsterTier tier() const { return tier_; }
    void setTier(MonsterTier tier) { tier_ = tier; }
    bool rewardsEligible() const { return rewardsEligible_; }
    void setRewardsEligible(bool value) { rewardsEligible_ = value; if (!value) setXpReward(0); }
    bool claimDeath() { if (deathClaimed_) return false; deathClaimed_ = true; return true; }

private:
    MonsterType type_;
    std::optional<EnemyIntent> intent_;
    MonsterTier tier_ = MonsterTier::Base;
    bool rewardsEligible_ = true;
    bool deathClaimed_ = false;
};

} // namespace engine
