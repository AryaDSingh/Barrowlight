// Real application handlers with a hidden SFML window. Saves/screenshots stay
// in build/rewards-checks; the player's normal savegame.txt is never touched.
#include <filesystem>
#include <iostream>
#include <sstream>
#include "core/Application.hpp"
#include "entities/MonsterFactory.hpp"
#include "entities/PlayerClassFactory.hpp"
#include "entities/StatusEffectLogic.hpp"
#include "entities/TalentEffects.hpp"

namespace engine {
struct ApplicationRewardsTestAccess {
    static int run() {
        int failures = 0, checks = 0;
        auto check = [&](bool condition, const char* message) {
            ++checks; if (!condition) ++failures;
            std::cout << (condition ? "[ok] " : "[FAIL] ") << message << '\n';
        };
        Application app;
        app.window_.setVisible(false);
        app.window_.setFramerateLimit(0);
        const auto originalPath = std::filesystem::current_path();
        const auto output = originalPath / "build" / "rewards-checks";
        std::filesystem::create_directories(output);
        auto roundTrip = [&] {
            std::filesystem::current_path(output);
            app.saveGame(); app.loadGame();
            std::filesystem::current_path(originalPath);
        };
        auto snapshot = [&](const char* name) {
            app.render(); app.render();
            sf::Texture texture(app.window_.getSize()); texture.update(app.window_);
            check(texture.copyToImage().saveToFile(output / name), "Write reward UI snapshot");
        };
        auto setup = [&](PlayerClass cls) {
            app.cancelTargeting(); app.mousePixel_.reset(); app.talentPage_ = 0;
            app.mode_ = GameMode::Playing; app.playerClass_ = cls;
            app.currentFloor_ = 1; app.boss_ = nullptr;
            app.pendingFinalVictory_ = false; app.pendingHybridChoices_.clear();
            app.inventoryOpen_ = app.runesOpen_ = app.runeChoiceAvailable_ = false;
            app.chestExists_ = app.chestClaimed_ = false;
            app.runeTalentSelection_ = app.runeSelection_ = 0;
            app.groundItems_.clear(); app.ordinaryDrops_ = 0; app.nextItemId_ = 1;
            app.loot_.restore(12345);
            app.player_.inventory() = Inventory{};
            app.player_.level() = 1; app.player_.xp() = 0;
            app.player_.hybridSpecced() = false; app.player_.unspentAttributePoints() = 0;
            app.player_.setPosition({10,10});
            app.player_.baseStats() = statsForClass(cls);
            app.player_.baseStats().maxHp = app.player_.baseStats().hp = 100;
            app.player_.baseStats().maxMana = 100; app.player_.baseStats().mana = 80;
            app.player_.stats() = app.player_.baseStats();
            app.player_.talents() = talentSetForClass(cls);
            app.player_.statusEffects().active().clear();
            app.map_ = Map(32,22);
            for (int y=0; y<22; ++y) for (int x=0; x<32; ++x) {
                const bool edge = x==0 || y==0 || x==31 || y==21;
                app.map_.setTile(x,y,Tile{edge ? TileType::Wall : TileType::Floor,!edge,!edge});
            }
            app.monsters_.clear();
            app.exploredMap_ = ExploredMap(app.map_); app.updateFieldOfView();
            app.scheduler_ = TurnScheduler{}; app.scheduler_.add(app.player_);
            app.currentActor_ = &app.scheduler_.nextTurn(); app.updateCamera();
        };
        auto enemy = [&](Position position) -> Monster* {
            auto monster = createMonster(MonsterType::Goblin, position);
            monster->stats().hp = monster->stats().maxHp = 100;
            monster->stats().dexterity = 0;
            monster->setXpReward(0);
            auto* result = monster.get(); app.monsters_.push_back(std::move(monster));
            return result; // not scheduled: only explicit player/status turns advance in combat cases
        };
        auto itemRecord = [](const Item& item) {
            std::ostringstream out;
            out << item.definition()->id << ':' << item.instanceId() << ':' << item.rollTier();
            for (const auto& affix : item.affixes()) out << ':' << affix.id << '=' << affix.value;
            return out.str();
        };

        // Reproducibility, rarity and affix constraints over a representative roll stream.
        LootGenerator a(9876), b(9876);
        bool sameRolls = true, valid = true;
        bool rarities[3]{};
        for (int i=0; i<500; ++i) {
            auto first = a.generate(1+i%10, i%3, i+1, {});
            auto second = b.generate(1+i%10, i%3, i+1, {});
            sameRolls &= itemRecord(*first) == itemRecord(*second) && a.state() == b.state();
            rarities[static_cast<int>(first->rarity())] = true;
            unsigned int stats = 0;
            for (const auto& rolled : first->affixes()) {
                const auto* definition = findAffix(rolled.id);
                const auto mask = 1u << static_cast<unsigned>(definition->stat);
                valid &= !(stats & mask) && (definition->slots & (1u << static_cast<unsigned>(first->definition()->slot)));
                valid &= rolled.value >= definition->minimum + first->rollTier()*definition->perTier &&
                         rolled.value <= definition->maximum + first->rollTier()*definition->perTier;
                stats |= mask;
            }
        }
        check(sameRolls, "Same seed reproduces 500 complete item rolls and RNG states");
        check(valid && rarities[0] && rarities[1] && rarities[2], "All rarities occur with compatible, distinct, bounded affixes");

        setup(PlayerClass::Mage);
        app.chestExists_ = true; app.chestPosition_ = app.player_.position();
        app.player_.talents().setCooldownRemaining(1,4);
        app.player_.statusEffects().apply({StatusEffectType::Poison,3,1});
        app.pickupItem();
        check(app.chestClaimed_ && app.runeChoiceAvailable_ && app.player_.inventory().items().size()==1,
              "Opening first chest grants one item and a deferred rune choice");
        check(app.player_.inventory().items()[0]->rarity()!=ItemRarity::Normal &&
              app.player_.stats().hp==99 && app.player_.talents().cooldownRemaining(1)==3,
              "Chest guarantees magic or rare gear and advances exactly one player turn");
        const auto chestItem = itemRecord(*app.player_.inventory().items()[0]);
        const auto chestRng = app.loot_.state();
        app.pickupItem();
        check(app.loot_.state()==chestRng && app.player_.stats().hp==99 && app.player_.inventory().items().size()==1,
              "Claiming an opened chest again spends nothing and grants nothing");
        roundTrip();
        check(app.chestClaimed_ && app.runeChoiceAvailable_ && app.loot_.state()==chestRng &&
              itemRecord(*app.player_.inventory().items()[0])==chestItem && app.player_.stats().hp==99,
              "Real application save/load preserves claimed chest, pending choice, actual affixes and RNG");
        app.pickupItem();
        check(app.player_.inventory().items().size()==1 && app.loot_.state()==chestRng,
              "Loading cannot reopen the claimed chest");
        app.runesOpen_ = true; app.handleRuneKey(sf::Keyboard::Key::Num2);
        const auto choiceId = app.player_.talents().runes()[0].instanceId;
        app.handleRuneKey(sf::Keyboard::Key::Num2);
        check(!app.runeChoiceAvailable_ && app.player_.talents().runes().size()==1 && app.player_.stats().hp==99,
              "Deferred rune choice is free and can be collected only once");
        app.runeTalentSelection_ = 1; app.handleRuneKey(sf::Keyboard::Key::Enter);
        roundTrip();
        check(app.player_.talents().attachedRune(1) && app.player_.talents().attachedRune(1)->instanceId==choiceId &&
              app.player_.talents().effectiveTalent(1).areaRadius==3 && app.player_.talents().cooldownRemaining(1)==2,
              "Rune attachment and running cooldown survive application save/load by ID");
        app.openInventory(); app.inventorySelection_ = 3;
        snapshot("rolled-affixes.png");

        setup(PlayerClass::Warrior);
        auto* distant = enemy({12,10});
        check(!app.targetPreview(1,app.player_.position()).valid, "Base Cleave radius 1 cannot reach an enemy two tiles away");
        app.giveRune("widen"); app.runesOpen_ = true; app.runeTalentSelection_ = 1;
        app.player_.talents().setCooldownRemaining(1,4);
        app.handleRuneKey(sf::Keyboard::Key::Enter);
        check(app.player_.talents().knownTalents()[1].areaRadius==1 && app.player_.talents().effectiveTalent(1).areaRadius==2 &&
              app.player_.talents().effectiveTalent(1).manaCost==6 && app.player_.talents().cooldownRemaining(1)==3,
              "Widen changes Cleave radius 1 to 2, mana 4 to 6; attaching advances but never resets cooldown");
        const auto manaAfterAttach = app.player_.stats().mana;
        app.runesOpen_ = true; app.handleRuneKey(sf::Keyboard::Key::Enter);
        check(app.player_.stats().mana==manaAfterAttach && app.player_.talents().cooldownRemaining(1)==3,
              "Reattaching the same rune is a free no-op");
        app.runesOpen_ = true; snapshot("widen-already-attached.png");
        const auto rngBeforeUI = app.loot_.state();
        app.render(); app.targetPreview(1,app.player_.position());
        check(app.loot_.state()==rngBeforeUI, "Rendering and modified targeting do not draw loot RNG");
        app.runesOpen_ = false; app.player_.talents().resetCooldowns(); app.player_.stats().mana=80;
        check(app.tryUseTalent(1,app.player_.position()) && distant->stats().hp<100 && app.player_.stats().mana==76,
              "Actual Widen cast hits the expanded area: 80 - 6 cost + 2 normal regeneration = 76 mana");
        app.runesOpen_ = true; app.handleRuneKey(sf::Keyboard::Key::U);
        check(app.player_.talents().effectiveTalent(1).areaRadius==1 && !app.targetPreview(1,app.player_.position()).valid &&
              app.player_.talents().cooldownRemaining(1)==1, "Removing Widen restores base geometry and preserves elapsed cooldown");

        setup(PlayerClass::Warrior);
        app.giveRune("chain"); app.runesOpen_ = true;
        app.player_.talents().setCooldownRemaining(1,4);
        app.handleRuneKey(sf::Keyboard::Key::Enter);
        check(!app.player_.talents().attachedRune(0) && app.player_.talents().cooldownRemaining(1)==4 &&
              !app.runeFeedback_.empty(), "Chain on Slam is rejected with a reason and no turn cost");

        setup(PlayerClass::Mage);
        auto* primary = enemy({13,10}); auto* secondary = enemy({15,10});
        app.giveRune("chain"); check(app.player_.talents().attachRune(0,0), "Chain attaches to Arcane Bolt");
        const auto chain = app.targetPreview(0,primary->position());
        check(chain.affected.size()==2 && chain.chainedTarget==secondary && !chain.chainPath.empty(),
              "Chain preview selects one distinct secondary and exposes its path");
        auto effective = app.player_.talents().effectiveTalent(0); effective.damagePercent=50;
        const auto half = estimateTalentDamage(effective, app.player_, *secondary);
        check(app.tryUseTalent(0,primary->position()) &&
              (100-secondary->stats().hp==half.normal || 100-secondary->stats().hp==half.critical),
              "Actual Chain secondary hit uses half damage including scaling");
        app.map_.setTile(14,10,Tile{TileType::Wall,false,false}); app.updateFieldOfView();
        check(!app.targetPreview(0,primary->position()).chainedTarget, "A wall prevents Chain reaching the second target");
        app.map_.setTile(14,10,Tile{TileType::Floor,true,true});
        app.exploredMap_.update({{10,10},{11,10},{12,10},{13,10},{14,10}});
        check(!app.targetPreview(0,primary->position()).chainedTarget, "Chain cannot select an unseen secondary");

        setup(PlayerClass::Mage);
        auto* poisoned = enemy({13,10}); app.giveRune("venom"); app.player_.talents().attachRune(0,0);
        const auto venom = app.player_.talents().effectiveTalent(0);
        const auto hit = estimateTalentDamage(venom, app.player_, *poisoned);
        check(app.tryUseTalent(0,poisoned->position()) && poisoned->statusEffects().has(StatusEffectType::Poison) &&
              (100-poisoned->stats().hp==hit.normal || 100-poisoned->stats().hp==hit.critical),
              "Venom cast applies reduced direct damage and poison on a successful hit");
        const int beforePoison = poisoned->stats().hp;
        tickStatusEffects(*poisoned); tickStatusEffects(*poisoned); tickStatusEffects(*poisoned);
        check(poisoned->stats().hp==beforePoison-6 && !poisoned->statusEffects().has(StatusEffectType::Poison),
              "Venom ticks for exactly 2 damage on three enemy turns");
        check(!runeUnavailableReason(app.player_.talents().knownTalents()[3], "venom").empty(),
              "Venom rejects Mind Shatter's existing on-hit stun");

        setup(PlayerClass::Mage);
        app.grantXpAndAnnounce(20); // real level-up sequence; spend both points to resume native unlocks
        app.player_.unspentAttributePoints()=0; app.mode_=GameMode::Playing; app.resumeLevelUpSequence();
        const auto& learned = app.player_.talents().knownTalents();
        auto found = std::find_if(learned.begin(), learned.end(), [](const auto& t) { return t.id=="mage.blink"; });
        check(found!=learned.end(), "Mage actually learns Blink through the level-2 reward flow");
        const auto blinkIndex = static_cast<std::size_t>(found-learned.begin());
        app.giveRune("swift_passage"); app.player_.talents().attachRune(0,blinkIndex);
        const auto blink = app.player_.talents().effectiveTalent(blinkIndex);
        check(blink.moveDistance==5 && blink.cooldownTurns==6 && blink.scalingCooldown==4,
              "Swift Passage extends Blink while preserving its original scaling tier");
        check(app.tryUseTalent(blinkIndex,{15,10}) && app.player_.position().x==15 &&
              app.player_.talents().cooldownRemaining(blinkIndex)==5, "Swift Passage changes actual movement and cast cooldown");
        app.player_.talents().removeRune(blinkIndex);
        check(app.player_.talents().effectiveTalent(blinkIndex).moveDistance==3 &&
              app.player_.talents().cooldownRemaining(blinkIndex)==5, "Removing Swift restores base movement without resetting a running cooldown");

        setup(PlayerClass::Mage);
        AIDecision summon; summon.type=AIActionType::Summon; summon.summonType=MonsterType::Skeleton; summon.movePosition={13,10};
        auto* lich = enemy({12,10}); app.executeAIDecision(*lich,summon);
        auto* skeleton = app.monsters_.back().get();
        check(!skeleton->rewardsEligible() && skeleton->xpReward()==0, "Real summon action creates a reward-ineligible minion");
        roundTrip(); skeleton=app.monsters_.back().get();
        const auto beforeSummonDeath = app.loot_.state(); skeleton->stats().hp=0; app.checkAndHandleDeath(*skeleton);
        check(!skeleton->rewardsEligible() && app.player_.xp()==0 && app.loot_.state()==beforeSummonDeath && app.groundItems_.empty(),
              "Summon reward exclusion survives save/load and death grants no XP, loot or RNG draw");

        setup(PlayerClass::Mage);
        for (int i=0; i<30; ++i) { auto* monster=enemy({12,10}); monster->setTier(MonsterTier::Nightmare); app.rewardMonster(*monster,false); }
        check(app.ordinaryDrops_==2 && app.groundItems_.size()==2, "Ordinary rewards stop at the two-item floor cap");
        roundTrip(); const auto cappedRng=app.loot_.state(); app.rewardMonster(*app.monsters_[0],false);
        check(app.ordinaryDrops_==2 && app.groundItems_.size()==2 && app.loot_.state()==cappedRng, "Floor drop cap survives loading");
        auto* boss = app.monsters_[0].get(); app.boss_=boss; app.currentFloor_=5; boss->stats().hp=0;
        app.checkAndHandleDeath(*boss);
        const auto bossRng=app.loot_.state(); app.checkAndHandleDeath(*boss);
        check(app.player_.inventory().items().size()==2 && app.player_.inventory().items()[0]->rarity()==ItemRarity::Rare &&
              app.player_.inventory().items()[1]->rarity()==ItemRarity::Rare && app.player_.talents().runes().size()==1 && app.loot_.state()==bossRng,
              "Boss death grants two rares and one rune exactly once");

        app.window_.close();
        std::cout << checks << " reward checks, " << failures << " failures.\n";
        return failures ? 1 : 0;
    }
};
}
int main() { return engine::ApplicationRewardsTestAccess::run(); }
