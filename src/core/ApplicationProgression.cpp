#include "core/Application.hpp"

#include "entities/HybridSpec.hpp"
#include "entities/PlayerClassFactory.hpp"
#include "entities/PlayerLeveling.hpp"

namespace engine {

void Application::grantXpAndAnnounce(int amount) {
    if (amount <= 0) return; // summons must not disturb an already pending level-up sequence
    // Captures level before/after specifically to detect a level-up
    // and announce it -- grantXp() itself is a plain void function
    // (data + logic, no logging; see PlayerLeveling.hpp), so this is
    // the one place XP gain actually becomes visible to the player. A
    // single large grant (the boss's 200 XP, well above any individual
    // level's threshold) can cross more than one level at once --
    // grantXp() loops internally to handle that, and this still only
    // prints one summary line, not one per level crossed.
    const int levelBefore = player_.level();
    grantXp(player_, amount);
    log("Gained ", amount, " XP.");
    if (player_.level() > levelBefore) {
        log("Level up! You are now level ", player_.level(), ".");
        soundManager_.play(SoundEffect::LevelUp);
    }

    pendingLevelUpFromLevel_ = levelBefore + 1;
    resumeLevelUpSequence();
}

void Application::resumeLevelUpSequence() {
    // Attribute allocation always resolves first, before any talent
    // unlocks or hybrid choices for the levels just gained -- a
    // deliberate ordering choice (see ARCHITECTURE_DECISIONS.md), not
    // an arbitrary one: it keeps every pause for a single XP grant in
    // one predictable sequence rather than interleaving two different
    // kinds of choice level-by-level.
    if (player_.unspentAttributePoints() > 0) {
        mode_ = GameMode::AttributeAllocation;
        return; // paused; the AttributeAllocation key handling calls this again once resolved
    }
    processLevelUpEffects(pendingLevelUpFromLevel_);

    // The sequence has now genuinely reached its own end -- either
    // nothing was ever pending, or every attribute point and hybrid
    // choice this XP grant produced has actually been offered and
    // resolved. Only now is it safe to make the deferred final-boss
    // victory transition (see checkAndHandleDeath) -- doing it any
    // earlier risked silently skipping a choice the player had genuinely
    // earned.
    if (mode_ == GameMode::Playing && pendingFinalVictory_) {
        pendingFinalVictory_ = false;
        mode_ = GameMode::GameOver;
        wonGame_ = true;
    }
}

void Application::processLevelUpEffects(int fromLevel) {
    // Prompt 23/24: checks every level actually crossed, not just the
    // final level reached -- a multi-level jump (the boss's XP against
    // an early character) must not skip a talent unlock or a hybrid
    // choice sitting at an intermediate level just because the grant
    // blew past it in one step.
    for (int level = fromLevel; level <= player_.level(); ++level) {
        if (const std::optional<Talent> unlocked = talentUnlockedAtLevel(playerClass_, level);
            unlocked.has_value()) {
            player_.talents().learnTalent(*unlocked);
            log("New talent unlocked: ", unlocked->name, "!");
        }

        offerHybridChoiceIfEligible(level);
        if (mode_ == GameMode::AbilityChoice) {
            // Paused for a real decision -- stop here. pendingLevelUpFromLevel_
            // is updated so resumeLevelUpSequence() (called by the
            // AbilityChoice key handling once the person responds)
            // continues from the *next* level, not from the start of
            // this call -- any further level in this same grant still
            // gets checked rather than silently skipped.
            pendingLevelUpFromLevel_ = level + 1;
            return;
        }
    }
}

void Application::offerHybridChoiceIfEligible(int level) {
    if (!isHybridEligible(playerClass_)) {
        return; // Thief/Spellblade -- no hybrid path at all, see HybridSpec.hpp
    }

    if (level == 5 && !player_.hybridSpecced()) {
        // The one-time spec-in decision -- level 5 is crossed exactly
        // once per character (levels only ever go up), so there's no
        // need for a separate "already declined" flag: if they decline
        // here, hybridSpecced() stays false and level 5 simply never
        // comes around again for this character.
        pendingHybridChoices_ = availableHybridPicks(playerClass_, player_.talents());
        pendingHybridChoiceIsSpecIn_ = true;
        pendingHybridChoiceLevel_ = level;
        mode_ = GameMode::AbilityChoice;
        return;
    }

    if (level >= 6 && level <= 10 && player_.hybridSpecced()) {
        const std::vector<Talent> available = availableHybridPicks(playerClass_, player_.talents());
        if (available.empty()) {
            return; // the whole opposing kit has already been picked -- nothing left to offer
        }
        pendingHybridChoices_ = available;
        pendingHybridChoiceIsSpecIn_ = false;
        pendingHybridChoiceLevel_ = level;
        mode_ = GameMode::AbilityChoice;
    }
}

} // namespace engine
