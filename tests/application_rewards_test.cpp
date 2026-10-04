// Real application handlers with a hidden SFML window. Saves/screenshots stay
// in build/rewards-checks; the player's normal savegame.txt is never touched.
#include <filesystem>
#include <iostream>
#include <sstream>
#include <fstream>
#include <thread>
#include <chrono>
#include <set>
#include "entities/TalentProgression.hpp"
#include "entities/PlayerLeveling.hpp"
#include "entities/Ascendancy.hpp"
#include "entities/RunProgression.hpp"
#include "world/EncounterPlan.hpp"
#include "world/LineOfFire.hpp"
#include "world/Pathfinder.hpp"
#include "core/Application.hpp"
#include "core/ScreenLayout.hpp"
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
            app.saveGame();
            const auto saved=loadGame("savegame.txt");
            check(saved.has_value(),"Round-trip file actually parses successfully");
            app.autoExploring_=true; // restoration must actually run, not leave old state untouched
            check(saved && app.restoreState(*saved),"Round-trip actually restores the application state");
            check(!app.autoExploring_,"Loading cancels transient automation");
            std::filesystem::current_path(originalPath);
        };
        auto snapshot = [&](const char* name) {
            app.render(); app.render();
            sf::Texture texture(app.window_.getSize()); texture.update(app.window_);
            check(texture.copyToImage().saveToFile(output / name), "Write reward UI snapshot");
        };
        auto setup = [&](PlayerClass cls) {
            app.cancelTargeting(); app.mousePixel_.reset(); app.talentPage_ = 0;
            app.inventoryDragSource_.reset();
            app.dungeonMenu_=false;
            app.mode_ = GameMode::Playing; app.playerClass_ = cls;
            app.darknessEnabled_ = false; app.player_.lightSource = 1; app.player_.lightLit = true; // darkness has its own checks
            app.autoExploring_=false; app.restTurns_=0; app.quietTurns_=0; app.combatThisTurn_=false;
            app.vaultExists_=app.vaultOpened_=app.vaultClaimed_=false; app.vaultMenu_=0; app.exitMenu_=false; app.vaultRewards_.clear();
            app.landmark_=LandmarkKind::None; app.landmarkUsed_=false; app.shrineMenu_=false;
            app.player_.patron=app.player_.favor=0; app.player_.bloodMagicUnlocked=false; app.pendingFall_=false;
            app.player_.ascendancy.clear(); app.player_.ascendancyPoints=app.player_.trialKeys=app.player_.trialsCleared=0;
            app.lightOrbs_.clear();
            app.trial_=app.trialReturnFloor_=0; app.ascendancyMenu_=app.trialMenu_=false;
            app.setProps({});
            app.floorCache_.clear(); app.floorEntrance_={1,1}; app.floorExit_={30,20};
            app.currentFloor_ = 1; app.boss_ = nullptr;
            app.pendingFinalVictory_ = false;
            app.inventoryOpen_ = false; app.progressionReviewPending_=false;
            app.chestExists_ = app.chestClaimed_ = false;
            app.groundItems_.clear(); app.ordinaryDrops_ = 0; app.nextItemId_ = 1;
            app.loot_.restore(12345);
            app.player_.inventory() = Inventory{};
            app.player_.level() = 1; app.player_.xp() = 0;
            app.player_.unspentAttributePoints() = 0;
            app.player_.setPosition({10,10});
            app.player_.baseStats() = statsForClass(cls);
            app.player_.baseStats().maxHp = app.player_.baseStats().hp = 100;
            app.player_.baseStats().maxMana = 100; app.player_.baseStats().mana = 80;
            app.player_.stats() = app.player_.baseStats();
            const auto tree=cls==PlayerClass::Mage ? "arcane" : "one_handed";
            const auto id=cls==PlayerClass::Mage ? "arcane.bolt" : "one_handed.quick_strike";
            app.player_.trees()={{tree,false}}; app.player_.treePoints()=0; app.player_.abilityPoints()=earnedAbilityPoints(1)-3;
            app.player_.talents()=TalentSet({basicAttack(),findTalentDefinition(id)->ranks[0],basicCleanse()});
            app.player_.talents().setRank(1,3);
            app.player_.statusEffects().active().clear();
            app.map_ = Map(32,22);
            for (int y=0; y<22; ++y) for (int x=0; x<32; ++x) {
                const bool edge = x==0 || y==0 || x==31 || y==21;
                app.map_.setTile(x,y,Tile{edge ? TileType::Wall : TileType::Floor,!edge,!edge});
            }
            app.clearSurfaces();
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
        check(app.chestClaimed_ && app.player_.inventory().items().size()==1,
              "Opening first chest grants one equipment item");
        check(app.player_.inventory().items()[0]->rarity()!=ItemRarity::Normal &&
              app.player_.stats().hp==99 && app.player_.talents().cooldownRemaining(1)==3,
              "Chest guarantees magic or rare gear and advances exactly one player turn");
        const auto chestItem = itemRecord(*app.player_.inventory().items()[0]);
        const auto chestRng = app.loot_.state();
        app.pickupItem();
        check(app.loot_.state()==chestRng && app.player_.stats().hp==99 && app.player_.inventory().items().size()==1,
              "Claiming an opened chest again spends nothing and grants nothing");
        roundTrip();
        check(app.chestClaimed_ && app.loot_.state()==chestRng &&
              itemRecord(*app.player_.inventory().items()[0])==chestItem && app.player_.stats().hp==99,
              "Real application save/load preserves claimed chest, actual affixes and RNG");
        app.pickupItem();
        check(app.player_.inventory().items().size()==1 && app.loot_.state()==chestRng,
              "Loading cannot reopen the claimed chest");
        app.openInventory(); app.inventorySelection_=kEquipmentSlotCount;
        snapshot("rolled-affixes.png");

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
        auto* ordinary=enemy({12,10});
        for (int i=0; i<30; ++i) app.rewardMonster(*ordinary,false);
        check(app.ordinaryDrops_==2 && app.groundItems_.size()==2, "Ordinary rewards stop at the two-item floor cap");
        roundTrip(); const auto cappedRng=app.loot_.state(); app.rewardMonster(*app.monsters_[0],false);
        check(app.ordinaryDrops_==2 && app.groundItems_.size()==2 && app.loot_.state()==cappedRng, "Floor drop cap survives loading");
        auto* boss = app.monsters_[0].get(); app.boss_=boss; app.currentFloor_=5; boss->stats().hp=0;
        app.checkAndHandleDeath(*boss);
        const auto bossRng=app.loot_.state(); app.checkAndHandleDeath(*boss);
        check(app.player_.inventory().items().size()==2 && app.player_.inventory().items()[0]->rarity()==ItemRarity::Rare &&
              app.player_.inventory().items()[1]->rarity()==ItemRarity::Rare && app.loot_.state()==bossRng,
              "Boss death grants two rare items exactly once");



        // Current combat regression scenarios through the real application handlers.
        setup(PlayerClass::Mage);
        app.ordinaryDrops_=2;
        for (auto tier:{MonsterTier::Elite,MonsterTier::Nightmare}) {
            auto m=createMonster(MonsterType::Goblin,{12,10},tier);
            const auto count=app.groundItems_.size(); app.rewardMonster(*m,false);
            check(app.groundItems_.size()==count+1 && app.ordinaryDrops_==2 &&
                static_cast<int>(app.groundItems_.back()->rarity())>=(tier==MonsterTier::Elite?1:2),
                "Elite/rare guaranteed quality bypasses the ordinary drop cap");
        }
        auto unique=createMonster(MonsterType::OssuaryWarden,{12,10});
        app.rewardMonster(*unique,false);
        check(app.groundItems_.size()==3 && app.groundItems_.back()->rarity()==ItemRarity::Rare,
            "Named enemy guarantees rare gear after ordinary cap");

        setup(PlayerClass::Mage);
        auto lichBoss=createMonster(MonsterType::Lich,{14,10});
        auto hex=lichBoss->ai()->decideAction(*lichBoss,app.map_,app.player_,{});
        check(hex.type==AIActionType::UseAbility && hex.effectToApply &&
            hex.effectToApply->type==StatusEffectType::ManaDrain,"Healthy real Lich chooses Mana Drain");
        app.executeAIDecision(*lichBoss,hex);
        check(app.player_.statusEffects().has(StatusEffectType::ManaDrain) &&
            lichBoss->talents().cooldownRemaining(1)==10,"Hex applies and reserves its saved cooldown");
        const int mana=app.player_.stats().mana;
        tickStatusEffects(app.player_);
        check(app.player_.stats().mana==mana-2,"Mana Drain removes exactly two mana per status tick");
        applyTalentSelfBuff(basicCleanse(),app.player_);
        check(!app.player_.statusEffects().has(StatusEffectType::ManaDrain),"Cleanse removes Mana Drain");
        lichBoss->stats().hp=80; lichBoss->talents().resetCooldowns();
        hex=lichBoss->ai()->decideAction(*lichBoss,app.map_,app.player_,{});
        check(hex.effectToApply && hex.effectToApply->type==StatusEffectType::Doom,"Wounded real Lich chooses Doom");
        app.executeAIDecision(*lichBoss,hex);
        const int doomHp=app.player_.stats().hp;
        for (int i=0;i<4;++i) tickStatusEffects(app.player_);
        check(app.player_.stats().hp==doomHp && app.player_.statusEffects().has(StatusEffectType::Doom),
            "Doom does not deal early damage during the four safe ticks");
        tickStatusEffects(app.player_);
        check(app.player_.stats().hp==doomHp-14 && !app.player_.statusEffects().has(StatusEffectType::Doom),
            "Doom detonates once for the displayed 14 HP at expiry");
        app.player_.statusEffects().apply({StatusEffectType::Doom,1,14});
        check(app.tryUseTalent(2,app.player_.position()) && app.player_.stats().hp==doomHp-14 &&
            !app.player_.statusEffects().has(StatusEffectType::Doom),"Real Cleanse cast beats Doom on its last action");
        lichBoss->talents().resetCooldowns(); app.map_.setTile(12,10,{TileType::Wall,false,false});
        hex=lichBoss->ai()->decideAction(*lichBoss,app.map_,app.player_,{});
        check(!hex.effectToApply,"Lich cannot curse through a blocking wall");

        setup(PlayerClass::Mage);
        app.player_.statusEffects().apply({StatusEffectType::Doom,4,14});
        app.startAutoExplore(); app.startRest(); app.returnToTown();
        check(!app.autoExploring_ && app.restTurns_==0 && app.mode_==GameMode::Playing,
            "Doom prevents auto-explore, rest and Waystone travel");
        snapshot("curse-countdown.png");
        auto realLich=createMonster(MonsterType::Lich,{14,10});
        realLich->talents().setCooldownRemaining(1,7);
        realLich->summonsCommitted=2;
        realLich->intent()=EnemyIntent{{14,10},{14,11},0,1,0,IntentKind::Summon};
        app.monsters_.push_back(std::move(realLich));
        roundTrip();
        check(app.player_.statusEffects().magnitudeOf(StatusEffectType::Doom)==14 &&
            app.monsters_[0]->talents().cooldownRemaining(1)==7 && app.monsters_[0]->summonsCommitted==2 &&
            app.monsters_[0]->intent() && app.monsters_[0]->intent()->playerActionsRemaining==1,
            "Save round-trip preserves curse, Hex cooldown and committed ritual");
        // Version 16 shares the record layout, but has no Hex talent or curses.
        auto legacy=app.captureState(false); legacy.playerStatusEffects.clear();
        legacy.monsters[0].talents.pop_back();
        const auto legacyPath=output/"legacy16.txt";
        check(saveGameAsVersion(legacy,legacyPath.string(),16),"Write representative pre-Hex save fixture");
        const auto migrated=loadGame(legacyPath.string());
        check(migrated && app.restoreState(*migrated) && app.monsters_[0]->talents().cooldownRemaining(1)==10,
            "Version-16 Lich migrates with a ten-turn Hex grace period");

        // Process a ready ritual with real scheduling, then check saved attempt order.
        for (int attempt=1;attempt<=3;++attempt) {
            setup(PlayerClass::Mage);
            auto boss=createMonster(MonsterType::Lich,{14,10}); auto* caster=boss.get();
            boss->summonsCommitted=attempt;
            boss->intent()=EnemyIntent{{14,10},{14,11},0,0,0,IntentKind::Summon};
            app.scheduler_.add(*boss); app.monsters_.push_back(std::move(boss));
            app.currentActor_=caster; app.processMonsterTurns();
            const auto expected=attempt==2?MonsterType::SkeletonArcher:MonsterType::SkeletonGuard;
            check(app.monsters_.size()==2 && app.monsters_.back()->type()==expected &&
                !app.monsters_.back()->rewardsEligible() && app.monsters_.back()->xpReward()==0,
                "Committed Lich ritual creates the correct reward-free reinforcement");
            check(caster->summonsCommitted==attempt,"Completing ritual does not spend a second attempt");
        }
        setup(PlayerClass::Mage);
        auto exhausted=createMonster(MonsterType::Lich,{14,10}); exhausted->summonsCommitted=3;
        exhausted->talents().setCooldownRemaining(1,10);
        check(exhausted->ai()->decideAction(*exhausted,app.map_,app.player_,{}).type==AIActionType::Attack,
            "Three committed rituals exhaust the real Lich's summon budget");

        setup(PlayerClass::Mage);
        auto bomber=createMonster(MonsterType::Bomber,{15,10}); auto* caster=bomber.get();
        bomber->intent()=EnemyIntent{{15,10},{12,10},2,0,100,IntentKind::MagicStrike};
        auto* victim=enemy({12,10}); victim->setRewardsEligible(false);
        app.player_.setPosition({7,10});
        app.scheduler_.add(*bomber); app.monsters_.push_back(std::move(bomber));
        app.currentActor_=caster; app.processMonsterTurns();
        check(app.monsters_.size()==1 && caster->stats().hp>0,"Committed enemy blast kills another enemy but not its caster");

        // Rank-up preserves an explicitly arranged hotbar including empty slots.
        setup(PlayerClass::Mage);
        app.player_.talents().setRank(1,1); app.player_.abilityPoints()=earnedAbilityPoints(1)-1;
        app.player_.talents().hotbar()={"basic.attack","","basic.cleanse","","","","arcane.bolt"};
        const auto bar=app.player_.talents().hotbar();
        check(purchaseAbility(app.player_,*findTalentDefinition("arcane.bolt")) &&
            purchaseAbility(app.player_,*findTalentDefinition("arcane.bolt")) && app.player_.talents().hotbar()==bar,
            "Ranks 2 and 3 preserve all hotbar positions and empty slots");

        setup(PlayerClass::Mage);
        auto stepExplore=[&] { std::this_thread::sleep_for(std::chrono::milliseconds(105)); app.stepAutoExplore(); };
        app.startAutoExplore(); const auto pos=app.player_.position();
        stepExplore();
        check(app.player_.position().x!=pos.x || app.player_.position().y!=pos.y,"Auto-explore spends a real movement action");
        auto* visibleEnemy=enemy({app.player_.position().x+1,app.player_.position().y});
        app.updateFieldOfView(); const int enemyHp=visibleEnemy->stats().hp;
        app.stepAutoExplore();
        check(!app.autoExploring_ && visibleEnemy->stats().hp==enemyHp,"Visible enemy stops automation without a bump attack");
        setup(PlayerClass::Mage); app.startAutoExplore();
        app.groundItems_.push_back(std::make_unique<Item>(kItemDefinitions[0],app.nextItemId_++,Position{11,10}));
        app.stepAutoExplore();
        check(!app.autoExploring_ && app.groundItems_.size()==1,"New visible loot stops exploration without pickup");
        app.startAutoExplore(); check(app.autoExploring_,"Explicit Z restart acknowledges visible loot");
        snapshot("auto-explore.png"); app.stopAutoExplore("test cleanup");
        setup(PlayerClass::Mage);
        std::vector<Position> all;
        for(int y=0;y<app.map_.height();++y) for(int x=0;x<app.map_.width();++x) all.push_back({x,y});
        app.exploredMap_.update(all); app.startAutoExplore();
        check(!app.autoExploring_,"Fully explored floor completes without extra turns");
        setup(PlayerClass::Mage); app.startAutoExplore();
        app.monsters_.push_back(createMonster(MonsterType::Bomber,{29,19}));
        app.monsters_.back()->intent()=EnemyIntent{{29,19},{28,19},2,3,7,IntentKind::MagicStrike};
        app.stepAutoExplore(); check(!app.autoExploring_,"Attack warning interrupts auto-explore even offscreen");


        // Explore a whole empty room, restarting only after deliberate discovery stops.
        setup(PlayerClass::Mage);
        app.player_.setPosition({2,2}); app.updateFieldOfView();
        int exploreMoves=0,restarts=0;
        app.startAutoExplore();
        while(exploreMoves<350) {
            if(!app.autoExploring_) { app.startAutoExplore(); ++restarts; if(!app.autoExploring_) break; }
            const auto before=app.player_.position(); stepExplore();
            if(before.x==app.player_.position().x && before.y==app.player_.position().y && !app.autoExploring_) break;
            ++exploreMoves;

        }
        bool fullyExplored=true;
        for(int y=1;y<21;++y) for(int x=1;x<31;++x)
            fullyExplored &= app.exploredMap_.at(x,y)!=Visibility::Hidden;
        check(fullyExplored && exploreMoves<350,"Auto-explore traverses an entire open floor and terminates without looping");

        // Darkness: you see lit tiles and your neighbours; your torch is a light.
        setup(PlayerClass::Warrior); app.darknessEnabled_=true;
        {
            auto* far=enemy({16,10}); auto* near=enemy({13,10});
            app.updateFieldOfView();
            check(app.exploredMap_.at(13,10)==Visibility::Visible && app.exploredMap_.at(16,10)!=Visibility::Visible,
                  "A torch shows four tiles; beyond it the dark hides enemies");
            snapshot("ui-darkness-torch.png");
            app.toggleLight();
            check(!app.player_.lightLit && app.exploredMap_.at(13,10)!=Visibility::Visible && app.exploredMap_.at(11,10)==Visibility::Visible,
                  "Doused, you only see what is beside you");
            snapshot("ui-darkness-doused.png");
            app.monsters_.clear();
            auto archer=createMonster(MonsterType::Archer,{13,10}); auto* human=archer.get(); app.monsters_.push_back(std::move(archer));
            auto goblin=createMonster(MonsterType::Goblin,{10,13}); auto* goblinPtr=goblin.get(); app.monsters_.push_back(std::move(goblin));
            app.updateFieldOfView();
            check(app.nearestOpponent(*human,false)==nullptr,"Humans can't see a player hiding in the dark");
            check(app.nearestOpponent(*goblinPtr,false)==&app.player_,"Goblins see in the dark");
            app.toggleLight();
            check(app.nearestOpponent(*human,false)==&app.player_,"A lit torch gives you away");
            app.toggleLight(); roundTrip();
            check(!app.player_.lightLit && app.player_.lightSource==1,"Save/load keeps the doused torch");
            app.player_.lightLit=true;
            app.currentFloor_=2; app.regenerateLevel(77); app.updateFieldOfView(); app.updateCamera();
            snapshot("ui-darkness-floor.png");
            app.toggleLight(); snapshot("ui-darkness-floor-doused.png"); app.toggleLight();
            {
                int puddles=0,braziers=0,oil=0;
                for (int y=0;y<app.map_.height();++y) for (int x=0;x<app.map_.width();++x) {
                    puddles+=app.surfaceAt({x,y})==SurfaceType::Water; oil+=app.surfaceAt({x,y})==SurfaceType::Oil;
                }
                for (const auto& prop:app.props_) braziers+=prop.kind==PropKind::Brazier || prop.kind==PropKind::ColdBrazier;
                check(puddles>0 && oil>0 && braziers>0,"New floors get puddles, oil slicks and braziers");
                std::vector<Position> all; for(int y=0;y<app.map_.height();++y) for(int x=0;x<app.map_.width();++x) all.push_back({x,y});
                app.darknessEnabled_=false; app.exploredMap_.update(all);
                for (const auto& prop:app.props_) if (prop.kind==PropKind::Brazier) { app.player_.setPosition({prop.pos.x,prop.pos.y+1}); break; }
                app.updateCamera(); snapshot("ui-surfaces-floor.png"); app.darknessEnabled_=true;
            }
        }
        // Surfaces: the elements meet oil, water, ice and fire.
        setup(PlayerClass::Mage);
        {
            app.clearSurfaces();
            for (int x=13;x<=16;++x) app.setSurface({x,10},SurfaceType::Oil,0);
            app.player_.talents().learnTalent(findTalentDefinition("fire.ember_bolt")->ranks[0]);
            std::size_t ember=0;
            for (std::size_t i=0;i<app.player_.talents().knownTalents().size();++i) if (app.player_.talents().knownTalents()[i].id=="fire.ember_bolt") ember=i;
            app.player_.stats().mana=80;
            check(app.tryUseTalent(ember,{13,10}) && app.surfaceAt({13,10})==SurfaceType::Fire,"An Ember Bolt ignites the oil it lands in");
            check(app.surfaceAt({14,10})==SurfaceType::Fire,"Fire spreads along the oil each turn");
            auto* walker=enemy({15,12});
            app.setSurface({15,11},SurfaceType::Fire,4);
            AIDecision step; step.type=AIActionType::Move; step.movePosition={15,11};
            app.executeAIDecision(*walker,step,0);
            check(!(walker->position().x==15 && walker->position().y==11),"Monsters won't step into fire on purpose");
            app.setSurface({11,10},SurfaceType::Fire,4); app.tickSurfaces();
            app.player_.setPosition({11,10}); app.tickSurfaces();
            check(app.player_.statusEffects().has(StatusEffectType::Burn),"Standing in flames sets you burning");
            app.player_.setPosition({10,10}); app.player_.statusEffects().active().clear();

            for (int x=4;x<=8;++x) app.setSurface({x,15},SurfaceType::Water,0);
            auto* wader=enemy({8,15}); const int waderHp=wader->stats().hp;
            app.applyElement(Element::Lightning,{{4,15}});
            check(app.surfaceAt({8,15})==SurfaceType::Electrified && wader->stats().hp<waderHp && wader->statusEffects().has(StatusEffectType::Shock),
                  "Lightning runs through the whole pool and shocks whoever stands in it");
            snapshot("ui-surfaces.png");
            app.applyElement(Element::Ice,{{5,15}});
            check(app.surfaceAt({5,15})==SurfaceType::Ice,"Cold freezes water");
            app.applyElement(Element::Fire,{{5,15}});
            check(app.surfaceAt({5,15})==SurfaceType::Water,"Fire melts ice back to water");
            app.applyElement(Element::Ice,{{13,10}});
            check(app.surfaceAt({13,10})==SurfaceType::None,"Cold puts out burning ground");

            // Braziers and oil barrels.
            auto props=app.props_;
            props.push_back({PropKind::ColdBrazier,{20,5}}); props.push_back({PropKind::OilBarrel,{24,15}});
            app.map_.setTile(20,5,Tile{TileType::Wall,false,true}); app.map_.setTile(24,15,Tile{TileType::Wall,false,true});
            app.setProps(props);
            app.applyElement(Element::Fire,{{20,6}});
            check(app.props_[static_cast<std::size_t>(app.propIndexAt(20,5))].kind==PropKind::Brazier,"Fire lights a cold brazier beside it");
            app.updateFieldOfView();
            check(app.tileLit({20,7}),"A lit brazier lights the dark around it");
            app.applyElement(Element::Ice,{{21,5}});
            check(app.props_[static_cast<std::size_t>(app.propIndexAt(20,5))].kind==PropKind::ColdBrazier,"Cold smothers a brazier");
            app.applyElement(Element::Fire,{{20,6}});
            app.player_.setPosition({20,6}); app.updateFieldOfView();
            check(app.tryMovePlayer(0,-1) && app.propIndexAt(20,5)<0 && app.surfaceAt({20,5})==SurfaceType::Fire && app.surfaceAt({20,4})==SurfaceType::Fire,
                  "Walking into a lit brazier knocks it over and spills fire");
            auto* bystander=enemy({25,15}); const int bystanderHp=bystander->stats().hp;
            app.applyElement(Element::Fire,{{23,15}});
            check(app.propIndexAt(24,15)<0 && bystander->stats().hp<bystanderHp && app.surfaceAt({24,15})==SurfaceType::Fire,
                  "Fire blows up an oil barrel, burning those beside it");

            // Wall torches: an arrow snuffs them, fire lights them again.
            app.updateFieldOfView();
            if (!app.wallTorches_.empty()) {
                const auto front=app.wallTorches_.front();
                const bool was=app.torchLit(front.x,front.y-1);
                app.applyElement(was?Element::Arrow:Element::Fire,{front});
                check(app.torchLit(front.x,front.y-1)!=was,"Arrows snuff wall torches and fire lights them");
            }
            app.player_.setPosition({10,10}); app.updateFieldOfView();
            app.player_.talents()=TalentSet({basicAttack(),findTalentDefinition("arcane.bolt")->ranks[0],basicCleanse()});
            app.player_.talents().setRank(1,3); // the fixture's own kit, so the save is valid
            roundTrip();
            check(app.surfaceAt({5,15})==SurfaceType::Water && app.surfaceAt({8,15})!=SurfaceType::None,"Save/load keeps the surfaces");
        }

        // Light and blood: wisps, the lantern, bleeding foes, and monsters that use the ground.
        setup(PlayerClass::Mage); app.darknessEnabled_=true; app.clearSurfaces();
        {
            app.player_.talents().learnTalent(basicLight());
            std::size_t wisp=0;
            for (std::size_t i=0;i<app.player_.talents().knownTalents().size();++i) if (app.player_.talents().knownTalents()[i].id=="basic.light") wisp=i;
            app.player_.lightLit=false; app.updateFieldOfView();
            check(!app.tileLit({13,10}),"Without light the room is dark");
            check(app.tryUseTalent(wisp,app.player_.position()) && app.tileLit({14,10}) && app.lightOrbs_.size()==1,"Conjure Light leaves a wisp lighting five tiles");
            app.player_.setPosition({4,4}); app.updateFieldOfView();
            check(app.tileLit({12,10}),"The wisp stays where it was cast");
            snapshot("ui-wisp.png");
            for (int i=0;i<40;++i) app.tickSurfaces();
            check(app.lightOrbs_.empty(),"The wisp fades after 40 turns");
            app.player_.lightLit=true;

            app.landmark_=LandmarkKind::LamplighterRest; app.landmarkAltar_={11,10};
            app.map_.setTile(11,10,Tile{TileType::Wall,false,true}); app.player_.setPosition({10,10}); app.updateFieldOfView();
            app.pickupItem(); snapshot("ui-lamplighter.png"); app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::Enter});
            check(app.player_.lightSource==2 && app.playerLightRadius()==6,"The Lamplighter's Rest gives a lantern that lights six tiles");

            auto* victim=enemy({14,14}); victim->stats().hp=0; app.checkAndHandleDeath(*victim); app.removeDeadMonsters();
            check(app.surfaceAt({14,14})==SurfaceType::Blood,"The living leave blood where they fall");
            app.setSurface({15,14},SurfaceType::Blood,0);
            auto* standing=enemy({15,14}); const int before=standing->stats().hp;
            app.applyElement(Element::Lightning,{{14,14}});
            check(standing->stats().hp<before,"Lightning runs through blood too");
            {
                auto bones=createMonster(MonsterType::Skeleton,{20,15}); auto* skeleton=bones.get(); app.monsters_.push_back(std::move(bones));
                skeleton->stats().hp=0; app.checkAndHandleDeath(*skeleton); app.removeDeadMonsters();
                check(app.surfaceAt({20,15})==SurfaceType::None,"Skeletons don't bleed");
            }

            app.player_.setPosition({10,10}); app.clearSurfaces();
            auto slinger=createMonster(MonsterType::GoblinSlinger,{14,10}); auto* sling=slinger.get(); app.monsters_.push_back(std::move(slinger));
            AIDecision lob; lob.type=AIActionType::Attack; lob.target=&app.player_; lob.attackPower=1;
            app.executeAIDecision(*sling,lob,0);
            check(app.surfaceAt({10,10})==SurfaceType::Oil,"Goblin Slingers splash oil where their pots land");
            app.setSurface({11,11},SurfaceType::Water,0);
            auto acolyte=createMonster(MonsterType::FrostAcolyte,{13,11}); auto* frost=acolyte.get(); app.monsters_.push_back(std::move(acolyte));
            lob.attackPower=1; app.executeAIDecision(*frost,lob,0);
            check(app.surfaceAt({11,11})==SurfaceType::Ice,"Frost Acolytes freeze the ground around their target");
        }

        // Gear affixes that do things, and loot beams.
        setup(PlayerClass::Warrior);
        {
            const Item named(*findItemDefinition("iron_sword"),999,{},{{"vampiric",1},{"embers",15}},0);
            check(named.name()=="Vampiric Iron Sword of Embers","Magic and rare items are named by their affixes");
            app.player_.inventory().add(std::make_unique<Item>(*findItemDefinition("iron_sword"),app.nextItemId_++,Position{},
                std::vector<RolledAffix>{{"embers",100},{"vampiric",2}},0));
            app.player_.equip(app.player_.inventory().items().size()-1);
            app.player_.stats().hp=50;
            auto* target=enemy({11,10});
            const bool landed=applyTalentDamage(basicAttack(),app.player_,*target);
            check(landed && target->statusEffects().has(StatusEffectType::Burn) && app.player_.stats().hp==52,
                  "Embers sets foes burning and Vampiric heals on every hit");
            app.player_.inventory().add(std::make_unique<Item>(*findItemDefinition("chain_coat"),app.nextItemId_++,Position{},
                std::vector<RolledAffix>{{"thorns",5}},0));
            app.player_.equip(app.player_.inventory().items().size()-1);
            const int thornedBefore=target->stats().hp;
            AIDecision bite; bite.type=AIActionType::Attack; bite.target=&app.player_; bite.attackPower=30;
            app.player_.stats().dexterity=0; app.player_.baseStats().dexterity=0;
            for (int i=0;i<6 && target->stats().hp==thornedBefore;++i) app.executeAIDecision(*target,bite,0);
            check(target->stats().hp<thornedBefore,"Thorns hurt melee attackers");
            app.player_.stats().hp=app.player_.stats().maxHp;
            app.player_.inventory().add(std::make_unique<Item>(*findItemDefinition("iron_helm"),app.nextItemId_++,Position{},
                std::vector<RolledAffix>{{"radiant",1}},0));
            app.player_.equip(app.player_.inventory().items().size()-1);
            check(app.playerLightRadius()==5,"Radiant gear widens your light");
            app.monsters_.clear();
            app.groundItems_.push_back(std::make_unique<Item>(*findItemDefinition("ash_staff"),app.nextItemId_++,Position{12,9},std::vector<RolledAffix>{{"frost",20}},1));
            app.groundItems_.push_back(std::make_unique<Item>(*findItemDefinition("hunting_bow"),app.nextItemId_++,Position{14,11},
                std::vector<RolledAffix>{{"precise",4},{"storms",18}},1));
            app.groundItems_.push_back(std::make_unique<Item>(*findItemDefinition("unique_lichbane"),app.nextItemId_++,Position{9,13},std::vector<RolledAffix>{},5));
            app.updateFieldOfView();
            snapshot("ui-loot-beams.png");
            app.openInventory(); app.inventorySelection_=0; snapshot("ui-affix-tooltip.png"); app.inventoryOpen_=false;
            app.groundItems_.clear();
        }

        // New monsters: light, dark, fire and water.
        setup(PlayerClass::Warrior); app.darknessEnabled_=true;
        {
            const auto spawn=[&](MonsterType type,Position p) {
                auto m=createMonster(type,p); auto* raw=m.get(); app.monsters_.push_back(std::move(m)); app.scheduler_.add(*raw); return raw;
            };
            auto* torch=spawn(MonsterType::Torchbearer,{20,15});
            app.updateFieldOfView();
            check(app.tileLit({22,15}) && !app.tileLit({26,15}),"A Torchbearer lights the ground around it");
            app.setSurface({21,15},SurfaceType::Oil,0);
            app.tickSurfaces();
            check(app.surfaceAt({21,15})==SurfaceType::Fire,"A Torchbearer's torch ignites oil beside it");
            torch->stats().hp=0; app.checkAndHandleDeath(*torch); app.removeDeadMonsters(); app.clearSurfaces();

            auto* gloom=spawn(MonsterType::Gloomstalker,{11,10});
            app.player_.lightLit=false; app.updateFieldOfView();
            app.player_.stats().dexterity=0; app.player_.baseStats().dexterity=0;
            AIDecision claw; claw.type=AIActionType::Attack; claw.target=&app.player_; claw.attackPower=10;
            int dark=0, lit=0;
            for (int i=0;i<3;++i) { const int before=app.player_.stats().hp; app.executeAIDecision(*gloom,claw,0); dark=std::max(dark,before-app.player_.stats().hp); app.player_.stats().hp=app.player_.stats().maxHp; }
            app.player_.lightLit=true; app.updateFieldOfView();
            for (int i=0;i<3;++i) { const int before=app.player_.stats().hp; app.executeAIDecision(*gloom,claw,0); lit=std::max(lit,before-app.player_.stats().hp); app.player_.stats().hp=app.player_.stats().maxHp; }
            check(dark>lit && lit>0,"Gloomstalkers hit far harder from the dark");
            const int gloomHp=gloom->stats().hp; app.tickSurfaces();
            check(gloom->stats().hp<gloomHp,"Light sears a Gloomstalker");
            gloom->stats().hp=0; app.checkAndHandleDeath(*gloom); app.removeDeadMonsters();

            auto* firebrand=spawn(MonsterType::OrcFirebrand,{15,10});
            AIDecision flask; flask.type=AIActionType::Attack; flask.target=&app.player_; flask.attackPower=1;
            app.executeAIDecision(*firebrand,flask,0);
            check(app.surfaceAt(app.player_.position())==SurfaceType::Fire,"An Orc Firebrand's flask bursts into burning oil");
            firebrand->stats().hp=0; app.checkAndHandleDeath(*firebrand); app.removeDeadMonsters(); app.clearSurfaces();

            auto* drowned=spawn(MonsterType::DrownedOne,{14,14});
            drowned->stats().hp=drowned->stats().maxHp-6;
            app.tickSurfaces();
            check(app.surfaceAt({14,14})==SurfaceType::Water && drowned->stats().hp==drowned->stats().maxHp-4,
                  "A Drowned One leaves water and heals while standing in it");
            spawn(MonsterType::Torchbearer,{16,12}); spawn(MonsterType::Gloomstalker,{8,13});
            app.updateFieldOfView(); snapshot("ui-new-monsters.png");
            app.player_.stats().hp=app.player_.stats().maxHp;
        }

        // Any window size: the layout letterboxes and clicks map back into it.
        setup(PlayerClass::Mage);
        {
            app.window_.setSize({2560,1080});
            app.handleEvent(sf::Event::Resized{{2560,1080}});
            const auto size=app.window_.getSize();
            if (size.x==2560 && size.y==1080) {
                // 21:9: the 16:9 game is 1920 wide (scale 1.5), centred with 320px bars.
                const auto box=app.letterbox();
                check(std::abs(box.position.x-.125f)<.001f && std::abs(box.size.x-.75f)<.001f,"Ultrawide windows letterbox the game");
                app.mode_=GameMode::Town; app.merchantOpen_=false;
                const auto at=[&](const sf::FloatRect& r){ const auto c=screen::center(r); return sf::Vector2i{320+c.x*3/2,c.y*3/2}; };
                app.handleEvent(sf::Event::MouseButtonPressed{sf::Mouse::Button::Left,at(screen::kTownMerchantSpot)});
                check(app.merchantOpen_,"Clicks on an ultrawide window land on what they look like they hit");
                app.merchantOpen_=false; app.mode_=GameMode::Playing;
            } else std::cout<<"(hidden window could not be resized here; letterbox click test skipped)"<<std::endl;
            app.window_.setSize({1280,720});
            app.handleEvent(sf::Event::Resized{{1280,720}});
        }

        // Bosses use the new systems.
        setup(PlayerClass::Warrior); app.darknessEnabled_=true;
        {
            auto warlord=createMonster(MonsterType::GoblinWarlord,{14,10}); auto* w=warlord.get();
            app.monsters_.push_back(std::move(warlord)); app.boss_=w; app.scheduler_.add(*w);
            w->tactics.alert=8;
            app.placeBraziers({14,10},{{1,0}});
            check(app.propIndexAt(15,10)>=0,"Boss rooms get braziers");
            app.setSurface({14,13},SurfaceType::Water,0);
            app.placeBraziers({14,10},{{0,3}});
            check(app.surfaceAt({14,13})==SurfaceType::None && saveGame(app.captureState(),(output/"brazier-puddle.txt").string()) &&
                  loadGame((output/"brazier-puddle.txt").string()).has_value(),
                  "A brazier placed on a puddle clears it, so the save stays valid (found by the playtest bot)");
            app.player_.setPosition({18,10}); app.updateFieldOfView();
            check(app.bossSurfaceAction(*w) && app.propIndexAt(15,10)<0 && app.surfaceAt({16,10})==SurfaceType::Fire && app.surfaceAt({17,10})==SurfaceType::Fire,
                  "The Warlord kicks a lit brazier at you, scattering coals in a line");
            app.monsters_.clear(); app.boss_=nullptr; app.clearSurfaces();

            auto lich=createMonster(MonsterType::Lich,{14,10}); auto* l=lich.get();
            app.monsters_.push_back(std::move(lich)); app.boss_=l; app.scheduler_.add(*l);
            l->tactics.alert=8; app.player_.setPosition({10,10}); app.player_.lightLit=true;
            app.lightOrbs_.push_back({{12,10},20});
            app.updateFieldOfView();
            bool unlit=false;
            for (int i=0;i<7 && !unlit;++i) unlit=app.bossSurfaceAction(*l);
            check(unlit && app.player_.statusEffects().has(StatusEffectType::Smothered) && app.playerLightRadius()==0 && app.lightOrbs_.empty(),
                  "The Lich breathes out the light: wisps die and you are Smothered");
            app.toggleLight(); app.toggleLight();
            check(app.playerLightRadius()==0,"Smothered, your light can't be relit");
            app.player_.statusEffects().remove(StatusEffectType::Smothered);
            check(app.playerLightRadius()==4,"When Smothered ends, your light returns by itself");
            l->stats().hp=l->stats().maxHp/2; app.bossSurfaceAction(*l);
            check(l->flooded && app.surfaceAt({14,12})==SurfaceType::Water,"Badly hurt, the Lich floods its sanctum");
            app.setSurface(app.player_.position(),SurfaceType::Water,0);
            AIDecision bolt; bolt.type=AIActionType::Attack; bolt.target=&app.player_; bolt.attackPower=1; bolt.scalingStat=ScalingStat::Intelligence;
            app.executeAIDecision(*l,bolt,0);
            check(app.surfaceAt(app.player_.position())==SurfaceType::Ice,"The Lich's bolts freeze the water you stand in");
            app.player_.lightLit=true; app.monsters_.clear(); app.boss_=nullptr; app.clearSurfaces();
        }

        // Pushing into hazards: Shove, walls, foes, braziers and chasms.
        setup(PlayerClass::Warrior);
        {
            app.player_.talents().learnTalent(basicShove());
            std::size_t shove=0;
            for (std::size_t i=0;i<app.player_.talents().knownTalents().size();++i) if (app.player_.talents().knownTalents()[i].id=="basic.shove") shove=i;
            const auto shoveAt=[&](Position p){ app.player_.talents().resetCooldowns(); return app.tryUseTalent(shove,p); };
            auto* goblin=enemy({11,10}); goblin->stats().dexterity=0;
            app.setSurface({12,10},SurfaceType::Fire,4);
            const int hp=goblin->stats().hp;
            check(shoveAt({11,10}) && goblin->position().x==12 && goblin->statusEffects().has(StatusEffectType::Burn) && goblin->stats().hp==hp,
                  "Shove pushes a foe into fire without hurting it itself");
            app.clearSurfaces();
            app.player_.setPosition({29,10}); goblin->setPosition({30,10}); goblin->statusEffects().active().clear(); app.updateFieldOfView();
            const int beforeWall=goblin->stats().hp; shoveAt({30,10});
            check(goblin->stats().hp==beforeWall-3,"A foe shoved into a wall takes 3");
            auto* other=enemy({28,12});
            app.player_.setPosition({28,10}); goblin->setPosition({28,11}); app.updateFieldOfView();
            const int a=goblin->stats().hp,b=other->stats().hp; shoveAt({28,11});
            check(goblin->stats().hp==a-2 && other->stats().hp==b-2,"Two foes shoved together both take 2");
            other->stats().hp=0; app.checkAndHandleDeath(*other); app.removeDeadMonsters();
            auto props=app.props_; props.push_back({PropKind::Brazier,{20,5}}); app.map_.setTile(20,5,Tile{TileType::Wall,false,true}); app.setProps(props);
            app.player_.setPosition({20,7}); goblin->setPosition({20,6}); app.updateFieldOfView(); shoveAt({20,6});
            check(app.propIndexAt(20,5)<0 && app.surfaceAt({20,4})==SurfaceType::Fire,"A foe shoved into a lit brazier tips it over");
            app.clearSurfaces();
            for (int x=5;x<=7;++x) app.map_.setTile(x,15,Tile{TileType::Chasm,false,true});
            app.player_.setPosition({6,13}); goblin->setPosition({6,14}); goblin->stats().hp=goblin->stats().maxHp; app.updateFieldOfView();
            goblin->setXpReward(30);
            const int xpBefore=app.player_.xp(), dropsBefore=static_cast<int>(app.groundItems_.size());
            shoveAt({6,14}); app.removeDeadMonsters();
            check(app.monsters_.empty() && app.player_.xp()>xpBefore && static_cast<int>(app.groundItems_.size())==dropsBefore,
                  "A foe shoved into a chasm is gone: XP, no loot");
            auto warlord=createMonster(MonsterType::GoblinWarlord,{6,14}); auto* w=warlord.get(); app.monsters_.push_back(std::move(warlord)); app.boss_=w;
            w->stats().dexterity=0; const int bossHp=w->stats().hp; shoveAt({6,14});
            check(w->stats().hp>0 && w->position().y==14 && w->stats().hp<bossHp,"Bosses teeter on the edge instead of falling");
            snapshot("ui-chasm.png");
            app.monsters_.clear(); app.boss_=nullptr;
        }

        // Brawling: charge, grab, drag and throw.
        setup(PlayerClass::Warrior);
        {
            check(findTree("brawling") && startingTreeAllowed(PlayerClass::Warrior,TalentTree::Brawling) &&
                  !startingTreeAllowed(PlayerClass::Mage,TalentTree::Brawling),"Brawling is a Warrior starting tree");
            const auto learn=[&](const char* id) {
                app.player_.talents().learnTalent(findTalentDefinition(id)->ranks[0]);
                return app.player_.talents().knownTalents().size()-1;
            };
            const auto use=[&](std::size_t index,Position p){ app.player_.talents().resetCooldowns(); app.player_.stats().mana=app.player_.stats().maxMana; return app.tryUseTalent(index,p); };
            const auto tackle=learn("brawling.tackle"), grapple=learn("brawling.grapple"), hurl=learn("brawling.hurl");
            auto* goblin=enemy({14,10}); goblin->stats().dexterity=0; goblin->stats().hp=goblin->stats().maxHp=60;
            app.setSurface({15,10},SurfaceType::Fire,4);
            check(!app.targetPreview(tackle,{13,13}).valid,"Tackle only charges along a straight line");
            check(use(tackle,{14,10}) && app.player_.position().x==13 && goblin->position().x==15 &&
                  goblin->statusEffects().has(StatusEffectType::Burn) && app.player_.statusEffects().has(StatusEffectType::Opening),
                  "Tackle charges in, strikes and knocks the foe into the fire");
            app.clearSurfaces(); goblin->statusEffects().active().clear();

            app.player_.setPosition({10,10}); goblin->setPosition({11,10}); app.updateFieldOfView();
            app.setSurface({10,10},SurfaceType::Fire,6);
            check(use(grapple,{11,10}) && goblin->statusEffects().has(StatusEffectType::Grappled),"Grapple seizes an adjacent foe");
            app.player_.setPosition({10,10});
            app.tryMovePlayer(-1,0);
            check(app.player_.position().x==9 && goblin->position().x==10 && goblin->statusEffects().has(StatusEffectType::Burn),
                  "A grappled foe is dragged into the tile you left, fire and all");
            app.clearSurfaces();
            AIDecision walk; walk.type=AIActionType::Move; walk.movePosition={11,10};
            goblin->statusEffects().apply({StatusEffectType::Grappled,3,0});
            app.executeAIDecision(*goblin,walk,0);
            check(goblin->position().x==10,"A grappled foe can't walk away");

            // Hurl over the shoulder, then Domino at rank 5.
            goblin->statusEffects().active().clear();
            app.player_.setPosition({10,10}); goblin->setPosition({11,10}); app.updateFieldOfView();
            check(use(hurl,{11,10}) && goblin->position().x==8 && goblin->position().y==10,"Hurl throws a foe over your shoulder, two tiles behind you");
            auto* other=enemy({7,10}); other->stats().hp=other->stats().maxHp=60;
            goblin->setPosition({11,10});
            app.player_.talents().setRank(hurl,5);
            check(use(hurl,{11,10}) && goblin->position().x==8 && other->position().x==6,
                  "Domino: a hurled foe knocks the one it hits a tile further");
            other->stats().hp=0; app.checkAndHandleDeath(*other); app.removeDeadMonsters(); app.player_.talents().setRank(hurl,1);

            // A wall right behind you: it comes down where it stood.
            goblin->stats().hp=60; app.player_.setPosition({1,10}); goblin->setPosition({2,10}); app.updateFieldOfView();
            const int beforeSlam=goblin->stats().hp; use(hurl,{2,10});
            check(goblin->position().x==2 && goblin->stats().hp<beforeSlam-3,"With a wall at your back, the throw slams the foe down where it stood");

            // Hard Landing adds to every collision you cause.
            learn("brawling.hard_landing");
            app.player_.talents().learnTalent(basicShove());
            const auto shove=app.player_.talents().knownTalents().size()-1;
            goblin->stats().hp=60; app.player_.setPosition({29,10}); goblin->setPosition({30,10}); app.updateFieldOfView();
            const int beforeWall=goblin->stats().hp; use(shove,{30,10});
            check(goblin->stats().hp==beforeWall-5,"Hard Landing: a wall slam deals 3 + 2");

            // Bosses can't be held or lifted.
            auto warlord=createMonster(MonsterType::GoblinWarlord,{28,11}); auto* w=warlord.get();
            app.monsters_.push_back(std::move(warlord)); app.boss_=w; w->stats().dexterity=0;
            app.player_.setPosition({28,10}); app.updateFieldOfView();
            use(grapple,{28,11});
            check(!w->statusEffects().has(StatusEffectType::Grappled),"Bosses are too massive to grapple");
            use(hurl,{28,11});
            check(w->position().y==11,"Bosses are too heavy to hurl");
            app.monsters_.clear(); app.boss_=nullptr;
        }

        // The new trees: Whip, Shadow, Radiance and Alchemy.
        const auto learnTalent=[&](const char* id) {
            app.player_.talents().learnTalent(findTalentDefinition(id)->ranks[0]);
            return app.player_.talents().knownTalents().size()-1;
        };
        const auto cast=[&](std::size_t index,Position p) {
            app.player_.talents().resetCooldowns(); app.player_.stats().mana=app.player_.stats().maxMana;
            app.player_.stats().hp=app.player_.stats().maxHp;
            return app.tryUseTalent(index,p);
        };
        setup(PlayerClass::Thief);
        {
            check(startingTreeAllowed(PlayerClass::Thief,TalentTree::Whip),"Whip is a Thief starting tree");
            const auto lash=learnTalent("whip.lash"), trip=learnTalent("whip.trip"), snare=learnTalent("whip.snare");
            check(!talentUnavailableReason(app.player_,lash).empty(),"Whip abilities need a whip in hand");
            app.player_.inventory().add(std::make_unique<Item>(*findItemDefinition("leather_whip"),app.nextItemId_++));
            app.player_.equip(app.player_.inventory().items().size()-1);
            check(talentUnavailableReason(app.player_,lash).empty(),"With a whip equipped they work");
            auto* goblin=enemy({12,10}); goblin->stats().dexterity=0; goblin->stats().hp=goblin->stats().maxHp=80;
            app.player_.setPosition({10,10}); app.updateFieldOfView();
            app.setSurface({11,10},SurfaceType::Fire,4);
            check(!app.targetPreview(lash,{12,12}).valid,"Lash reaches only along a straight line");
            check(cast(lash,{12,10}) && goblin->position().x==11 && goblin->statusEffects().has(StatusEffectType::Burn),
                  "Lash strikes from two tiles away and pulls the foe through the fire");
            app.clearSurfaces(); goblin->statusEffects().active().clear();
            goblin->setPosition({12,10}); app.updateFieldOfView();
            check(cast(trip,{12,10}) && (goblin->statusEffects().has(StatusEffectType::Stun) || goblin->statusEffects().has(StatusEffectType::StunRecovery)),
                  "Trip knocks a foe down from two tiles away");
            goblin->statusEffects().active().clear(); goblin->setPosition({13,10}); app.updateFieldOfView();
            check(cast(snare,{13,10}) && goblin->position().x==11 && goblin->statusEffects().has(StatusEffectType::Grappled),
                  "Snare drags a foe from three tiles away right up to you, and holds it");
            app.monsters_.clear();
        }
        setup(PlayerClass::Mage); app.darknessEnabled_=true;
        {
            const auto& boltDef=findTalentDefinition("shadow.bolt")->ranks[0];
            check(isSpell(boltDef) && boltDef.manaCost==4,"Shadow spells are spells, priced like the other schools");
            const auto snuff=learnTalent("shadow.snuff"), veil=learnTalent("shadow.veil");
            learnTalent("shadow.umbral");
            app.player_.setPosition({10,10}); app.player_.lightLit=true; app.updateFieldOfView();
            cast(snuff,{10,10});
            check(!app.player_.lightLit && app.playerLightRadius()==0,"Snuff puts out your own light along with the rest");
            check(app.exploredMap_.at(13,10)==Visibility::Visible,"Umbral Shroud: with your light out you see three tiles into the dark");
            app.advanceEnemyIntents(); // as the enemies take their turn
            check(app.player_.statusEffects().magnitudeOf(StatusEffectType::Evasion)>=6,"Umbral Shroud: harder to hit in the dark");
            auto* goblin=enemy({12,10}); goblin->stats().dexterity=0; goblin->stats().hp=goblin->stats().maxHp=80;
            app.updateFieldOfView();
            check(cast(veil,{12,10}) && goblin->statusEffects().has(StatusEffectType::Blinded),"Veil of Night blinds");
            app.player_.setPosition({10,10});
            check(!app.canSee(*goblin,{10,10}) && app.canSee(*goblin,{12,11}),"A blinded foe sees only what is beside it");
            app.monsters_.clear();
        }
        setup(PlayerClass::Mage); app.darknessEnabled_=true;
        {
            const auto flare=learnTalent("radiance.flare"), dawn=learnTalent("radiance.dawn");
            learnTalent("radiance.inner_light");
            app.player_.lightLit=true;
            check(app.playerLightRadius()==5,"Inner Light: your light reaches a tile further");
            app.player_.setPosition({10,10}); app.updateFieldOfView();
            auto* goblin=enemy({12,10}); goblin->stats().dexterity=0; goblin->stats().hp=goblin->stats().maxHp=80; goblin->tactics.concealed=true;
            app.lightOrbs_.clear();
            check(cast(flare,{12,10}) && !goblin->tactics.concealed && goblin->statusEffects().has(StatusEffectType::Blinded) && !app.lightOrbs_.empty(),
                  "Flare blinds, reveals the hidden and leaves the spot lit");
            app.monsters_.clear();
            app.placeBraziers({10,10},{{3,-3}});
            for (auto& prop:app.props_) if (prop.kind==PropKind::Brazier) prop.kind=PropKind::ColdBrazier;
            app.player_.statusEffects().apply({StatusEffectType::Smothered,4,0}); app.player_.lightLit=false;
            cast(dawn,{10,10});
            const int brazier=app.propIndexAt(13,7);
            check(brazier>=0 && app.props_[static_cast<std::size_t>(brazier)].kind==PropKind::Brazier && app.player_.lightLit &&
                  !app.player_.statusEffects().has(StatusEffectType::Smothered),"Dawn relights braziers and breaks the smothering dark");
        }
        setup(PlayerClass::Thief);
        {
            const auto oil=learnTalent("alchemy.oil"), firebomb=learnTalent("alchemy.firebomb"), acid=learnTalent("alchemy.acid");
            app.player_.setPosition({10,10}); app.updateFieldOfView();
            cast(oil,{14,10});
            check(app.surfaceAt({14,10})==SurfaceType::Oil && app.surfaceAt({15,10})==SurfaceType::Oil,"Oil Flask splashes oil over a tile and its neighbours");
            cast(firebomb,{14,10});
            check(app.surfaceAt({14,10})==SurfaceType::Fire && app.surfaceAt({15,10})==SurfaceType::Fire,"Firebomb sets the ground (and the oil) alight");
            app.clearSurfaces();
            auto* goblin=enemy({14,13}); goblin->stats().dexterity=0; goblin->stats().hp=goblin->stats().maxHp=80;
            app.updateFieldOfView();
            cast(acid,{14,13});
            check(app.surfaceAt({14,13})==SurfaceType::Acid && goblin->statusEffects().has(StatusEffectType::Marked) && goblin->stats().hp<80,
                  "Acid eats at whatever stands in it and leaves it Marked");
            app.monsters_.clear(); app.clearSurfaces();
        }
        // The weapon trees: Spear, Daggers, Mace and Crossbow.
        setup(PlayerClass::Warrior);
        {
            for (int y=8;y<=14;++y) for (int x=8;x<=18;++x) app.map_.setTile(x,y,Tile{TileType::Floor,true,true});
            app.setProps({}); app.clearSurfaces();
            const auto wield=[&](const char* id) {
                app.player_.inventory().add(std::make_unique<Item>(*findItemDefinition(id),app.nextItemId_++));
                return app.player_.equip(app.player_.inventory().items().size()-1);
            };
            const auto foe=[&](Position p) { auto* g=enemy(p); g->stats().dexterity=0; g->stats().hp=g->stats().maxHp=90; return g; };
            const auto clearFoes=[&] { for (auto& m:app.monsters_) app.scheduler_.remove(*m); app.monsters_.clear(); app.clearSurfaces(); };
            const auto logged=[&](std::size_t since,const char* text) {
                const std::size_t fresh=std::min(app.logMessages_.size(),app.logTotal_-since);
                for (std::size_t i=app.logMessages_.size()-fresh;i<app.logMessages_.size();++i) if (app.logMessages_[i].find(text)!=std::string::npos) return true;
                return false;
            };
            app.player_.stats().hp=app.player_.stats().maxHp=500;

            // Spear.
            check(wield("iron_spear"),"A spear can be wielded");
            const auto thrust=learnTalent("spear.thrust"), brace=learnTalent("spear.brace"), vault=learnTalent("spear.vault");
            app.player_.setPosition({10,10});
            auto* a=foe({12,10}); auto* b=foe({13,10}); app.updateFieldOfView();
            cast(thrust,{12,10});
            check(a->stats().hp<90 && b->stats().hp<90,"Thrust reaches two tiles and runs on into the foe behind");
            clearFoes();
            cast(brace,app.player_.position());
            auto* c=foe({12,10});
            AIDecision step; step.type=AIActionType::Move; step.movePosition={11,10};
            app.executeAIDecision(*c,step,0);
            check(c->position().x==11 && c->stats().hp<90,"A braced spear strikes whatever steps up beside you");
            clearFoes(); app.player_.statusEffects().active().clear();
            app.map_.setTile(11,10,Tile{TileType::Chasm,false,true});
            foe({12,10}); app.updateFieldOfView();
            cast(vault,{14,10});
            check(app.player_.position().x>=13,"Pole Vault leaps over a chasm and a foe");
            app.map_.setTile(11,10,Tile{TileType::Floor,true,true}); clearFoes();

            // Daggers.
            check(wield("steel_dagger"),"A dagger can be wielded");
            const auto lacerate=learnTalent("daggers.lacerate"), backstab=learnTalent("daggers.backstab");
            app.player_.setPosition({10,10});
            auto* d=foe({11,10}); app.updateFieldOfView();
            cast(lacerate,{11,10});
            check(d->statusEffects().has(StatusEffectType::Bleed),"Lacerate makes a foe bleed");
            app.tickSurfaces();
            check(app.surfaceAt(d->position())==SurfaceType::Blood,"Bleeding foes leave a trail of blood");
            d->statusEffects().apply({StatusEffectType::Stun,2,0});
            auto since=app.logTotal_; cast(backstab,{11,10});
            check(logged(since,"Backstab: double damage"),"Backstab doubles its damage against a stunned foe");
            clearFoes();

            // Mace.
            check(wield("iron_mace"),"A mace can be wielded");
            const auto crush=learnTalent("mace.crush"), stagger=learnTalent("mace.stagger"), shatter=learnTalent("mace.shatter");
            auto* e=foe({11,10}); app.updateFieldOfView();
            cast(crush,{11,10});
            check(e->statusEffects().magnitudeOf(StatusEffectType::Sundered)==2,"Crush sunders the target's guard");
            e->intent()=EnemyIntent{{11,10},{10,10},0,1,5,IntentKind::StunStrike};
            cast(stagger,{11,10});
            check(e->intent() && e->intent()->playerActionsRemaining>=2,"Stagger delays a warned attack");
            e->intent().reset();
            auto* f=foe({12,11});
            app.setSurface({11,10},SurfaceType::Ice,0); app.setSurface({12,11},SurfaceType::Ice,0);
            const int fBefore=f->stats().hp;
            cast(shatter,{11,10});
            check(app.surfaceAt({12,11})==SurfaceType::None && f->stats().hp<fBefore,"Shatter breaks the ice into shards that cut those standing on it");
            clearFoes();

            // Crossbow.
            check(wield("light_crossbow"),"A crossbow can be wielded");
            const auto heavy=learnTalent("crossbow.heavy"), pierce=learnTalent("crossbow.pierce"), pin=learnTalent("crossbow.pin");
            auto* g=foe({13,10}); app.updateFieldOfView();
            cast(heavy,{13,10});
            check(g->position().x==14,"A heavy bolt knocks its target back");
            clearFoes();
            auto* h1=foe({12,10}); auto* h2=foe({14,10}); app.updateFieldOfView();
            cast(pierce,{14,10});
            check(h1->stats().hp<90 && h2->stats().hp<90,"A piercing bolt passes through every foe in its line");
            cast(pin,{12,10});
            AIDecision run; run.type=AIActionType::Move; run.movePosition={12,11};
            app.executeAIDecision(*h1,run,0);
            check(h1->statusEffects().has(StatusEffectType::Pinned) && h1->position().y==10,"A pinned foe can't move");
            clearFoes();

            // Opening a weapon tree hands you a training weapon of that kind.
            app.player_.level()=5; app.player_.treePoints()=1;
            std::size_t spearTree=0; while(std::string(kTalentTrees[spearTree].id)!="spear") ++spearTree;
            for (std::size_t i=app.player_.inventory().items().size();i-->0;)
                if (app.player_.inventory().items()[i]->definition()->weaponKind==WeaponKind::Spear) app.player_.inventory().take(i);
            if (const auto* worn=app.player_.inventory().equipped(EquipmentSlot::Weapon); worn && worn->definition()->weaponKind==WeaponKind::Spear)
                app.player_.inventory().unequip(EquipmentSlot::Weapon);
            app.treeSelection_=spearTree; app.abilitySelection_=0; app.handleTreeKey(sf::Keyboard::Key::Enter,false);
            bool training=false;
            for (const auto& item:app.player_.inventory().items()) training=training || std::string(item->definition()->id)=="training_spear";
            check(treeAccess(app.player_,"spear") && training,"Opening the Spear tree puts a training spear in your bag");
        }

        // The magic batch: Earth, Tide, Hexes and Venom.
        setup(PlayerClass::Mage);
        {
            for (int y=6;y<=16;++y) for (int x=6;x<=20;++x) app.map_.setTile(x,y,Tile{TileType::Floor,true,true});
            app.setProps({}); app.clearSurfaces();
            app.player_.stats().hp=app.player_.stats().maxHp=500; app.player_.stats().maxMana=500;
            app.player_.setPosition({10,10});
            const auto foe=[&](Position p) { auto* g=enemy(p); g->stats().dexterity=0; g->stats().hp=g->stats().maxHp=90; return g; };
            const auto clearFoes=[&] { for (auto& m:app.monsters_) app.scheduler_.remove(*m); app.monsters_.clear(); app.clearSurfaces(); app.boss_=nullptr; };
            check(isSpell(findTalentDefinition("earth.spike")->ranks[0]) && isSpell(findTalentDefinition("venom.plague")->ranks[0]),"Earth, Tide, Hexes and Venom are spell schools");

            // Earth.
            const auto spike=learnTalent("earth.spike"), pillar=learnTalent("earth.pillar"), quake=learnTalent("earth.quake");
            auto* a=foe({13,10}); app.updateFieldOfView();
            cast(spike,{13,10});
            check(a->statusEffects().has(StatusEffectType::Pinned),"Stone Spike pins its target");
            clearFoes(); app.updateFieldOfView();
            cast(pillar,{12,12});
            check(app.propIndexAt(12,12)>=0 && !app.map_.isWalkable(12,12),"Raise Pillar puts a stone pillar on the ground");
            app.setSurface({13,10},SurfaceType::Gas,6); app.setSurface({14,10},SurfaceType::Gas,6);
            snapshot("ui-earth-pillar.png"); app.clearSurfaces();
            app.player_.talents().learnTalent(basicShove());
            const auto shove=app.player_.talents().knownTalents().size()-1;
            auto* b=foe({12,11}); app.player_.setPosition({12,10}); app.updateFieldOfView();
            const int beforeSlam=b->stats().hp; cast(shove,{12,11});
            check(b->stats().hp<beforeSlam,"Foes can be shoved into a pillar");
            for (int i=0;i<13;++i) app.tickSurfaces();
            check(app.propIndexAt(12,12)<0 && app.map_.isWalkable(12,12),"The pillar crumbles in time");
            clearFoes(); app.player_.setPosition({10,10});
            auto* c=foe({11,10}); app.updateFieldOfView();
            cast(quake,{10,10});
            check(c->position().x==12 && c->stats().hp<90,"Quake strikes and throws back everything around you");
            clearFoes();

            // Tide.
            const auto bolt=learnTalent("tide.bolt"), wave=learnTalent("tide.wave"), maelstrom=learnTalent("tide.maelstrom");
            auto* d=foe({13,10}); app.updateFieldOfView();
            cast(bolt,{13,10});
            check(app.surfaceAt({13,10})==SurfaceType::Water,"Water Bolt floods the tile it strikes");
            clearFoes();
            auto* e1=foe({12,10}); auto* e2=foe({14,10}); app.updateFieldOfView();
            cast(wave,{14,10});
            check(e1->position().x>=13 && e2->position().x>=15 && app.surfaceAt({11,10})==SurfaceType::Water,"Wave shoves a line of foes back and leaves water behind");
            clearFoes();
            auto* f=foe({16,10}); app.updateFieldOfView();
            cast(maelstrom,{14,10});
            check(f->position().x==15 && app.surfaceAt({14,10})==SurfaceType::Water && f->statusEffects().has(StatusEffectType::Chill),
                  "Maelstrom floods, chills and drags foes to its centre");
            clearFoes(); (void)d;

            // Hexes.
            const auto misfortune=learnTalent("hexes.misfortune"), link=learnTalent("hexes.link"), puppet=learnTalent("hexes.puppet");
            auto* g=foe({11,10}); app.updateFieldOfView();
            cast(misfortune,{11,10});
            check(g->statusEffects().has(StatusEffectType::Misfortune),"Misfortune curses a foe");
            g->statusEffects().apply({StatusEffectType::Misfortune,4,100});
            app.player_.stats().dexterity=0;
            const int hp=app.player_.stats().hp;
            for (int i=0;i<5;++i) { AIDecision swing; swing.type=AIActionType::Attack; swing.target=&app.player_; swing.attackPower=5; app.executeAIDecision(*g,swing,0); }
            check(app.player_.stats().hp==hp,"A foe cursed with total misfortune never lands a blow");
            clearFoes();
            auto* h1=foe({13,10}); auto* h2=foe({14,12}); app.updateFieldOfView();
            cast(link,{13,10});
            cast(bolt,{13,10});
            check(h1->stats().hp<90 && h2->stats().hp<90,"Soul Link carries part of a hit to a nearby foe");
            cast(puppet,{13,10});
            check(h1->statusEffects().has(StatusEffectType::Puppeted) && app.nearestOpponent(*h1,false)==h2,"A puppet turns on its own kind");
            clearFoes();
            auto warlord=createMonster(MonsterType::GoblinWarlord,{13,10}); auto* w=warlord.get();
            app.monsters_.push_back(std::move(warlord)); app.boss_=w; app.updateFieldOfView();
            cast(puppet,{13,10});
            check(!w->statusEffects().has(StatusEffectType::Puppeted),"Bosses resist being made puppets");
            clearFoes();

            // Venom.
            const auto venom=learnTalent("venom.bolt"), miasma=learnTalent("venom.miasma"), plague=learnTalent("venom.plague");
            auto* v=foe({13,10}); app.updateFieldOfView();
            cast(venom,{13,10});
            check(v->statusEffects().has(StatusEffectType::Poison),"Venom Bolt poisons");
            clearFoes();
            cast(miasma,{14,10});
            check(app.surfaceAt({14,10})==SurfaceType::Gas,"Miasma fills the ground with poison gas");
            auto* x=foe({14,10}); const int xBefore=x->stats().hp;
            app.applyElement(Element::Fire,{{14,10}});
            check(app.surfaceAt({14,10})==SurfaceType::Fire && x->stats().hp<xBefore,"Fire makes the gas explode");
            clearFoes();
            auto* p1=foe({13,10}); auto* p2=foe({14,10}); app.updateFieldOfView();
            cast(plague,{13,10});
            check(p1->statusEffects().has(StatusEffectType::Plague),"Plague infects its target");
            p1->stats().hp=0; app.checkAndHandleDeath(*p1); app.removeDeadMonsters();
            check(p2->statusEffects().has(StatusEffectType::Plague),"When the host dies, the plague spreads to its neighbours");
            clearFoes();
        }

        // The last batch: Traps, Skirmish and five more hybrids.
        setup(PlayerClass::Warrior);
        {
            for (int y=6;y<=16;++y) for (int x=6;x<=20;++x) app.map_.setTile(x,y,Tile{TileType::Floor,true,true});
            app.setProps({}); app.clearSurfaces();
            app.player_.stats().hp=app.player_.stats().maxHp=500; app.player_.stats().maxMana=500;
            app.player_.setPosition({10,10});
            const auto foe=[&](Position p) { auto* g=enemy(p); g->stats().dexterity=0; g->stats().hp=g->stats().maxHp=90; return g; };
            const auto clearFoes=[&] { for (auto& m:app.monsters_) app.scheduler_.remove(*m); app.monsters_.clear(); app.clearSurfaces(); app.traps_.clear(); };
            const auto walk=[&](Monster* m,Position to) { AIDecision step; step.type=AIActionType::Move; step.movePosition=to; app.executeAIDecision(*m,step,0); };
            const auto wield=[&](const char* id) {
                app.player_.inventory().add(std::make_unique<Item>(*findItemDefinition(id),app.nextItemId_++));
                return app.player_.equip(app.player_.inventory().items().size()-1);
            };
            app.updateFieldOfView();

            // Traps.
            const auto snare=learnTalent("traps.snare"), tripwire=learnTalent("traps.tripwire"), rigged=learnTalent("traps.rigged");
            cast(snare,{12,10});
            check(app.traps_.size()==1,"A snare is set on the ground");
            app.traps_.push_back({{12,12},2,40,1}); app.traps_.push_back({{13,11},3,40,1}); app.traps_.push_back({{11,12},4,40,1}); app.traps_.push_back({{13,9},5,40,1});
            snapshot("ui-traps.png"); app.traps_.resize(1);
            auto* a=foe({13,10}); walk(a,{12,10});
            check(a->statusEffects().has(StatusEffectType::Pinned) && a->stats().hp<90 && app.traps_.empty(),"A foe that steps on the snare is caught and pinned");
            clearFoes();
            cast(tripwire,{12,12});
            auto* b=foe({12,13}); walk(b,{12,12});
            check(b->position().y<=10,"A tripwire flings the foe on in the direction it was walking");
            clearFoes();
            cast(rigged,{14,10});
            auto* c=foe({15,10}); auto* c2=foe({15,11}); walk(c,{14,10});
            check(c->stats().hp<90 && c2->stats().hp<90 && app.surfaceAt({14,10})==SurfaceType::Fire,"A rigged charge blows up everything near and sets the ground alight");
            clearFoes();

            // Skirmish: moving is attacking.
            learnTalent("skirmish.lunge"); learnTalent("skirmish.pass");
            app.player_.setPosition({10,10});
            auto* d=foe({12,10}); app.updateFieldOfView();
            app.tryMovePlayer(1,0);
            check(d->stats().hp<90,"Lunge: stepping toward a foe two tiles ahead strikes it");
            clearFoes();
            app.player_.setPosition({10,10});
            auto* e=foe({11,11}); app.updateFieldOfView();
            app.tryMovePlayer(1,0);
            check(e->stats().hp<90,"Pass Strike: stepping past a foe cuts it");
            clearFoes();
            const auto blitz=learnTalent("skirmish.blitz");
            app.player_.setPosition({10,10});
            auto* f1=foe({11,11}); auto* f2=foe({12,9}); app.updateFieldOfView();
            cast(blitz,{13,10});
            check(f1->stats().hp<90 && f2->stats().hp<90,"Blitz strikes everything beside its path");
            clearFoes();

            // Lamplighter.
            const auto swing=learnTalent("lamplighter.swing"), hurlTorch=learnTalent("lamplighter.hurl");
            app.player_.setPosition({10,10}); app.player_.lightLit=false;
            check(!talentUnavailableReason(app.player_,swing).empty(),"Lamplighter needs your light burning");
            app.player_.lightLit=true;
            auto* g=foe({11,10}); app.updateFieldOfView();
            cast(swing,{11,10});
            check(g->statusEffects().has(StatusEffectType::Burn),"Torch Swing sets the foe burning");
            clearFoes(); app.lightOrbs_.clear();
            foe({14,10}); app.updateFieldOfView();
            cast(hurlTorch,{14,10});
            check(!app.player_.lightLit && !app.lightOrbs_.empty(),"Hurl Torch leaves your torch burning where it lands, and your hand empty");
            clearFoes(); app.player_.lightLit=true;

            // Stormlance.
            wield("iron_spear");
            const auto thrust=learnTalent("stormlance.thrust"), tvault=learnTalent("stormlance.vault");
            app.player_.setPosition({10,10});
            auto* h=foe({12,10}); app.updateFieldOfView();
            cast(thrust,{12,10});
            check(h->statusEffects().has(StatusEffectType::Shock),"Charged Thrust shocks from two tiles away");
            clearFoes();
            auto* i1=foe({11,10}); auto* i2=foe({14,11}); app.updateFieldOfView();
            cast(tvault,{14,10});
            check(i2->statusEffects().has(StatusEffectType::Shock) && i2->stats().hp<90 && i1->stats().hp==90,"Thunder Vault lands in a burst of lightning");
            clearFoes();

            // Hexblade.
            wield("iron_sword");
            const auto edge=learnTalent("hexblade.edge"), rend=learnTalent("hexblade.rend");
            app.player_.setPosition({10,10});
            auto* j=foe({11,10}); app.updateFieldOfView();
            cast(edge,{11,10});
            check(j->statusEffects().has(StatusEffectType::Misfortune),"Cursed Edge lays Misfortune");
            const auto since=app.logTotal_; cast(rend,{11,10});
            bool rent=false;
            for (std::size_t k=app.logMessages_.size()-std::min(app.logMessages_.size(),app.logTotal_-since);k<app.logMessages_.size();++k)
                rent=rent || app.logMessages_[k].find("curse is rent")!=std::string::npos;
            check(rent,"Soul Rend doubles its damage against the cursed");
            clearFoes();

            // Saboteur.
            const auto caltrops=learnTalent("saboteur.caltrops"), smoke=learnTalent("saboteur.smoke"), booby=learnTalent("saboteur.booby");
            cast(caltrops,{13,12});
            check(app.traps_.size()>=4,"Caltrops scatter over a tile and its neighbours");
            app.traps_.clear();
            auto* k=foe({11,11}); app.updateFieldOfView();
            cast(smoke,app.player_.position());
            check(app.player_.statusEffects().has(StatusEffectType::Concealed) && k->statusEffects().has(StatusEffectType::Blinded),"Smoke Bomb hides you and blinds those near");
            clearFoes(); app.player_.statusEffects().active().clear();
            cast(booby,{14,12});
            auto* l=foe({15,12}); walk(l,{14,12});
            check(app.surfaceAt({14,12})==SurfaceType::Fire && l->stats().hp<90,"A booby trap bursts into burning gas");
            clearFoes();

            // Stonefist.
            const auto slam=learnTalent("stonefist.slam");
            app.player_.setPosition({10,10});
            auto* m=foe({11,10}); app.updateFieldOfView();
            cast(slam,{11,10});
            check(app.propIndexAt(12,10)>=0 && m->position().x==11 && m->stats().hp<85,"Pillar Slam raises a pillar behind the foe and drives it in");
            clearFoes(); app.pillarTurns_.clear(); app.setProps({});
            app.map_.setTile(12,10,Tile{TileType::Floor,true,true});

            // The hybrids open with their parent trees.
            check(!hiddenTreeAvailable(app.player_,"lamplighter") && !hybridRequirement("stonefist").empty(),"The new hybrids start locked, showing what they need");
            const auto invest=[&](const char* id) {
                app.player_.talents().learnTalent(findTalentDefinition(id)->ranks[0]);
                app.player_.talents().setRank(app.player_.talents().knownTalents().size()-1,5);
            };
            invest("radiance.sear"); invest("fire.ember_bolt");
            check(hiddenTreeAvailable(app.player_,"lamplighter"),"Lamplighter opens with 5 ranks in Radiance and in Fire");
        }

        // Acid and blindness survive a save (a fresh Warrior: the fixture's
        // One-Handed tree makes a level-1 Thief save invalid).
        setup(PlayerClass::Warrior);
        {
            auto* goblin=enemy({14,13});
            goblin->statusEffects().apply({StatusEffectType::Blinded,3,0});
            app.setSurface({14,13},SurfaceType::Acid,8);
            roundTrip();
            check(app.surfaceAt({14,13})==SurfaceType::Acid && std::any_of(app.monsters_.begin(),app.monsters_.end(),[](const auto& m){ return m->statusEffects().has(StatusEffectType::Blinded); }),
                  "Acid and blindness survive a save");
            app.monsters_.clear(); app.clearSurfaces();
        }

        // Enemies push back: slams and heavy blows knock you into whatever is behind you.
        setup(PlayerClass::Warrior);
        {
            const auto strike=[&](MonsterType type,Position from,IntentKind kind) {
                auto made=createMonster(type,from); auto* m=made.get();
                app.scheduler_.add(*m); app.monsters_.push_back(std::move(made));
                bool pushed=false;
                const Position start=app.player_.position();
                for (int attempt=0;attempt<12 && !pushed;++attempt) {
                    app.player_.setPosition(start); app.player_.stats().hp=app.player_.stats().maxHp;
                    app.player_.statusEffects().active().clear();
                    m->intent()=EnemyIntent{from,start,kind==IntentKind::HeavyStrike?1:0,0,2,kind};
                    m->recoveryActions=0;
                    app.currentActor_=m; app.processMonsterTurns();
                    pushed=app.player_.position().x!=start.x || app.player_.position().y!=start.y || app.pendingFall_;
                }
                return m;
            };
            app.player_.stats().dexterity=0;
            app.player_.setPosition({11,10}); app.updateFieldOfView();
            app.setSurface({10,10},SurfaceType::Fire,6);
            strike(MonsterType::Ogre,{12,10},IntentKind::StunStrike);
            check(app.player_.position().x==10 && app.player_.statusEffects().has(StatusEffectType::Burn),"An Ogre's slam knocks you back into the fire");
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.clearSurfaces();

            app.player_.setPosition({20,10}); app.updateFieldOfView();
            auto* w=strike(MonsterType::GoblinWarlord,{21,10},IntentKind::HeavyStrike);
            check(app.player_.position().x==18,"The Warlord's heavy blow knocks you back two tiles");
            (void)w; for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.boss_=nullptr;

            // Knocked into a chasm: you drop to the floor below, hurt but alive.
            for (int x=5;x<=7;++x) app.map_.setTile(x,12,Tile{TileType::Chasm,false,true});
            app.player_.setPosition({6,13}); app.updateFieldOfView();
            const int floor=app.currentFloor_;
            strike(MonsterType::Ogre,{6,14},IntentKind::StunStrike);
            check(app.pendingFall_,"Knocked over the edge, you start to fall");
            app.player_.stats().hp=3;
            app.advanceTurnsUntilPlayerCanAct();
            check(app.currentFloor_==floor+1 && app.player_.stats().hp>=1 && !app.pendingFall_ && app.map_.isWalkable(app.player_.position().x,app.player_.position().y),
                  "...and land on the floor below, hurt but never killed by the fall");
            app.trial_=0; app.pendingFall_=false;
        }

        // The Drowned Cathedral: a sealed side dungeon of six floors.
        setup(PlayerClass::Warrior);
        {
            app.player_.stats().hp=app.player_.stats().maxHp=400;
            app.mode_=GameMode::Town; app.player_.trialKeys=0;
            app.travelFloor(kCathedralFirst,true);
            check(app.mode_==GameMode::Town,"The Cathedral stays sealed until the Warlord falls");
            app.player_.trialKeys=1;
            app.travelFloor(kCathedralFirst,true);
            check(app.mode_==GameMode::Playing && app.currentFloor_==kCathedralFirst && std::string(floorTheme(app.currentFloor_).name)=="Drowned Cathedral",
                  "The Warlord's sigil opens the Drowned Cathedral");
            check(floorDepth(kCathedralFirst)==7 && floorDepth(kCathedralLast)==12 && floorInDungeon(kCathedralLast)==6,
                  "Its six floors are as deep as Ruins 7 to 12");
            int water=0;
            for (int y=0;y<app.map_.height();++y) for (int x=0;x<app.map_.width();++x) water+=app.surfaceAt({x,y})==SurfaceType::Water;
            check(water>=40,"The Cathedral's floors stand in water");
            bool natives=false;
            for (const auto& m:app.monsters_) natives=natives || m->type()==MonsterType::DeepLurker || m->type()==MonsterType::DrownedChorister;
            check(natives,"Deep Lurkers and Drowned Choristers live there");
            {
                // Stage the Cathedral's creatures in view for the screenshot.
                const auto me=app.player_.position();
                std::vector<Position> open;
                for (int dy=-3;dy<=3;++dy) for (int dx=-3;dx<=3;++dx) {
                    const Position p{me.x+dx,me.y+dy};
                    if ((dx||dy) && app.map_.isWalkable(p.x,p.y) && !app.isOccupied(p,nullptr)) open.push_back(p);
                }
                std::sort(open.begin(),open.end(),[&](Position a,Position b){ return std::abs(a.x-me.x)+std::abs(a.y-me.y)<std::abs(b.x-me.x)+std::abs(b.y-me.y); });
                for (std::size_t i=0;i<open.size() && i<2;++i) {
                    auto m=createMonster(i?MonsterType::DrownedChorister:MonsterType::DeepLurker,open[i*2]);
                    app.monsters_.push_back(std::move(m));
                }
                app.updateFieldOfView();
            }
            snapshot("ui-cathedral.png");
            app.mode_=GameMode::Town; app.dungeonMenu_=true; app.dungeonSelection_=2; app.dungeonDepth_=1; snapshot("ui-dungeon-cathedral.png");
            app.dungeonMenu_=false; app.mode_=GameMode::Playing;
            app.player_.statusEffects().active().clear(); app.updateFieldOfView();
            app.combatThisTurn_=false;
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.clearSurfaces();

            // A Deep Lurker drags you into the water.
            for (int y=9;y<=11;++y) for (int x=10;x<=17;++x) { app.map_.setTile(x,y,Tile{TileType::Floor,true,true}); }
            app.setProps({});
            for (int x=12;x<=13;++x) app.setSurface({x,10},SurfaceType::Water,0);
            app.player_.setPosition({11,10}); app.updateFieldOfView();
            auto lurker=createMonster(MonsterType::DeepLurker,{12,10}); auto* l=lurker.get();
            app.scheduler_.add(*l); app.monsters_.push_back(std::move(lurker));
            AIDecision bite; bite.type=AIActionType::Attack; bite.target=&app.player_; bite.attackPower=1;
            app.executeAIDecision(*l,bite,0);
            check(l->position().x==13 && app.player_.position().x==12,"A Deep Lurker sinks back into the water and drags you in after it");
            // A Chorister's hymn charges the pool you stand in.
            auto singer=createMonster(MonsterType::DrownedChorister,{16,10}); auto* c=singer.get();
            app.scheduler_.add(*c); app.monsters_.push_back(std::move(singer));
            AIDecision hymn; hymn.type=AIActionType::Attack; hymn.target=&app.player_; hymn.attackPower=1; hymn.scalingStat=ScalingStat::Intelligence;
            app.executeAIDecision(*c,hymn,0);
            check(app.surfaceAt({12,10})==SurfaceType::Electrified,"A Drowned Chorister's hymn charges the water you stand in");
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.clearSurfaces(); app.player_.statusEffects().active().clear();

            // The Sleeper Below, on the last floor.
            app.floorCache_.clear(); app.currentFloor_=kCathedralLast; app.regenerateLevel(77);
            check(app.boss_ && app.boss_->type()==MonsterType::TheSleeper && app.surfaceAt(app.boss_->position())==SurfaceType::Water,
                  "The Sleeper Below waits on the sixth floor, in a pool of black water");
            auto* sleeper=dynamic_cast<Monster*>(app.boss_);
            sleeper->tactics.alert=8;
            const Position near{sleeper->position().x-3,sleeper->position().y};
            if (app.map_.isWalkable(near.x,near.y)) app.player_.setPosition(near);
            app.updateFieldOfView();
            snapshot("ui-sleeper.png");
            bool flooded=false, warned=false, charged=false;
            for (int turn=0;turn<7;++turn) {
                const auto before=app.logTotal_;
                app.bossSurfaceAction(*sleeper);
                for (std::size_t i=app.logMessages_.size()-std::min(app.logMessages_.size(),app.logTotal_-before);i<app.logMessages_.size();++i) {
                    flooded=flooded || app.logMessages_[i].find("wells up")!=std::string::npos;
                    warned=warned || app.logMessages_[i].find("will be charged")!=std::string::npos;
                    charged=charged || app.logMessages_[i].find("races through the water")!=std::string::npos;
                }
            }
            check(flooded && warned && charged,"It floods the room, warns you, then charges the water");
            sleeper->stats().hp=sleeper->stats().maxHp/2;
            const auto before=app.monsters_.size();
            app.bossSurfaceAction(*sleeper);
            check(app.monsters_.size()==before+2,"Badly hurt, it calls the Drowned");
            app.player_.patron=static_cast<int>(Patron::Sleeper); app.player_.favor=40;
            const auto drops=app.groundItems_.size();
            sleeper->stats().hp=0; app.checkAndHandleDeath(*sleeper); app.removeDeadMonsters();
            for (auto& m:app.monsters_) { m->stats().hp=0; app.scheduler_.remove(*m); }
            app.removeDeadMonsters(); app.player_.statusEffects().active().clear(); app.combatThisTurn_=false; app.updateFieldOfView();
            check(app.groundItems_.size()>drops && app.player_.patron==0,"Slain, it drops a unique, and its faithful lose their god");
            app.player_.position(); app.travelFloor(kCathedralLast+1);
            check(app.mode_==GameMode::Town,"The Cathedral's last stairs lead back to town");
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.boss_=nullptr; app.mode_=GameMode::Playing; app.floorCache_.clear();
            app.currentFloor_=1; app.regenerateLevel(1); app.player_.trialKeys=0;
        }

        // Hybrid trees open with five ranks in each parent tree.
        setup(PlayerClass::Warrior);
        {
            const auto invest=[&](const char* id,int rank) {
                app.player_.talents().learnTalent(findTalentDefinition(id)->ranks[0]);
                app.player_.talents().setRank(app.player_.talents().knownTalents().size()-1,rank);
            };
            check(!hiddenTreeAvailable(app.player_,"spellblade") && !hiddenTreeAvailable(app.player_,"shadow_archer") &&
                  !hiddenTreeAvailable(app.player_,"animation"),"Hybrid trees start locked");
            const auto* archer=findTree("shadow_archer");
            check(treePurchaseReason(app.player_,PlayerClass::Warrior,*archer).find("Bow")!=std::string::npos,
                  "A locked hybrid tree says what it needs");
            invest("bow.quick_shot",5);
            check(!hiddenTreeAvailable(app.player_,"shadow_archer"),"One parent is not enough");
            invest("stealth.conceal",3); invest("stealth.strike",2);
            check(hiddenTreeAvailable(app.player_,"shadow_archer"),"Shadow Archer opens with 5 ranks in Bow and in Stealth");
            invest("fire.ember_bolt",5);
            check(!hiddenTreeAvailable(app.player_,"spellblade"),"Spellblade needs a melee tree too");
            invest("whip.lash",5);
            check(hiddenTreeAvailable(app.player_,"spellblade"),"Spellblade opens with 5 melee ranks and 5 magic ranks");
            invest("shadow.bolt",5);
            check(hiddenTreeAvailable(app.player_,"animation"),"Animation opens with Shadow and another school");
            check(!hiddenTreeAvailable(app.player_,"blood_magic"),"Blood Magic still needs the altar");
        }

        // The Blood Altar: its Vampire Lord, and the Blood Magic it teaches.
        setup(PlayerClass::Warrior);
        {
            check(!hiddenTreeAvailable(app.player_,"blood_magic"),"Blood Magic starts locked");
            bool found=false;
            for (unsigned seed=1;seed<400 && !found;++seed) {
                app.currentFloor_=4+static_cast<int>(seed%2)*2; app.regenerateLevel(seed);
                found=app.landmark_==LandmarkKind::BloodAltar;
            }
            check(found,"The Blood Altar turns up on deeper floors");
            if (found) {
                Monster* lord=nullptr;
                for (auto& m:app.monsters_) if (m->eventChampion==kChampionVampire) lord=m.get();
                check(lord && lord->tactics.alert==0 && app.vampireLordAlive(),"A Vampire Lord sleeps beside the altar");
                bool pool=false;
                for (int dy=-1;dy<=2;++dy) for (int dx=-2;dx<=2;++dx) pool=pool || app.surfaceAt({app.landmarkAltar_.x+dx,app.landmarkAltar_.y+dy})==SurfaceType::Blood;
                check(pool,"The altar stands in a pool of blood");
                check(!app.landmarkChoices().front().affordable,"While he lives, the altar won't take your offering");
                for (const Position d:{Position{0,2},Position{-1,2},Position{1,2},Position{0,3},Position{-2,1},Position{2,1}}) {
                    const Position p{app.landmarkAltar_.x+d.x,app.landmarkAltar_.y+d.y};
                    if (app.map_.isWalkable(p.x,p.y) && !app.isOccupied(p,nullptr)) { app.player_.setPosition(p); break; }
                }
                app.updateFieldOfView(); snapshot("ui-blood-altar.png");
                if (lord) {
                    // He drinks what he strikes.
                    app.darknessEnabled_=false;
                    lord->stats().hp=lord->stats().maxHp/2;
                    app.player_.stats().dexterity=0; app.player_.stats().hp=app.player_.stats().maxHp=500;
                    const int wounded=lord->stats().hp;
                    for (int i=0;i<12 && lord->stats().hp==wounded;++i) {
                        AIDecision bite; bite.type=AIActionType::Attack; bite.target=&app.player_; bite.attackPower=4;
                        app.executeAIDecision(*lord,bite,0);
                    }
                    check(lord->stats().hp>wounded,"The Vampire Lord heals by the blood he draws");
                    lord->stats().hp=lord->stats().maxHp/2;
                    app.setSurface(lord->position(),SurfaceType::Blood,0);
                    const int before=lord->stats().hp; app.tickSurfaces();
                    check(lord->stats().hp>before,"...and by standing in blood");
                    const auto drops=app.groundItems_.size();
                    lord->stats().hp=0; app.checkAndHandleDeath(*lord); app.removeDeadMonsters();
                    check(!app.vampireLordAlive() && app.groundItems_.size()>drops,"Slain, he drops a unique item");
                }
                app.player_.stats().maxHp=app.player_.baseStats().maxHp; app.player_.refreshEquipmentStats();
                app.player_.stats().hp=app.player_.stats().maxHp;
                const int hp=app.player_.stats().hp;
                app.shrineMenu_=true; app.chooseBlessing(0);
                check(app.player_.bloodMagicUnlocked && app.player_.stats().hp<hp && hiddenTreeAvailable(app.player_,"blood_magic"),
                      "An offering of blood opens the Blood Magic tree");
                app.landmark_=LandmarkKind::None; app.monsters_.clear(); app.boss_=nullptr; app.clearSurfaces();
                app.player_.stats().dexterity=2;
                app.currentFloor_=1; app.regenerateLevel(1); app.player_.bloodMagicUnlocked=true;
                roundTrip();
                check(app.player_.bloodMagicUnlocked,"The rite is remembered across saves");
                app.player_.bloodMagicUnlocked=false;
            }
        }
        // Chasms never cut a floor in two.
        {
            int floorsWithChasms=0; bool connected=true;
            for (unsigned seed=900;seed<930;++seed) {
                app.currentFloor_=1+static_cast<int>(seed%9); app.regenerateLevel(seed);
                bool any=false;
                for (int y=0;y<app.map_.height() && !any;++y) for (int x=0;x<app.map_.width();++x) if (app.map_.tileAt(x,y).type==TileType::Chasm) { any=true; break; }
                floorsWithChasms+=any;
                if (app.floorExit_.x>=0) connected=connected && findPath(app.map_,app.floorEntrance_,app.floorExit_).has_value();
            }
            check(floorsWithChasms>5 && connected,"Chasms appear on many floors and never cut off the stairs");
        }

        // Auto-explore still sweeps a dark room by torchlight.
        setup(PlayerClass::Mage); app.darknessEnabled_=true;
        app.player_.setPosition({2,2}); app.updateFieldOfView();
        exploreMoves=0;
        app.startAutoExplore();
        while(exploreMoves<900) {
            if(!app.autoExploring_) { app.startAutoExplore(); if(!app.autoExploring_) break; }
            stepExplore(); ++exploreMoves;
        }
        fullyExplored=true;
        for(int y=1;y<21;++y) for(int x=1;x<31;++x) fullyExplored &= app.exploredMap_.at(x,y)!=Visibility::Hidden;
        check(fullyExplored && exploreMoves<900,"Auto-explore finishes a dark room by torchlight");
        std::cout<<"Explore simulation: "<<exploreMoves<<" moves, "<<restarts<<" restarts.\n";

        setup(PlayerClass::Mage);
        app.map_.setTile(11,10,{TileType::Wall,false,false});
        check(!hasLineOfFire(app.map_,{10,10},{11,11}) && !hasLineOfFire(app.map_,{11,11},{10,10}),
            "Both directions reject the same blocked diagonal corner");
        bool symmetric=true;
        for(int y=8;y<=14;++y) for(int x=8;x<=14;++x)
            for(int yy=8;yy<=14;++yy) for(int xx=8;xx<=14;++xx)
                symmetric &= hasLineOfFire(app.map_,{x,y},{xx,yy})==hasLineOfFire(app.map_,{xx,yy},{x,y});
        check(symmetric,"Shared firing geometry is symmetric over 2401 endpoint pairs");


        setup(PlayerClass::Mage);
        app.player_.setPosition(app.floorExit_); app.updateFieldOfView();
        app.groundItems_.push_back(std::make_unique<Item>(kItemDefinitions[0],app.nextItemId_++,Position{12,10}));
        const auto retainedItem=app.groundItems_[0]->instanceId();
        app.travelFloor(2);
        check(app.currentFloor_==2 && app.floorCache_.count(1),"Descending preserves the previous floor");
        roundTrip();
        check(app.floorCache_.count(1) && app.currentFloor_==2,"Saved campaign restores its cached floor");
        app.travelFloor(1);
        check(app.currentFloor_==1 && app.groundItems_.size()==1 && app.groundItems_[0]->instanceId()==retainedItem &&
            app.player_.position().x==30 && app.player_.position().y==20,
            "Return stairs restore old loot and the matching exit position without respawning");

        setup(PlayerClass::Mage);
        app.player_.stats().hp=50; app.quietTurns_=10;
        app.player_.statusEffects().apply({StatusEffectType::Doom,2,14});
        app.recordQuietTurn();
        check(app.player_.stats().hp==50 && app.quietTurns_==0,"Active curse prevents quiet health regeneration");
        app.player_.statusEffects().active().clear(); app.quietTurns_=9; app.recordQuietTurn();
        check(app.player_.stats().hp==55,"Natural recovery resumes after ten quiet turns");


        // UI acceptance through the same single-event dispatcher used by the window.
        auto click=[&](int x,int y,sf::Mouse::Button button=sf::Mouse::Button::Left) {
            app.handleEvent(sf::Event::MouseButtonPressed{button,{x,y}});
        };
        auto clickOn=[&](const sf::FloatRect& r,sf::Mouse::Button button=sf::Mouse::Button::Left) {
            const auto c=screen::center(r); click(c.x,c.y,button);
        };
        auto release=[&](int x,int y) { app.handleEvent(sf::Event::MouseButtonReleased{sf::Mouse::Button::Left,{x,y}}); };
        auto addItem=[&](const char* id) { app.player_.inventory().add(std::make_unique<Item>(*findItemDefinition(id),app.nextItemId_++)); };
        setup(PlayerClass::Mage);
        app.mode_=GameMode::GameOver; snapshot("ui-game-over.png"); clickOn(screen::kRestart);
        check(app.mode_==GameMode::ClassSelection,"Restart click opens class selection");
        snapshot("ui-class-selection.png"); clickOn(screen::classCard(1));
        check(app.playerClass_==PlayerClass::Mage && app.mode_==GameMode::AbilityChoice && app.player_.treePoints()==1 && app.player_.abilityPoints()==earnedAbilityPoints(1),
            "Class card starts Mage without spending talent points");
        snapshot("ui-talent-start.png"); click(1140,40);
        check(app.mode_==GameMode::AbilityChoice,"Continue cannot bypass the first tree choice");
        // Find the visible Fire row without assuming undiscovered trees are displayed.
        std::size_t fire=0; while(std::string(kTalentTrees[fire].id)!="fire") ++fire;
        const auto fireIcon=app.talentTreeAbilityRect(fire,0);
        click(static_cast<int>(fireIcon.position.x)+10,static_cast<int>(fireIcon.position.y)+10);
        check(app.treeSelection_==fire && app.player_.treePoints()==1,"Selecting a tree spends nothing");
        click(950,584);
        check(app.player_.treePoints()==0 && treeAccess(app.player_,"fire"),"Unlock button spends exactly one tree point");
        click(1140,584);
        check(app.player_.abilityPoints()==earnedAbilityPoints(1)-1 && app.player_.talents().rankOf(talentCatalog()[fire*4].id)==1,"Learn button spends exactly one ability point");
        // More trees than fit: the columns scroll with the wheel and follow the keyboard.
        {
            check(app.treeScrollMax()>0,"With the new trees, the columns overflow and scroll");
            std::size_t acrobatics=0; while(std::string(kTalentTrees[acrobatics].id)!="acrobatics") ++acrobatics;
            std::size_t alchemy=0; while(std::string(kTalentTrees[alchemy].id)!="alchemy") ++alchemy;
            std::size_t oneHanded=0; while(std::string(kTalentTrees[oneHanded].id)!="one_handed") ++oneHanded;
            const float before=app.talentTreeAbilityRect(alchemy,0).position.y;
            check(before+50>app.treeViewBottom_,"Some trees start below the fold");
            app.handleEvent(sf::Event::MouseWheelScrolled{sf::Mouse::Wheel::Vertical,-6.f,{100,300}});
            const auto icon=app.talentTreeAbilityRect(alchemy,0);
            check(app.treeScroll_>0 && app.treeScroll_<=app.treeScrollMax() && icon.position.y<before && icon.position.y+50<=app.treeViewBottom_,
                  "The mouse wheel scrolls the tree columns");
            click(static_cast<int>(icon.position.x)+10,static_cast<int>(icon.position.y)+10);
            check(app.treeSelection_==alchemy,"Clicks land on the scrolled rows");
            snapshot("ui-talent-scroll.png");
            app.handleEvent(sf::Event::MouseWheelScrolled{sf::Mouse::Wheel::Vertical,20.f,{100,300}});
            check(app.treeScroll_==0,"Scrolling stops at the top");
            app.treeSelection_=oneHanded; app.handleTreeKey(sf::Keyboard::Key::Up,false);
            check(app.treeSelection_!=oneHanded && app.treeScroll_>0 && app.talentTreeAbilityRect(app.treeSelection_,0).position.y+50<=app.treeViewBottom_,
                  "Browsing with the keyboard scrolls the selected tree into view");
            app.treeViewBottom_=712; app.treeScroll_=0; app.treeSelection_=fire;
        }
        {
            // Five ranks: rank ups continue past 3 and rank 5 adds the mastery.
            const auto& fireball=*findTalentDefinition("fire.fireball");
            check(fireball.ranks[4].areaRadius==3 && fireball.ranks[3].areaRadius==2 && !fireball.mastery.empty() &&
                  fireball.ranks[4].damagePercent==200 && fireball.ranks[4].manaCost>fireball.ranks[0].manaCost,
                  "Rank 5 Fireball has its mastery, double damage and a higher mana cost");
            const int pointsBefore=app.player_.abilityPoints(), oldRank=app.player_.talents().rankOf(fireball.id);
            app.player_.abilityPoints()=10;
            while (purchaseAbility(app.player_,*findTalentDefinition(talentCatalog()[fire*4].id))) {}
            check(app.player_.talents().rankOf(talentCatalog()[fire*4].id)==kMaxTalentRank,"Abilities rank up to 5");
            for (std::size_t i=0;i<app.player_.talents().knownTalents().size();++i)
                if (app.player_.talents().knownTalents()[i].id==talentCatalog()[fire*4].id) app.player_.talents().setRank(i,1);
            app.player_.abilityPoints()=pointsBefore; (void)oldRank;
            Player probe({0,0},statsForClass(PlayerClass::Mage),TalentSet{});
            probe.abilityPoints()=0; grantXp(probe,xpForNextLevel(1));
            check(probe.level()==2 && probe.abilityPoints()==2,"Even levels grant an extra ability point");
            grantXp(probe,xpForNextLevel(2));
            check(probe.level()==3 && probe.abilityPoints()==3,"Odd levels grant the usual one");
        }
        click(950,628); snapshot("ui-binding.png");
        check(app.bindingTalent_,"Assign button opens the binding picker");
        click(290,440);
        check(!app.bindingTalent_ && app.player_.talents().hotbar()[9]==talentCatalog()[fire*4].id,"Picker binds the selected ability to page two");
        click(1140,40); check(app.mode_==GameMode::Playing,"Continue enters play after valid starting choices");

        setup(PlayerClass::Warrior); app.player_.unspentAttributePoints()=2; app.mode_=GameMode::AttributeAllocation;
        const int oldStrength=app.player_.baseStats().strength;
        snapshot("ui-attributes.png"); clickOn(screen::attributeChoice(0));
        check(app.player_.unspentAttributePoints()==1 && app.player_.baseStats().strength==oldStrength+1 && app.mode_==GameMode::AttributeAllocation,
            "Attribute click spends one point without skipping the remaining choice");
        clickOn(screen::attributeChoice(1));
        check(app.player_.unspentAttributePoints()==0 && app.mode_==GameMode::Playing,"Final attribute click returns to play");

        // Level-ups wait for the player instead of interrupting play.
        setup(PlayerClass::Warrior);
        app.grantXpAndAnnounce(xpForNextLevel(1));
        check(app.player_.level()==2 && app.mode_==GameMode::Playing && app.pointsToSpend(),"Levelling up keeps playing, with points waiting");
        snapshot("ui-levelup-badge.png");
        click(200,50); check(app.mode_==GameMode::AttributeAllocation,"The Level up badge opens the attribute choice");
        const int waiting=app.player_.unspentAttributePoints();
        clickOn(screen::kAttributeClose);
        check(app.mode_==GameMode::Playing && app.player_.unspentAttributePoints()==waiting,"Later closes the choice and keeps the points");
        app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::P});
        check(app.mode_==GameMode::AttributeAllocation,"P opens the waiting points");
        app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::Escape});
        check(app.mode_==GameMode::Playing,"Esc closes them again");

        setup(PlayerClass::Mage); addItem("copper_ring"); addItem("silver_ring");
        app.openInventory(); click(598,138); release(354,360);
        check(app.player_.inventory().equipped(EquipmentSlot::Ring2) && app.inventoryOpen_ && app.player_.position().x==10,
            "Dragging a ring to Ring 2 equips it and keeps the inventory open, without moving the player");
        app.openInventory(); click(598,138,sf::Mouse::Button::Right);
        check(app.player_.inventory().equipped(EquipmentSlot::Ring1) && app.player_.inventory().items().empty(),"Right-click fills the other empty ring slot");
        roundTrip();
        check(app.player_.inventory().equipped(EquipmentSlot::Ring1) && app.player_.inventory().equipped(EquipmentSlot::Ring2),"Save/load preserves both ring slots");
        for(const auto* id:{"chain_coat","iron_helm"}) { addItem(id); app.player_.equip(0); }
        check(app.player_.inventory().armourKind()==ArmourKind::Heavy,"Two heavy pieces tie two empty cloth pieces; body breaks the tie");
        roundTrip();
        check(app.player_.inventory().armourKind()==ArmourKind::Heavy && app.player_.inventory().equipped(EquipmentSlot::Head),"Expanded armour slots survive save/load");
        app.player_.unequip(EquipmentSlot::Head); app.player_.inventory().take(0);
        check(app.player_.inventory().armourKind()==ArmourKind::Cloth,"Three empty armour slots outvote a heavy body piece");
        for(int i=0;i<50;++i) addItem("iron_helm");
        app.openInventory(); snapshot("ui-full-inventory.png");
        app.handleEvent(sf::Event::MouseMoved{{598,138}}); snapshot("ui-item-comparison.png");
        click(182,360,sf::Mouse::Button::Right);
        check(app.player_.inventory().items().size()==50 && app.player_.inventory().equipped(EquipmentSlot::Ring1) && app.inventoryOpen_,"Full bag blocks unequipping without losing an item");
        click(598,138); release(100,256);
        check(app.inventoryOpen_ && app.player_.inventory().items().size()==50 && !app.player_.inventory().equipped(EquipmentSlot::Weapon),"Incompatible drag does not equip or consume an item");
        click(598,138); release(268,116);
        check(app.player_.inventory().items().size()==49 && app.player_.inventory().equipped(EquipmentSlot::Head),"Equipping into an empty slot frees bag space");
        addItem("iron_boots"); app.openInventory(); click(598,138); release(268,116);
        check(app.player_.inventory().items().size()==50,"Swapping equipped gear works with a full bag");
        addItem("iron_boots"); app.openInventory(); click(1060,412);
        check(app.inventoryBagPage_==1,"Inventory next-page button reaches the overflow page");
        app.player_.inventory().take(app.player_.inventory().items().size()-1); app.inventoryBagPage_=0;
        click(1190,35); check(!app.inventoryOpen_ && app.player_.position().x==10,"Inventory close click cannot activate the underlying world");

        app.mode_=GameMode::Town; app.gold_=100; app.selling_=false; app.shopSelection_=0; app.merchantOpen_=false;
        snapshot("ui-town-square.png");
        app.mousePixel_=screen::center(screen::kTownGateSpot); snapshot("ui-town-hover.png"); app.mousePixel_.reset();
        const auto itemIdBefore=app.nextItemId_;
        clickOn(screen::kTownSell);
        check(app.gold_==100 && app.nextItemId_==itemIdBefore && !app.merchantOpen_ && !app.selling_,"Shop buttons are inert until the merchant is visited");
        clickOn(screen::kTownMerchantSpot); check(app.merchantOpen_,"Clicking the merchant's stall opens the merchant");
        clickOn(screen::kTownTrade);
        check(app.gold_==100 && app.nextItemId_==itemIdBefore && app.player_.inventory().items().size()==50,"Full bag purchase spends no gold or item IDs");
        clickOn(screen::kTownSell); const int goldBeforeSale=app.gold_; clickOn(screen::kTownTrade);
        check(app.gold_>goldBeforeSale && app.player_.inventory().items().size()==49,"Sell button trades exactly one bag item");
        clickOn(screen::kTownBuy); const int goldBeforeBuy=app.gold_; clickOn(screen::kTownTrade);
        check(app.gold_==goldBeforeBuy-30 && app.player_.inventory().items().size()==50,"Buy button trades exactly one stock item");
        snapshot("ui-town.png");
        clickOn(screen::kTownLeaveShop); check(!app.merchantOpen_,"Leave returns to the town square");
        clickOn(screen::kTownGateSpot); check(app.dungeonMenu_,"Clicking the gate opens the dungeon choice");
        app.handleTownKey(sf::Keyboard::Key::Escape); check(!app.dungeonMenu_ && app.window_.isOpen(),"Esc leaves the dungeon choice");
        clickOn(screen::kTownReturn); check(app.mode_==GameMode::Playing && app.player_.position().x==10,"Town return button resumes the same floor position");

        // Playtest fixes: a dragged item rides the cursor; alerted enemies are
        // glimpsed only on the turn they're alerted; aiming shows a banner, not a tooltip.
        setup(PlayerClass::Mage); addItem("copper_ring");
        app.openInventory(); click(598,138);
        app.handleEvent(sf::Event::MouseMoved{{420,300}});
        check(app.inventoryDragSource_.has_value(),"Pressing on an item picks it up");
        snapshot("ui-inventory-drag.png");
        release(900,650); app.inventoryOpen_=false;
        {
            auto* lurker=enemy({25,25}); lurker->tactics.alert=8;
            app.updateGlimpses();
            check(app.sensedMonster(*lurker),"A newly alerted enemy is glimpsed through the walls");
            app.updateGlimpses();
            check(!app.sensedMonster(*lurker),"...but only on the turn it is alerted");
            app.monsters_.clear();
            app.player_.talents().learnTalent(findTalentDefinition("fire.ember_bolt")->ranks[0]);
            auto* target=enemy({13,10}); (void)target; app.updateFieldOfView();
            app.requestTalent(app.player_.talents().knownTalents().size()-1);
            check(app.aimingTalent_ && app.aimingSummary().find("Ember Bolt")!=std::string::npos,"Aiming shows a one-line banner");
            snapshot("ui-aiming-banner.png");
            app.cancelTargeting(); app.monsters_.clear();
        }

        // Landmark shrine: kneel beside the altar, pick one blessing, then it's spent.
        setup(PlayerClass::Mage);
        app.landmark_=LandmarkKind::Shrine; app.landmarkAltar_={11,10};
        app.map_.setTile(11,10,Tile{TileType::Wall,false,false}); app.updateFieldOfView();
        app.player_.stats().hp=40; app.gold_=0;
        app.pickupItem();
        check(app.shrineMenu_,"G beside the altar opens the shrine");
        snapshot("ui-shrine.png");
        check(app.landmarkTitle().rfind("Shrine of ",0)==0,"Every shrine belongs to a god");
        for (const auto& [x,y]:std::vector<std::pair<int,int>>{{11,10},{12,10},{13,10},{14,10},{15,10},{16,10},{17,10},{18,10}}) {
            app.landmarkAltar_={x,y};
            if (app.shrineGod()==Patron::Whisperer) break;
        }
        snapshot("ui-shrine-whisperer.png"); app.landmarkAltar_={11,10};
        clickOn(screen::shrineChoice(2));
        check(!app.shrineMenu_ && app.landmarkUsed_ && app.player_.stats().hp==app.player_.stats().maxHp && app.player_.patron==0,
            "Praying for rest heals fully, asks no oath, and spends the shrine");
        app.pickupItem();
        check(!app.shrineMenu_,"A spent shrine stays closed");
        roundTrip();
        check(app.landmark_==LandmarkKind::Shrine && app.landmarkUsed_ && app.landmarkAltar_.x==11,"Save/load keeps the landmark and its state");

        // Patron gods: oaths, favor, boons, prayers and wrath.
        setup(PlayerClass::Warrior);
        {
            app.landmark_=LandmarkKind::Shrine; app.landmarkAltar_={11,10};
            app.map_.setTile(11,10,Tile{TileType::Wall,false,false}); app.updateFieldOfView();
            const Patron god=app.shrineGod();
            app.shrineMenu_=true; app.chooseBlessing(0);
            check(app.patron()==god && app.player_.favor==kSwornFavor && app.player_.talents().rankOf("basic.pray")==1,
                  "Swearing at a shrine makes its god your patron and teaches you to pray");
            std::size_t prayIndex=0;
            for (std::size_t i=0;i<app.player_.talents().knownTalents().size();++i) if (app.player_.talents().knownTalents()[i].id=="basic.pray") prayIndex=i;
            check(!talentUnavailableReason(app.player_,prayIndex).empty(),"A new oath is not yet enough to pray");

            // The Seraph: likes the dark slain, hates forbidden magic.
            app.player_.patron=static_cast<int>(Patron::Seraph); app.player_.favor=10;
            auto skeleton=createMonster(MonsterType::Skeleton,{20,20});
            app.judgeKill(*skeleton);
            check(app.player_.favor==13,"The Seraph rewards slaying undead");
            Talent forbidden=findTalentDefinition("shadow.bolt")->ranks[0];
            app.judgeCast(forbidden);
            check(app.player_.favor==8,"...and frowns on Shadow magic");
            app.gainFavor(Patron::Seraph,30,"test");
            check(app.playerLightRadius()==5,"At 30 favor its boon is yours: your light burns further");
            app.gainFavor(Patron::Seraph,30,"test");
            app.player_.stats().hp=10;
            app.player_.talents().resetCooldowns();
            check(talentUnavailableReason(app.player_,prayIndex).empty() && app.tryUseTalent(prayIndex,app.player_.position()) &&
                  app.player_.stats().hp>10 && app.player_.statusEffects().has(StatusEffectType::Guard) && app.player_.favor==28,
                  "At 60 favor you can pray: Wings of Mercy heals and guards, for 40 favor");
            app.player_.favor=-15; app.player_.stats().hp=app.player_.stats().maxHp;
            app.gainFavor(Patron::Seraph,-5,"test");
            check(app.player_.statusEffects().has(StatusEffectType::Smothered) && app.player_.favor==0,"Fall to -20 and its wrath falls on you");
            app.player_.statusEffects().active().clear();

            // Forsaking a god brings its wrath; the Ash Saint walks through fire.
            app.player_.favor=10; app.swearTo(Patron::AshSaint);
            check(app.patron()==Patron::AshSaint && app.player_.statusEffects().has(StatusEffectType::Smothered),"Forsaking the Seraph brings its wrath");
            app.player_.statusEffects().active().clear();
            app.player_.favor=35;
            app.setSurface(app.player_.position(),SurfaceType::Fire,4); app.tickSurfaces();
            check(!app.player_.statusEffects().has(StatusEffectType::Burn),"The Ash Saint's faithful walk through fire unburnt");
            app.clearSurfaces();

            // The Whisperer's prayer blinds everything near.
            app.player_.patron=static_cast<int>(Patron::Whisperer); app.player_.favor=70;
            auto* goblin=enemy({14,10});
            app.player_.talents().resetCooldowns(); app.tryUseTalent(prayIndex,app.player_.position());
            check(app.player_.statusEffects().has(StatusEffectType::Concealed) && goblin->statusEffects().has(StatusEffectType::Blinded),
                  "Unseeing: you vanish and everything near you is blinded");
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.landmark_=LandmarkKind::None; app.player_.statusEffects().active().clear();
            app.player_.patron=static_cast<int>(Patron::Sleeper); app.player_.favor=42;
            roundTrip();
            check(app.patron()==Patron::Sleeper && app.player_.favor==42 && app.player_.talents().rankOf("basic.pray")==1,"Your patron and favor are saved");
        }

        // The other landmarks: one offer each, all spent by using them.
        setup(PlayerClass::Mage);
        app.landmarkAltar_={11,10}; app.map_.setTile(11,10,Tile{TileType::Wall,false,false}); app.updateFieldOfView();
        app.landmark_=LandmarkKind::HealingFountain; app.player_.stats().hp=30;
        app.player_.statusEffects().apply({StatusEffectType::Poison,5,2});
        app.pickupItem(); snapshot("ui-fountain.png"); clickOn(screen::shrineChoice(1));
        check(app.landmarkUsed_ && app.player_.stats().hp==app.player_.stats().maxHp && !app.player_.statusEffects().has(StatusEffectType::Poison),
            "The healing fountain restores life and washes away poison");
        app.landmark_=LandmarkKind::BloodFont; app.landmarkUsed_=false;
        const int maxBefore=app.player_.baseStats().maxHp;
        app.pickupItem(); app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::Enter});
        check(app.landmarkUsed_ && app.player_.baseStats().maxHp==maxBefore+4 && app.player_.stats().hp<app.player_.stats().maxHp,
            "The blood font trades current life for permanent maximum life");
        app.landmark_=LandmarkKind::RitualCircle; app.landmarkUsed_=false;
        app.pickupItem(); clickOn(screen::shrineChoice(1));
        check(app.landmarkUsed_ && app.player_.unspentAttributePoints()==1 && app.player_.statusEffects().has(StatusEffectType::Doom) &&
            app.mode_==GameMode::AttributeAllocation,"The ritual circle grants an attribute point and lays a Doom curse");

        // Treasure hoard: a handful is free; seizing it all pays more and wakes guardians.
        const auto placeAltar=[&](LandmarkKind kind) {
            setup(PlayerClass::Mage);
            app.landmarkAltar_={11,10}; app.map_.setTile(11,10,Tile{TileType::Wall,false,true}); app.updateFieldOfView();
            app.landmark_=kind;
        };
        placeAltar(LandmarkKind::TreasureHoard); app.gold_=0;
        snapshot("ui-hoard-map.png");
        app.pickupItem(); snapshot("ui-hoard.png");
        clickOn(screen::shrineChoice(0));
        check(app.landmarkUsed_ && app.gold_==13 && app.monsters_.empty(),"A handful of the hoard is free and wakes nothing");
        app.landmarkUsed_=false; app.gold_=0;
        auto bagBefore=app.player_.inventory().items().size();
        app.pickupItem(); clickOn(screen::shrineChoice(2));
        check(app.landmarkUsed_ && app.gold_==52 && app.player_.inventory().items().size()==bagBefore+1 && app.monsters_.size()==3 &&
              app.monsters_.front()->tier()==MonsterTier::Elite &&
              std::all_of(app.monsters_.begin(),app.monsters_.end(),[](const auto& m){return m->tactics.alert>0;}),
              "Seizing the hoard pays out a rare item and wakes three hunting guardians");
        snapshot("ui-hoard-guardians.png");

        // Prisoner's cage: picking the lock needs Dexterity; breaking it is loud.
        placeAltar(LandmarkKind::PrisonerCage);
        app.player_.baseStats().dexterity=app.player_.stats().dexterity=0;
        auto* sleeper=enemy({20,10}); auto* distant=enemy({30,20});
        snapshot("ui-cage-map.png");
        app.pickupItem(); snapshot("ui-cage.png");
        clickOn(screen::shrineChoice(2));
        check(app.shrineMenu_ && !app.landmarkUsed_,"Picking the lock is refused without the Dexterity");
        bagBefore=app.player_.inventory().items().size();
        clickOn(screen::shrineChoice(0));
        check(app.landmarkUsed_ && app.player_.inventory().items().size()==bagBefore+1 &&
              app.player_.inventory().items().back()->rarity()==ItemRarity::Rare,"Breaking the lock frees the prisoner for a rare item");
        check(sleeper->tactics.alert>0 && distant->tactics.alert==0,"The clang wakes enemies within range, not beyond it");

        // Champion's pit: stoking the brazier summons two nightmare champions.
        placeAltar(LandmarkKind::ChampionPit);
        snapshot("ui-pit-map.png");
        app.pickupItem(); clickOn(screen::shrineChoice(2));
        check(app.landmarkUsed_ && app.monsters_.size()==2 &&
              std::all_of(app.monsters_.begin(),app.monsters_.end(),[](const auto& m){return m->tier()==MonsterTier::Nightmare && m->tactics.alert>0;}),
              "Stoking the brazier brings two hunting nightmare champions");
        snapshot("ui-pit-champions.png");

        // Ascendancy: the Warlord's sigil opens the first trial at the town obelisk.
        setup(PlayerClass::Warrior);
        {
            auto warlord=createMonster(MonsterType::GoblinWarlord,{12,10}); auto* w=warlord.get();
            app.monsters_.push_back(std::move(warlord)); app.boss_=w; app.scheduler_.add(*w);
            w->stats().hp=0; app.checkAndHandleDeath(*w); app.removeDeadMonsters();
        }
        check(app.player_.trialKeys==1 && app.player_.trialsCleared==0,"The Goblin Warlord drops the Stone Sigil");
        app.mode_=GameMode::Town; app.merchantOpen_=false;
        snapshot("ui-town-obelisk.png");
        clickOn(screen::kTownObeliskSpot); check(app.trialMenu_,"The obelisk opens the trials");
        snapshot("ui-trials.png");
        const auto dungeonPos=app.player_.position(); const int dungeonFloor=app.currentFloor_;
        clickOn(screen::trialEnter(1)); check(app.trial_==0,"The second trial stays sealed without its sigil");
        clickOn(screen::trialEnter(0));
        check(app.trial_==1 && app.mode_==GameMode::Playing && app.boss_ && app.boss_->eventChampion==kChampionStoneWarden,
              "Entering the trial meets the Stone Warden");
        snapshot("ui-trial-arena.png");
        app.player_.setPosition(app.floorEntrance_); app.interactStairs();
        check(app.trial_==1 && app.mode_==GameMode::Playing,"There is no leaving the trial while its guardian lives");
        roundTrip();
        check(app.trial_==1 && app.boss_ && app.boss_->name()=="The Stone Warden","Save/load keeps the trial and its guardian");
        app.boss_->setPosition({app.player_.position().x,app.player_.position().y-3}); app.updateFieldOfView();
        snapshot("ui-trial-warden.png");
        auto* warden=app.boss_; warden->stats().hp=0; app.checkAndHandleDeath(*warden); app.removeDeadMonsters();
        check(app.player_.trialsCleared==1 && app.player_.ascendancy.empty() && app.player_.ascendancyPoints==1 && app.ascendancyChoice_,
              "Winning the trial grants a point and asks which ascendancy to take");
        check(ascendanciesFor(PlayerClass::Warrior).size()==4 && ascendanciesFor(PlayerClass::Mage).size()==4 && ascendanciesFor(PlayerClass::Thief).size()==4,
              "Every class chooses among four ascendancies");
        snapshot("ui-ascendancy-choice.png");
        app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::Escape});
        check(app.ascendancyChoice_ && app.player_.ascendancy.empty(),"The choice can't be skipped");
        clickOn(screen::ascendChoice(0)); clickOn(screen::kAscendLearn);
        check(app.player_.ascendancy=="juggernaut" && !app.ascendancyChoice_ && app.ascendancyMenu_,"Choosing an ascendancy opens its nodes");
        snapshot("ui-ascendancy.png");
        const int lifeBeforeSkin=app.player_.stats().maxHp;
        clickOn(screen::ascendNode(4)); clickOn(screen::kAscendLearn);
        check(app.player_.talents().rankOf("juggernaut.iron_skin")==1 && app.player_.ascendancyPoints==0 &&
              app.player_.stats().maxHp==lifeBeforeSkin+lifeBeforeSkin*15/100,
              "Learning Iron Skin spends the point and raises maximum life by 15%");
        clickOn(screen::ascendNode(1)); clickOn(screen::kAscendLearn);
        check(!app.player_.talents().rankOf("juggernaut.earthshaker"),"A second node needs a second point");
        clickOn(screen::kAscendClose); check(!app.ascendancyMenu_,"Close leaves the ascendancy screen");
        roundTrip();
        check(app.player_.talents().rankOf("juggernaut.iron_skin")==1 && app.player_.stats().maxHp==lifeBeforeSkin+lifeBeforeSkin*15/100 &&
              app.player_.ascendancy=="juggernaut",
              "Save/load keeps the ascendancy and its nodes");
        app.player_.setPosition(app.floorEntrance_); app.interactStairs();
        check(app.trial_==0 && app.mode_==GameMode::Town && app.currentFloor_==dungeonFloor &&
              app.player_.position().x==dungeonPos.x && app.player_.position().y==dungeonPos.y,
              "Leaving the won trial returns to town with the dungeon floor as it was");
        app.mode_=GameMode::Playing; app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::Y});
        check(app.ascendancyMenu_,"Y opens the ascendancy screen"); app.ascendancyMenu_=false;
        // The second trial: the Lich's sigil, after the first is won.
        app.player_.trialKeys|=2; app.mode_=GameMode::Town;
        check(app.enterTrial(2) && app.boss_ && app.boss_->eventChampion==kChampionFallenSaint && app.boss_->type()==MonsterType::Lich,
              "The Bone Sigil opens the Trial of the Fallen and its saint");
        app.boss_->setPosition({app.player_.position().x,app.player_.position().y-3}); app.updateFieldOfView();
        snapshot("ui-trial-saint.png");
        app.boss_->stats().hp=0; app.checkAndHandleDeath(*app.boss_); app.removeDeadMonsters();
        check(app.player_.trialsCleared==3 && app.player_.ascendancyPoints==1,"The second trial grants the second point");
        clickOn(screen::ascendNode(1)); clickOn(screen::kAscendLearn);
        check(app.player_.talents().rankOf("juggernaut.earthshaker")==1 && app.player_.ascendancyPoints==0,"The second point buys a second node");
        app.ascendancyMenu_=false; roundTrip();
        check(app.player_.trialsCleared==3 && app.player_.talents().rankOf("juggernaut.earthshaker")==1,"Both trials and nodes survive save/load");
        app.player_.setPosition(app.floorEntrance_); app.interactStairs();
        check(app.trial_==0 && app.mode_==GameMode::Town,"The second trial also returns to town");

        // Ascendancy passives feed the damage formula.
        setup(PlayerClass::Thief);
        {
            auto* target=enemy({11,10});
            const auto before=estimateTalentDamage(basicAttack(),app.player_,*target).normal;
            target->stats().hp=target->stats().maxHp/3;
            app.player_.talents().learnTalent(findTalentDefinition("trickster.killer_instinct")->ranks[0]);
            check(estimateTalentDamage(basicAttack(),app.player_,*target).normal==before+4,"Killer Instinct adds damage against wounded enemies");
        }

        // Combat effects: worn statuses, telegraphs, and spells caught mid-flight.
        setup(PlayerClass::Mage);
        {
            auto* burning=enemy({14,10}); burning->statusEffects().apply({StatusEffectType::Burn,3,2});
            auto* shocked=enemy({12,13}); shocked->statusEffects().apply({StatusEffectType::Shock,3,0}); shocked->statusEffects().apply({StatusEffectType::Stun,1,0});
            auto* chilled=enemy({7,9}); chilled->statusEffects().apply({StatusEffectType::Chill,3,20}); chilled->statusEffects().apply({StatusEffectType::Marked,3,1});
            auto* poisoned=enemy({8,12}); poisoned->statusEffects().apply({StatusEffectType::Poison,3,1});
            auto* blaster=enemy({16,8});
            blaster->intent()=EnemyIntent{{16,8},{15,10},1,1,5,IntentKind::MagicStrike};
            app.player_.statusEffects().apply({StatusEffectType::Guard,2,3});
            app.updateFieldOfView(); app.updateCamera();
            const float now=app.animNow();
            app.spawnVfx({Application::Vfx::Kind::Lightning,{10.5f,10.5f},{14.5f,10.5f},sf::Color(175,205,255),0,.4f,1.f});
            app.spawnVfx({Application::Vfx::Kind::Bolt,{10.5f,10.5f},{7.5f,9.5f},sf::Color(255,140,50),0,.3f});
            app.spawnVfx({Application::Vfx::Kind::Burst,{12.5f,13.5f},{12.5f,13.5f},sf::Color(255,140,50),0,.5f,2.f});
            app.spawnVfx({Application::Vfx::Kind::Slash,{10.5f,10.5f},{8.5f,12.5f},sf::Color(255,220,200),0,.4f});
            app.animTimeOffset_=0; (void)now;
            app.animTimeOffset_+=.12f; snapshot("ui-vfx.png");
            app.player_.statusEffects().apply({StatusEffectType::Concealed,3,1});
            app.spawnVfx({Application::Vfx::Kind::Smoke,{10.5f,10.5f},{10.5f,10.5f},sf::Color(120,120,140),0,.9f,.9f});
            app.animTimeOffset_+=.25f; snapshot("ui-vfx-conceal.png");
            app.animTimeOffset_=0;
            check(!app.vfx_.empty(),"Spells leave visible effects while they play");
        }

        // Hybrid ascendancy nodes.
        setup(PlayerClass::Warrior);
        {
            const int strBefore=app.player_.stats().strength;
            app.player_.talents().learnTalent(findTalentDefinition("paragon.balance")->ranks[0]); app.player_.refreshEquipmentStats();
            check(app.player_.stats().strength==strBefore+2,"Paragon's Balance raises every attribute");
            auto* foe=enemy({11,10});
            foe->stats().hp=foe->stats().maxHp/5;
            const int plain=estimateTalentDamage(basicAttack(),app.player_,*foe).normal;
            app.player_.talents().learnTalent(findTalentDefinition("duelist.finisher")->ranks[0]);
            check(estimateTalentDamage(basicAttack(),app.player_,*foe).normal==plain+6,"Duelist's Finisher hits wounded foes harder");
            app.player_.talents().learnTalent(findTalentDefinition("duelist.counter")->ranks[0]);
            app.player_.statusEffects().apply({StatusEffectType::Evasion,100,60});
            foe->stats().hp=foe->stats().maxHp; const int foeBefore=foe->stats().hp;
            AIDecision swing; swing.type=AIActionType::Attack; swing.target=&app.player_; swing.attackPower=1;
            for (int i=0;i<30 && foe->stats().hp==foeBefore;++i) app.executeAIDecision(*foe,swing,0);
            check(foe->stats().hp<foeBefore,"Duelist's Counter strikes back on a dodge");
            app.player_.talents().learnTalent(findTalentDefinition("templar.zeal")->ranks[0]);
            app.player_.statusEffects().active().clear();
            Talent spell=findTalentDefinition("arcane.bolt")->ranks[0];
            app.afterHiddenCast(spell,true,false,0);
            check(app.player_.statusEffects().has(StatusEffectType::Guard),"Templar's Zeal guards you after a spell");
        }

        // Very rare events: each leads to a unique item.
        const auto ordinaryBases=rewardItemDefinitions();
        check(uniqueItemDefinitions().size()==14 && std::none_of(ordinaryBases.begin(),ordinaryBases.end(),
              [](const auto* d){return d->unique;}),"Uniques never appear in ordinary loot or the merchant's stock");
        placeAltar(LandmarkKind::SealedTomb); app.currentFloor_=8;
        snapshot("ui-tomb-map.png");
        app.pickupItem(); snapshot("ui-tomb.png");
        app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::Enter});
        Monster* king=nullptr;
        for (auto& m:app.monsters_) if (m->eventChampion==kChampionRevenant) king=m.get();
        check(app.landmarkUsed_ && app.monsters_.size()==3 && king && king->tier()==MonsterTier::Nightmare && king->name()=="The Risen King",
              "Breaking the tomb's seal raises the Risen King and two guards");
        snapshot("ui-tomb-king.png");
        roundTrip();
        king=nullptr;
        for (auto& m:app.monsters_) if (m->eventChampion==kChampionRevenant) king=m.get();
        check(king && king->name()=="The Risen King","Save/load keeps the event champion and its name");
        const auto groundBefore=app.groundItems_.size();
        king->stats().hp=0; app.checkAndHandleDeath(*king);
        check(std::any_of(app.groundItems_.begin()+static_cast<std::ptrdiff_t>(groundBefore),app.groundItems_.end(),
              [](const auto& item){return item->rarity()==ItemRarity::Unique;}),"The Risen King drops a unique item");

        placeAltar(LandmarkKind::PalePeddler); app.gold_=0;
        snapshot("ui-peddler-map.png"); app.pickupItem(); snapshot("ui-peddler.png");
        clickOn(screen::shrineChoice(0));
        check(!app.landmarkUsed_,"The peddler refuses a buyer without the gold");
        const int lifeBefore=app.player_.baseStats().maxHp;
        clickOn(screen::shrineChoice(2));
        check(app.landmarkUsed_ && app.player_.baseStats().maxHp==lifeBefore-10 &&
              app.player_.inventory().items().back()->rarity()==ItemRarity::Unique,"Paying in blood buys a unique for ten maximum life");

        placeAltar(LandmarkKind::ChainedDemon);
        snapshot("ui-demon-map.png");
        app.pickupItem(); clickOn(screen::shrineChoice(0));
        check(app.landmarkUsed_ && app.player_.inventory().items().back()->rarity()==ItemRarity::Unique &&
              app.player_.statusEffects().has(StatusEffectType::Doom) && app.player_.statusEffects().has(StatusEffectType::ManaDrain),
              "The demon's bargain gives a unique and two curses");
        placeAltar(LandmarkKind::ChainedDemon);
        app.pickupItem(); clickOn(screen::shrineChoice(2));
        check(app.monsters_.size()==1 && app.monsters_[0]->eventChampion==kChampionDemon,"Slaying the demon makes it fight as a champion");
        snapshot("ui-demon-free.png");
        app.grantUnique(std::nullopt);
        app.openInventory(); app.inventorySelection_=kEquipmentSlotCount; snapshot("ui-unique-tooltip.png"); app.inventoryOpen_=false;

        setup(PlayerClass::Mage);
        app.vaultExists_=true; app.vaultEntrance_={11,10}; app.vaultMenu_=1;
        snapshot("ui-vault-warning.png"); clickOn(screen::vaultCancel(1));
        check(!app.vaultMenu_ && !app.vaultOpened_ && app.player_.position().x==10,"Leave sealed closes without opening or moving");
        app.vaultMenu_=1; clickOn(screen::vaultCommit(1));
        check(app.vaultOpened_ && !app.vaultMenu_,"Open vault button commits the opening");
        app.vaultRewards_.clear();
        for(const auto* id:{"iron_helm","silver_ring","leather_boots"}) app.vaultRewards_.push_back(std::make_unique<Item>(*findItemDefinition(id),app.nextItemId_++));
        app.vaultMenu_=2; app.vaultSelection_=0; clickOn(screen::vaultRewardCard(1));
        check(app.vaultSelection_==1 && app.player_.inventory().items().empty(),"Clicking a vault reward only selects it");
        snapshot("ui-vault-rewards.png");
        for(int i=0;i<50;++i) addItem("copper_ring");
        clickOn(screen::vaultCommit(2));
        check(!app.vaultClaimed_ && app.vaultRewards_.size()==3 && app.vaultMenu_==2,"Full bag keeps all unclaimed vault choices");
        app.player_.inventory().take(0); clickOn(screen::vaultCommit(2));
        check(app.vaultClaimed_ && app.vaultRewards_.empty() && app.player_.inventory().items().size()==50 && !app.vaultMenu_,"Claim transfers exactly the selected reward after space is available");

        setup(PlayerClass::Mage); app.exitMenu_=true; snapshot("ui-travel.png"); clickOn(screen::kTravelStay);
        check(!app.exitMenu_ && app.player_.position().x==10,"Stay closes descent without movement");
        app.exitMenu_=true; app.quietTurns_=0; clickOn(screen::kTravelTown);
        check(app.mode_==GameMode::Playing,"Town travel button respects the ten quiet turns requirement");
        app.exitMenu_=true; app.quietTurns_=10; clickOn(screen::kTravelTown);
        check(app.mode_==GameMode::Town,"Town travel button succeeds after ten quiet turns");

        setup(PlayerClass::Mage); app.player_.statusEffects().apply({StatusEffectType::Poison,3,1});
        click(30,638); check(app.inventoryOpen_ && app.player_.stats().hp==100,"Action-bar Bag is free");
        click(1190,35); click(147,638);
        check(app.player_.stats().hp==99 && app.player_.statusEffects().active()[0].turnsRemaining==2,"Action-bar Wait advances exactly one turn");
        app.player_.statusEffects().active().clear(); click(186,638);
        check(app.restTurns_>0,"Action-bar Rest starts resting");
        click(30,638); check(app.restTurns_==0 && !app.inventoryOpen_,"First click during rest only stops resting");
        app.requestTalent(1); click(1068,517);
        check(!app.aimingTalent_ && !app.inspecting_,"Action-bar Cancel leaves targeting without casting");
        snapshot("ui-dungeon.png");


        setup(PlayerClass::Mage); app.mode_=GameMode::Town;
        clickOn(screen::kTownDungeons);
        check(app.dungeonMenu_,"Dungeon chooser opens");
        clickOn(screen::dungeonCard(1)); clickOn(screen::depthCard(2)); // Deep Crypts, depth 2
        check(app.dungeonSelection_==1 && app.dungeonDepth_==2 && app.currentFloor_==1,"Dungeon and depth choices only preview the destination");
        snapshot("ui-dungeon-selection.png"); clickOn(screen::kDungeonBack);
        check(!app.dungeonMenu_,"Cancelling dungeon selection spends nothing");
        const auto raiseLevel=[&](int level) {
            app.player_.level()=level; app.player_.abilityPoints()=earnedAbilityPoints(level)-3;
            app.player_.treePoints()=earnedTreePoints(level)-1;
        };
        raiseLevel(12); app.player_.stats().hp=60; app.player_.stats().mana=40;
        app.groundItems_.push_back(std::make_unique<Item>(kItemDefinitions[0],app.nextItemId_++,Position{11,10}));
        const auto homeItem=app.groundItems_[0]->instanceId();
        clickOn(screen::kTownDungeons); clickOn(screen::dungeonCard(1)); clickOn(screen::depthCard(2)); clickOn(screen::kDungeonEnter);
        check(app.currentFloor_==12 && app.mode_==GameMode::Playing && !app.dungeonMenu_,"Crypts entry opens the selected depth");
        check(app.player_.stats().hp==60 && app.player_.stats().mana==40,"Choosing a dungeon neither heals nor spends a turn");
        check(!app.monsters_.empty(),"Selected depth has generated encounters");
        const int originalHp=app.monsters_[0]->stats().maxHp, originalStrength=app.monsters_[0]->stats().strength, originalXp=app.monsters_[0]->xpReward();
        auto baselineEnemy=createMonster(app.monsters_[0]->type(),app.monsters_[0]->position(),app.monsters_[0]->tier());
        app.scaleDeepMonster(*baselineEnemy,12);
        check(originalHp>baselineEnemy->stats().maxHp && originalXp>baselineEnemy->xpReward(),"Depth scaling increases enemy health and XP above base Crypts stats");
        app.monsters_[0]->stats().hp=originalHp-1;
        const auto population=app.monsters_.size();
        const auto lootState=app.loot_.state();
        app.mode_=GameMode::Town; app.travelFloor(1,true);
        check(app.currentFloor_==1 && app.groundItems_.size()==1 && app.groundItems_[0]->instanceId()==homeItem,"Switching dungeons preserves existing ground loot");
        raiseLevel(18); app.mode_=GameMode::Town; app.travelFloor(12,true);
        check(app.monsters_.size()==population && app.monsters_[0]->stats().hp==originalHp-1,
            "Revisiting after level-ups preserves damaged enemy population");
        check(app.monsters_[0]->stats().strength==originalStrength && app.monsters_[0]->xpReward()==originalXp && app.loot_.state()==lootState,
            "Revisiting does not rescale enemies or reroll loot");
        roundTrip();
        check(app.monsters_[0]->stats().maxHp==originalHp && app.monsters_[0]->stats().strength==originalStrength && app.monsters_[0]->xpReward()==originalXp,
            "Version 21 restores depth-scaled enemy HP, attack attributes and XP without double scaling");
        app.mode_=GameMode::Town; app.travelFloor(13,true);
        check(app.currentFloor_==13,"New depths remain accessible after a level-up");
        const auto at=app.player_.position(); app.mode_=GameMode::Town; app.travelFloor(13,true);
        check(app.mode_==GameMode::Playing && app.player_.position().x==at.x && app.player_.position().y==at.y,"Choosing the current depth resumes the exact location");
        auto invalidLock=app.captureState(false); invalidLock.dungeonLevels[1]=21;
        check(!saveGame(invalidLock,(output/"invalid-lock.txt").string()),"Save validation rejects out-of-band dungeon locks");
        setup(PlayerClass::Mage); app.mode_=GameMode::Town; app.travelFloor(20,true);
        check(app.currentFloor_==20 && app.boss_,"Underleveled entry and deepest choice are allowed");
        auto oldState=app.captureState(false); const auto oldPath=output/"legacy18.txt";
        // A real version-18 save can't hold monster kinds added in version 22.
        const auto version18Monsters=[](auto& monsters) {
            monsters.erase(std::remove_if(monsters.begin(),monsters.end(),[](const auto& m){
                return static_cast<int>(m.type)>static_cast<int>(MonsterType::OssuaryWarden); }),monsters.end());
        };
        version18Monsters(oldState.monsters);
        for(auto& floor:oldState.savedFloors) version18Monsters(floor.monsters);
        check(saveGameAsVersion(oldState,oldPath.string(),18),"Write migration fixture for a pre-lock save");
        const auto oldLoaded=loadGame(oldPath.string());
        check(oldLoaded && oldLoaded->dungeonLevels[1]==0 && app.restoreState(*oldLoaded),"Version 18 migrates to depth-based difficulty");

        bool packsSafe=true, varied=false, rares=false, named=false;
        for(int floor=1;floor<=20;++floor) for(unsigned seed=1;seed<=25;++seed) {
            DungeonGenerationParams params;
            params.includeBossRoom=(floor==5 || floor==10 || floor==20); params.includeVault=floor>=3 && !params.includeBossRoom;
            auto dungeon=generateDungeon(params,seed); const auto spawns=planEncounters(dungeon,floor);
            std::set<std::pair<int,int>> occupied;
            int rareCount=0,uniqueCount=0;
            for(const auto& spawn:spawns) {
                const int dx=spawn.position.x-dungeon.playerStart.x,dy=spawn.position.y-dungeon.playerStart.y;
                packsSafe &= dungeon.map.isWalkable(spawn.position.x,spawn.position.y) && dx*dx+dy*dy>64 &&
                    occupied.emplace(spawn.position.x,spawn.position.y).second;
                rareCount+=spawn.tier==MonsterTier::Nightmare; uniqueCount+=isUniqueMonster(spawn.type);
                varied |= spawn.type==MonsterType::Bonecaller;
            }
            packsSafe &= rareCount<=1 && uniqueCount<=1;
            if(!spawns.empty()) packsSafe &= spawns[0].tier==MonsterTier::Base && !isUniqueMonster(spawns[0].type);
            rares |= rareCount>0; named |= uniqueCount>0;
        }
        check(packsSafe && varied && rares && named,"500 generated floors: safe unique positions, opening packs, rare caps and new roster");

        // Real generated campaigns: valid saves across every floor and every class.
        bool generatedSaves=true;
        for(auto cls:{PlayerClass::Warrior,PlayerClass::Thief,PlayerClass::Mage}) {
            app.selectClass(cls);
            for(int floor=1;floor<=20;++floor) {
                app.currentFloor_=floor; app.regenerateLevel(400+floor);
                const auto state=app.captureState(false); const auto path=(output/"generated.txt").string();
                generatedSaves &= saveGame(state,path) && loadGame(path).has_value();
            }
        }
        check(generatedSaves,"60 real generated class/floor saves parse successfully");

        app.window_.close();
        std::cout << checks << " reward checks, " << failures << " failures.\n";
        return failures ? 1 : 0;
    }
};
}
int main() { return engine::ApplicationRewardsTestAccess::run(); }
