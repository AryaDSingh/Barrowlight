#include "core/Application.hpp"

#include "core/GameRules.hpp"
#include "core/SaveGame.hpp"
#include "entities/HybridSpec.hpp"
#include "entities/MonsterFactory.hpp"

namespace engine {

void Application::saveGame() {
    SaveGameState state;
    state.map = map_;
    state.exploredMap = exploredMap_;
    state.playerPosition = player_.position();
    state.playerClass = playerClass_;
    state.playerLevel = player_.level();
    state.playerXp = player_.xp();
    state.currentFloor = currentFloor_;
    state.playerStats = player_.baseStats();
    state.playerStats.hp = player_.stats().hp;
    state.playerStats.mana = player_.stats().mana;
    state.unspentAttributePoints = player_.unspentAttributePoints();
    state.nextItemId = nextItemId_;
    state.lootRngState = loot_.state();
    state.chestPosition = chestPosition_; state.chestExists = chestExists_; state.chestClaimed = chestClaimed_;
    state.ordinaryDrops = ordinaryDrops_;
    const auto saveItem = [&](const Item& item, int location) {
        state.items.push_back({item.definition()->id, item.instanceId(), location,
                              location == -2 ? item.position() : Position{}, item.rollTier(), item.affixes()});
    };
    for (const auto& item : groundItems_) saveItem(*item, -2);
    for (const auto& item : player_.inventory().items()) saveItem(*item, -1);
    for (int slot = 0; slot < 3; ++slot) {
        if (const auto* item = player_.inventory().equipped(static_cast<EquipmentSlot>(slot)))
            saveItem(*item, slot);
    }
    state.lastMoveDirection = lastMoveDirection_;

    const auto& talents = player_.talents().knownTalents();
    for (std::size_t i = 0; i < talents.size(); ++i)
        state.playerTalents.push_back({talents[i].id, player_.talents().cooldownRemaining(i)});
    state.runes = player_.talents().runes();
    state.runeChoiceAvailable = runeChoiceAvailable_;
    state.playerStatusEffects = player_.statusEffects().active();
    state.playerHybridSpecced = player_.hybridSpecced();

    for (auto& m : monsters_) {
        SaveGameState::MonsterSaveData data;
        data.type = m->type();
        data.position = m->position();
        data.hp = m->stats().hp;
        data.maxHp = m->stats().maxHp;
        data.isBoss = (m.get() == boss_);
        data.tier = m->tier();
        data.rewardsEligible = m->rewardsEligible();
        data.statusEffects = m->statusEffects().active();
        const auto& known = m->talents().knownTalents();
        for (std::size_t i = 0; i < known.size(); ++i)
            data.talents.push_back({known[i].id, m->talents().cooldownRemaining(i)});
        state.monsters.push_back(std::move(data));
    }

    if (engine::saveGame(state, kSaveFilePath)) {
        log("Game saved.");
    } else {
        log("Failed to save game (could not write ", kSaveFilePath, ").");
    }
}

void Application::loadGame() {
    runesOpen_ = false;
    inventoryOpen_ = false;
    inventorySelection_ = 0;
    cancelTargeting();
    mousePixel_.reset();
    talentPage_ = 0;
    const std::optional<SaveGameState> loaded = engine::loadGame(kSaveFilePath);
    if (!loaded.has_value()) {
        log("No valid save file found (", kSaveFilePath, ").");
        return;
    }
    const SaveGameState& state = *loaded;
    auto catalog = fullKitForClass(state.playerClass);
    if (isHybridEligible(state.playerClass)) {
        const auto hybrid = fullKitForClass(hybridPoolClass(state.playerClass));
        catalog.insert(catalog.end(), hybrid.begin(), hybrid.end());
    }
    TalentSet restoredTalents;
    for (const auto& saved : state.playerTalents) {
        const auto found = std::find_if(catalog.begin(), catalog.end(), [&](const auto& talent) { return talent.id == saved.id; });
        if (found == catalog.end()) { log("Save contains an unknown player talent."); return; }
        restoredTalents.learnTalent(*found);
        restoredTalents.setCooldownRemaining(restoredTalents.knownTalents().size()-1, saved.cooldown);
    }
    if (restoredTalents.empty()) { log("Save contains no player talents."); return; }
    for (const auto& rune : state.runes) {
        if (!rune.talentId.empty()) {
            const auto& known = restoredTalents.knownTalents();
            const auto found = std::find_if(known.begin(), known.end(), [&](const auto& talent) { return talent.id == rune.talentId; });
            if (found == known.end() || !runeUnavailableReason(*found, rune.definitionId).empty()) {
                log("Save contains an incompatible rune attachment."); return;
            }
        }
    }
    restoredTalents.runes() = state.runes;
    std::vector<std::unique_ptr<Monster>> restoredMonsters;
    Monster* restoredBoss = nullptr;
    for (const auto& savedMonster : state.monsters) {
        auto monster = createMonster(savedMonster.type, savedMonster.position, savedMonster.tier);
        const auto& known = monster->talents().knownTalents();
        if (savedMonster.talents.size() != known.size()) { log("Save contains an incomplete enemy talent kit."); return; }
        for (const auto& saved : savedMonster.talents) {
            const auto found = std::find_if(known.begin(), known.end(), [&](const auto& talent) { return talent.id == saved.id; });
            if (found == known.end()) { log("Save contains an unknown enemy talent."); return; }
            monster->talents().setCooldownRemaining(static_cast<std::size_t>(found-known.begin()), saved.cooldown);
        }
        monster->setRewardsEligible(savedMonster.rewardsEligible);
        monster->stats().hp = savedMonster.hp;
        monster->stats().maxHp = savedMonster.maxHp;
        monster->statusEffects().active() = savedMonster.statusEffects;
        if (savedMonster.isBoss) {
            if (restoredBoss) { log("Save contains multiple bosses."); return; }
            restoredBoss = monster.get();
        }
        restoredMonsters.push_back(std::move(monster));
    }



    // Reachable from ClassSelection now (see processEvents()) -- a
    // load that started there needs to actually switch to Playing, the
    // same way selectClass() does after a real choice. Harmless if
    // we're already in Playing (regenerateLevel-style "replace
    // everything" below overwrites map_/monsters_/etc regardless of
    // which mode we were in before this).
    mode_ = GameMode::Playing;

    map_ = state.map;
    exploredMap_ = state.exploredMap;

    player_.setPosition(state.playerPosition);
    playerClass_ = state.playerClass;
    player_.talents() = std::move(restoredTalents);
    runeChoiceAvailable_ = state.runeChoiceAvailable;
    player_.level() = state.playerLevel;
    player_.xp() = state.playerXp;
    currentFloor_ = state.currentFloor;
    player_.inventory() = Inventory{};
    groundItems_.clear();
    nextItemId_ = state.nextItemId;
    loot_.restore(state.lootRngState);
    chestPosition_ = state.chestPosition; chestExists_ = state.chestExists; chestClaimed_ = state.chestClaimed;
    ordinaryDrops_ = state.ordinaryDrops;
    player_.baseStats() = state.playerStats;
    player_.stats() = state.playerStats;
    player_.unspentAttributePoints() = state.unspentAttributePoints;
    pendingFinalVictory_ = false;
    pendingHybridChoices_.clear();
    pendingLevelUpFromLevel_ = player_.level() + 1;
    // Validation in loadGame guarantees unique IDs, known definitions and slot compatibility.
    for (const auto& savedItem : state.items) {
        auto item = std::make_unique<Item>(*findItemDefinition(savedItem.definitionId),
                                          savedItem.instanceId, savedItem.position, savedItem.affixes, savedItem.rollTier);
        if (savedItem.location == -2) groundItems_.push_back(std::move(item));
        else {
            player_.inventory().add(std::move(item));
            if (savedItem.location >= 0)
                player_.inventory().equip(player_.inventory().items().size() - 1);
        }
    }
    player_.refreshEquipmentStats();
    lastMoveDirection_ = state.lastMoveDirection;

    player_.hybridSpecced() = state.playerHybridSpecced;
    player_.statusEffects().active() = state.playerStatusEffects;

    boss_ = restoredBoss;
    monsters_ = std::move(restoredMonsters);

    scheduler_ = TurnScheduler{};
    scheduler_.add(player_);
    for (auto& m : monsters_) {
        scheduler_.add(*m);
    }

    // Deliberately just this, unlike regenerateLevel() -- a freshly
    // generated player never has status effects yet, so running
    // processMonsterTurns()/advanceTurnsUntilPlayerCanAct() there is
    // harmless. A *loaded* player might already be poisoned or stunned;
    // doing the same here would immediately re-tick their status effects
    // before they've taken any action since resuming, applying an extra
    // tick beyond what was actually saved. Any monster whose turn is
    // technically still pending gets caught up naturally the moment the
    // player next moves or casts (tryMovePlayer/tryUseTalent already
    // call processMonsterTurns() themselves) -- the same mechanism that
    // handles it in ordinary play, not a special case for loading.
    currentActor_ = &scheduler_.nextTurn();

    if (player_.unspentAttributePoints() > 0) resumeLevelUpSequence();
    log("Game loaded: ", monsters_.size(), " monsters",
        (boss_ != nullptr ? " (boss present)" : ""), ", player at (", player_.position().x, ',',
        player_.position().y, "), ", player_.stats().hp, '/', player_.stats().maxHp, " hp.");
}

} // namespace engine
