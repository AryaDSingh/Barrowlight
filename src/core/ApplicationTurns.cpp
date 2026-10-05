#include "core/Application.hpp"

#include "core/GameRules.hpp"
#include "entities/MonsterFactory.hpp"
#include "entities/StatusEffectLogic.hpp"

namespace engine {

namespace {
// Passive regen, applied once per player turn (see
// advanceTurnsUntilPlayerCanAct) regardless of what action was taken --
// without it, a fight that outlasts the player's starting mana pool
// leaves them with talents they can see are "off cooldown" but can never
// actually afford again. Modest on purpose: enough to matter over a
// multi-turn fight, not enough to make mana cost meaningless.
constexpr int kManaRegenPerTurn = 2;
} // namespace

void Application::processMonsterTurns() {
    while (window_.isOpen() && currentActor_ != &player_) {
        Actor* actor = currentActor_;

        const bool hadPoison = actor->statusEffects().has(StatusEffectType::Poison);
        const bool stunned = tickStatusEffects(*actor);
        if (hadPoison && actor->stats().hp > 0) {
            log(actor->name(), " takes poison damage (", actor->stats().hp, "/",
                actor->stats().maxHp, " hp left)");
        }
        checkAndHandleDeath(*actor);

        if (actor->stats().hp > 0) {
            if (stunned) {
                log(actor->name(), " is stunned and loses a turn!");
            } else if (actor->ai() != nullptr) {
                const AIDecision decision =
                    actor->ai()->decideAction(*actor, map_, player_, aliveAllies(actor));
                executeAIDecision(*actor, decision);
            }
            actor->talents().tickCooldowns();
        }

        currentActor_ = &scheduler_.nextTurn();
    }

    removeDeadMonsters();
}

void Application::advanceTurnsUntilPlayerCanAct() {
    // Safety bound, not expected to be hit given finite stun durations --
    // defensive against a genuine bug rather than an anticipated case.
    constexpr int kMaxStunSkips = 50;

    for (int i = 0; i < kMaxStunSkips && window_.isOpen(); ++i) {
        player_.stats().mana =
            std::min(player_.stats().mana + kManaRegenPerTurn, player_.stats().maxMana);

        const bool hadPoison = player_.statusEffects().has(StatusEffectType::Poison);
        const bool stunned = tickStatusEffects(player_);
        if (hadPoison && player_.stats().hp > 0) {
            log("Poison deals damage (", player_.stats().hp, "/", player_.stats().maxHp,
                " hp left)");
        }
        checkAndHandleDeath(player_);
        if (!window_.isOpen()) {
            return;
        }
        if (!stunned) {
            return; // genuinely the player's turn now
        }
        log("You are stunned and lose a turn!");
        currentActor_ = &scheduler_.nextTurn();
        processMonsterTurns();
    }
}

void Application::executeAIDecision(Actor& actor, const AIDecision& decision) {
    if (!decision.announcement.empty()) {
        log(decision.announcement);
    }

    switch (decision.type) {
        case AIActionType::Move:
            if (map_.isWalkable(decision.movePosition.x, decision.movePosition.y) &&
                !isOccupied(decision.movePosition, &actor)) {
                actor.setPosition(decision.movePosition);
            }
            break;

        case AIActionType::Attack:
        case AIActionType::UseAbility: {
            if (decision.target == nullptr) {
                break;
            }

            bool dodged = false;
            if (decision.attackPower > 0) {
                dodged = rollDodge(decision.target->stats().dexterity);
                if (dodged) {
                    log(decision.target->name(), " dodges ", actor.name(), "'s attack!");
                    soundManager_.play(SoundEffect::Dodge);
                } else {
                    int damage = decision.attackPower;
                    const int statValue = statValueForScalingStat(
                        decision.scalingStat, actor.stats().strength, actor.stats().dexterity,
                        actor.stats().intelligence);
                    // Filler tier (cooldown 0): MonsterAttackProfile-backed
                    // attacks have no cooldown concept at all, and "every
                    // eligible turn" is the closest existing tier to that
                    // -- see MonsterAttackProfile's own comment.
                    damage += abilityDamageBonus(decision.scalingStat, statValue, /*cooldownTurns=*/0);
                    if (actor.statusEffects().has(StatusEffectType::Empowered)) {
                        damage += actor.statusEffects().magnitudeOf(StatusEffectType::Empowered);
                    }
                    // Global crit (per design: every creature, player and
                    // monster alike, rolls it) -- multiplies the result of
                    // everything above.
                    bool crit = false;
                    if (rollCrit(actor.stats().dexterity)) {
                        crit = true;
                        damage = static_cast<int>(static_cast<float>(damage) * critDamageMultiplier());
                    }
                    decision.target->stats().hp -= damage;
                    log(actor.name(), crit ? " critically hits " : " hits ", decision.target->name(),
                        " for ", damage, " (", decision.target->stats().hp, "/",
                        decision.target->stats().maxHp, " hp left)");
                    soundManager_.play(SoundEffect::Hit);
                    checkAndHandleDeath(*decision.target);
                }
            }

            // A dodged hit lands no on-hit effect either -- avoiding the
            // blow avoids the poison that would have ridden in on it.
            if (!dodged && decision.effectToApply.has_value() && decision.target->stats().hp > 0) {
                decision.target->statusEffects().apply(*decision.effectToApply);
                log(decision.target->name(), " is affected by ",
                    (decision.effectToApply->type == StatusEffectType::Poison    ? "poison"
                     : decision.effectToApply->type == StatusEffectType::Stun ? "a stun"
                                                                               : "a buff"),
                    "!");
            }

            if (decision.type == AIActionType::UseAbility &&
                decision.abilityIndex < actor.talents().knownTalents().size()) {
                actor.talents().startCooldown(decision.abilityIndex);
            }
            break;
        }

        case AIActionType::SelfBuff:
            if (decision.effectToApply.has_value()) {
                actor.statusEffects().apply(*decision.effectToApply);
            }
            break;

        case AIActionType::Summon: {
            // The only action type that creates a new Monster rather
            // than acting on an existing one -- LichBehavior only
            // decided to do this (and picked movePosition), everything
            // about actually bringing the monster into existence
            // happens here, the same "AIBehavior decides, Application
            // executes" split every other action type already follows.
            // Safe to push into monsters_/scheduler_ mid-turn-
            // processing: monsters_ is vector<unique_ptr<Monster>>, so
            // the underlying Monster objects' addresses never move even
            // if the vector itself reallocates, and
            // processMonsterTurns()'s own loop only ever holds
            // currentActor_ (a single Actor*), never an iterator into
            // monsters_, so nothing here can invalidate what that loop
            // is holding onto.
            if (map_.isWalkable(decision.movePosition.x, decision.movePosition.y) &&
                !isOccupied(decision.movePosition, &actor)) {
                std::unique_ptr<Monster> summoned =
                    createMonster(decision.summonType, decision.movePosition, decision.summonTier);
                summoned->setRewardsEligible(false);
                scheduler_.add(*summoned);
                monsters_.push_back(std::move(summoned));
            }
            if (decision.abilityIndex < actor.talents().knownTalents().size()) {
                actor.talents().startCooldown(decision.abilityIndex);
            }
            break;
        }

        case AIActionType::Wait:
            break;
    }
}

void Application::checkAndHandleDeath(Actor& actor) {
    if (mode_ == GameMode::GameOver) {
        // Already handled -- without this, a dead player's hp stays <=
        // 0 indefinitely, and this function gets called again on every
        // subsequent status-effect tick (advanceTurnsUntilPlayerCanAct
        // calls it unconditionally each iteration), re-running the
        // entire death branch below repeatedly. Invisible before this
        // prompt, since window_.close() used to make window_.isOpen()
        // false immediately, short-circuiting those later calls before
        // they ever reached here -- now that death leads to a GameOver
        // screen instead of closing the window, that accidental
        // short-circuit is gone, so this needs to be explicit.
        return;
    }

    if (actor.stats().hp > 0) {
        return;
    }

    soundManager_.play(SoundEffect::Death);

    if (&actor == &player_) {
        log("You have died!");
        // As of Prompt 17: a real GameOver screen instead of closing
        // the window outright. selectClass() (reachable from the
        // ClassSelection screen this leads to) already does a complete
        // reset of player_/map_/monsters_/scheduler_, so nothing extra
        // needs cleaning up here -- transitioning mode_ is the whole fix.
        mode_ = GameMode::GameOver;
        wonGame_ = false;
        return;
    }

    auto* defeated = dynamic_cast<Monster*>(&actor);
    if (defeated && !defeated->claimDeath()) return;
    if (defeated) rewardMonster(*defeated, &actor == boss_);

    if (&actor == boss_) {
        boss_ = nullptr; // must clear before removeDeadMonsters() erases the underlying object
        scheduler_.remove(actor);
        grantXpAndAnnounce(actor.xpReward());
        if (currentFloor_ == kFinalFloor) {
            log(actor.name(), " falls! Victory is yours!");
            defeatedBossName_ = actor.name(); // renderGameOver() needs this after actor is gone
            // Deliberately NOT setting mode_ = GameOver here directly --
            // the grantXpAndAnnounce() call just above already ran the
            // full level-up sequence internally, and may have left
            // mode_ on AttributeAllocation or AbilityChoice if this
            // kill's XP crossed a level-up threshold with its own
            // pending choice. pendingFinalVictory_ just records that a
            // victory is waiting; resumeLevelUpSequence() itself (called
            // again by the AttributeAllocation/AbilityChoice key
            // handling once each pause resolves, or already reached its
            // own "nothing left pending" point just now if this kill
            // triggered no pause at all) is what actually makes the
            // GameOver transition, once every earned choice has
            // genuinely been offered.
            pendingFinalVictory_ = true;
            if (mode_ == GameMode::Playing) {
                // Nothing was left pending by the call above -- the
                // sequence already reached its own end without this
                // flag existing yet, so trigger the transition directly
                // rather than waiting for a key handler that will never
                // fire.
                mode_ = GameMode::GameOver;
                wonGame_ = true;
                pendingFinalVictory_ = false;
            }
        } else {
            // Any earlier boss floor (currently just kFirstBossFloor) --
            // defeating it doesn't end the run at all. The door
            // occupying this same position (see regenerateLevel()) is
            // now reachable, since the boss no longer blocks it; play
            // continues normally until the player chooses to step
            // through.
            log(actor.name(), " falls! The way forward opens.");
        }
        return;
    }

    log(actor.name(), " dies!");
    grantXpAndAnnounce(actor.xpReward());
    scheduler_.remove(actor);
    // Actual erase from monsters_ happens in removeDeadMonsters(), after
    // the current processMonsterTurns() loop finishes -- never mid-loop,
    // to avoid invalidating pointers still in use this turn.
}

void Application::removeDeadMonsters() {
    monsters_.erase(std::remove_if(monsters_.begin(), monsters_.end(),
                                    [](const std::unique_ptr<Monster>& m) {
                                        return m->stats().hp <= 0;
                                    }),
                     monsters_.end());
}

std::vector<Actor*> Application::aliveAllies(const Actor* exclude) {
    std::vector<Actor*> allies;
    for (auto& m : monsters_) {
        if (m.get() != exclude && m->stats().hp > 0) {
            allies.push_back(m.get());
        }
    }
    return allies;
}

} // namespace engine
