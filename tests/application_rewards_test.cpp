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
#include "core/Keywords.hpp"
#include "entities/PlayerLeveling.hpp"
#include "entities/Ascendancy.hpp"
#include "entities/Lore.hpp"
#include "entities/RunProgression.hpp"
#include "world/EncounterPlan.hpp"
#include "world/LineOfFire.hpp"
#include "world/Pathfinder.hpp"
#include "core/Application.hpp"
#include "world/FloorTheme.hpp"
#include "core/ScreenLayout.hpp"
#include "entities/MonsterFactory.hpp"
#include "entities/PlayerClassFactory.hpp"
#include "entities/StatusEffectLogic.hpp"
#include "entities/ArmourTalents.hpp"
#include "entities/LootGenerator.hpp"
#include "world/DungeonGenerator.hpp"
#include <tuple>
#include "core/PlayLayout.hpp"
#include "entities/TalentEffects.hpp"

namespace engine {
// Colour points for a test: ranks spread across that colour's base-tree
// nodes, then its hybrids' if it has no base tree (Death). Never a deep
// tree's, so the deep tree under test stays unlearned.
void giveColour(Player& p, Affinity colour, int points) {
    for (const bool hybrids : {false, true})
        for (const auto& d : talentCatalog()) {
            if (points <= 0) return;
            if (d.affinity != colour || d.treeId == "resonance" || deepGate(d.treeId) || isAscendancyTree(d.treeId) ||
                static_cast<bool>(hybridGate(d.treeId)) != hybrids || p.talents().rankOf(d.id)) continue;
            const int rank = std::min(points, d.maxRank());
            p.talents().learnTalent(d.ranks[0]);
            p.talents().setRank(p.talents().knownTalents().size() - 1, rank);
            points -= rank;
        }
}

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
            app.landmark_=LandmarkKind::None; app.landmarkUsed_=false; app.shrineMenu_=false; app.extraLandmarks_.clear(); app.decals_.clear(); app.floorTurns_=0; app.breachTurns_=0; app.breachKills_=0;
            app.player_.patron=app.player_.favor=0; app.player_.bloodMagicUnlocked=false; app.pendingFall_=false;
            app.player_.ascendancy.clear(); app.player_.ascendancyPoints=app.player_.trialKeys=app.player_.trialsCleared=0;
            app.lightOrbs_.clear(); app.loreDrops_.clear(); app.banner_.reset(); app.player_.lore.clear();
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
            app.player_.trees()={{tree,false}}; app.player_.treePoints()=0; app.player_.abilityPoints()=earnedAbilityPoints(1)-3; app.player_.utilityPoints()=earnedUtilityPoints(1);
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
            std::uint64_t stats = 0;
            int prefixes = 0, suffixes = 0, cursed = 0;
            for (const auto& rolled : first->affixes()) {
                const auto* definition = findAffix(rolled.id);
                (definition->prefix ? prefixes : suffixes)++; cursed += definition->cursed;
                const auto mask = std::uint64_t{1} << static_cast<unsigned>(definition->stat);
                valid &= !(stats & mask) && (definition->slots & (1u << static_cast<unsigned>(first->definition()->slot)));
                valid &= rolled.value >= definition->minimum + first->rollTier()*definition->perTier &&
                         rolled.value <= definition->maximum + first->rollTier()*definition->perTier;
                stats |= mask;
            }
            const int cap = first->rarity()==ItemRarity::Rare ? 3 : 1;
            valid &= prefixes <= cap && suffixes <= cap && first->affixes().size() <= static_cast<std::size_t>(kMaxAffixes);
            valid &= first->definition()->depth <= std::max(1, 1+i%10) && first->definition()->depth > 0;
            valid &= cursed == 0 || (first->rarity()==ItemRarity::Rare && cursed == 1);
        }
        check(sameRolls, "Same seed reproduces 500 complete item rolls and RNG states");
        check(valid && rarities[0] && rarities[1] && rarities[2], "All rarities occur with compatible, distinct, bounded affixes");
        {
            LootGenerator shallow(4242);
            int normals = 0, magics = 0, rares = 0; bool firstBasesOnly = true;
            for (int i = 0; i < 2000; ++i) {
                const auto item = shallow.generate(1, 0, i + 1, {});
                firstBasesOnly &= item->definition()->depth == 1;
                const auto r = item->rarity();
                normals += r == ItemRarity::Normal; magics += r == ItemRarity::Magic; rares += r == ItemRarity::Rare;
            }
            std::cout << "Floor-1 finds: " << normals << " plain, " << magics << " magic, " << rares << " rare\n";
            check(firstBasesOnly, "Floor one only ever drops the first bases");
            check(normals > magics && magics > rares * 4 && rares > 0, "Most finds are plain; rares are rare");
            LootGenerator deep(77);
            bool deepest = false; std::size_t longest = 0;
            for (int i = 0; i < 2000; ++i) {
                const auto item = deep.generate(13, 2, i + 1, {}, ItemRarity::Rare);
                deepest |= item->definition()->depth == 13;
                longest = std::max(longest, item->affixes().size());
            }
            check(deepest && longest >= 5, "Deep floors bring the best bases, and rares with five or six affixes");
        }

        setup(PlayerClass::Mage);
        app.chestExists_ = true; app.chestMimic_ = false; app.chestPosition_ = {11,10};
        app.player_.talents().setCooldownRemaining(1,4);
        app.player_.statusEffects().apply({StatusEffectType::Poison,3,1});
        check(app.isOccupied({11,10},nullptr),"A chest stands in the way of everyone");
        app.tryMovePlayer(1,0);
        check(app.chestClaimed_ && app.player_.position().x==10 && app.groundItems_.size()==1 && app.player_.inventory().items().empty(),
              "Walking into a chest opens it, and its loot spills onto the floor");
        const auto spill=app.groundItems_[0]->position();
        check(std::max(std::abs(spill.x-11),std::abs(spill.y-10))==1 && !(spill.x==11 && spill.y==10),"The loot lands in the 8 tiles around the chest");
        check(app.groundItems_[0]->rarity()!=ItemRarity::Normal &&
              app.player_.stats().hp==99 && app.player_.talents().cooldownRemaining(1)==3,
              "Chest guarantees magic or rare gear and advances exactly one player turn");
        const auto chestItem = itemRecord(*app.groundItems_[0]);
        const auto chestRng = app.loot_.state();
        app.pickupItem(); app.tryMovePlayer(1,0);
        check(app.loot_.state()==chestRng && app.player_.stats().hp==99 && app.groundItems_.size()==1 && app.player_.position().x==10,
              "An open chest spends nothing, grants nothing, and still stands in the way");
        roundTrip();
        check(app.chestClaimed_ && app.chestExists_ && app.loot_.state()==chestRng && app.groundItems_.size()==1 &&
              itemRecord(*app.groundItems_[0])==chestItem && app.player_.stats().hp==99,
              "Real application save/load preserves the open chest, its spilled loot and RNG");
        app.pickupItem();
        check(app.groundItems_.size()==1 && app.loot_.state()==chestRng,"Loading cannot reopen the claimed chest");
        app.player_.setPosition(spill); app.pickupItem();
        check(app.player_.inventory().items().size()==1,"The spilled loot can be picked up");
        app.openInventory(); app.inventorySelection_=kEquipmentSlotCount;
        snapshot("rolled-affixes.png");
        // Some chests are hungry.
        setup(PlayerClass::Mage);
        app.chestExists_ = true; app.chestMimic_ = true; app.chestPosition_ = {11,10};
        roundTrip();
        check(app.chestMimic_,"A mimic stays a mimic across a save");
        app.tryMovePlayer(1,0);
        Monster* mimic=nullptr;
        for (auto& m:app.monsters_) if (m->type()==MonsterType::Mimic) mimic=m.get();
        check(mimic && !app.chestExists_ && mimic->position().x==11 && mimic->tactics.alert>0,"Opening a mimic wakes it where the chest stood");
        if (mimic) {
            mimic->stats().hp=0; app.checkAndHandleDeath(*mimic);
            bool rare=false;
            for (const auto& item:app.groundItems_) rare=rare || item->rarity()==ItemRarity::Rare;
            check(rare && app.groundItems_.size()==2,"A slain mimic spills better loot than a chest: a rare and a magic item");
        }
        {
            // Mimics wait below the first floors, and chests never wall anything off.
            int mimics=0, chests=0; bool safe=true;
            for (int floor:{1,2,6,12}) for (int run=0; run<30; ++run) {
                setup(PlayerClass::Mage); app.currentFloor_=floor; app.loot_.restore(1000+run*7+floor);
                for (int y=1;y<21;++y) if (y!=10) app.map_.setTile(16,y,Tile{TileType::Wall,false,false}); // two rooms joined by one gap
                app.spawnFloorChest();
                chests+=app.chestExists_;
                if (floor<=2) safe=safe && !app.chestMimic_;
                mimics+=app.chestMimic_;
                safe=safe && !(app.chestPosition_.x==16 && app.chestPosition_.y==10) && !(app.chestPosition_.x==15 && app.chestPosition_.y==10) &&
                     !(app.chestPosition_.x==17 && app.chestPosition_.y==10);
            }
            check(chests==120 && safe,"No mimics on floors 1-2, and a chest never blocks the way between rooms");
            check(mimics>0 && mimics<15,"Deeper down, now and then a chest is a mimic");
        }

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
                  "A torch shows three tiles; beyond it the dark hides enemies");
            app.setSurface({15,9},SurfaceType::Fire,20); app.updateFieldOfView();
            check(app.exploredMap_.at(15,9)==Visibility::Visible,"With your torch lit, you see other lights in the distance");
            snapshot("ui-darkness-torch.png");
            app.toggleLight();
            check(!app.player_.lightLit && app.exploredMap_.at(13,10)!=Visibility::Visible && app.exploredMap_.at(11,10)==Visibility::Visible,
                  "Doused, you only see what is beside you");
            check(app.exploredMap_.at(15,9)!=Visibility::Visible && app.exploredMap_.at(9,9)==Visibility::Visible,
                  "...only the 8 tiles around you: even distant fire is lost to you");
            app.clearSurfaces();
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
        // Water and blood art, and the floor's notice of its events.
        setup(PlayerClass::Mage);
        {
            app.clearSurfaces();
            for (int y=12;y<=15;++y) for (int x=12;x<=18;++x) if ((x+y)%7!=0 && !(y==15 && x>16)) app.setSurface({x,y},SurfaceType::Water,0);
            for (int x=14;x<=15;++x) app.setSurface({x,13},SurfaceType::Electrified,4);
            for (const Position p:{Position{6,12},Position{7,12},Position{6,13},Position{8,14},Position{5,15}}) app.setSurface(p,SurfaceType::Blood,0);
            for (const Position p:{Position{8,10},Position{9,10},Position{8,11},Position{10,9}}) app.setSurface(p,SurfaceType::Oil,0); // oil beside the blood
            for (const Position p:{Position{12,9},Position{13,9},Position{12,10}}) app.setSurface(p,SurfaceType::Ice,0); // and ice
            for (const Position p:{Position{5,11},Position{4,12},Position{5,12}}) app.setSurface(p,SurfaceType::Acid,0); // and acid
            app.landmark_=LandmarkKind::Shrine; app.landmarkAltar_={0,0}; app.landmarkUsed_=false;
            app.extraLandmarks_.push_back({LandmarkKind::HealingFountain,{0,1},false});
            app.extraLandmarks_.push_back({LandmarkKind::TreasureHoard,{0,2},true});
            app.announceFloor();
            check(app.floorNotice_.rfind("On this floor: Shrine of ",0)==0 && app.floorNotice_.find(',')!=std::string::npos,
                  "Arriving names every unused event on the floor");
            check(app.floorNotice_.find(landmarkName(LandmarkKind::TreasureHoard,floorTheme(app.currentFloor_).region))==std::string::npos,
                  "A used event isn't announced");
            app.player_.setPosition({10,13}); app.updateFieldOfView(); app.updateCamera();
            snapshot("ui-water-blood.png");
            app.landmark_=LandmarkKind::None; app.extraLandmarks_.clear(); app.floorNotice_.clear(); app.clearSurfaces();
        }
        // Roaming threats: patrols, a wandering champion, hunters, and flanking.
        setup(PlayerClass::Warrior);
        {
            // Blocked by an ally, a chaser goes round it instead of queuing.
            auto* front=enemy({11,10}); auto* back=enemy({12,10});
            (void)front;
            const auto flank=app.enemyDecision(*back,&app.player_);
            check(flank.type==AIActionType::Move && !app.isOccupied(flank.movePosition,back) &&
                  std::abs(flank.movePosition.x-10)+std::abs(flank.movePosition.y-10)<=3,"A chaser blocked by an ally goes round it to surround you");
            int patrols=0, champions=0;
            Monster* champion=nullptr;
            for (unsigned seed=1;seed<=24;++seed) {
                app.currentFloor_=4; app.regenerateLevel(seed);
                bool patrol=false;
                for (auto& m:app.monsters_) {
                    patrol|=m->roam==Roam::Patrol;
                    if (m->roam==Roam::Champion) {
                        ++champions; champion=m.get();
                        check(m->name().size()>14 && m->name().substr(m->name().size()-14)==", the Wanderer" && m->name().rfind(namePrefixForTier(MonsterTier::Nightmare),0)!=0 &&
                              m->tier()==MonsterTier::Nightmare,"The wandering champion is a Nightmare, named plainly: \"Ogre, the Wanderer\"");
                        check(std::max(std::abs(m->position().x-app.player_.position().x),std::abs(m->position().y-app.player_.position().y))>15,
                              "The wandering champion starts far from you");
                        check(app.floorNotice_.find(m->name()+" roams these halls")!=std::string::npos,"The floor notice names the wandering champion");
                    }
                }
                patrols+=patrol;
                if (patrol) check(app.floorNotice_.find("a patrol walks the halls")!=std::string::npos,"The floor notice warns of the patrol");
                if (champion && patrols>2) break;
            }
            std::cout << patrols << " patrols, " << champions << " champions on depth-4 floors\n";
            check(patrols>0 && champions>0,"Floors get patrols and wandering champions");
            if (champion) {
                const auto points=app.roamWaypoints();
                const auto before=champion->position();
                const auto step=app.enemyDecision(*champion,nullptr);
                const auto goal=points[static_cast<std::size_t>(champion->tactics.patrol)]; // the next room, chosen on arrival
                check(step.type==AIActionType::Move && std::abs(step.movePosition.x-goal.x)+std::abs(step.movePosition.y-goal.y)<
                      std::abs(before.x-goal.x)+std::abs(before.y-goal.y),"A roamer walks toward the next room");
                const auto name=champion->name(); const int hp=champion->stats().maxHp;
                app.floorTurns_=321; roundTrip();
                bool kept=false;
                for (auto& m:app.monsters_) kept|=m->roam==Roam::Champion && m->name()==name && m->stats().maxHp==hp;
                check(kept && app.floorTurns_==321,"Save/load keeps the roamers, the champion's name and the hunt's count");
            }
            // The hunt: a warning, then two packs that know where you are.
            app.floorTurns_=799; app.tickHunt();
            check(app.logMessages_.back()=="You feel watched.","Lingering brings a quiet warning first");
            const auto before=app.monsters_.size();
            app.floorTurns_=899; app.tickHunt();
            std::vector<Monster*> hunters;
            for (auto& m:app.monsters_) if (m->roam==Roam::Hunter) hunters.push_back(m.get());
            check(app.monsters_.size()==before+hunters.size() && hunters.size()>=4,"Then the hunters come, two packs of them");
            bool hidden=true, fair=true;
            for (auto* h:hunters) {
                hidden&=app.exploredMap_.at(h->position().x,h->position().y)!=Visibility::Visible;
                fair&=!h->rewardsEligible() && h->tactics.alert>0;
            }
            check(hidden && fair,"Hunters arrive out of sight, alert, and carry nothing");
            if (!hunters.empty()) {
                // The one nearest you leads; the rest queue or go round it.
                const auto me=app.player_.position();
                auto* h=*std::min_element(hunters.begin(),hunters.end(),[&](Monster* a,Monster* b) {
                    return findPath(app.map_,a->position(),me)->size()<findPath(app.map_,b->position(),me)->size(); });
                h->tactics.alert=0;
                const auto was=h->position();
                const auto chase=app.enemyDecision(*h,nullptr);
                const auto path=findPath(app.map_,was,me);
                check(chase.type==AIActionType::Move && h->tactics.alert>0 && h->tactics.lastKnown.x==me.x && h->tactics.lastKnown.y==me.y &&
                      path && findPath(app.map_,chase.movePosition,me)->size()<=path->size()+1,"Hunters always know where you are");
            }
            check(std::find(app.logMessages_.begin(),app.logMessages_.end(),"Footsteps, closing in.")!=app.logMessages_.end(),"The hunters are heard, not announced");
            snapshot("ui-hunt.png");
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
            check(app.player_.lightSource==2 && app.playerLightRadius()==5,"The Lamplighter's Rest gives a lantern that lights five tiles");

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
            check(app.playerLightRadius()==4,"Radiant gear widens your light");
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
            gloom->tactics.alert=8; const int gloomHp=gloom->stats().hp; app.tickSurfaces();
            check(gloom->stats().hp<gloomHp,"Light sears a Gloomstalker in the fight");
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
            check(app.playerLightRadius()==3,"When Smothered ends, your light returns by itself");
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

        // Lived-in rooms, seen in the dark: a goblin mess, and grave-robbers in the crypts.
        setup(PlayerClass::Warrior); app.darknessEnabled_=true;
        for (const auto& [floor,scene,shot]:{std::tuple{2,Vignette::Mess,"ui-room-mess.png"},std::tuple{8,Vignette::GraveDig,"ui-room-dig.png"}}) {
            bool found=false;
            for (unsigned seed=1;seed<=80 && !found;++seed) {
                app.currentFloor_=floor; app.regenerateLevel(seed);
                DungeonGenerationParams params; params.region=floorTheme(floor).region; params.includeBossRoom=false;
                params.rareEventChance=floor>=kRareEventFloor?kRareEventChance:0.f;
                params.bloodAltarChance=floor>=kBloodAltarFloor && !app.player_.bloodMagicUnlocked?0.3f:0.f;
                params.includeVault=floor>=3;
                const auto d=generateDungeon(params,seed);
                for (std::size_t i=0;i<d.roomVignettes.size() && !found;++i) {
                    if (d.roomVignettes[i]!=scene) continue;
                    const auto a=d.otherRoomCenters[i];
                    // Stand back from the room, torch lit, and look.
                    for (const Position step:{Position{0,4},Position{0,-4},Position{4,0},Position{-4,0},Position{2,3},Position{-2,3}})
                        if (app.map_.isWalkable(a.x+step.x,a.y+step.y) && !app.isOccupied({a.x+step.x,a.y+step.y},nullptr)) {
                            for (auto& m:app.monsters_) m->tactics.alert=0;
                            app.player_.setPosition({a.x+step.x,a.y+step.y}); app.player_.lightLit=true;
                            app.updateFieldOfView(); app.updateCamera(); found=true; break;
                        }
                }
            }
            check(found,(std::string("A floor with a dressed room to look at: ")+shot).c_str());
            if (!found) continue;
            check(!app.decals_.empty(),"Lived-in rooms leave things lying about");
            snapshot(shot);
            if (scene==Vignette::Mess) {
                // The same room on a 21:9 ultrawide: the map fills it, the HUD keeps to the edges.
                app.log("Entering Barracks. Warm stone halls."); app.log("Goblin hits Player for 4 (96/100 hp left)");
                app.player_.ward=6; app.player_.statusEffects().apply({StatusEffectType::Guard,3,2});
                app.player_.statusEffects().apply({StatusEffectType::Poison,4,1});
                app.window_.setSize({1680,720}); app.fitView(); app.updateCamera();
                check(playLayout::screenWidth==1680.f && playLayout::mapWidth==1680.f,"An ultrawide window gets a wider play screen, all of it map");
                snapshot("ui-hud-ultrawide.png");
                // Menus on the ultrawide: halves docked to the edges, or centred on a full backdrop.
                app.openInventory(); snapshot("ui-menu-inventory-ultrawide.png");
                {
                    // A click on the bag's right half lands on the same cell as at 16:9.
                    const auto atWide=app.designFromPlay({1680.f-100.f,300.f}), inGap=app.designFromPlay({840.f,300.f});
                    check(atWide.x==1180.f && inGap.x<0,"Docked menu halves keep their own coordinates; the gap is the map");
                }
                app.inventoryOpen_=false;
                app.openTalentTrees(); snapshot("ui-menu-talents-ultrawide.png"); app.mode_=GameMode::Playing;
                app.mode_=GameMode::Town; snapshot("ui-menu-town-ultrawide.png"); app.mode_=GameMode::Playing;
                app.mode_=GameMode::ClassSelection; snapshot("ui-menu-class-ultrawide.png");
                app.mode_=GameMode::GameOver; snapshot("ui-menu-gameover-ultrawide.png"); app.mode_=GameMode::Playing;
                app.window_.setSize({1280,720}); app.fitView(); app.updateCamera();
                snapshot("ui-hud-16x9.png");
                app.player_.statusEffects().active().clear();
            }
        }
        {
            const auto litter=app.decals_.size();
            roundTrip();
            check(app.decals_.size()==litter && litter>0,"Save/load keeps what lies on the floor");
        }

        // Esc pauses instead of quitting: resume, options, save and exit.
        setup(PlayerClass::Warrior);
        {
            const auto press=[&](int x,int y) { app.handleEvent(sf::Event::MouseButtonPressed{sf::Mouse::Button::Left,{x,y}}); };
            const auto at=app.player_.position();
            app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::Escape});
            check(app.pauseMenu_ && app.window_.isOpen(),"Esc opens the pause menu, and doesn't quit");
            app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::D});
            check(app.player_.position().x==at.x,"While paused, the game takes no input");
            snapshot("ui-pause.png");
            const auto r=app.pauseButton(1);
            press(static_cast<int>(r.position.x+r.size.x/2),static_cast<int>(r.position.y+r.size.y/2));
            check(app.pauseOptions_,"Options opens its own page");
            app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::Escape});
            check(app.pauseMenu_ && !app.pauseOptions_,"Esc backs out of options to the pause menu");
            const auto resume=app.pauseButton(0);
            press(static_cast<int>(resume.position.x+resume.size.x/2),static_cast<int>(resume.position.y+resume.size.y/2));
            check(!app.pauseMenu_,"Resume closes the pause menu");
        }

        // Gear: a base's own damage, defences, requirements, traits, curses and wards.
        setup(PlayerClass::Warrior);
        {
            auto& hero = app.player_;
            const auto give = [&](const char* id, std::vector<RolledAffix> affixes = {}) {
                hero.inventory().add(std::make_unique<Item>(*findItemDefinition(id), app.nextItemId_++, Position{}, affixes, 0));
                return hero.inventory().items().size() - 1;
            };
            for (int slot = 0; slot < kEquipmentSlotCount; ++slot) hero.unequip(static_cast<EquipmentSlot>(slot));
            auto* target = enemy({11, 10});
            const auto attack = basicAttack();
            const int bare = estimateTalentDamage(attack, hero, *target).normal;
            check(hero.equip(give("iron_sword")) && estimateTalentDamage(attack, hero, *target).normal == bare + 2,
                  "A sword's own damage lands on every attack");
            auto spell = findTalentDefinition("fire.ember_bolt")->ranks[0];
            hero.unequip(EquipmentSlot::Weapon);
            const int bareSpell = estimateTalentDamage(spell, hero, *target).normal;
            check(hero.equip(give("ash_staff")) && estimateTalentDamage(attack, hero, *target).normal == bare &&
                  estimateTalentDamage(spell, hero, *target).normal == bareSpell + 2, "A staff's damage goes to spells, not blows");
            hero.unequip(EquipmentSlot::Weapon);
            check(Item(*findItemDefinition("iron_mace"), 1).affixValue(BonusStat::StunChance) == 5 &&
                  Item(*findItemDefinition("iron_spear"), 1).affixValue(BonusStat::BleedChance) == 10,
                  "Every base carries its trait: maces stun, spears bleed");

            // Requirements: deeper bases ask for the attribute.
            hero.baseStats().strength = hero.stats().strength = 6;
            const auto longsword = give("steel_longsword");
            check(!hero.equip(longsword), "A Steel Longsword is too heavy at 6 Strength");
            hero.baseStats().strength = hero.stats().strength = 12; hero.baseStats().dexterity = hero.stats().dexterity = 6;
            check(hero.equip(longsword), "...and wielded at 12");
            hero.unequip(EquipmentSlot::Weapon);

            // Armour, evasion and ward.
            check(hero.equip(give("chain_coat")) && hero.equip(give("wooden_shield")) && gearArmour(hero) == 12 &&
                  armourReduction(8) == 2 && armourReduction(12) == 3, "Heavy armour and shields add armour: a quarter of it comes off every direct hit");
            check(afterArmour(5, 8) == 3 && afterArmour(2, 100) == 1 && afterArmour(0, 8) == 0,
                  "Armour's flat cut never takes a landed hit below 1");
            hero.unequip(EquipmentSlot::Armour); hero.unequip(EquipmentSlot::OffHand);
            const int dodgeBefore = armourDodgeBonus(hero);
            check(hero.equip(give("scout_leathers")) && armourDodgeBonus(hero) == dodgeBefore + 4, "Light armour is evasion: more dodge");
            hero.unequip(EquipmentSlot::Armour);
            check(hero.equip(give("woven_robes")) && gearWard(hero) == 6, "Cloth is ward");
            hero.ward = 0;
            for (int i = 0; i < 6; ++i) app.tickWard();
            check(hero.ward == 6, "Ward refills once you've been left alone a few turns");
            hero.baseStats().dexterity = hero.stats().dexterity = 0;
            const int life = hero.stats().hp;
            AIDecision blow; blow.type = AIActionType::Attack; blow.target = &hero; blow.attackPower = 2;
            target->stats().strength = target->stats().dexterity = target->stats().intelligence = 0;
            app.executeAIDecision(*target, blow, 0);
            check(hero.stats().hp == life && hero.ward < 6, "A small hit is soaked by ward, not life");
            snapshot("ui-ward.png");
            hero.unequip(EquipmentSlot::Armour);

            // Cursed affixes, conditional damage and ailment wards.
            const Item cursed(*findItemDefinition("iron_sword"), 2, {}, {{"bloodthirsty", 4}}, 0);
            check(cursed.affixValue(BonusStat::FlatDamage) == 4 && cursed.bonuses().maxHp == -16, "A cursed affix gives, and takes");
            app.darknessEnabled_ = false;
            const int plain = app.situationalBonus(attack, *target);
            check(hero.equip(give("copper_ring", {{"dawnlit", 3}, {"stalking", 5}})), "Rings with conditions can be worn");
            target->tactics.alert = 0;
            check(app.situationalBonus(attack, *target) == plain + 8, "In the light, against an unaware foe, both conditions pay");
            target->tactics.alert = 8;
            check(app.situationalBonus(attack, *target) == plain + 3, "...and a foe that's noticed you loses the surprise");
            app.darknessEnabled_ = true;
            hero.equip(give("silver_ring", {{"salamander", 100}}), EquipmentSlot::Ring2);
            hero.statusEffects().apply({StatusEffectType::Burn, 5, 1});
            tickStatusEffects(hero);
            check(!hero.statusEffects().has(StatusEffectType::Burn), "A burn ward shakes off burning");

            // The shop only sells bases you've reached.
            const auto shallowStock = app.shopStock();
            check(std::all_of(shallowStock.begin(), shallowStock.end(), [](const auto* d) { return d->depth <= 1; }) && !shallowStock.empty(),
                  "At first the merchant sells only the first bases");
            app.floorCache_[9] = app.captureState(false);
            const auto deeperStock = app.shopStock();
            check(std::any_of(deeperStock.begin(), deeperStock.end(), [](const auto* d) { return d->depth == 8; }),
                  "Reach deeper and better bases appear in the shop");
            app.floorCache_.erase(9);

            // A long rare survives a save (the hand-made test rings above wouldn't).
            hero.inventory() = Inventory{}; hero.refreshEquipmentStats();
            LootGenerator deep(5);
            std::unique_ptr<Item> longRare;
            for (int i = 0; i < 4000 && (!longRare || longRare->affixes().size() < 6); ++i)
                longRare = deep.generate(13, 2, app.nextItemId_, {}, ItemRarity::Rare);
            const auto affixCount = longRare->affixes().size();
            ++app.nextItemId_;
            hero.inventory().add(std::move(longRare));
            roundTrip();
            check(app.player_.inventory().items().back()->affixes().size() == affixCount && affixCount == 6, "A six-affix rare survives a save");
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
            goblin->setPosition({11,10}); goblin->stats().hp=60; // a crit could otherwise finish it off
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
            check(goblin->stats().hp==beforeWall-3-app.player_.talents().passiveValue(PassiveKind::HardLanding,app.player_.stats()),"Hard Landing: a wall slam deals 3, plus the grown bonus");

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
            check(app.exploredMap_.at(12,10)!=Visibility::Visible && app.exploredMap_.at(11,10)==Visibility::Visible,
                  "With your light out, even Umbral Shroud sees only the 8 tiles around you");
            app.advanceEnemyIntents(); // as the enemies take their turn
            check(app.player_.statusEffects().magnitudeOf(StatusEffectType::Evasion)>=6,"Umbral Shroud: harder to hit in the dark");
            auto* goblin=enemy({11,10}); goblin->stats().dexterity=0; goblin->stats().hp=goblin->stats().maxHp=80;
            app.updateFieldOfView();
            check(cast(veil,{11,10}) && goblin->statusEffects().has(StatusEffectType::Blinded),"Veil of Night blinds");
            app.player_.setPosition({10,10});
            check(!app.canSee(*goblin,{13,10}) && app.canSee(*goblin,{11,11}),"A blinded foe sees only what is beside it");
            app.monsters_.clear();
        }
        setup(PlayerClass::Mage); app.darknessEnabled_=true;
        {
            const auto flare=learnTalent("radiance.flare"), dawn=learnTalent("radiance.dawn");
            learnTalent("radiance.inner_light");
            app.player_.lightLit=true;
            check(app.playerLightRadius()==4,"Inner Light: your light reaches a tile further");
            app.player_.setPosition({10,10}); app.updateFieldOfView();
            auto* goblin=enemy({12,10}); goblin->stats().dexterity=0; goblin->stats().hp=goblin->stats().maxHp=80; goblin->tactics.concealed=true;
            app.lightOrbs_.clear(); app.loreDrops_.clear(); app.banner_.reset(); app.player_.lore.clear();
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

            // The hybrids open on their colours.
            check(!hiddenTreeAvailable(app.player_,"lamplighter") && !hybridRequirement(app.player_,"stonefist").empty(),"The new hybrids start locked, showing what they need");
            const auto invest=[&](const char* id) {
                app.player_.talents().learnTalent(findTalentDefinition(id)->ranks[0]);
                app.player_.talents().setRank(app.player_.talents().knownTalents().size()-1,6);
            };
            (void)invest; giveColour(app.player_,Affinity::Light,6); giveColour(app.player_,Affinity::Flame,6);
            check(hiddenTreeAvailable(app.player_,"lamplighter"),"Lamplighter opens with Light 6 and Flame 6");
        }

        // The forked trees: Fire, One-Handed and Arcane.
        {
            const auto arena=[&](PlayerClass cls) {
                setup(cls);
                for (int y=6;y<=16;++y) for (int x=6;x<=20;++x) app.map_.setTile(x,y,Tile{TileType::Floor,true,true});
                app.setProps({}); app.clearSurfaces();
                app.player_.stats().hp=app.player_.stats().maxHp=500; app.player_.stats().maxMana=500;
                app.player_.setPosition({10,10}); app.updateFieldOfView();
            };
            const auto foe=[&](Position p) { auto* g=enemy(p); g->stats().dexterity=0; g->stats().hp=g->stats().maxHp=90; return g; };
            const auto clearFoes=[&] { for (auto& m:app.monsters_) app.scheduler_.remove(*m); app.monsters_.clear(); app.clearSurfaces(); app.boss_=nullptr; };
            const auto ranked=[&](const char* id,int rank) { const auto i=learnTalent(id); app.player_.talents().setRank(i,rank); return i; };
            const auto sawLog=[&](const char* text) {
                for (const auto& line:app.logMessages_) if (line.find(text)!=std::string::npos) return true;
                return false;
            };

            // Fire: the fork closes behind you.
            arena(PlayerClass::Mage);
            app.player_.trees()={{"fire",false}}; app.player_.abilityPoints()=10; app.player_.level()=6;
            check(purchaseAbility(app.player_,*findTalentDefinition("fire.ember_bolt")) &&
                  purchaseAbility(app.player_,*findTalentDefinition("fire.flame_wall")) &&
                  !purchaseAbility(app.player_,*findTalentDefinition("fire.fireball")) &&
                  abilityPurchaseReason(app.player_,*findTalentDefinition("fire.fireball"))=="You took Flame Wall instead.",
                  "Taking Flame Wall closes Fireball");
            check(abilityPurchaseReason(app.player_,*findTalentDefinition("fire.kindling"))=="Requires Fireball.",
                  "Kindling follows the Fireball side");
            app.player_.trees()={{"fire",false},{"one_handed",false},{"arcane",false}};
            {
                std::size_t fireTree=0; while (std::string(kTalentTrees[fireTree].id)!="fire") ++fireTree;
                app.mode_=GameMode::AbilityChoice; app.treeSelection_=fireTree; app.abilitySelection_=1; app.treeScroll_={};
                snapshot("ui-talent-forked.png");
                // The talent map: the same trees around their colours.
                app.handleTreeKey(sf::Keyboard::Key::M,false);
                check(app.talentMap_,"M opens the talent map");
                snapshot("ui-talent-map.png");
                std::optional<std::size_t> found;
                for (int y=110;y<690 && !found;y+=4) for (int x=30;x<812 && !found;x+=4)
                    if (const auto t=app.talentMapTreeAt({static_cast<float>(x),static_cast<float>(y)}); t && std::string(kTalentTrees[*t].id)=="arcane") found=t;
                check(found.has_value(),"Arcane sits somewhere on the map");
                app.player_.sandbox=true; snapshot("ui-talent-map-all.png"); app.player_.sandbox=false;
                app.talentMap_=false;
                app.mode_=GameMode::Playing;
            }

            // Flame Wall: fire along the line, and the foe set burning.
            arena(PlayerClass::Mage);
            {
                const auto wall=ranked("fire.flame_wall",1);
                auto* a=foe({14,10});
                cast(wall,{14,10});
                check(app.surfaceAt({12,10})==SurfaceType::Fire && app.surfaceAt({13,10})==SurfaceType::Fire && a->statusEffects().has(StatusEffectType::Burn),
                      "Flame Wall lays fire along its line and sets the foe burning");
                clearFoes();
            }
            // Firestorm: burning foes, fire left in patches.
            {
                const auto storm=ranked("fire.firestorm",1);
                auto* a=foe({14,10}); app.updateFieldOfView();
                cast(storm,{14,10});
                check(a->statusEffects().has(StatusEffectType::Burn) && app.surfaceAt({14,10})==SurfaceType::Fire &&
                      app.surfaceAt({15,10})!=SurfaceType::Fire && app.surfaceAt({15,11})==SurfaceType::Fire,
                      "Firestorm burns what it hits and leaves fire in patches");
                clearFoes();
            }
            // Wildfire: a burning foe's fire leaps on when it dies.
            {
                ranked("fire.wildfire",1);
                auto* a=foe({14,10}); auto* b=foe({16,10});
                a->statusEffects().apply({StatusEffectType::Burn,3,2});
                a->stats().hp=0; app.checkAndHandleDeath(*a);
                check(b->statusEffects().has(StatusEffectType::Burn),"Wildfire: a burning foe's fire leaps to the next one when it dies");
                clearFoes();
            }
            // Kindling: more damage to the burning.
            {
                auto* a=foe({11,10});
                const Talent bolt=findTalentDefinition("fire.ember_bolt")->ranks[0];
                a->statusEffects().apply({StatusEffectType::Burn,3,1});
                const int without=estimateTalentDamage(bolt,app.player_,*a).normal;
                ranked("fire.kindling",1);
                check(estimateTalentDamage(bolt,app.player_,*a).normal==without+app.player_.talents().passiveValue(PassiveKind::Kindling,app.player_.stats()) && app.player_.talents().passiveValue(PassiveKind::Kindling,app.player_.stats())>=3,"Kindling: +3 (and more with Intelligence) against burning foes");
                clearFoes();
            }

            // Arcane: Repulse at rank 3 stuns what hits a wall; Kinetic adds to the impact.
            arena(PlayerClass::Mage);
            {
                const auto repulse=ranked("arcane.repulse",3);
                app.map_.setTile(12,10,Tile{TileType::Wall,false,false});
                auto* a=foe({11,10}); auto* b=foe({9,10}); app.updateFieldOfView();
                cast(repulse,app.player_.position());
                check(a->position().x==11 && a->statusEffects().has(StatusEffectType::Stun) && b->position().x==7 && !b->statusEffects().has(StatusEffectType::Stun),
                      "Repulse shoves foes two tiles; at rank 3 the one that hits a wall is stunned");
                app.map_.setTile(12,10,Tile{TileType::Floor,true,true});
                clearFoes();
            }
            // Afterimage: the tile you blinked from bursts once the enemies have answered.
            {
                const auto blink=ranked("arcane.blink",1); ranked("arcane.afterimage",1);
                auto* a=foe({9,10}); app.updateFieldOfView();
                cast(blink,{13,10});
                check(app.player_.position().x==13 && a->stats().hp<90 && app.afterimages_.empty(),"Afterimage: the place you blinked from bursts");
                clearFoes();
            }
            // Arcane Torrent at rank 3 fires again as your next turn begins.
            {
                app.player_.setPosition({10,10});
                const auto torrent=ranked("arcane.torrent",3);
                foe({13,10}); foe({15,10}); app.updateFieldOfView();
                app.logMessages_.clear();
                cast(torrent,{13,10});
                check(sawLog("The torrent strikes") && !app.echo_,"Arcane Torrent's third rank fires down the same line again");
                clearFoes();
            }

            // One-Handed.
            arena(PlayerClass::Warrior);
            {
                app.player_.inventory().add(std::make_unique<Item>(*findItemDefinition("iron_sword"),app.nextItemId_++));
                app.player_.equip(app.player_.inventory().items().size()-1);
                const auto pommel=ranked("one_handed.pommel",3);
                auto* a=foe({11,10}); app.updateFieldOfView();
                cast(pommel,{11,10});
                check(a->statusEffects().has(StatusEffectType::Stun) && a->statusEffects().has(StatusEffectType::Marked),
                      "Pommel Strike stuns; at rank 3 it also marks");
                clearFoes();
                const auto dance=ranked("one_handed.blade_dance",1);
                foe({11,10}); foe({9,10}); foe({10,11}); app.updateFieldOfView();
                app.player_.statusEffects().active().clear();
                cast(dance,app.player_.position());
                check(app.player_.statusEffects().magnitudeOf(StatusEffectType::Guard)>=2,"Blade Dance gives Guard for each foe struck");
                clearFoes();
                auto* b=foe({11,10});
                const Talent strike=findTalentDefinition("one_handed.quick_strike")->ranks[0];
                b->statusEffects().apply({StatusEffectType::Burn,3,1}); b->statusEffects().apply({StatusEffectType::Poison,3,1});
                const int plain=estimateTalentDamage(strike,app.player_,*b).normal;
                ranked("one_handed.exploit",1);
                check(estimateTalentDamage(strike,app.player_,*b).normal==plain+2*app.player_.talents().passiveValue(PassiveKind::Exploit,app.player_.stats()),"Exploit: a bonus for each ailment on the target");
                clearFoes();
            }

            // Resonances: hidden, then a dim shape, then awake at 4 + 4.
            setup(PlayerClass::Warrior); // One-Handed, Quick Strike at rank 3: Steel 3
            {
                const auto* edge=findTalentDefinition("resonance.searing_edge");
                const auto* sword=findTalentDefinition("resonance.spellsword");
                check(edge && sword && affinityPoints(app.player_,Affinity::Steel)==3 && affinityPoints(app.player_,Affinity::Flame)==0,
                      "Each rank adds a point of its node's colour");
                check(resonanceGlimpsed(app.player_,*edge) && !resonanceAwake(app.player_,*edge) &&
                      abilityPurchaseReason(app.player_,*edge)=="Not yet.","With one colour, a resonance is only a dim shape");
                app.player_.level()=6; app.player_.utilityPoints()=earnedUtilityPoints(6); app.player_.trees().push_back({"fire",false});
                app.player_.abilityPoints()=6;
                for (const char* id:{"one_handed.pommel","fire.ember_bolt","fire.ember_bolt","fire.ember_bolt","fire.flame_wall"})
                    purchaseAbility(app.player_,*findTalentDefinition(id));
                check(affinityPoints(app.player_,Affinity::Steel)==4 && affinityPoints(app.player_,Affinity::Flame)==4 &&
                      resonanceAwake(app.player_,*edge) && !resonanceAwake(app.player_,*sword),"Steel 4 and Flame 4 wake Searing Edge, not Spellsword");
                check(purchaseAbility(app.player_,*edge) && app.player_.abilityPoints()==0 && app.player_.talents().rankOf(edge->id)==1,
                      "An awake resonance costs one ability point");
                app.player_.treePoints()=0; app.player_.abilityPoints()=earnedAbilityPoints(6)-9; // 3+1+3+1 in trees, 1 in the resonance
                roundTrip();
                check(app.player_.talents().rankOf("resonance.searing_edge")==1,"A learned resonance survives a save");
                // The talent screen: colours beside the points, the resonance above the details.
                app.mode_=GameMode::AbilityChoice; app.resonanceSelection_=0; app.treeScroll_={};
                snapshot("ui-resonance.png");
                app.resonanceSelection_.reset(); app.mode_=GameMode::Playing;

                // Searing Edge in a fight: the burn flares and the ground behind catches.
                for (int y=6;y<=16;++y) for (int x=6;x<=20;++x) app.map_.setTile(x,y,Tile{TileType::Floor,true,true});
                app.clearSurfaces(); app.player_.setPosition({10,10});
                app.player_.inventory().add(std::make_unique<Item>(*findItemDefinition("iron_sword"),app.nextItemId_++));
                app.player_.equip(app.player_.inventory().items().size()-1);
                std::size_t pommel=0;
                for (std::size_t i=0;i<app.player_.talents().knownTalents().size();++i) if (app.player_.talents().knownTalents()[i].id=="one_handed.pommel") pommel=i;
                auto* a=foe({11,10}); a->statusEffects().apply({StatusEffectType::Burn,3,2}); app.updateFieldOfView();
                cast(pommel,{11,10});
                check(!a->statusEffects().has(StatusEffectType::Burn) && app.surfaceAt({12,10})==SurfaceType::Fire,
                      "Searing Edge: a melee ability spends the burn and sets the ground behind alight");
                clearFoes();

                // Spellsword is Battle Rhythm, without the Spellblade tree.
                app.player_.talents().learnTalent(sword->ranks[0]);
                check(app.player_.talents().passiveValue(PassiveKind::BattleRhythm)==6,"Spellsword grants Battle Rhythm");
            }

            // The second batch: Ice.
            arena(PlayerClass::Mage);
            {
                const auto field=ranked("ice.rime_field",1);
                auto* a=foe({14,10}); app.updateFieldOfView();
                cast(field,{14,10});
                check(app.surfaceAt({14,10})==SurfaceType::Ice && app.surfaceAt({15,10})==SurfaceType::Ice && app.surfaceAt({14,11})==SurfaceType::Ice && a->statusEffects().has(StatusEffectType::Chill),
                      "Rime Field ices a spot and its neighbours and chills what stands there");
                clearFoes();
                const auto nova=ranked("ice.nova",3);
                foe({11,10}); app.updateFieldOfView();
                cast(nova,app.player_.position());
                check(app.surfaceAt({11,10})==SurfaceType::Ice && app.surfaceAt({9,10})==SurfaceType::Ice && app.surfaceAt({10,11})==SurfaceType::Ice && app.surfaceAt({10,10})!=SurfaceType::Ice,
                      "Frost Nova at rank 3 leaves a ring of ice, not under your own feet");
                clearFoes();
                ranked("ice.hoarfrost",1);
                auto* b=foe({14,10}); auto* c=foe({15,10});
                b->statusEffects().apply({StatusEffectType::Chill,3,20}); b->stats().hp=0; app.checkAndHandleDeath(*b);
                check(c->statusEffects().has(StatusEffectType::Chill),"Hoarfrost: a chilled foe shatters and chills its neighbours");
                clearFoes();
                const auto blizzard=ranked("ice.blizzard",1);
                auto* d=foe({14,10}); app.updateFieldOfView();
                cast(blizzard,{14,10});
                const int afterCast=d->stats().hp;
                check(d->stats().hp<90 && d->statusEffects().has(StatusEffectType::Chill) && app.storms_.size()==1,"Blizzard strikes, chills and stays");
                app.tickStorms();
                check(d->stats().hp<afterCast && app.storms_.empty(),"...striking again as your turns begin, then dies away");
                clearFoes();
            }
            // Lightning.
            arena(PlayerClass::Mage);
            {
                const auto chain=ranked("lightning.chain",3);
                auto* a=foe({13,10}); auto* b=foe({15,10}); auto* c=foe({17,10}); app.updateFieldOfView();
                cast(chain,{13,10});
                check(a->stats().hp<90 && b->stats().hp<90 && c->stats().hp<90,"Chain Lightning at rank 3 jumps twice");
                clearFoes();
                const auto clap=ranked("lightning.thunderclap",1);
                auto* d=foe({11,10}); app.updateFieldOfView();
                cast(clap,app.player_.position());
                check(d->statusEffects().has(StatusEffectType::Shock) && d->position().x==12,"Thunderclap shocks and shoves");
                clearFoes();
                ranked("lightning.arc_flash",1);
                const auto bolt=ranked("lightning.bolt",1);
                auto* e=foe({13,10}); auto* f=foe({13,12}); e->statusEffects().apply({StatusEffectType::Shock,3,0}); app.updateFieldOfView();
                cast(bolt,{13,10});
                check(f->stats().hp<90,"Arc Flash: a hit on a shocked foe arcs to the next");
                clearFoes();
                const auto tempest=ranked("lightning.tempest",1);
                auto* g=foe({13,10}); auto* h=foe({10,7}); auto* far=foe({17,10}); app.updateFieldOfView();
                cast(tempest,app.player_.position());
                check(g->stats().hp<90 && h->stats().hp<90 && far->stats().hp==90,"Tempest strikes every foe within three tiles");
                clearFoes();
                // Thermal Shock: chilled and shocked at once, it locks up.
                app.player_.talents().learnTalent(findTalentDefinition("resonance.thermal_shock")->ranks[0]);
                auto* k=foe({13,10}); k->statusEffects().apply({StatusEffectType::Chill,3,20}); k->statusEffects().apply({StatusEffectType::Shock,3,0});
                app.updateFieldOfView();
                cast(bolt,{13,10});
                check(k->statusEffects().has(StatusEffectType::Stun),"Thermal Shock: a chilled, shocked foe is stunned by your hit");
                clearFoes();
            }
            // Two-Handed.
            arena(PlayerClass::Warrior);
            {
                app.player_.baseStats().strength=app.player_.stats().strength=30;
                app.player_.inventory().add(std::make_unique<Item>(*findItemDefinition("greatsword"),app.nextItemId_++));
                check(app.player_.equip(app.player_.inventory().items().size()-1),"Wield a greatsword");
                const auto leap=ranked("two_handed.leap_slam",3); ranked("two_handed.follow_through",1);
                auto* a=foe({14,10}); app.updateFieldOfView();
                cast(leap,{13,10});
                check(app.player_.position().x==13 && a->stats().hp<90 && a->statusEffects().has(StatusEffectType::Stun) &&
                      app.player_.statusEffects().has(StatusEffectType::Empowered),
                      "Leap Slam lands, strikes and at rank 3 stuns; Follow Through empowers the next blow");
                clearFoes();
                app.player_.setPosition({10,10});
                const auto frenzy=ranked("two_handed.blood_frenzy",1);
                cast(frenzy,app.player_.position());
                auto* b=foe({11,10}); app.updateFieldOfView();
                app.player_.stats().hp=50; // below the greatsword-wielder's maximum, so healing shows
                Talent strike=findTalentDefinition("two_handed.cleave")->ranks[0]; strike.id="probe";
                for (int tries=0;tries<10 && app.player_.stats().hp<=50;++tries) applyTalentDamage(strike,app.player_,*b);
                check(app.player_.statusEffects().has(StatusEffectType::Frenzy) && app.player_.stats().hp>50,"Blood Frenzy: your hits heal you");
                clearFoes();
                // Cold Steel: melee abilities chill.
                app.player_.talents().learnTalent(findTalentDefinition("resonance.cold_steel")->ranks[0]);
                const auto cleave=ranked("two_handed.cleave",1);
                auto* c=foe({11,10}); app.updateFieldOfView();
                cast(cleave,app.player_.position());
                check(c->statusEffects().has(StatusEffectType::Chill),"Cold Steel: melee abilities chill what they strike");
                clearFoes();
            }

            // The third batch: Shadow.
            arena(PlayerClass::Mage);
            {
                const auto step=ranked("shadow.step",3);
                app.darknessEnabled_=false; app.updateFieldOfView();
                auto* a=foe({15,10}); app.updateFieldOfView();
                app.player_.talents().resetCooldowns();
                check(!app.tryUseTalent(step,{14,10}) && app.player_.position().x==10,"Shadow Step won't land in the light");
                app.darknessEnabled_=true; app.player_.lightLit=false; app.updateFieldOfView();
                cast(step,{14,10});
                check(app.player_.position().x==14 && a->statusEffects().has(StatusEffectType::Blinded),
                      "Shadow Step lands in the dark; at rank 3 the nearest foe loses you");
                app.darknessEnabled_=false; app.player_.lightLit=true; app.player_.setPosition({10,10}); clearFoes();
                ranked("shadow.dread",1);
                auto* b=foe({11,10});
                const Talent bolt=findTalentDefinition("shadow.bolt")->ranks[0];
                const int plain=estimateTalentDamage(bolt,app.player_,*b).normal;
                b->statusEffects().apply({StatusEffectType::Blinded,3,0});
                check(estimateTalentDamage(bolt,app.player_,*b).normal==plain+app.player_.talents().passiveValue(PassiveKind::Dread,app.player_.stats()),"Dread: blinded foes take more from your hits");
                clearFoes();
                const auto devour=ranked("shadow.devour",1);
                foe({13,10}); app.updateFieldOfView();
                app.player_.talents().resetCooldowns(); app.player_.stats().mana=app.player_.stats().maxMana; app.player_.stats().hp=100;
                app.tryUseTalent(devour,{13,10});
                check(app.player_.stats().hp>100,"Devour returns half its damage as life");
                clearFoes();
            }
            // Radiance.
            arena(PlayerClass::Mage);
            {
                const auto holy=ranked("radiance.holy_light",1);
                app.player_.talents().resetCooldowns(); app.player_.stats().hp=100;
                app.tryUseTalent(holy,app.player_.position());
                check(app.player_.stats().hp>=200,"Holy Light restores a fifth of your life");
                const auto judgement=ranked("radiance.judgement",1);
                auto* a=foe({14,10}); app.updateFieldOfView();
                cast(judgement,{14,10});
                check(a->stats().hp<90 && a->statusEffects().has(StatusEffectType::Blinded),"Judgement strikes and blinds");
                clearFoes();
                ranked("radiance.halo",1);
                auto* b=foe({11,10}); app.player_.lightLit=true;
                app.tickSurfaces();
                check(b->stats().hp<90,"Halo sears foes beside you while your light burns");
                clearFoes();
                // Twilight: Radiance bites harder in the dark.
                app.player_.talents().learnTalent(findTalentDefinition("resonance.twilight")->ranks[0]);
                const auto sear=ranked("radiance.sear",1);
                app.darknessEnabled_=true; app.player_.lightLit=false;
                foe({11,10}); app.updateFieldOfView();
                app.logMessages_.clear();
                cast(sear,{11,10});
                check(sawLog("In darkness: +50%"),"Twilight: Radiance spells strike harder at foes in darkness");
                app.darknessEnabled_=false; app.player_.lightLit=true; clearFoes();
            }
            // Shield.
            arena(PlayerClass::Warrior);
            {
                for (const char* id:{"iron_sword","wooden_shield"}) {
                    app.player_.inventory().add(std::make_unique<Item>(*findItemDefinition(id),app.nextItemId_++));
                    app.player_.equip(app.player_.inventory().items().size()-1);
                }
                const auto rush=ranked("shield.rush",1); ranked("shield.bulwark",1);
                auto* a=foe({13,10}); app.updateFieldOfView();
                cast(rush,{13,10});
                check(app.player_.position().x==12 && a->position().x==15 && app.player_.statusEffects().magnitudeOf(StatusEffectType::Guard)>=3,
                      "Shield Rush charges and knocks back; Bulwark guards you after");
                clearFoes(); app.player_.setPosition({10,10}); app.player_.statusEffects().active().clear();
                app.player_.talents().learnTalent(findTalentDefinition("resonance.hallowed_guard")->ranks[0]);
                const auto bastion=ranked("shield.bastion",1);
                app.player_.talents().resetCooldowns(); app.player_.stats().hp=50;
                app.tryUseTalent(bastion,app.player_.position());
                check(app.player_.statusEffects().magnitudeOf(StatusEffectType::Guard)==8 && app.player_.statusEffects().has(StatusEffectType::Pinned) &&
                      !app.tryMovePlayer(0,1) && app.player_.position().y==10,"Bastion: Guard 8, and your feet stay planted");
                check(app.player_.stats().hp>50,"Hallowed Guard: taking up Guard heals you");
                app.player_.statusEffects().active().clear();
                // Templar's Edge: melee abilities blind foes standing in light.
                app.player_.talents().learnTalent(findTalentDefinition("resonance.templars_edge")->ranks[0]);
                std::size_t strike=0;
                for (std::size_t i=0;i<app.player_.talents().knownTalents().size();++i) if (app.player_.talents().knownTalents()[i].id=="one_handed.quick_strike") strike=i;
                auto* b=foe({11,10}); app.updateFieldOfView();
                cast(strike,{11,10});
                check(b->statusEffects().has(StatusEffectType::Blinded),"Templar's Edge: melee abilities blind foes standing in light");
                clearFoes();
            }

            // The fourth batch: Bow.
            arena(PlayerClass::Thief);
            {
                app.player_.inventory().add(std::make_unique<Item>(*findItemDefinition("hunting_bow"),app.nextItemId_++));
                check(app.player_.equip(app.player_.inventory().items().size()-1),"Take up a bow");
                const auto blank=ranked("bow.point_blank",1);
                auto* a=foe({11,10}); app.updateFieldOfView();
                cast(blank,{11,10});
                check(app.player_.position().x==8 && a->stats().hp<90,"Point Blank: shoot the foe beside you and spring back");
                clearFoes(); app.player_.setPosition({10,10});
                const auto volley=ranked("bow.volley",3);
                auto* b=foe({14,10}); app.updateFieldOfView();
                cast(volley,{14,10});
                check(b->statusEffects().has(StatusEffectType::Pinned) && b->statusEffects().has(StatusEffectType::Marked),"Volley at rank 3 marks and pins");
                clearFoes();
                const auto rain=ranked("bow.rain",1);
                auto* c=foe({14,10}); app.updateFieldOfView();
                cast(rain,{14,10});
                const int afterRain=c->stats().hp;
                check(c->stats().hp<90 && app.storms_.size()==1,"Rain of Arrows strikes and keeps falling");
                app.tickStorms();
                check(c->stats().hp<afterRain,"...striking again as your turn begins");
                clearFoes();
                app.player_.talents().learnTalent(findTalentDefinition("resonance.fire_arrows")->ranks[0]);
                const auto quick=ranked("bow.quick_shot",1);
                auto* d=foe({14,10}); app.updateFieldOfView();
                cast(quick,{14,10});
                check(d->statusEffects().has(StatusEffectType::Burn),"Fire Arrows: your bow attacks set foes burning");
                clearFoes();
            }
            // Stealth.
            arena(PlayerClass::Thief);
            {
                const auto feign=ranked("stealth.feign",1);
                auto* a=foe({13,10}); a->tactics.alert=8; app.updateFieldOfView();
                cast(feign,app.player_.position());
                check(a->tactics.alert==0 && app.player_.statusEffects().has(StatusEffectType::Concealed),"Feign Death: you hide and they lose track of you");
                clearFoes();
                const auto& killer=findTalentDefinition("daggers.assassinate")->ranks[0];
                auto* b=foe({11,10});
                const int healthy=estimateTalentDamage(killer,app.player_,*b).normal;
                b->stats().hp=40;
                check(estimateTalentDamage(killer,app.player_,*b).normal>=healthy*2,"Assassinate: triple damage on a foe below half its life");
                clearFoes();
                // Unseen Hand: attacking from the dark keeps you hidden. Assassin's Edge: +50% from hiding.
                app.player_.talents().learnTalent(findTalentDefinition("resonance.unseen_hand")->ranks[0]);
                app.player_.talents().learnTalent(findTalentDefinition("resonance.assassins_edge")->ranks[0]);
                app.player_.inventory().add(std::make_unique<Item>(*findItemDefinition("steel_dagger"),app.nextItemId_++));
                app.player_.equip(app.player_.inventory().items().size()-1);
                const auto ambush=ranked("daggers.assassinate",1);
                app.darknessEnabled_=true; app.player_.lightLit=false;
                foe({11,10}); app.updateFieldOfView();
                app.player_.statusEffects().apply({StatusEffectType::Concealed,3,3});
                app.logMessages_.clear();
                cast(ambush,{11,10});
                check(sawLog("From hiding: +50%"),"Assassin's Edge: melee hits from hiding deal +50%");
                check(app.player_.statusEffects().has(StatusEffectType::Concealed),"Unseen Hand: striking from an unlit tile keeps you hidden");
                app.darknessEnabled_=false; app.player_.lightLit=true; app.player_.statusEffects().active().clear(); clearFoes();
            }
            // Warbanner: the first deep tree. Hidden until the Warlord's standard is read.
            setup(PlayerClass::Warrior);
            {
                const auto* banner=findTree("warbanner");
                std::size_t bannerIndex=0; while (std::string(kTalentTrees[bannerIndex].id)!="warbanner") ++bannerIndex;
                check(!deepTreeKnown(app.player_,"warbanner") && !hiddenTreeAvailable(app.player_,"warbanner"),"Warbanner is unseen before its lore"); (void)bannerIndex;
                auto made=createMonster(MonsterType::GoblinWarlord,{12,10}); auto* lord=made.get(); app.monsters_.push_back(std::move(made));
                app.onBossDefeated(*lord); clearFoes();
                check(app.loreDrops_.size()==1 && app.loreDrops_[0].id=="warlord_standard","The Warlord's standard falls with him");
                roundTrip();
                check(app.loreDrops_.size()==1,"Save/load keeps lore lying on the floor");
                app.player_.setPosition(app.loreDrops_[0].at); app.pickupItem();
                check(app.loreDrops_.empty() && app.player_.knowsLore("warlord_standard") && deepTreeKnown(app.player_,"warbanner"),
                      "Reading it reveals Warbanner among your talents");
                check(treePurchaseReason(app.player_,PlayerClass::Warrior,*banner).find("Steel")!=std::string::npos,"It says what it needs");
                roundTrip();
                check(app.player_.knowsLore("warlord_standard"),"Save/load keeps the lore you found");
                giveColour(app.player_,Affinity::Steel,8); giveColour(app.player_,Affinity::Guard,6); app.player_.level()=10; app.player_.treePoints()=1;
                check(purchaseTree(app.player_,PlayerClass::Warrior,*banner),"With Steel 8, Guard 6 and level 10 it opens");
                arena(PlayerClass::Warrior); app.lastMoveDirection_={1,0};
                cast(ranked("warbanner.plant",1),app.player_.position());
                check(app.banner_ && app.banner_->at.x==11 && app.banner_->at.y==10 && app.player_.statusEffects().magnitudeOf(StatusEffectType::Guard)>=3,
                      "Plant the Standard: the banner stands beside you, and you are guarded");
                ranked("warbanner.hold",1); app.tickBanner();
                check(app.player_.statusEffects().has(StatusEffectType::Steadfast) && !app.player_.statusEffects().canReceiveStun(),
                      "Hold the Line: beside your banner you can't be stunned");
                auto* near=foe({11,11}); auto* far=foe({16,16});
                cast(ranked("warbanner.rally",1),app.player_.position());
                check(near->statusEffects().has(StatusEffectType::Shaken) && !far->statusEffects().has(StatusEffectType::Shaken),
                      "Rally Cry shakes the foes within two tiles");
                clearFoes(); app.player_.statusEffects().active().clear();
                auto* front=foe({11,10}); auto* behind=foe({13,10});
                cast(ranked("warbanner.bash",1),{11,10});
                check(front->statusEffects().has(StatusEffectType::Stun) && behind->statusEffects().has(StatusEffectType::Stun),
                      "Standard Bash: knocked into another foe, both are stunned");
                clearFoes();
                auto* lone=foe({11,10});
                const int alone=app.situationalBonus(findTalentDefinition("one_handed.quick_strike")->ranks[0],*lone);
                foe({12,10}); foe({12,11});
                ranked("warbanner.ranks",1);
                check(app.situationalBonus(findTalentDefinition("one_handed.quick_strike")->ranks[0],*lone)>=alone+4,
                      "Break Their Ranks: +2 for each other foe around the target");
                clearFoes(); app.banner_.reset();
                for (int i=0;i<12;++i) app.tickBanner();
            }

            // Forgeborn: the Foundry's deep tree, revealed by the Forgemaster's brand.
            setup(PlayerClass::Warrior);
            {
                auto made=createMonster(MonsterType::Forgemaster,{12,10}); auto* smith=made.get(); app.monsters_.push_back(std::move(made));
                app.onBossDefeated(*smith); clearFoes();
                check(app.loreDrops_.size()==1 && app.loreDrops_[0].id=="forgemaster_brand","The Forgemaster drops its brand");
                app.player_.setPosition(app.loreDrops_[0].at); app.pickupItem();
                check(deepTreeKnown(app.player_,"forgeborn"),"Reading the brand reveals Forgeborn");
                giveColour(app.player_,Affinity::Steel,8); giveColour(app.player_,Affinity::Flame,6); app.player_.level()=10; app.player_.treePoints()=1;
                check(purchaseTree(app.player_,PlayerClass::Warrior,*findTree("forgeborn")),"With Steel 8, Flame 6 and level 10 Forgeborn opens");
                arena(PlayerClass::Warrior);
                const auto heat=[&]{ return app.player_.statusEffects().magnitudeOf(StatusEffectType::Heat); };
                cast(ranked("forgeborn.stoke",1),app.player_.position());
                check(heat()==4 && app.player_.statusEffects().magnitudeOf(StatusEffectType::Guard)>=3,"Stoke: 4 Heat and Guard");
                auto* a=foe({11,10});
                const int before=a->stats().hp;
                cast(ranked("forgeborn.searing",1),{11,10});
                check(heat()==0 && a->statusEffects().has(StatusEffectType::Burn) && before-a->stats().hp>=12,"Searing Blow spends the Heat for a burning blow");
                ranked("forgeborn.tempered",1);
                app.addHeat(5);
                const int hot=app.situationalBonus(findTalentDefinition("one_handed.quick_strike")->ranks[0],*a);
                app.addHeat(-5);
                check(hot>=app.situationalBonus(findTalentDefinition("one_handed.quick_strike")->ranks[0],*a)+3,"Tempered: hot melee hits deal more");
                ranked("forgeborn.heat_sink",1);
                app.addHeat(12); const int hp=app.player_.stats().hp; app.player_.setPosition({10,10}); app.tickHeat();
                check(app.player_.stats().hp==hp,"Heat Sink: Heat never burns you");
                clearFoes(); app.addHeat(-20); app.addHeat(5);
                auto* b=foe({11,11});
                cast(ranked("forgeborn.vent",1),app.player_.position());
                check(heat()==0 && b->stats().hp<90 && app.surfaceAt({11,10})==SurfaceType::Fire,"Vent: the Heat bursts out and the ground burns");
                clearFoes(); app.addHeat(6);
                cast(ranked("forgeborn.forgeheart",1),app.player_.position());
                app.tickHeat();
                check(heat()==6 && app.player_.statusEffects().has(StatusEffectType::Hasted),"Forgeheart: your Heat holds, and you are hastened");
                cast(ranked("forgeborn.plate",1),app.player_.position());
                check(app.player_.statusEffects().has(StatusEffectType::MoltenPlate),"Molten Plate wraps you");
                clearFoes(); app.player_.statusEffects().active().clear();
            }

            // Slagcaller: the Foundry's second deep tree, from the formula in its vaults.
            setup(PlayerClass::Mage);
            {
                app.loreDrops_.push_back({{11,10},"slag_formula"});
                app.player_.setPosition({11,10}); app.pickupItem();
                check(deepTreeKnown(app.player_,"slagcaller"),"The slag formula reveals Slagcaller");
                giveColour(app.player_,Affinity::Earth,8); giveColour(app.player_,Affinity::Flame,6); app.player_.level()=12; app.player_.treePoints()=1;
                check(purchaseTree(app.player_,PlayerClass::Mage,*findTree("slagcaller")),"With Earth 8, Flame 6 and level 12 Slagcaller opens");
                arena(PlayerClass::Mage);
                auto* a=foe({13,10});
                cast(ranked("slagcaller.pool",1),{13,10});
                check(app.surfaceAt({13,10})==SurfaceType::Fire && a->statusEffects().has(StatusEffectType::Slowed),"Slag Pool: the ground burns, and foes in it are slowed");
                clearFoes();
                cast(ranked("slagcaller.slagling",1),app.player_.position());
                Monster* mine=nullptr;
                for (auto& m:app.monsters_) if (m->allied && m->type()==MonsterType::Slagling) mine=m.get();
                check(mine && mine->remainingLife>0,"Raise Slagling: a slagling rises to fight for you, for a while");
                ranked("slagcaller.brittle",1);
                if (mine) {
                    const auto at=mine->position();
                    mine->stats().hp=0; app.checkAndHandleDeath(*mine); app.removeDeadMonsters();
                    int shards=0; Monster* shard=nullptr;
                    for (auto& m:app.monsters_) if (m->allied && m->type()==MonsterType::Slagling && m->stats().hp>0) { ++shards; shard=m.get(); }
                    check(app.surfaceAt(at)==SurfaceType::Fire && shards==2,"Brittle Slag: your slagling bursts into flame and splits in two");
                    if (shard) {
                        const auto before=app.monsters_.size();
                        shard->stats().hp=0; app.checkAndHandleDeath(*shard); app.removeDeadMonsters();
                        check(app.monsters_.size()==before-1,"A split slagling doesn't split again");
                    }
                }
                clearFoes();
                cast(ranked("slagcaller.golem",1),app.player_.position());
                Monster* golem=nullptr;
                for (auto& m:app.monsters_) if (m->allied && m->type()==MonsterType::SlagGolem) golem=m.get();
                check(golem && golem->stats().speed==70,"Slag Golem: a slow, tough golem rises for you");
                clearFoes();
                auto* b=foe({14,10});
                cast(ranked("slagcaller.eruption",1),{14,10});
                check(b->statusEffects().has(StatusEffectType::Stun) && app.surfaceAt({15,11})==SurfaceType::Fire,"Eruption: the target is stunned and the ground around it burns");
                clearFoes();
                ranked("slagcaller.pyroclasm",1);
                auto* c=foe({13,12}); app.setSurface({13,12},SurfaceType::Fire,5); app.tickSurfaces();
                check(c->statusEffects().has(StatusEffectType::Slowed),"Pyroclasm: fire on the ground slows the foes in it");
                clearFoes(); app.player_.statusEffects().active().clear();
            }

            // Tempest: the Cathedral's deep tree, from the first Drowned Chorister's hymn.
            arena(PlayerClass::Mage);
            {
                auto* singer=foe({12,10});
                app.monsters_.back()->stats().hp=0; app.checkAndHandleDeath(*app.monsters_.back()); app.removeDeadMonsters(); (void)singer;
                check(app.loreDrops_.empty(),"Only a Chorister carries the hymn");
                auto made=createMonster(MonsterType::DrownedChorister,{12,10}); auto* chorister=made.get();
                app.scheduler_.add(*chorister); app.monsters_.push_back(std::move(made));
                chorister->stats().hp=0; app.checkAndHandleDeath(*chorister); app.removeDeadMonsters();
                check(app.loreDrops_.size()==1 && app.loreDrops_[0].id=="chorister_hymn","The first Chorister slain drops its hymn");
                app.player_.setPosition(app.loreDrops_[0].at); app.pickupItem(); app.player_.setPosition({10,10});
                check(deepTreeKnown(app.player_,"tempest"),"The hymn reveals Tempest");
                giveColour(app.player_,Affinity::Storm,10); app.player_.level()=12; app.player_.treePoints()=1;
                check(purchaseTree(app.player_,PlayerClass::Mage,*findTree("tempest")),"With Storm 10 and level 12 Tempest opens");
                auto* a=foe({12,10});
                cast(ranked("tempest.stormcall",1),app.player_.position());
                app.tickStormcall();
                check(a->stats().hp<90 && a->statusEffects().has(StatusEffectType::Shock),"Stormcall: the storm over you strikes the nearest foe and shocks it");
                clearFoes(); app.player_.statusEffects().active().clear();
                cast(ranked("tempest.static",1),app.player_.position());
                check(app.surfaceAt({11,11})==SurfaceType::Electrified && app.surfaceAt({10,10})!=SurfaceType::Electrified,"Static Field: the ground around you crackles, not under you");
                ranked("tempest.eye",1);
                app.player_.setPosition({11,11}); const int hp=app.player_.stats().hp; app.shockStanding({{11,11}}); app.tickStormcall();
                check(app.player_.stats().hp==hp && app.player_.statusEffects().has(StatusEffectType::Hasted),"Eye of the Storm: electrified ground can't hurt you, and quickens you");
                app.clearSurfaces(); app.player_.setPosition({10,10}); app.player_.statusEffects().active().clear();
                auto* b=foe({13,10}); auto* c=foe({14,11});
                cast(ranked("tempest.forked",1),{13,10});
                check(b->stats().hp<90 && c->stats().hp<90,"Forked Bolt leaps on to a second foe");
                ranked("tempest.overcharge",1);
                const auto& bolt=findTalentDefinition("tempest.forked")->ranks[0];
                const int shocked=app.situationalBonus(bolt,*b); b->statusEffects().remove(StatusEffectType::Shock);
                check(shocked>=app.situationalBonus(bolt,*b)+3,"Overcharge: lightning hits shocked foes harder");
                clearFoes();
                auto* d=foe({14,10});
                cast(ranked("tempest.thunderhead",1),{14,10});
                const int afterCast=d->stats().hp; app.tickStorms();
                check(d->stats().hp<afterCast,"Thunderhead: the storm strikes again as your turn begins");
                clearFoes(); app.storms_.clear();
                auto* e=foe({12,11});
                cast(ranked("tempest.ride",1),{15,10});
                check(app.player_.position().x>10 && e->stats().hp<90,"Ride the Lightning: you flash along and strike what you pass");
                clearFoes(); app.player_.statusEffects().active().clear();
            }

            // Bonewright: the Crypts' deep tree, from the first Bonecaller's journal.
            arena(PlayerClass::Mage);
            {
                auto made=createMonster(MonsterType::Bonecaller,{12,10}); auto* caller=made.get();
                app.scheduler_.add(*caller); app.monsters_.push_back(std::move(made));
                caller->stats().hp=0; app.checkAndHandleDeath(*caller); app.removeDeadMonsters();
                check(app.loreDrops_.size()==1 && app.loreDrops_[0].id=="bonecaller_journal","The first Bonecaller slain drops its journal");
                app.player_.setPosition(app.loreDrops_[0].at); app.pickupItem(); app.player_.setPosition({10,10});
                check(deepTreeKnown(app.player_,"bonewright"),"The journal reveals Bonewright");
                giveColour(app.player_,Affinity::Death,8); giveColour(app.player_,Affinity::Earth,4); app.player_.level()=12; app.player_.treePoints()=1;
                check(purchaseTree(app.player_,PlayerClass::Mage,*findTree("bonewright")),"With Death 8, Earth 4 and level 12 Bonewright opens");
                cast(ranked("bonewright.armour",1),app.player_.position());
                check(app.player_.statusEffects().magnitudeOf(StatusEffectType::Guard)>=4,"Bone Armour: Guard 4");
                app.lastMoveDirection_={1,0};
                cast(ranked("bonewright.wall",1),app.player_.position());
                check(app.propIndexAt(12,9)>=0 && app.propIndexAt(12,10)>=0 && app.propIndexAt(12,11)>=0,"Bone Wall: three pillars across your way, two tiles ahead");
                app.setProps({}); for (int y=9;y<=11;++y) app.map_.setTile(12,y,Tile{TileType::Floor,true,true}); app.pillarTurns_.clear();
                cast(ranked("bonewright.guard",1),app.player_.position());
                Monster* guardian=nullptr;
                for (auto& m:app.monsters_) if (m->allied && m->type()==MonsterType::SkeletonGuard) guardian=m.get();
                check(guardian && guardian->remainingLife==0,"Ossuary Guard: a bone guardian stays until it falls");
                if (guardian) {
                    app.player_.stats().intelligence=20; app.tickStormcall();
                    const int plain=guardian->stats().strength, plainLife=guardian->stats().maxHp;
                    ranked("bonewright.grown",1); app.tickStormcall();
                    check(guardian->stats().strength==plain+4 && guardian->stats().maxHp==plainLife+8,"Grown Guard: +1 Strength and +2 life per 5 Intelligence");
                    app.tickStormcall();
                    check(guardian->stats().strength==plain+4,"Grown Guard doesn't keep stacking");
                }
                ranked("bonewright.marrow",1); app.player_.statusEffects().active().clear();
                auto* near=foe({11,11}); near->stats().hp=0; app.checkAndHandleDeath(*near); app.removeDeadMonsters();
                check(app.player_.statusEffects().magnitudeOf(StatusEffectType::Guard)>=2,"Marrow: a foe dying near you gives Guard");
                auto* cut=foe({9,10});
                cast(ranked("bonewright.storm",1),app.player_.position());
                app.tickStormcall();
                check(cut->stats().hp<90 && cut->statusEffects().has(StatusEffectType::Bleed),"Bone Storm cuts the foes beside you");
                cast(ranked("bonewright.lord",1),app.player_.position());
                app.tickStormcall();
                check(!guardian || guardian->statusEffects().has(StatusEffectType::Hasted),"Bone Lord hastens your minions");
                clearFoes(); app.player_.statusEffects().active().clear();
            }

            // Rimeheart: the Crypts' second deep tree, from the first Frost Acolyte's catechism.
            arena(PlayerClass::Mage);
            {
                auto made=createMonster(MonsterType::FrostAcolyte,{12,10}); auto* acolyte=made.get();
                app.scheduler_.add(*acolyte); app.monsters_.push_back(std::move(made));
                acolyte->stats().hp=0; app.checkAndHandleDeath(*acolyte); app.removeDeadMonsters();
                check(app.loreDrops_.size()==1 && app.loreDrops_[0].id=="acolyte_catechism","The first Frost Acolyte slain drops its catechism");
                app.player_.setPosition(app.loreDrops_[0].at); app.pickupItem(); app.player_.setPosition({10,10});
                check(deepTreeKnown(app.player_,"rimeheart"),"The catechism reveals Rimeheart");
                giveColour(app.player_,Affinity::Frost,8); giveColour(app.player_,Affinity::Water,4); app.player_.level()=12; app.player_.treePoints()=1;
                check(purchaseTree(app.player_,PlayerClass::Mage,*findTree("rimeheart")),"With Frost 8, Water 4 and level 12 Rimeheart opens");
                auto* a=foe({14,10});
                cast(ranked("rimeheart.rime",1),{14,10});
                check(app.surfaceAt({14,10})==SurfaceType::Ice && app.surfaceAt({15,10})==SurfaceType::Ice && app.surfaceAt({15,11})==SurfaceType::Ice && a->statusEffects().has(StatusEffectType::Chill),
                      "Rime freezes the ground and chills those on it");
                // Four casts each way, so a critical can't decide it.
                a->stats().hp=a->stats().maxHp=2000;
                const auto lanceAt=ranked("rimeheart.lance",1);
                int onIce=0, dry=0;
                for (int i=0;i<4;++i) { for (int y=9;y<=11;++y) for (int x=13;x<=15;++x) app.setSurface({x,y},SurfaceType::Ice,12);
                    const int before=a->stats().hp; cast(lanceAt,{14,10}); onIce+=before-a->stats().hp; }
                app.clearSurfaces();
                for (int i=0;i<4;++i) { const int before=a->stats().hp; cast(lanceAt,{14,10}); dry+=before-a->stats().hp; }
                check(dry>0 && onIce*10>=dry*16,"Shatter Lance hits twice as hard on ice");
                a->stats().hp=a->stats().maxHp=200;
                ranked("rimeheart.brittle",1);
                const auto& lance=findTalentDefinition("rimeheart.lance")->ranks[0];
                a->statusEffects().apply({StatusEffectType::Chill,3,20}); const int chilled=app.situationalBonus(lance,*a);
                a->statusEffects().remove(StatusEffectType::Chill);
                check(chilled>=app.situationalBonus(lance,*a)+3,"Brittle Cold: chilled foes take more");
                clearFoes();
                ranked("rimeheart.creeping",1);
                app.lastMoveDirection_={1,0};
                cast(ranked("rimeheart.path",1),{14,10});
                check(app.surfaceAt({10,10})==SurfaceType::Ice && app.surfaceAt({12,10})==SurfaceType::Ice,"Glacial Path leaves ice behind you");
                int iceBefore=0, iceAfter=0;
                for (int y=0;y<22;++y) for (int x=0;x<32;++x) iceBefore+=app.surfaceAt({x,y})==SurfaceType::Ice;
                app.tickStormcall();
                for (int y=0;y<22;++y) for (int x=0;x<32;++x) iceAfter+=app.surfaceAt({x,y})==SurfaceType::Ice;
                check(iceAfter>iceBefore,"Creeping Frost: your ice spreads");
                app.clearSurfaces(); app.frostCreep_.clear(); app.player_.setPosition({10,10});
                auto* b=foe({12,10}); app.setSurface({12,10},SurfaceType::Water,0);
                cast(ranked("rimeheart.freeze",1),app.player_.position());
                check(app.surfaceAt({12,10})==SurfaceType::Ice && b->statusEffects().has(StatusEffectType::Stun),"Deep Freeze: the water freezes and the foe in it is frozen");
                clearFoes(); app.clearSurfaces();
                // Encased, nothing hurts you.
                app.player_.statusEffects().apply({StatusEffectType::Encased,3,2}); app.encasedHp_=app.player_.stats().hp;
                const int life=app.player_.stats().hp;
                app.player_.stats().hp=life-30; app.player_.statusEffects().apply({StatusEffectType::Poison,3,2}); app.tickStormcall();
                check(app.player_.stats().hp==life && !app.player_.statusEffects().has(StatusEffectType::Poison),"Encased, nothing hurts you and no ailment takes hold");
                app.player_.statusEffects().active().clear();
                // Cast for real: your turns pass inside the ice until it bursts.
                auto* c=foe({11,10});
                app.logMessages_.clear();
                cast(ranked("rimeheart.heart",1),app.player_.position());
                check(sawLog("Ice closes over you") && sawLog("You wait inside the ice") && !app.player_.statusEffects().has(StatusEffectType::Encased),
                      "Winter's Heart: you wait inside the ice, and then it is over");
                check(c->stats().hp<90 && c->statusEffects().has(StatusEffectType::Chill) && app.surfaceAt({11,11})==SurfaceType::Ice,
                      "When the ice bursts, it hits and chills everything near you, and the ground freezes");
                clearFoes(); app.clearSurfaces(); app.player_.statusEffects().active().clear();
            }

            // Briarheart: Thornwood Hollow's deep tree, from the first Rot Witch's seed.
            arena(PlayerClass::Thief);
            {
                auto made=createMonster(MonsterType::RotWitch,{12,10}); auto* witch=made.get();
                app.scheduler_.add(*witch); app.monsters_.push_back(std::move(made));
                witch->stats().hp=0; app.checkAndHandleDeath(*witch); app.removeDeadMonsters();
                check(app.loreDrops_.size()==1 && app.loreDrops_[0].id=="witch_seed","The first Rot Witch slain drops her seed");
                app.player_.setPosition(app.loreDrops_[0].at); app.pickupItem(); app.player_.setPosition({10,10});
                check(deepTreeKnown(app.player_,"briarheart"),"The seed reveals Briarheart");
                giveColour(app.player_,Affinity::Rot,8); giveColour(app.player_,Affinity::Hunt,6); app.player_.level()=13; app.player_.treePoints()=1;
                check(!purchaseTree(app.player_,PlayerClass::Thief,*findTree("briarheart")),"Briarheart waits for level 14");
                app.player_.level()=14;
                check(purchaseTree(app.player_,PlayerClass::Thief,*findTree("briarheart")),"With Rot 8, Hunt 6 and level 14 Briarheart opens");
                // Seed the Briar.
                auto* a=foe({14,10});
                cast(ranked("briarheart.seed",1),{14,10});
                check(app.surfaceAt({14,10})==SurfaceType::Thorns,"Seed the Briar: thorns grow where it lands");
                check(app.surfaceAt({15,11})==SurfaceType::Thorns && app.surfaceAt({13,9})==SurfaceType::Thorns,"Seed the Briar: a 3 by 3 patch");
                check(a->statusEffects().has(StatusEffectType::Bleed),"Seed the Briar: the foe in it bleeds");
                clearFoes();
                // Bramble Lash: three tiles' reach; twice as hard on thorns, and it pins.
                auto* b=foe({13,10}); b->stats().hp=b->stats().maxHp=300;
                const auto lash=ranked("briarheart.lash",1);
                cast(lash,{13,10});
                const int plain=300-b->stats().hp;
                check(plain>0 && !b->statusEffects().has(StatusEffectType::Pinned),"Bramble Lash reaches a foe 3 tiles away");
                // Four lashes each way, so a critical can't decide it.
                b->stats().hp=b->stats().maxHp=3000;
                int dry=0, thorned=0;
                for (int i=0;i<4;++i) { const int before=b->stats().hp; cast(lash,{13,10}); dry+=before-b->stats().hp; }
                app.setSurface({13,10},SurfaceType::Thorns,0);
                for (int i=0;i<4;++i) { const int before=b->stats().hp; cast(lash,{13,10}); thorned+=before-b->stats().hp; }
                check(dry>0 && thorned*10>=dry*16 && b->statusEffects().has(StatusEffectType::Pinned),"On thorns, Bramble Lash hits twice as hard and pins");
                clearFoes();
                // Blood Briar.
                auto* c=foe({11,10});
                cast(ranked("briarheart.blood",1),app.player_.position());
                check(app.player_.stats().hp==app.player_.stats().maxHp-6,"Blood Briar costs 6 life");
                check(app.surfaceAt({9,9})==SurfaceType::Thorns && app.surfaceAt({11,11})==SurfaceType::Thorns && app.surfaceAt({10,10})!=SurfaceType::Thorns &&
                      c->statusEffects().has(StatusEffectType::Pinned),"Thorns burst on the 8 tiles around you, and the foe beside you is pinned");
                clearFoes(); app.player_.statusEffects().active().clear();
                // Your own thorns cut you, until Thornborn.
                app.setSurface({10,10},SurfaceType::Thorns,0); app.tickSurfaces();
                check(app.player_.statusEffects().has(StatusEffectType::Bleed),"Your own thorns cut you too");
                app.player_.statusEffects().active().clear();
                ranked("briarheart.thornborn",1);
                app.setSurface({10,10},SurfaceType::Thorns,0); app.tickSurfaces(); app.tickStormcall();
                check(!app.player_.statusEffects().has(StatusEffectType::Bleed) && !app.player_.statusEffects().has(StatusEffectType::Slowed) &&
                      app.player_.statusEffects().magnitudeOf(StatusEffectType::Thornguard)>=3 && ascendancyGuardBonus(app.player_)>=3,
                      "Thornborn: thorns don't cut you, and among them hits deal 3 less");
                app.clearSurfaces(); app.player_.statusEffects().active().clear();
                // Briar Snares.
                ranked("briarheart.snares",1);
                auto* d=foe({14,12});
                app.traps_.push_back({{14,12},1,0,0});
                app.triggerTrap(app.traps_.size()-1,*d,{0,0});
                check(app.surfaceAt({13,11})==SurfaceType::Thorns && app.surfaceAt({15,13})==SurfaceType::Thorns,"Briar Snares: thorns grow around a trap that goes off");
                clearFoes(); app.briarTiles_.clear();
                // Overgrowth: your thorns creep, and pin a foe they reach once.
                app.growBriar({12,10});
                auto* e=foe({14,10});
                cast(ranked("briarheart.overgrowth",1),app.player_.position());
                int before=0, after=0;
                for (int y=0;y<22;++y) for (int x=0;x<32;++x) before+=app.surfaceAt({x,y})==SurfaceType::Thorns;
                for (int i=0;i<3;++i) app.tickStormcall();
                bool far=false;
                for (int y=0;y<22;++y) for (int x=0;x<32;++x) if (app.surfaceAt({x,y})==SurfaceType::Thorns) {
                    ++after; far=far || std::max(std::abs(x-10),std::abs(y-10))>4; }
                check(after>before && !far,"Overgrowth: your thorns spread, but never past 4 tiles from you");
                check(e->statusEffects().has(StatusEffectType::Pinned),"A foe the thorns reach is pinned");
                clearFoes(); app.briarTiles_.clear(); app.player_.statusEffects().active().clear();
                // Heart of Briars: whatever hits you is caught in thorns.
                cast(ranked("briarheart.heart",1),app.player_.position());
                auto* f=foe({11,10}); f->stats().dexterity=10; f->stats().strength=20;
                const int life=app.player_.stats().hp;
                for (int i=0;i<10 && app.player_.stats().hp==life;++i) { app.currentActor_=f; app.processMonsterTurns(); }
                check(app.player_.stats().hp<life && f->statusEffects().has(StatusEffectType::Bleed) && app.surfaceAt(f->position())==SurfaceType::Thorns,
                      "Heart of Briars: whatever hits you bleeds, with thorns beneath it");
                clearFoes(); app.player_.statusEffects().active().clear();
                cast(ranked("briarheart.heart",3),app.player_.position());
                auto* g=foe({12,10}); g->statusEffects().apply({StatusEffectType::Bleed,3,2});
                app.updateFieldOfView();
                app.player_.stats().hp=100; app.tickStormcall();
                check(app.player_.stats().hp==102,"Its mastery heals you 2 a turn for each bleeding foe in view");
                clearFoes(); app.clearSurfaces(); app.briarTiles_.clear(); app.player_.statusEffects().active().clear();
            }

            // Packmaster: Thornwood Hollow's second deep tree, from the first Briar Hound's collar.
            arena(PlayerClass::Thief);
            {
                auto made=createMonster(MonsterType::BriarHound,{12,10}); auto* wild=made.get();
                app.scheduler_.add(*wild); app.monsters_.push_back(std::move(made));
                wild->stats().hp=0; app.checkAndHandleDeath(*wild); app.removeDeadMonsters();
                check(app.loreDrops_.size()==1 && app.loreDrops_[0].id=="hound_collar","The first Briar Hound slain drops its collar");
                app.player_.setPosition(app.loreDrops_[0].at); app.pickupItem(); app.player_.setPosition({10,10});
                check(deepTreeKnown(app.player_,"packmaster"),"The collar reveals Packmaster");
                giveColour(app.player_,Affinity::Hunt,8); giveColour(app.player_,Affinity::Blood,4); app.player_.level()=14; app.player_.treePoints()=1;
                check(purchaseTree(app.player_,PlayerClass::Thief,*findTree("packmaster")),"With Hunt 8, Blood 4 and level 14 Packmaster opens");
                app.player_.packBlood=0;
                const auto hounds=[&]{ int n=0; for (auto& m:app.monsters_) n+=app.packBeast(*m) && m->stats().hp>0; return n; };
                const auto hound=[&]()->Monster* { for (auto& m:app.monsters_) if (app.packBeast(*m) && m->stats().hp>0) return m.get(); return nullptr; };
                const auto call=ranked("packmaster.call",1);
                cast(call,app.player_.position());
                auto* h=hound();
                check(hounds()==1 && h && h->allied,"Call the Pack: a hound bound to you comes to your side");
                if (h) {
                    h->stats().hp=5; cast(call,app.player_.position());
                    check(hounds()==1 && h->stats().hp==h->stats().maxHp,"Calling again heals it to full, and doesn't call a second");
                    app.enforceMinionCap();
                    check(hounds()==1,"The skeleton cap leaves your hound alone");
                    // Blood: your hound's kills make it stronger.
                    const int life=h->stats().maxHp, strength=h->stats().strength;
                    auto* prey=foe({h->position().x+1,h->position().y}); prey->stats().hp=1;
                    for (int i=0;i<10 && prey->stats().hp>0;++i) { app.currentActor_=h; app.processMonsterTurns(); }
                    app.removeDeadMonsters();
                    check(app.player_.packBlood==1 && h->stats().maxHp==life+3 && h->stats().strength==strength+1,
                          "A kill by your hound: 1 Blood, and it gains +1 Strength and +3 life");
                    clearFoes(); cast(call,app.player_.position()); h=hound();
                }
                // Sic 'Em: your beasts hunt the foe you mark, hastened.
                if (h) {
                    h->setPosition({11,10});
                    auto* nearFoe=foe({13,10}); auto* farFoe=foe({17,10});
                    cast(ranked("packmaster.sic",1),{17,10});
                    check(farFoe->statusEffects().has(StatusEffectType::Marked) && h->statusEffects().has(StatusEffectType::Hasted),"Sic 'Em marks the foe and hastens your hound");
                    check(app.nearestOpponent(*h,false)==farFoe,"Your hound goes for the marked foe, not the nearer one");
                    // Pack Tactics.
                    ranked("packmaster.tactics",1);
                    const auto& lash=findTalentDefinition("packmaster.sic")->ranks[0];
                    h->setPosition({12,10});
                    const int beside=app.situationalBonus(lash,*nearFoe), apart=app.situationalBonus(lash,*farFoe);
                    check(beside>=apart+2,"Pack Tactics: +2 against a foe beside your hound");
                    (void)nearFoe; clearFoes(); cast(call,app.player_.position()); h=hound();
                }
                // Blood Bond.
                if (h) {
                    h->stats().hp=5;
                    cast(ranked("packmaster.bond",1),app.player_.position());
                    check(app.player_.stats().hp==app.player_.stats().maxHp-8 && h->stats().hp==std::min(21,h->stats().maxHp) &&
                          h->statusEffects().magnitudeOf(StatusEffectType::Empowered)==3,"Blood Bond: pay 8 life, your hound heals 16 and bites harder");
                }
                // A fallen hound halves your Blood; Blooded changes the cost.
                if (h) {
                    app.player_.packBlood=4;
                    h->stats().hp=0; app.checkAndHandleDeath(*h); app.removeDeadMonsters();
                    check(app.player_.packBlood==2 && hounds()==0,"Your hound falls, and your Blood halves");
                    ranked("packmaster.blooded",1);
                    app.packKill();
                    check(app.player_.packBlood==4,"Blooded: a kill gives 2 Blood");
                    cast(call,app.player_.position()); h=hound();
                    if (h) { h->stats().hp=0; app.checkAndHandleDeath(*h); app.removeDeadMonsters(); }
                    check(app.player_.packBlood==2,"Blooded: a fallen hound costs only 2");
                }
                // Alpha's Howl.
                cast(call,app.player_.position()); h=hound();
                auto* shaken=foe({11,11});
                cast(ranked("packmaster.howl",1),app.player_.position());
                app.tickStormcall();
                check(h && h->statusEffects().has(StatusEffectType::Hasted) && shaken->statusEffects().has(StatusEffectType::Shaken),
                      "Alpha's Howl: your hound is hastened and the foe beside you is shaken");
                clearFoes(); app.player_.statusEffects().active().clear();
                // Feral Bond: half of a hit on you goes to your hound.
                cast(call,app.player_.position()); h=hound();
                if (h) {
                    h->stats().hp=h->stats().maxHp=500;
                    cast(ranked("packmaster.feral",1),app.player_.position());
                    auto* brute=foe({9,10}); brute->stats().strength=30; brute->stats().dexterity=10;
                    const int life=app.player_.stats().hp;
                    for (int i=0;i<10 && h->stats().hp==500;++i) { app.player_.stats().hp=life; app.currentActor_=brute; app.processMonsterTurns(); }
                    check(h->stats().hp<500,"Feral Bond: your hound takes half of the blow");
                    clearFoes(); // your hound goes with them
                }
                app.player_.statusEffects().active().clear(); app.player_.packBlood=0;
            }
            // Your hounds follow you from floor to floor, into town and back, and through a save.
            {
                setup(PlayerClass::Warrior); // the test setup gives a Thief a tree outside its pool, which no save accepts
                app.mode_=GameMode::Playing; app.currentFloor_=2; app.regenerateLevel(81);
                for (auto& m:app.monsters_) app.scheduler_.remove(*m);
                app.monsters_.clear(); app.boss_=nullptr;
                auto* first=app.spawnHound(0);
                check(first!=nullptr,"A hound at your side");
                const auto count=[&]{ int n=0; for (auto& m:app.monsters_) n+=app.packBeast(*m) && m->stats().hp>0; return n; };
                if (first) {
                    first->stats().hp=first->stats().maxHp-7; const int hp=first->stats().hp;
                    app.player_.setPosition(app.floorExit_); app.combatThisTurn_=false;
                    app.travelFloor(3,false); 
                    Monster* again=nullptr; for (auto& m:app.monsters_) if (app.packBeast(*m)) again=m.get();
                    check(app.currentFloor_==3 && count()==1 && again && again->stats().hp==hp &&
                          std::max(std::abs(again->position().x-app.player_.position().x),std::abs(again->position().y-app.player_.position().y))<=3,
                          "Your hound follows you down the stairs, hurt as it was");
                    const auto path=(output/"pack.txt").string();
                    check(saveGame(app.captureState(false),path),"A floor with your hound on it saves");
                    const auto loaded=loadGame(path);
                    check(loaded && app.restoreState(*loaded) && count()==1,"And loads with your hound");
                    for (auto& m:app.monsters_) if (!app.packBeast(*m) && !m->vaultGuard) { m->stats().hp=0; app.scheduler_.remove(*m); } // the vault keeps its guards
                    app.removeDeadMonsters(); app.combatThisTurn_=false;
                    app.returnToTown(true); 
                    check(app.mode_==GameMode::Town && app.player_.packHp.size()==1,"In town, your hound waits");
                    const bool townSaved=saveGame(app.captureState(false),path);
                    const auto townLoaded=townSaved?loadGame(path):std::nullopt;
                    check(townSaved,"A save in town writes");
                    check(townLoaded && townLoaded->packHp.size()==1,"A save in town remembers your hound");
                    app.travelFloor(4,true); 
                    check(app.currentFloor_==4 && count()==1 && app.player_.packHp.empty(),"Back in the dungeon, it comes with you");
                }
                for (auto& m:app.monsters_) app.scheduler_.remove(*m);
                app.monsters_.clear(); app.player_.packHp.clear(); app.mode_=GameMode::Playing; app.currentFloor_=1;
            }

            // Wintermarch: Rimeholt's deep tree, from the first Rime Wight's frozen oath.
            arena(PlayerClass::Warrior);
            {
                auto made=createMonster(MonsterType::RimeWight,{12,10}); auto* wight=made.get();
                app.scheduler_.add(*wight); app.monsters_.push_back(std::move(made));
                wight->stats().hp=0; app.checkAndHandleDeath(*wight); app.removeDeadMonsters();
                check(app.loreDrops_.size()==1 && app.loreDrops_[0].id=="wight_oath","The first Rime Wight slain drops its frozen oath");
                app.player_.setPosition(app.loreDrops_[0].at); app.pickupItem(); app.player_.setPosition({10,10});
                check(deepTreeKnown(app.player_,"wintermarch"),"The oath reveals Wintermarch");
                giveColour(app.player_,Affinity::Frost,10); giveColour(app.player_,Affinity::Guard,6); app.player_.level()=22; app.player_.treePoints()=1;
                check(purchaseTree(app.player_,PlayerClass::Warrior,*findTree("wintermarch")),"With Frost 10, Guard 6 and level 22 Wintermarch opens");
                // Rime Plate.
                cast(ranked("wintermarch.plate",1),app.player_.position());
                check(app.player_.statusEffects().magnitudeOf(StatusEffectType::Guard)>=4 && app.player_.statusEffects().has(StatusEffectType::RimePlate),"Rime Plate: Guard 4");
                auto* striker=foe({11,10}); striker->stats().dexterity=10;
                const int life=app.player_.stats().hp;
                for (int i=0;i<10 && app.player_.stats().hp==life;++i) { app.currentActor_=striker; app.processMonsterTurns(); }
                check(striker->statusEffects().has(StatusEffectType::Chill),"A foe that strikes you through Rime Plate is chilled");
                clearFoes(); app.player_.statusEffects().active().clear();
                // Frozen Advance.
                auto* waiting=foe({13,10});
                app.lastMoveDirection_={1,0};
                cast(ranked("wintermarch.advance",1),{12,10});
                check(app.player_.position().x==12 && waiting->statusEffects().has(StatusEffectType::Slowed),"Frozen Advance: step, and the foe beside where you land slows");
                clearFoes(); app.player_.setPosition({10,10});
                // Hoarfrost and Bitter Cold.
                auto* close=foe({11,10});
                cast(ranked("wintermarch.hoarfrost",1),app.player_.position());
                app.tickStormcall();
                check(close->statusEffects().has(StatusEffectType::Chill),"Hoarfrost chills the foes around you each turn");
                ranked("wintermarch.bitter",1); app.tickStormcall();
                check(close->statusEffects().magnitudeOf(StatusEffectType::Slowed)>=20,"Bitter Cold: a chilled foe beside you is slowed further");
                clearFoes(); app.player_.statusEffects().active().clear();
                // Permafrost: chilled foes hit softer.
                ranked("wintermarch.permafrost",1);
                const auto blows=[&](bool chilled) {
                    auto* brute=foe({11,10}); brute->stats().strength=20; brute->stats().dexterity=10;
                    int total=0;
                    for (int i=0;i<30;++i) {
                        app.player_.stats().hp=500; app.player_.statusEffects().active().clear();
                        brute->statusEffects().active().clear();
                        if (chilled) brute->statusEffects().apply({StatusEffectType::Chill,5,1});
                        app.currentActor_=brute; app.processMonsterTurns();
                        total+=500-app.player_.stats().hp;
                    }
                    clearFoes(); return total;
                };
                const int warm=blows(false), cold=blows(true);
                check(warm>0 && cold<warm,"Permafrost: chilled foes deal less damage to you");
                // Winter's March.
                auto* far=foe({13,10});
                cast(ranked("wintermarch.march",1),app.player_.position());
                app.tickStormcall();
                check(far->statusEffects().magnitudeOf(StatusEffectType::Slowed)>=50 && ascendancyGuardBonus(app.player_)>=3,
                      "Winter's March: foes within 3 tiles crawl, and hits on you deal 3 less");
                clearFoes(); app.player_.statusEffects().active().clear();
                // Avalanche.
                auto* under=foe({14,10});
                cast(ranked("wintermarch.avalanche",1),{14,10});
                check(under->stats().hp<90 && under->statusEffects().has(StatusEffectType::Stun) && app.surfaceAt({15,11})==SurfaceType::Ice,
                      "Avalanche: it crushes, it freezes the ground, and it stuns");
                clearFoes(); app.player_.statusEffects().active().clear();
            }

            // Gravecold: Rimeholt's second deep tree, from the first Frozen Thrall's binding.
            arena(PlayerClass::Mage);
            {
                auto made=createMonster(MonsterType::FrozenThrall,{15,10}); auto* thrall=made.get();
                app.scheduler_.add(*thrall); app.monsters_.push_back(std::move(made));
                thrall->stats().hp=0; app.checkAndHandleDeath(*thrall); app.removeDeadMonsters();
                check(app.loreDrops_.size()==1 && app.loreDrops_[0].id=="thrall_binding","The first Frozen Thrall slain drops its binding");
                app.player_.setPosition(app.loreDrops_[0].at); app.pickupItem(); app.player_.setPosition({10,10});
                check(deepTreeKnown(app.player_,"gravecold"),"The binding reveals Gravecold");
                giveColour(app.player_,Affinity::Death,10); giveColour(app.player_,Affinity::Frost,6); app.player_.level()=24; app.player_.treePoints()=1;
                check(purchaseTree(app.player_,PlayerClass::Mage,*findTree("gravecold")),"With Death 10, Frost 6 and level 24 Gravecold opens");
                clearFoes();
                const auto risen=[&](MonsterType t,bool lasting) { int n=0; for (auto& m:app.monsters_) n+=m->allied && m->type()==t && m->stats().hp>0 && (lasting ? !m->remainingLife : m->remainingLife>0); return n; };
                // Raise the Frozen.
                const auto raise=ranked("gravecold.raise",1);
                cast(raise,app.player_.position());
                Monster* mine=nullptr; for (auto& m:app.monsters_) if (app.frozenThrall(*m)) mine=m.get();
                check(risen(MonsterType::FrozenThrall,true)==1 && mine,"Raise the Frozen: a thrall rises beside you and stays");
                if (mine) {
                    mine->stats().hp=3; cast(raise,app.player_.position());
                    check(risen(MonsterType::FrozenThrall,true)==1 && mine->stats().hp==mine->stats().maxHp,"Raising again mends it, and calls no second");
                    app.enforceMinionCap();
                    check(risen(MonsterType::FrozenThrall,true)==1,"Your thrall doesn't count against your skeletons");
                }
                // Cold Grasp: the foe it kills rises for you.
                auto* doomed=foe({14,12}); doomed->stats().hp=1;
                cast(ranked("gravecold.grasp",1),{14,12});
                app.removeDeadMonsters();
                check(risen(MonsterType::RimeWight,false)==1,"Cold Grasp: the foe it kills rises as your Rime Wight, for a while");
                // Grave Chill: your risen chill what they hit.
                ranked("gravecold.chill",1);
                Monster* wight=nullptr; for (auto& m:app.monsters_) if (m->allied && m->type()==MonsterType::RimeWight) wight=m.get();
                if (wight) {
                    auto* target=foe({wight->position().x+1,wight->position().y}); target->stats().hp=target->stats().maxHp=300;
                    for (int i=0;i<10 && target->stats().hp==300;++i) app.actMinion(*wight);
                    check(target->stats().hp<300 && target->statusEffects().has(StatusEffectType::Chill),"Grave Chill: the blows of your raised dead chill");
                    target->stats().hp=0; app.scheduler_.remove(*target); app.removeDeadMonsters();
                }
                // Unrotting.
                ranked("gravecold.unrotting",1);
                if (mine) {
                    auto* gnaw=foe({mine->position().x+1,mine->position().y}); gnaw->stats().strength=0;
                    mine->stats().hp=mine->stats().maxHp;
                    AIDecision bite; bite.type=AIActionType::Attack; bite.target=mine; bite.attackPower=2;
                    for (int i=0;i<5;++i) app.executeAIDecision(*gnaw,bite,0);
                    check(mine->stats().hp==mine->stats().maxHp,"Unrotting: small blows don't wound your raised dead");
                    gnaw->stats().hp=0; app.scheduler_.remove(*gnaw); app.removeDeadMonsters();
                }
                // Shatter: your thralls burst, and never cut you.
                if (mine) {
                    const auto at=mine->position();
                    auto* beside=foe({at.x+1,at.y+1}); const int life=app.player_.stats().hp;
                    cast(ranked("gravecold.shatter",1),app.player_.position());
                    check(beside->stats().hp<90 && risen(MonsterType::FrozenThrall,true)==0 && app.surfaceAt(at)==SurfaceType::Ice && app.player_.stats().hp>=life,
                          "Shatter: your thralls burst into ice that cuts your foes, never you");
                }
                clearFoes();
                // Winter's Host.
                cast(ranked("gravecold.host",1),app.player_.position());
                check(risen(MonsterType::RimeWight,false)==3,"Winter's Host: three Rime Wights rise around you, for a while");
                clearFoes();
                // Lichfrost.
                cast(ranked("gravecold.lichfrost",1),app.player_.position());
                auto* near=foe({13,10});
                near->stats().hp=0; app.checkAndHandleDeath(*near); app.removeDeadMonsters();
                check(risen(MonsterType::RimeWight,false)==1,"Lichfrost: a foe that dies near you rises for you");
                clearFoes(); app.player_.statusEffects().active().clear();
            }

            // Daggers.
            arena(PlayerClass::Thief);
            {
                app.player_.inventory().add(std::make_unique<Item>(*findItemDefinition("steel_dagger"),app.nextItemId_++));
                check(app.player_.equip(app.player_.inventory().items().size()-1),"Take up a dagger");
                ranked("daggers.hemorrhage",1);
                const auto knife=ranked("daggers.throw",1);
                auto* a=foe({14,10}); app.updateFieldOfView();
                cast(knife,{14,10});
                int bleedTurns=0;
                for (const auto& e:a->statusEffects().active()) if (e.type==StatusEffectType::Bleed) bleedTurns=e.turnsRemaining;
                check(a->stats().hp<90 && bleedTurns>=4,"Throwing Knife bleeds, and Hemorrhage makes it last longer");
                clearFoes();
                const auto rip=ranked("daggers.eviscerate",1);
                auto* b=foe({11,10}); b->statusEffects().apply({StatusEffectType::Bleed,4,3}); app.updateFieldOfView();
                cast(rip,{11,10});
                check(!b->statusEffects().has(StatusEffectType::Bleed) && b->stats().hp<=90-24,"Eviscerate: the whole bleed comes due at once, doubled");
                clearFoes();
            }

            // The fifth batch: Earth.
            arena(PlayerClass::Mage);
            {
                const auto spike=ranked("earth.spike",3);
                auto* a=foe({14,10}); auto* b=foe({15,10}); app.updateFieldOfView();
                cast(spike,{14,10});
                check(a->statusEffects().has(StatusEffectType::Pinned) && b->statusEffects().has(StatusEffectType::Pinned),"Stone Spike at rank 3 pins the foes beside the target too");
                clearFoes();
                ranked("earth.aftershock",1);
                for (std::size_t i=0;i<app.player_.talents().knownTalents().size();++i)
                    if (app.player_.talents().knownTalents()[i].id=="earth.spike") app.player_.talents().setRank(i,1);
                auto* c=foe({14,10}); auto* d=foe({15,10}); app.updateFieldOfView();
                cast(spike,{14,10});
                check(c->statusEffects().has(StatusEffectType::Pinned) && d->stats().hp==90-app.player_.talents().passiveValue(PassiveKind::Aftershock,app.player_.stats()),"Aftershock: pinning a foe shakes the ground beside it");
                clearFoes();
                const auto grasp=ranked("earth.grasp",1);
                auto* e=foe({14,10}); auto* f=foe({14,11}); app.updateFieldOfView();
                cast(grasp,{14,10});
                check(e->statusEffects().has(StatusEffectType::Pinned) && f->statusEffects().has(StatusEffectType::Pinned),"Grasping Earth pins everything around a spot");
                clearFoes();
                const auto pillar=ranked("earth.pillar",3);
                auto* g=foe({12,11}); app.updateFieldOfView();
                cast(pillar,{12,12});
                check(app.propIndexAt(12,12)>=0 && g->position().y==10,"Raise Pillar at rank 3 shoves the foes beside it");
                clearFoes(); app.pillarTurns_.clear(); app.setProps({}); app.map_.setTile(12,12,Tile{TileType::Floor,true,true});
                const auto boulder=ranked("earth.boulder",1);
                auto* h=foe({13,10}); auto* k=foe({15,10}); app.updateFieldOfView();
                cast(boulder,{15,10});
                check(h->stats().hp<90 && k->stats().hp<90 && k->position().x==16,"Boulder rolls through everything in its line, shoving it");
                clearFoes();
            }
            // Tide.
            arena(PlayerClass::Mage);
            {
                const auto undertow=ranked("tide.undertow",3);
                auto* a=foe({14,10}); app.updateFieldOfView();
                cast(undertow,{14,10});
                check(a->position().x==11 && a->statusEffects().has(StatusEffectType::Pinned),"Undertow drags a foe in; at rank 3 it comes up pinned");
                clearFoes();
                ranked("tide.tidecaller",1);
                app.setSurface(app.player_.position(),SurfaceType::Water,0); app.player_.stats().hp=100;
                app.tickSurfaces();
                check(app.player_.stats().hp==100+app.player_.talents().passiveValue(PassiveKind::Tidecaller,app.player_.stats()),"Tidecaller: standing in water heals you");
                app.clearSurfaces();
                const auto flood=ranked("tide.flood",1);
                cast(flood,app.player_.position());
                check(app.surfaceAt({13,10})==SurfaceType::Water && app.surfaceAt({10,10})==SurfaceType::Water,"Flood fills everything around you with water");
                app.player_.talents().learnTalent(findTalentDefinition("resonance.foul_water")->ranks[0]);
                auto* b=foe({12,10});
                app.tickSurfaces();
                check(b->statusEffects().has(StatusEffectType::Poison),"Foul Water: foes standing in water are poisoned");
                app.player_.talents().learnTalent(findTalentDefinition("resonance.mire")->ranks[0]);
                const auto bolt=ranked("tide.bolt",1);
                app.updateFieldOfView();
                cast(bolt,{12,10});
                check(b->statusEffects().has(StatusEffectType::Pinned),"Mire: your hits pin foes standing in water");
                clearFoes();
            }
            // Venom.
            arena(PlayerClass::Mage);
            {
                const auto fester=ranked("venom.fester",3);
                auto* a=foe({14,10}); auto* b=foe({15,10}); a->statusEffects().apply({StatusEffectType::Poison,4,2}); app.updateFieldOfView();
                cast(fester,{14,10});
                check(a->statusEffects().magnitudeOf(StatusEffectType::Poison)==4 && b->statusEffects().has(StatusEffectType::Poison),
                      "Fester doubles a foe's poison, and at rank 3 spreads it");
                clearFoes();
                ranked("venom.virulence",1);
                const auto bolt=ranked("venom.bolt",1);
                auto* c=foe({14,10}); app.updateFieldOfView();
                cast(bolt,{14,10});
                int turns=0;
                for (const auto& e:c->statusEffects().active()) if (e.type==StatusEffectType::Poison) turns=e.turnsRemaining;
                check(turns>=5,"Virulence: your poisons last longer");
                clearFoes();
                const auto blight=ranked("venom.blight",1);
                auto* d=foe({14,10}); app.updateFieldOfView();
                cast(blight,{14,10});
                check(d->statusEffects().magnitudeOf(StatusEffectType::Sundered)==3,"Blight: every hit the foe takes deals 3 more");
                clearFoes();
            }
            arena(PlayerClass::Warrior);
            {
                app.player_.inventory().add(std::make_unique<Item>(*findItemDefinition("iron_sword"),app.nextItemId_++));
                app.player_.equip(app.player_.inventory().items().size()-1);
                app.player_.talents().learnTalent(findTalentDefinition("resonance.envenomed_blades")->ranks[0]);
                std::size_t strike=0;
                for (std::size_t i=0;i<app.player_.talents().knownTalents().size();++i) if (app.player_.talents().knownTalents()[i].id=="one_handed.quick_strike") strike=i;
                auto* a=foe({11,10}); app.updateFieldOfView();
                cast(strike,{11,10});
                check(a->statusEffects().has(StatusEffectType::Poison),"Envenomed Blades: your melee abilities poison");
                clearFoes();
            }

            // The sixth batch: Spear, Mace and Crossbow.
            const auto wield=[&](const char* id) {
                app.player_.baseStats().strength=app.player_.stats().strength=30;
                app.player_.baseStats().dexterity=app.player_.stats().dexterity=30;
                app.player_.inventory().add(std::make_unique<Item>(*findItemDefinition(id),app.nextItemId_++));
                return app.player_.equip(app.player_.inventory().items().size()-1);
            };
            arena(PlayerClass::Warrior);
            {
                check(wield("iron_spear"),"Take up a spear");
                const auto impale=ranked("spear.impale",1);
                auto* a=foe({12,10}); app.updateFieldOfView();
                cast(impale,{12,10});
                check(a->stats().hp<90 && a->statusEffects().has(StatusEffectType::Pinned),"Impale reaches two tiles and pins");
                const Talent thrust=findTalentDefinition("spear.thrust")->ranks[0];
                const int pinned=estimateTalentDamage(thrust,app.player_,*a).normal;
                a->statusEffects().remove(StatusEffectType::Pinned);
                const int loose=estimateTalentDamage(thrust,app.player_,*a).normal;
                ranked("spear.skewer",1); a->statusEffects().apply({StatusEffectType::Pinned,2,0});
                check(estimateTalentDamage(thrust,app.player_,*a).normal==pinned+app.player_.talents().passiveValue(PassiveKind::Skewer,app.player_.stats()) && pinned==loose,"Skewer: more against pinned foes");
                clearFoes();
                const auto toss=ranked("spear.throw",1);
                auto* b=foe({13,10}); auto* c=foe({15,10}); app.updateFieldOfView();
                cast(toss,{15,10});
                check(b->stats().hp<90 && c->stats().hp<90,"Spear Throw runs through every foe in its line");
                clearFoes();
                const auto vault=ranked("spear.vault",3);
                auto* d=foe({14,10}); app.updateFieldOfView();
                cast(vault,{13,10});
                check(app.player_.position().x==13 && d->stats().hp<90,"Pole Vault at rank 3 lands with a strike");
                clearFoes(); app.player_.setPosition({10,10});
            }
            arena(PlayerClass::Warrior);
            {
                check(wield("iron_mace"),"Take up a mace");
                const auto slam=ranked("mace.slam",1);
                auto* a=foe({11,10}); auto* b=foe({10,11}); app.updateFieldOfView();
                cast(slam,app.player_.position());
                check(a->statusEffects().has(StatusEffectType::Sundered) && b->statusEffects().has(StatusEffectType::Sundered),"Ground Slam sunders everything beside you");
                clearFoes();
                const auto skull=ranked("mace.skullcracker",1);
                auto* c=foe({11,10}); app.updateFieldOfView();
                cast(skull,{11,10});
                check(c->statusEffects().has(StatusEffectType::Stun),"Skullcracker stuns");
                const Talent crush=findTalentDefinition("mace.crush")->ranks[0];
                const int dazed=estimateTalentDamage(crush,app.player_,*c).normal;
                ranked("mace.concussion",1);
                check(estimateTalentDamage(crush,app.player_,*c).normal>dazed,"Concussion: more damage to stunned foes");
                clearFoes();
                // Bonecrusher: melee abilities knock foes back. Bedrock: guarded, you can't be moved.
                app.player_.talents().learnTalent(findTalentDefinition("resonance.bonecrusher")->ranks[0]);
                app.player_.talents().learnTalent(findTalentDefinition("resonance.bedrock")->ranks[0]);
                const auto crushIndex=ranked("mace.crush",1);
                auto* d=foe({11,10}); app.updateFieldOfView();
                cast(crushIndex,{11,10});
                check(d->position().x==12,"Bonecrusher: your melee abilities knock foes back a tile");
                app.player_.statusEffects().apply({StatusEffectType::Guard,2,3});
                app.pushActor(app.player_,{-1,0},2,*d);
                check(app.player_.position().x==10,"Bedrock: while guarded, nothing moves you");
                clearFoes();
            }
            arena(PlayerClass::Thief);
            {
                check(wield("light_crossbow"),"Take up a crossbow");
                const auto boom=ranked("crossbow.explosive",1);
                auto* a=foe({14,10}); auto* b=foe({14,11}); app.updateFieldOfView();
                cast(boom,{14,10});
                check(a->stats().hp<90 && b->stats().hp<90 && a->statusEffects().has(StatusEffectType::Burn),"Explosive Bolt bursts and scorches");
                clearFoes();
                ranked("crossbow.heavy_draw",1);
                app.player_.talents().learnTalent(findTalentDefinition("resonance.storm_bolts")->ranks[0]);
                const auto heavy=ranked("crossbow.heavy",1);
                auto* c=foe({13,10}); app.updateFieldOfView();
                cast(heavy,{13,10});
                check(c->position().x==15,"Heavy Draw: your bolts knock foes a tile further");
                check(c->statusEffects().has(StatusEffectType::Shock),"Storm Bolts: your crossbow attacks shock");
                clearFoes();
                const auto ballista=ranked("crossbow.ballista",1);
                auto* d=foe({13,10}); auto* e=foe({15,10}); app.updateFieldOfView();
                cast(ballista,{15,10});
                check(d->stats().hp<90 && e->stats().hp<90 && e->position().x>=17,"Ballista Bolt tears through the line and knocks them back");
                clearFoes();
            }

            // The seventh batch: Brawling.
            arena(PlayerClass::Warrior);
            {
                const auto haymaker=ranked("brawling.haymaker",1);
                auto* a=foe({11,10}); app.updateFieldOfView();
                cast(haymaker,{11,10});
                check(a->stats().hp<90 && a->position().x==14,"Haymaker knocks a foe three tiles back");
                clearFoes();
                ranked("brawling.knockout",1);
                app.map_.setTile(13,10,Tile{TileType::Wall,false,false});
                auto* b=foe({11,10}); app.updateFieldOfView();
                cast(haymaker,{11,10});
                check(b->position().x==12 && b->statusEffects().has(StatusEffectType::Stun),"Knockout: a foe knocked into a wall is stunned");
                app.map_.setTile(13,10,Tile{TileType::Floor,true,true}); clearFoes();
                const auto pile=ranked("brawling.piledriver",1);
                auto* c=foe({11,10}); app.updateFieldOfView();
                cast(pile,{11,10});
                check(c->stats().hp<90 && c->statusEffects().has(StatusEffectType::Stun),"Piledriver slams and stuns");
                clearFoes();
            }
            // Whip.
            arena(PlayerClass::Thief);
            {
                check(wield("leather_whip"),"Take up a whip");
                const auto disarm=ranked("whip.disarm",1);
                auto* a=foe({12,10}); app.updateFieldOfView();
                cast(disarm,{12,10});
                check(a->statusEffects().magnitudeOf(StatusEffectType::Misfortune)==30,"Disarm: its attacks miss 30% more often");
                clearFoes();
                ranked("whip.taskmaster",1);
                const auto lash=ranked("whip.lash",1);
                auto* b=foe({12,10}); app.updateFieldOfView();
                cast(lash,{12,10});
                check(b->position().x==11 && b->statusEffects().has(StatusEffectType::Marked),"Taskmaster: a foe you pull is marked");
                clearFoes();
                const auto whirl=ranked("whip.whirl",1);
                auto* c=foe({12,10}); auto* d=foe({10,12}); app.updateFieldOfView();
                cast(whirl,app.player_.position());
                check(c->position().x==11 && d->position().y==11,"Whirling Lash drags everything around you a tile closer");
                clearFoes();
            }
            // Skirmish.
            arena(PlayerClass::Thief);
            {
                const auto run=ranked("skirmish.hit_and_run",1);
                auto* a=foe({11,10}); app.updateFieldOfView();
                cast(run,{11,10});
                check(a->stats().hp<90 && app.player_.position().x==9 && app.player_.statusEffects().has(StatusEffectType::Opening),
                      "Hit and Run: strike, step back, and gain Opening");
                clearFoes(); app.player_.setPosition({10,10}); app.player_.statusEffects().active().clear();
                const auto slip=ranked("skirmish.slipstream",1);
                cast(slip,app.player_.position());
                check(app.player_.statusEffects().magnitudeOf(StatusEffectType::Evasion)==25 && app.player_.statusEffects().has(StatusEffectType::Opening),
                      "Slipstream: dodge and Opening");
                check(app.player_.statusEffects().magnitudeOf(StatusEffectType::Hasted)==50,"Slipstream: and you move 50% faster");
                app.player_.statusEffects().active().clear();
                app.player_.talents().learnTalent(findTalentDefinition("resonance.tremor")->ranks[0]);
                const auto kick=ranked("skirmish.flying_kick",1);
                auto* b=foe({14,10}); auto* c=foe({13,11}); app.updateFieldOfView();
                cast(kick,{14,10});
                check(app.player_.position().x==13 && b->position().x==16,"Flying Kick charges in and kicks the foe two tiles back");
                check(c->position().y==12,"Tremor: landing from a charge knocks the foes beside you back");
                clearFoes(); app.player_.setPosition({10,10});
                // Ghost Step and Lightning Feet.
                app.player_.talents().learnTalent(findTalentDefinition("resonance.ghost_step")->ranks[0]);
                app.player_.talents().learnTalent(findTalentDefinition("resonance.lightning_feet")->ranks[0]);
                const auto blitz=ranked("skirmish.blitz",1);
                auto* d=foe({12,11}); app.updateFieldOfView();
                cast(blitz,{13,10});
                check(d->statusEffects().has(StatusEffectType::Shock),"Lightning Feet: foes beside your path are shocked");
                clearFoes(); app.player_.setPosition({10,10}); app.player_.statusEffects().active().clear();
                cast(blitz,{13,10});
                check(app.player_.statusEffects().has(StatusEffectType::Concealed),"Ghost Step: movement leaves you concealed");
            }

            // The eighth batch: Alchemy.
            arena(PlayerClass::Thief);
            {
                ranked("alchemy.volatile",1);
                const auto frost=ranked("alchemy.frost",1);
                auto* a=foe({14,10}); app.updateFieldOfView();
                cast(frost,{14,10});
                const auto& tile=app.surfaces_[static_cast<std::size_t>(10*app.map_.width()+14)];
                check(tile.type==SurfaceType::Ice && tile.turns>=8 && a->statusEffects().has(StatusEffectType::Chill),
                      "Frost Flask ices and chills; Volatile Mix makes the ice last longer");
                clearFoes();
                app.player_.talents().learnTalent(findTalentDefinition("resonance.incendiary")->ranks[0]);
                const auto greek=ranked("alchemy.greek_fire",1);
                auto* b=foe({14,10}); app.updateFieldOfView();
                cast(greek,{14,10});
                check(app.surfaceAt({14,10})==SurfaceType::Fire && app.surfaceAt({15,11})==SurfaceType::Fire && b->statusEffects().has(StatusEffectType::Burn),
                      "Greek Fire sets everything around a spot ablaze");
                clearFoes();
                auto* c=foe({14,10}); app.updateFieldOfView();
                cast(frost,{14,10});
                check(c->statusEffects().has(StatusEffectType::Burn),"Incendiary: your flasks set what they catch burning");
                clearFoes();
            }
            // Traps.
            arena(PlayerClass::Thief);
            {
                const auto snare=ranked("traps.snare",3);
                cast(snare,{13,10});
                check(app.traps_.size()==5,"Snare at rank 3 sets a spot and every tile around it");
                app.traps_.clear();
                ranked("traps.ambusher",1);
                app.traps_.push_back({{13,10},6,40,1});
                auto* a=foe({13,10});
                app.triggerTrap(0,*a,{0,0});
                check(a->stats().hp<=82 && a->statusEffects().has(StatusEffectType::Stun) && a->statusEffects().has(StatusEffectType::Marked),
                      "Bear Trap bites and holds; Ambusher marks what it catches");
                clearFoes(); app.traps_.clear();
                app.traps_.push_back({{13,10},8,40,1});
                auto* b=foe({13,10}); auto* c=foe({14,10});
                app.triggerTrap(0,*b,{0,0});
                check(b->statusEffects().has(StatusEffectType::Pinned) && c->statusEffects().has(StatusEffectType::Pinned),"Net Trap pins everything nearby");
                clearFoes(); app.traps_.clear();
            }
            // Hexes.
            arena(PlayerClass::Mage);
            {
                app.player_.talents().learnTalent(findTalentDefinition("resonance.wasting_curse")->ranks[0]);
                app.player_.talents().learnTalent(findTalentDefinition("resonance.witchfire")->ranks[0]);
                const auto enfeeble=ranked("hexes.enfeeble",1);
                auto* a=foe({14,10}); app.updateFieldOfView();
                cast(enfeeble,{14,10});
                check(a->statusEffects().magnitudeOf(StatusEffectType::Slowed)==40,"Enfeeble: a real slow, 40%");
                check(a->statusEffects().has(StatusEffectType::Poison) && a->statusEffects().has(StatusEffectType::Burn),
                      "Wasting Curse poisons and Witchfire burns the foes you curse");
                ranked("hexes.echo",1);
                auto* b=foe({16,10});
                a->stats().hp=0; app.checkAndHandleDeath(*a);
                check(b->statusEffects().has(StatusEffectType::Slowed),"Hex Echo: a cursed foe's curse leaps on when it dies");
                clearFoes();
                const auto misfortune=ranked("hexes.misfortune",3);
                auto* c=foe({14,10}); auto* d=foe({15,10}); app.updateFieldOfView();
                cast(misfortune,{14,10});
                check(c->statusEffects().has(StatusEffectType::Misfortune) && d->statusEffects().has(StatusEffectType::Misfortune),"Misfortune at rank 3 curses the foes beside it too");
                clearFoes();
                const auto doom=ranked("hexes.doom",1);
                auto* e=foe({14,10}); app.updateFieldOfView();
                cast(doom,{14,10});
                check(e->statusEffects().magnitudeOf(StatusEffectType::Doom)==20,"Doom is laid on the foe");
                clearFoes();
            }

            // The ninth batch: Acrobatics.
            arena(PlayerClass::Thief);
            {
                const auto roll=ranked("acrobatics.somersault",1);
                foe({11,10}); app.updateFieldOfView();
                cast(roll,{12,10});
                check(app.player_.position().x==12,"Somersault rolls you over a foe in the way");
                clearFoes(); app.player_.setPosition({10,10});
                ranked("acrobatics.fleet",1);
                app.player_.statusEffects().active().clear();
                app.advanceTurnsUntilPlayerCanAct();
                check(app.player_.statusEffects().magnitudeOf(StatusEffectType::Hasted)>=15,"Fleet: you are always a little faster");
                const auto untouchable=ranked("acrobatics.untouchable",1);
                cast(untouchable,app.player_.position());
                check(app.player_.statusEffects().magnitudeOf(StatusEffectType::Evasion)==50,"Untouchable: +50% dodge");
                app.player_.statusEffects().active().clear();
            }
            // Cloth.
            arena(PlayerClass::Mage);
            {
                const auto shroud=ranked("cloth.shroud",1);
                cast(shroud,app.player_.position());
                check(app.player_.spellWard==app.player_.stats().maxMana/5,"Arcane Shroud: ward worth a fifth of your mana");
                auto* a=foe({11,10});
                const int hp=app.player_.stats().hp, ward=app.player_.spellWard;
                AIDecision claw; claw.type=AIActionType::Attack; claw.target=&app.player_; claw.attackPower=4;
                for (int i=0;i<10 && app.player_.spellWard==ward;++i) app.executeAIDecision(*a,claw,0); // a claw can miss
                check(app.player_.stats().hp==hp && app.player_.spellWard<ward,"...and it soaks the blows that land");
                clearFoes();
                const auto surge=ranked("cloth.surge",1);
                app.player_.talents().resetCooldowns(); app.player_.stats().mana=0;
                app.tryUseTalent(surge,app.player_.position());
                check(app.player_.stats().mana>=20,"Mana Surge restores mana");
                const auto burst=ranked("cloth.burst",1);
                auto* b=foe({11,10}); app.updateFieldOfView();
                app.player_.talents().resetCooldowns(); app.player_.stats().mana=40;
                app.tryUseTalent(burst,app.player_.position());
                check(app.player_.stats().mana<=5 && b->stats().hp<=50,"Mana Burst spends all your mana as one blast");
                clearFoes();
                // Flowing Mana: movement abilities restore mana.
                app.player_.talents().learnTalent(findTalentDefinition("resonance.flowing_mana")->ranks[0]);
                const auto tumble=ranked("acrobatics.tumble",1);
                app.player_.talents().resetCooldowns(); app.player_.stats().mana=app.player_.talents().effectiveTalent(tumble).manaCost;
                app.tryUseTalent(tumble,{12,10});
                check(app.player_.stats().mana>=3,"Flowing Mana: movement abilities restore mana");
            }
            // Light armour.
            arena(PlayerClass::Thief);
            {
                check(wield("scout_leathers") && wield("leather_cap") && wield("leather_gloves") && wield("leather_boots"),"Put on light armour");
                const auto feint=ranked("light_armour.feint",1);
                auto* a=foe({11,10}); app.updateFieldOfView();
                cast(feint,{11,10});
                check(a->statusEffects().has(StatusEffectType::Marked) && app.player_.statusEffects().has(StatusEffectType::Opening),"Feint marks the foe and gives you Opening");
                clearFoes(); app.player_.statusEffects().active().clear();
                const auto blur=ranked("light_armour.blur",1);
                cast(blur,app.player_.position());
                check(app.player_.statusEffects().magnitudeOf(StatusEffectType::Evasion)==30 && app.player_.statusEffects().magnitudeOf(StatusEffectType::Hasted)==30,
                      "Blur: dodge and speed");
                app.player_.statusEffects().active().clear();
                const auto perfect=ranked("light_armour.perfect",1);
                auto* b=foe({11,10}); app.updateFieldOfView();
                app.player_.statusEffects().apply({StatusEffectType::Opening,2,0});
                Talent probe=app.player_.talents().effectiveTalent(perfect);
                const int crit=estimateTalentDamage(probe,app.player_,*b).critical;
                app.player_.talents().resetCooldowns();
                app.tryUseTalent(perfect,{11,10});
                check(b->stats().hp==90-crit,"Perfect Opening: always a critical hit while you have Opening");
                clearFoes();
            }
            // Heavy armour.
            arena(PlayerClass::Warrior);
            {
                check(wield("chain_coat") && wield("iron_helm") && wield("iron_gauntlets") && wield("iron_boots"),"Put on heavy armour");
                app.player_.talents().learnTalent(findTalentDefinition("resonance.arcane_bulwark")->ranks[0]);
                app.player_.talents().learnTalent(findTalentDefinition("resonance.unstoppable")->ranks[0]);
                const auto fortify=ranked("heavy_armour.fortify",1);
                app.player_.statusEffects().active().clear(); app.player_.spellWard=0;
                cast(fortify,app.player_.position());
                check(app.player_.statusEffects().magnitudeOf(StatusEffectType::Guard)==4 && app.player_.statusEffects().magnitudeOf(StatusEffectType::Slowed)==25,
                      "Fortify: Guard 4, but you are slower");
                check(app.player_.spellWard>=4 && app.player_.statusEffects().has(StatusEffectType::Hasted),
                      "Arcane Bulwark and Unstoppable: Guard brings ward and speed");
                app.player_.statusEffects().active().clear();
                const auto unbreakable=ranked("heavy_armour.unbreakable",1);
                app.player_.statusEffects().apply({StatusEffectType::Pinned,3,0}); app.player_.statusEffects().apply({StatusEffectType::Slowed,3,40});
                cast(unbreakable,app.player_.position());
                check(!app.player_.statusEffects().has(StatusEffectType::Pinned) && app.player_.statusEffects().magnitudeOf(StatusEffectType::Guard)==8,
                      "Unbreakable throws off holds and stands guarded");
                app.player_.statusEffects().active().clear();
                const auto charge=ranked("heavy_armour.juggernaut",1);
                auto* a=foe({14,10}); app.updateFieldOfView();
                cast(charge,{14,10});
                check(app.player_.position().x==13 && a->position().x==16 && app.player_.statusEffects().magnitudeOf(StatusEffectType::Guard)>=4,
                      "Juggernaut charges, knocks back, and ends guarded");
                clearFoes();
            }

            // The first hybrid batch: Spellblade.
            arena(PlayerClass::Warrior);
            {
                check(wield("iron_sword"),"Take up a sword");
                ranked("spellblade.ward",1);
                const auto cleave=ranked("spellblade.cleave",1);
                auto* a=foe({11,10}); auto* b=foe({9,10}); app.updateFieldOfView();
                app.player_.spellWard=0;
                cast(cleave,app.player_.position());
                check(a->stats().hp<90 && b->stats().hp<90,"Arcane Cleave sweeps through every foe beside you");
                check(app.player_.spellWard>0,"Blade Ward: melee hits give you spell ward");
                clearFoes();
                const auto blink=ranked("spellblade.blink_strike",1);
                auto* c=foe({14,10}); app.updateFieldOfView();
                cast(blink,{14,10});
                check(app.player_.position().x==13 && c->stats().hp<90,"Blink Strike flashes to a foe and strikes it");
                clearFoes(); app.player_.setPosition({10,10});
            }
            // Animation.
            arena(PlayerClass::Mage);
            {
                const auto raise=ranked("animation.raise",3);
                app.player_.baseStats().intelligence=app.player_.stats().intelligence=30; // room for more than one skeleton
                cast(raise,app.player_.position());
                int skeletons=0; for (const auto& m:app.monsters_) skeletons+=m->allied;
                check(skeletons==2,"Raise Skeleton at rank 3 raises two at once");
                ranked("animation.bone_armour",1);
                auto* a=foe({10,12}); app.updateFieldOfView();
                AIDecision claw; claw.type=AIActionType::Attack; claw.target=&app.player_; claw.attackPower=8;
                const int unarmoured=app.player_.stats().hp;
                app.executeAIDecision(*a,claw,0);
                const int lost=unarmoured-app.player_.stats().hp;
                app.player_.talents()=TalentSet({basicAttack(),basicCleanse()});
                app.player_.stats().hp=unarmoured;
                for (int tries=0;tries<8 && app.player_.stats().hp==unarmoured;++tries) app.executeAIDecision(*a,claw,0);
                check(unarmoured-app.player_.stats().hp>lost,"Bone Armour: each skeleton takes the edge off hits on you");
                for (auto& m:app.monsters_) app.scheduler_.remove(*m);
                app.monsters_.clear();
                const auto spear=ranked("animation.spear",1);
                auto* b=foe({13,10}); auto* c=foe({15,10}); app.updateFieldOfView();
                cast(spear,{15,10});
                check(b->stats().hp<90 && c->stats().hp<90,"Bone Spear runs through a line");
                clearFoes();
                const auto prison=ranked("animation.prison",1);
                auto* d=foe({14,10}); app.updateFieldOfView();
                cast(prison,{14,10});
                check(app.propIndexAt(13,10)>=0 && app.propIndexAt(15,11)>=0 && d->position().x==14,"Bone Prison walls a foe in");
                clearFoes(); app.pillarTurns_.clear(); app.setProps({});
                for (int y=6;y<=16;++y) for (int x=6;x<=20;++x) app.map_.setTile(x,y,Tile{TileType::Floor,true,true});
            }
            // Blood Magic.
            arena(PlayerClass::Mage);
            {
                app.player_.talents().learnTalent(findTalentDefinition("resonance.boiling_blood")->ranks[0]);
                ranked("blood_magic.transfusion",1);
                const auto boil=ranked("blood_magic.boil",1);
                auto* a=foe({11,10}); auto* b=foe({10,12}); app.updateFieldOfView();
                cast(boil,app.player_.position());
                check(a->statusEffects().has(StatusEffectType::Bleed) && b->statusEffects().has(StatusEffectType::Bleed),"Blood Boil: everything within two tiles bleeds");
                check(a->statusEffects().has(StatusEffectType::Burn),"Boiling Blood: spending life sets the foes beside you alight");
                app.player_.stats().hp=100;
                Talent strike=basicAttack();
                for (int tries=0;tries<8 && app.player_.stats().hp<=100;++tries) applyTalentDamage(strike,app.player_,*a);
                clearFoes();
                const auto rite=ranked("blood_magic.rite",1);
                app.player_.talents().resetCooldowns(); app.player_.stats().hp=200;
                app.tryUseTalent(rite,app.player_.position());
                check(app.player_.statusEffects().magnitudeOf(StatusEffectType::Empowered)==8 && app.player_.stats().hp<200,"Blood Rite: pay life for harder hits");
                app.player_.statusEffects().active().clear();
            }
            // Grave Light and Bloodletter.
            arena(PlayerClass::Warrior);
            {
                check(wield("iron_sword"),"Take up a sword again");
                app.player_.talents().learnTalent(findTalentDefinition("resonance.bloodletter")->ranks[0]);
                app.player_.talents().learnTalent(findTalentDefinition("resonance.grave_light")->ranks[0]);
                std::size_t strike=0;
                for (std::size_t i=0;i<app.player_.talents().knownTalents().size();++i) if (app.player_.talents().knownTalents()[i].id=="one_handed.quick_strike") strike=i;
                auto* a=foe({11,10}); app.updateFieldOfView();
                cast(strike,{11,10});
                check(a->statusEffects().has(StatusEffectType::Bleed),"Bloodletter: your melee abilities make foes bleed");
                auto bones=createMonster(MonsterType::Skeleton,{12,11}); app.configureMinion(*bones,1,10);
                auto* skeleton=bones.get(); app.scheduler_.add(*skeleton); app.monsters_.push_back(std::move(bones));
                AIDecision bite; bite.type=AIActionType::Attack; bite.target=a; bite.attackPower=4;
                for (int tries=0;tries<8 && !a->statusEffects().has(StatusEffectType::Burn);++tries) app.executeAIDecision(*skeleton,bite,0);
                check(a->statusEffects().has(StatusEffectType::Burn),"Grave Light: your skeletons set what they hit burning");
                clearFoes();
            }

            // The second hybrid batch: Shadow Archer.
            arena(PlayerClass::Thief);
            {
                check(wield("hunting_bow"),"Take up a bow for the shadows");
                const auto smoke=ranked("shadow_archer.smoke",1);
                auto* a=foe({14,10}); auto* b=foe({14,11}); app.updateFieldOfView();
                cast(smoke,{14,10});
                check(a->statusEffects().has(StatusEffectType::Blinded) && b->statusEffects().has(StatusEffectType::Blinded) &&
                      app.player_.statusEffects().has(StatusEffectType::Concealed),"Smoke Arrow blinds them and hides you");
                clearFoes(); app.player_.statusEffects().active().clear();
                const auto pin=ranked("shadow_archer.pin",1);
                auto* c=foe({14,10}); app.updateFieldOfView();
                cast(pin,{14,10});
                check(c->statusEffects().has(StatusEffectType::Pinned) && c->statusEffects().has(StatusEffectType::Blinded),"Shadow Pin pins and blinds");
                clearFoes();
                ranked("shadow_archer.long_shadow",1);
                const Talent shot=findTalentDefinition("bow.quick_shot")->ranks[0];
                auto* near=foe({12,10}); auto* far=foe({15,10});
                check(estimateTalentDamage(shot,app.player_,*far).normal==estimateTalentDamage(shot,app.player_,*near).normal+
                      app.player_.talents().passiveValue(PassiveKind::LongShadow,app.player_.stats()),"Long Shadow: more damage to distant foes");
                clearFoes();
                app.player_.talents().learnTalent(findTalentDefinition("resonance.ghost_arrows")->ranks[0]);
                const auto shadowShot=ranked("shadow_archer.shot",1);
                auto* d=foe({14,10}); d->stats().dexterity=500; d->stats().hp=d->stats().maxHp=500; app.updateFieldOfView();
                bool alwaysHit=true;
                for (int i=0;i<6;++i) {
                    const int before=d->stats().hp;
                    app.player_.statusEffects().apply({StatusEffectType::Concealed,3,3});
                    cast(shadowShot,{14,10});
                    alwaysHit&=d->stats().hp<before;
                }
                check(alwaysHit,"Ghost Arrows: bow attacks from hiding can't be dodged");
                clearFoes(); app.player_.statusEffects().active().clear();
            }
            // Lamplighter.
            arena(PlayerClass::Mage);
            {
                app.player_.lightLit=true;
                const auto brandish=ranked("lamplighter.brandish",1);
                auto* a=foe({11,10}); app.updateFieldOfView();
                cast(brandish,app.player_.position());
                check(a->statusEffects().has(StatusEffectType::Blinded) && a->position().x==12,"Brandish blinds and drives back the foes beside you");
                clearFoes();
                const int before=app.playerLightRadius();
                ranked("lamplighter.bearer",1);
                check(app.playerLightRadius()==before+2,"Lantern Bearer: your light reaches two tiles further");
                const auto pyre=ranked("lamplighter.pyre",1);
                auto* b=foe({14,10}); app.updateFieldOfView();
                cast(pyre,{14,10});
                check(b->statusEffects().magnitudeOf(StatusEffectType::Burn)==5 && app.surfaceAt({14,10})==SurfaceType::Fire,"Pyre sets one foe fiercely ablaze");
                clearFoes();
                // Thunderflash and Ionise: lightning blinds and burns.
                app.player_.talents().learnTalent(findTalentDefinition("resonance.thunderflash")->ranks[0]);
                app.player_.talents().learnTalent(findTalentDefinition("resonance.ionise")->ranks[0]);
                const auto bolt=ranked("lightning.bolt",1);
                auto* c=foe({14,10}); app.updateFieldOfView();
                cast(bolt,{14,10});
                check(c->statusEffects().has(StatusEffectType::Blinded) && c->statusEffects().has(StatusEffectType::Burn),"Thunderflash and Ionise: your lightning blinds and burns");
                clearFoes();
            }
            // Stormlance.
            arena(PlayerClass::Warrior);
            {
                check(wield("iron_spear"),"Take up a spear for the storm");
                const auto rod=ranked("stormlance.rod",1);
                auto* a=foe({11,10}); app.updateFieldOfView();
                cast(rod,app.player_.position());
                const int afterRod=a->stats().hp;
                check(a->statusEffects().has(StatusEffectType::Shock) && app.storms_.size()==1,"Lightning Rod strikes and keeps striking");
                app.tickStorms();
                check(a->stats().hp<afterRod,"...as your next turn begins");
                clearFoes(); app.storms_.clear();
                const auto ride=ranked("stormlance.ride",1);
                auto* b=foe({12,11}); app.updateFieldOfView();
                cast(ride,{14,10});
                check(app.player_.position().x==14 && b->statusEffects().has(StatusEffectType::Shock) && b->stats().hp<90,
                      "Ride the Lightning strikes and shocks everything beside your path");
                clearFoes(); app.player_.setPosition({10,10});
            }

            // The last hybrid batch: Hexblade.
            arena(PlayerClass::Warrior);
            {
                check(wield("iron_sword"),"Take up a sword for the hexes");
                ranked("hexblade.hunger",1);
                const auto cleave=ranked("hexblade.cleave",1);
                auto* a=foe({11,10}); auto* b=foe({9,10}); app.updateFieldOfView();
                cast(cleave,app.player_.position());
                check(a->statusEffects().has(StatusEffectType::Misfortune) && b->statusEffects().has(StatusEffectType::Misfortune),"Cursed Cleave curses every foe beside you");
                app.player_.stats().hp=100;
                const auto reap=ranked("hexblade.reap",1);
                app.player_.talents().resetCooldowns();
                app.logMessages_.clear();
                app.tryUseTalent(reap,{11,10});
                check(sawLog("Curses torn away") && !a->statusEffects().has(StatusEffectType::Misfortune),"Soul Reap tears the curses off for extra damage");
                clearFoes();
                auto* c=foe({11,10}); c->statusEffects().apply({StatusEffectType::Misfortune,3,25}); c->statusEffects().apply({StatusEffectType::Stun,3,0}); app.updateFieldOfView();
                app.player_.stats().hp=50; // below the sword-wielder's maximum, so healing shows
                std::size_t strike=0;
                for (std::size_t i=0;i<app.player_.talents().knownTalents().size();++i) if (app.player_.talents().knownTalents()[i].id=="one_handed.quick_strike") strike=i;
                app.player_.talents().resetCooldowns();
                app.tryUseTalent(strike,{11,10});
                check(app.player_.stats().hp>50,"Hungering Blade: hits on cursed foes heal you");
                clearFoes();
            }
            // Saboteur.
            arena(PlayerClass::Thief);
            {
                const auto cracker=ranked("saboteur.firecracker",1);
                auto* a=foe({14,10}); auto* b=foe({14,11}); app.updateFieldOfView();
                cast(cracker,{14,10});
                check(a->statusEffects().has(StatusEffectType::Blinded) && b->statusEffects().has(StatusEffectType::Burn),"Firecracker blinds and burns");
                clearFoes();
                const auto demo=ranked("saboteur.demolition",1);
                auto* c=foe({14,10}); app.updateFieldOfView();
                cast(demo,{14,10});
                // The enemies get their turn, then it blows as yours begins.
                check(c->stats().hp<90 && c->statusEffects().has(StatusEffectType::Burn) && app.storms_.empty(),"Demolition blows once your next turn begins");
                clearFoes();
                ranked("saboteur.trap_sense",1);
                const auto caltrops=ranked("saboteur.caltrops",1);
                for (int i=0;i<4;++i) cast(caltrops,{12+i*2,12});
                check(app.traps_.size()>8,"Trap Sense: more traps set at once");
                app.traps_.clear();
            }
            // Stonefist.
            arena(PlayerClass::Warrior);
            {
                const auto tremor=ranked("stonefist.tremor",1);
                auto* a=foe({11,10}); auto* b=foe({10,9}); app.updateFieldOfView();
                cast(tremor,app.player_.position());
                check(a->statusEffects().has(StatusEffectType::Pinned) && b->statusEffects().has(StatusEffectType::Pinned),"Tremor Punch pins every foe beside you");
                clearFoes();
                const auto fall=ranked("stonefist.rockfall",1);
                auto* c=foe({14,10}); app.updateFieldOfView();
                cast(fall,{13,10});
                check(app.player_.position().x==13 && c->stats().hp<90,"Rockfall comes down on everything beside you");
                clearFoes(); app.player_.setPosition({10,10});
            }
            // Soul Harvest, Magma and Bloodhound.
            arena(PlayerClass::Mage);
            {
                app.player_.baseStats().intelligence=app.player_.stats().intelligence=30;
                app.player_.talents().learnTalent(findTalentDefinition("resonance.soul_harvest")->ranks[0]);
                auto* a=foe({12,10}); a->statusEffects().apply({StatusEffectType::Misfortune,3,25});
                a->stats().hp=0; app.checkAndHandleDeath(*a); app.removeDeadMonsters();
                int raised=0; for (const auto& m:app.monsters_) raised+=m->allied;
                check(raised==1,"Soul Harvest: a cursed foe rises as your skeleton");
                for (auto& m:app.monsters_) app.scheduler_.remove(*m);
                app.monsters_.clear();
                app.player_.talents().learnTalent(findTalentDefinition("resonance.magma")->ranks[0]);
                const auto spike=ranked("earth.spike",1);
                foe({14,10}); app.updateFieldOfView();
                cast(spike,{14,10});
                check(app.surfaceAt({14,10})==SurfaceType::Fire,"Magma: Earth spells leave the ground burning");
                clearFoes();
            }
            arena(PlayerClass::Thief);
            {
                check(wield("hunting_bow"),"Take up a bow for the hunt");
                app.player_.talents().learnTalent(findTalentDefinition("resonance.bloodhound")->ranks[0]);
                const auto shot=ranked("bow.quick_shot",1);
                auto* a=foe({14,10}); a->statusEffects().apply({StatusEffectType::Bleed,3,2}); app.updateFieldOfView();
                app.logMessages_.clear();
                cast(shot,{14,10});
                check(sawLog("Blood scent"),"Bloodhound: bow hits on bleeding foes strike harder");
                clearFoes();
            }
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
            check(!app.player_.statusEffects().has(StatusEffectType::Stun),"The fall shakes off the stun, so you act first below");
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
                    warned=warned || app.logMessages_[i].find("kindles with lightning")!=std::string::npos;
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

        // Hybrid trees open on their colours, held in any tree. Until then they
        // are silhouettes: seen once you hold either colour, named once you hold both.
        setup(PlayerClass::Warrior);
        {
            const auto invest=[&](const char* id,int rank) {
                app.player_.talents().learnTalent(findTalentDefinition(id)->ranks[0]);
                app.player_.talents().setRank(app.player_.talents().knownTalents().size()-1,rank);
            };
            check(!hiddenTreeAvailable(app.player_,"spellblade") && !hiddenTreeAvailable(app.player_,"shadow_archer") &&
                  !hiddenTreeAvailable(app.player_,"animation"),"Hybrid trees start locked");
            const auto* archer=findTree("shadow_archer");
            check(!hybridGlimpsed(app.player_,"shadow_archer"),"A hybrid you hold neither colour of stays unseen");
            check(treePurchaseReason(app.player_,PlayerClass::Warrior,*archer).find("Hunt 0/6")!=std::string::npos,
                  "A locked hybrid tree says which colours it needs, and how far you are");
            (void)invest; giveColour(app.player_,Affinity::Hunt,6);
            check(!hiddenTreeAvailable(app.player_,"shadow_archer") && hybridGlimpsed(app.player_,"shadow_archer") && !hybridNamed(app.player_,"shadow_archer"),
                  "One colour shows its silhouette, nameless");
            giveColour(app.player_,Affinity::Guile,6);
            check(hiddenTreeAvailable(app.player_,"shadow_archer") && hybridNamed(app.player_,"shadow_archer"),"Shadow Archer opens with Hunt 6 and Guile 6");
            giveColour(app.player_,Affinity::Flame,6);
            check(!hiddenTreeAvailable(app.player_,"spellblade"),"Spellblade needs Steel and Arcane, not just any magic");
            giveColour(app.player_,Affinity::Steel,6); giveColour(app.player_,Affinity::Arcane,6);
            check(hiddenTreeAvailable(app.player_,"spellblade"),"Spellblade opens with Steel 6 and Arcane 6");
            giveColour(app.player_,Affinity::Dark,6);
            check(hiddenTreeAvailable(app.player_,"animation"),"Animation opens with Dark 6 and another school at 6");
            check(!hiddenTreeAvailable(app.player_,"blood_magic"),"Blood Magic still needs the altar");
            // Ascendancies by colour, not class.
            Player probe({0,0},statsForClass(PlayerClass::Mage),TalentSet{});
            check(!ascendancyQualified(probe,"juggernaut") && !ascendancyQualified(probe,"paragon"),"An unbuilt character qualifies for no ascendancy");
            giveColour(probe,Affinity::Steel,8); giveColour(probe,Affinity::Guard,6);
            check(ascendancyQualified(probe,"juggernaut"),"A mage with Steel 8 and Guard 6 may become a Juggernaut");
            giveColour(probe,Affinity::Flame,6);
            check(!ascendancyQualified(probe,"elementalist"),"One element is not enough for the Elementalist");
            giveColour(probe,Affinity::Frost,6);
            check(ascendancyQualified(probe,"elementalist"),"Two elements at 6 make an Elementalist");
            giveColour(probe,Affinity::Arcane,3);
            check(ascendancyQualified(probe,"paragon"),"Any five colours at 3 make a Paragon");
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
                if (lord) {
                    const int asleep=lord->stats().hp;
                    const bool dark=app.darknessEnabled_; app.darknessEnabled_=false; // every tile lit
                    for (int i=0;i<20;++i) app.tickSurfaces();
                    app.darknessEnabled_=dark;
                    check(lord->stats().hp==asleep,"Asleep, the Vampire Lord doesn't burn in the light (found in playtest)");
                }
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
        // The middle of a HUD element (its place follows the play screen's width).
        const auto clickRect=[&](sf::FloatRect r) { click(static_cast<int>(r.position.x+r.size.x/2),static_cast<int>(r.position.y+r.size.y/2)); };
        auto clickOn=[&](const sf::FloatRect& r,sf::Mouse::Button button=sf::Mouse::Button::Left) {
            const auto c=screen::center(r); click(c.x,c.y,button);
        };
        auto release=[&](int x,int y) { app.handleEvent(sf::Event::MouseButtonReleased{sf::Mouse::Button::Left,{x,y}}); };
        auto addItem=[&](const char* id) { app.player_.inventory().add(std::make_unique<Item>(*findItemDefinition(id),app.nextItemId_++)); };
        // Every combat sound family is on disk and loaded.
        {
            bool all=true;
            for (const char* family:{"slash","pierce","blunt","fire","frost","lightning","water","arcane","shadow","light","earth","rot","blood",
                                     "crit_gore","crit_bone","crit_fire","crit_frost","crit_storm","crit_magic","dodge","levelup","death","chest_open","mimic"})
                all&=app.soundManager_.hasFamily(family);
            check(all,"Every combat sound family loads");
            bool voiced=true;
            for (int t=0; t<=static_cast<int>(MonsterType::Forgemaster); ++t)
                for (const char* event:{"alert","hurt","death"})
                    voiced&=app.soundManager_.hasFamily(std::string("voice_")+monsterVoice(static_cast<MonsterType>(t))+"_"+event);
            check(voiced,"Every monster has a voice for spotting you, pain and death");
        }
        // The death screen lists the blows that led there, with what dealt them.
        setup(PlayerClass::Warrior);
        {
            app.resetHarms();
            const Position at=app.player_.position();
            auto made=createMonster(MonsterType::Ogre,{at.x+1,at.y}); auto* ogre=made.get();
            app.scheduler_.add(*ogre); app.monsters_.push_back(std::move(made));
            app.player_.stats().dexterity=0;
            const auto slam=[&] {
                ogre->setPosition({at.x+1,at.y}); app.player_.setPosition(at);
                ogre->intent()=EnemyIntent{ogre->position(),at,0,0,4,IntentKind::StunStrike}; ogre->recoveryActions=0;
                app.currentActor_=ogre; app.processMonsterTurns();
            };
            for (int attempt=0;attempt<20 && app.harms_.empty();++attempt) slam();
            check(app.harms_.size()==1 && app.harms_.back().source==ogre->name() && app.harms_.back().amount>0 &&
                  app.harms_.back().hp==app.player_.stats().hp,"A monster's blow is remembered with its name and the life left");
            app.player_.statusEffects().active().clear();
            app.player_.statusEffects().apply({StatusEffectType::Poison,3,2});
            app.advanceTurnsUntilPlayerCanAct();
            check(app.harms_.size()==2 && app.harms_.back().source=="Poison" && app.harms_.back().clock>app.harms_.front().clock,
                  "Poison ticking on the player is remembered as poison");
            app.player_.stats().hp=1; app.harmHp_=1;
            for (int attempt=0;attempt<20 && app.mode_!=GameMode::GameOver;++attempt) slam();
            check(app.mode_==GameMode::GameOver && app.harms_.back().hp==0 && app.harms_.back().source==ogre->name(),
                  "The killing blow ends the recap");
            for (int i=0;i<12;++i) { app.harmClock_+=i%3==0; app.harmSource_=i%2?"Goblin Archer":"Poison"; app.harmHp_=10; app.player_.stats().hp=9; app.noteHarm(); }
            app.player_.stats().hp=0; app.harmHp_=9; app.harmSource_=ogre->name();
            app.player_.statusEffects().apply({StatusEffectType::Stun,1,0}); app.noteHarm(); app.harmSource_.clear();
            check(app.harms_.size()==Application::kRecapLength && app.harms_.back().state.find("Stun")!=std::string::npos,
                  "The recap keeps only the last blows, and what was holding you");
            snapshot("ui-death-recap.png");
            app.selectClass(PlayerClass::Mage);
            check(app.harms_.empty(),"A new character starts with no recap");
            const auto* staff=app.player_.inventory().equipped(EquipmentSlot::Weapon);
            const auto* robes=app.player_.inventory().equipped(EquipmentSlot::Armour);
            check(staff && staff->definition()->id==std::string("ash_staff") && robes && robes->definition()->id==std::string("woven_robes") &&
                  app.player_.inventory().equipped(EquipmentSlot::Charm) && app.groundItems_.empty(),
                  "A new character starts wearing its class's gear, none of it left on the floor");
        }
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
        check(app.player_.abilityPoints()==earnedAbilityPoints(1)-1 && app.player_.talents().rankOf((*treeNodes(fire)[0]).id)==1,"Learn button spends exactly one ability point");
        // More trees than fit: the columns scroll with the wheel and follow the keyboard.
        {
            check(app.treeScrollMax(0)>0 || app.treeScrollMax(1)>0 || app.treeScrollMax(2)>0,"With the new trees, the columns overflow and scroll");
            std::size_t acrobatics=0; while(std::string(kTalentTrees[acrobatics].id)!="acrobatics") ++acrobatics;
            std::size_t alchemy=0; while(std::string(kTalentTrees[alchemy].id)!="alchemy") ++alchemy;
            std::size_t oneHanded=0; while(std::string(kTalentTrees[oneHanded].id)!="one_handed") ++oneHanded;
            const float before=app.talentTreeAbilityRect(alchemy,0).position.y;
            check(before+50>app.treeViewBottom_,"Some trees start below the fold");
            const int column=app.treeColumnOf(alchemy);
            const int overAlchemy=static_cast<int>(app.talentTreeAbilityRect(alchemy,0).position.x)+10;
            const auto others=app.treeScroll_;
            // Scroll down notch by notch until Alchemy is in view.
            for (int notch=0;notch<40 && app.talentTreeAbilityRect(alchemy,0).position.y+50>app.treeViewBottom_;++notch)
                app.handleEvent(sf::Event::MouseWheelScrolled{sf::Mouse::Wheel::Vertical,-1.f,{overAlchemy,300}});
            const auto icon=app.talentTreeAbilityRect(alchemy,0);
            const auto c=static_cast<std::size_t>(column);
            check(app.treeScroll_[c]>0 && app.treeScroll_[c]<=app.treeScrollMax(column) && icon.position.y<before && icon.position.y+50<=app.treeViewBottom_,
                  "The mouse wheel scrolls the column it's over");
            bool othersStill=true;
            for (std::size_t k=0;k<3;++k) if (k!=c) othersStill&=app.treeScroll_[k]==others[k];
            check(othersStill,"...and leaves the other columns where they were");
            click(static_cast<int>(icon.position.x)+10,static_cast<int>(icon.position.y)+10);
            check(app.treeSelection_==alchemy,"Clicks land on the scrolled rows");
            snapshot("ui-talent-scroll.png");
            app.handleEvent(sf::Event::MouseWheelScrolled{sf::Mouse::Wheel::Vertical,20.f,{overAlchemy,300}});
            check(app.treeScroll_[c]==0,"Scrolling stops at the top");
            app.treeSelection_=oneHanded; app.handleTreeKey(sf::Keyboard::Key::Up,false);
            const auto picked=static_cast<std::size_t>(app.treeColumnOf(app.treeSelection_));
            check(app.treeSelection_!=oneHanded && app.treeScroll_[picked]>0 && app.talentTreeAbilityRect(app.treeSelection_,0).position.y+50<=app.treeViewBottom_,
                  "Browsing with the keyboard scrolls the selected tree into view");
            app.treeViewBottom_=712; app.treeScroll_={}; app.treeSelection_=fire;
        }
        {
            // Three ranks: the third changes how Fireball plays.
            const auto& fireball=*findTalentDefinition("fire.fireball");
            check(fireball.maxRank()==3 && fireball.ranks[2].splashSurface==3 && fireball.ranks[1].splashSurface==0 && !fireball.mastery.empty() &&
                  fireball.ranks[2].damagePercent==200 && fireball.ranks[2].manaCost>fireball.ranks[0].manaCost,
                  "Rank 3 Fireball leaves fire behind, at double damage and a higher mana cost");
            const int pointsBefore=app.player_.abilityPoints(), oldRank=app.player_.talents().rankOf(fireball.id);
            app.player_.abilityPoints()=10;
            while (purchaseAbility(app.player_,*findTalentDefinition((*treeNodes(fire)[0]).id))) {}
            check(app.player_.talents().rankOf((*treeNodes(fire)[0]).id)==3,"Abilities rank up to their last rank");
            for (std::size_t i=0;i<app.player_.talents().knownTalents().size();++i)
                if (app.player_.talents().knownTalents()[i].id==(*treeNodes(fire)[0]).id) app.player_.talents().setRank(i,1);
            app.player_.abilityPoints()=pointsBefore; (void)oldRank;
            Player probe({0,0},statsForClass(PlayerClass::Mage),TalentSet{});
            probe.abilityPoints()=0; probe.utilityPoints()=0; grantXp(probe,xpForNextLevel(1));
            check(probe.level()==2 && probe.abilityPoints()==1 && probe.utilityPoints()==1,"A level brings one ability point and one utility point");
            grantXp(probe,xpForNextLevel(2));
            check(probe.level()==3 && probe.abilityPoints()==2 && probe.utilityPoints()==2,"Every level does");
            // Utility trees open without tree points and take utility points.
            probe.trees()={{"bow",false}}; probe.treePoints()=0; probe.abilityPoints()=0;
            const auto* plate=findTree("heavy_armour");
            check(purchaseTree(probe,PlayerClass::Mage,*plate) && probe.treePoints()==0,"A utility tree opens at level 3 without a tree point");
            check(!treePurchaseReason(probe,PlayerClass::Mage,*findTree("acrobatics")).empty(),"A second utility tree waits for level 5");
            const auto& brace=*treeNodes("heavy_armour")[0];
            check(purchaseAbility(probe,brace) && probe.utilityPoints()==1 && probe.abilityPoints()==0,"Utility ranks spend utility points, not ability points");
        }
        click(950,628); snapshot("ui-binding.png");
        check(app.bindingTalent_,"Assign button opens the binding picker");
        click(290,440);
        check(!app.bindingTalent_ && app.player_.talents().hotbar()[9]==(*treeNodes(fire)[0]).id,"Picker binds the selected ability to page two");
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
        clickRect(app.levelBadgeRect()); check(app.mode_==GameMode::AttributeAllocation,"The Level up badge opens the attribute choice");
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
            check(app.playerLightRadius()==4,"At 30 favor its boon is yours: your light burns further");
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
        app.pickupItem(); snapshot("ui-fountain.png");
        check(!app.shrineMenu_,"A fountain is simply drunk from: no menu");
        check(app.landmarkUsed_ && app.player_.stats().hp==app.player_.stats().maxHp && !app.player_.statusEffects().has(StatusEffectType::Poison),
            "The healing fountain restores life and washes away poison");
        app.landmark_=LandmarkKind::BloodFont; app.landmarkUsed_=false;
        const int maxBefore=app.player_.baseStats().maxHp;
        app.pickupItem();
        check(app.landmarkUsed_ && app.player_.baseStats().maxHp==maxBefore+4 && app.player_.stats().hp<app.player_.stats().maxHp,
            "The blood font trades current life for permanent maximum life");
        app.landmark_=LandmarkKind::RitualCircle; app.landmarkUsed_=false;
        app.pickupItem();
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

        // Strongbox: open it and its guardians burst out around you as the loot spills.
        placeAltar(LandmarkKind::Strongbox);
        check(app.landmarkTitle().find("Strongbox")!=std::string::npos,"A strongbox is named for its kind");
        snapshot("ui-strongbox-map.png");
        {
            const auto groundBefore=app.groundItems_.size();
            const int hpBefore=app.player_.stats().hp;
            app.pickupItem();
            check(!app.shrineMenu_,"A strongbox opens at a touch, with no menu");
            const auto me=app.player_.position();
            std::set<std::pair<int,int>> sides;
            bool ringed=true;
            for (const auto& m:app.monsters_) {
                const int d=std::max(std::abs(m->position().x-me.x),std::abs(m->position().y-me.y));
                ringed&=d>=2 && d<=3 && m->tactics.alert>0;
                sides.insert({(m->position().x>me.x)-(m->position().x<me.x),(m->position().y>me.y)-(m->position().y<me.y)});
            }
            check(app.landmarkUsed_ && app.monsters_.size()==3 && ringed,"Opening a strongbox: its guardians burst out in a ring around you");
            check(app.player_.stats().hp==hpBefore,"...but none strikes before you can act");
            check(sides.size()>=3,"...from every side, not in a clump");
            check(app.groundItems_.size()>groundBefore,"...while its loot spills on the floor");
            snapshot("ui-strongbox-ambush.png");
            roundTrip();
            check(app.landmark_==LandmarkKind::Strongbox && app.landmarkUsed_,"Save/load keeps the opened strongbox");
            const auto guardians=app.monsters_.size();
            app.pickupItem();
            check(app.monsters_.size()==guardians && !app.shrineMenu_,"An open strongbox does nothing more");
            std::set<int> kinds;
            for (int y=0;y<10;++y) for (int x=0;x<10;++x) kinds.insert(static_cast<int>(app.strongboxVariant({x,y})));
            check(kinds.size()==3,"Strongboxes come in three kinds: Armourer's, Arcanist's and Gilded");
        }

        // A crystal: strike it three times and its captive breaks free, bearing the
        // crystal's power; slain, it leaves gear bearing that power at its strongest.
        placeAltar(LandmarkKind::Essence);
        {
            const auto strike=[&](Position altar) {
                app.landmark_=LandmarkKind::Essence; app.landmarkAltar_=altar; app.landmarkUsed_=false;
                app.map_.setTile(altar.x,altar.y,Tile{TileType::Wall,false,true});
                app.player_.setPosition({altar.x-1,altar.y}); app.updateFieldOfView();
                app.pickupItem();
            };
            const auto freed=[&]() -> Monster* {
                for (auto& m:app.monsters_) if (m->essence && m->stats().hp>0) return m.get();
                return nullptr;
            };
            // One clear crystal and one weeping one, whichever this floor offers first.
            for (const bool weeping:{false,true}) {
                Position altar{0,10};
                for (int y=8;y<=12 && !altar.x;++y) for (int x=7;x<=15 && !altar.x;++x) if (app.essenceAt({x,y}).corrupted==weeping) altar={x,y};
                check(altar.x>0,"Some crystals weep, most don't");
                const auto captive=app.essenceAt(altar);
                const auto& info=essenceInfo(captive.essence);
                app.landmarkAltar_=altar;
                check(app.landmarkLabel(LandmarkKind::Essence,altar)==(weeping?std::string(kWeepingCrystal):std::string(info.crystal)),
                      "A crystal is named only for how it looks");
                strike(altar);
                check(!app.shrineMenu_ && app.crystalHits(altar)==1 && !app.landmarkUsed_ && !freed(),"Striking the crystal cracks it; no menu");
                if (!weeping) snapshot("ui-essence-map.png");
                app.pickupItem();
                check(app.crystalHits(altar)==2 && !freed(),"A second strike spreads the cracks");
                if (!weeping) snapshot("ui-essence-cracked.png");
                app.pickupItem();
                Monster* m=freed();
                check(app.landmarkUsed_ && m && m->type()==captive.type && m->tier()==(weeping?MonsterTier::Nightmare:MonsterTier::Elite) &&
                      m->corrupted==weeping && m->name().find(info.look)==0 && m->tactics.alert>0,"The third strike shatters it and frees its captive");
                if (!m) continue;
                const auto baseline=createMonster(captive.type,m->position(),m->tier());
                app.scaleDungeonMonster(*baseline,floorDepth(app.currentFloor_));
                check(m->stats().maxHp>=baseline->stats().maxHp*2-1,"...with at least double life");
                if (captive.essence==Essence::Haste || weeping) check(m->stats().speed>baseline->stats().speed,"...faster, if restless or weeping");
                if (captive.essence!=Essence::Haste) {
                    AIDecision blow; blow.type=AIActionType::Attack; blow.target=&app.player_;
                    app.essenceStrike(*m,blow);
                    const auto want=captive.essence==Essence::Flame?StatusEffectType::Burn:captive.essence==Essence::Frost?StatusEffectType::Chill:StatusEffectType::Shock;
                    check(blow.effectToApply && blow.effectToApply->type==want,"...and its blows carry the crystal's power");
                }
                if (!weeping) {
                    snapshot("ui-essence-freed.png");
                    roundTrip();
                    m=freed();
                    check(m && m->name().find(info.look)==0 && static_cast<Essence>(m->essence)==captive.essence,"Save/load keeps the freed captive");
                }
                if (!m) continue;
                const auto ground=app.groundItems_.size();
                m->stats().hp=0; app.checkAndHandleDeath(*m); app.removeDeadMonsters();
                bool perfect=false;
                for (std::size_t i=ground;i<app.groundItems_.size();++i) {
                    const auto& item=*app.groundItems_[i];
                    for (const auto& roll:item.affixes()) {
                        const auto* affix=findAffix(roll.id);
                        perfect|=roll.value==affix->maximum+item.rollTier()*affix->perTier;
                    }
                }
                check(app.groundItems_.size()>=ground+(weeping?2:1) && perfect,
                      weeping?"A weeping crystal's captive leaves two such items":"Slain, it leaves a rare item with an affix at its highest roll");
            }
        }

        // Breach: the rift pours out creatures until its keeper falls or it closes.
        placeAltar(LandmarkKind::Breach);
        {
            app.pickupItem();
            check(!app.shrineMenu_,"The rift tears open at a touch, with no menu");
            const auto riftborn=[&]{ return std::count_if(app.monsters_.begin(),app.monsters_.end(),[](const auto& m){return m->rift && m->stats().hp>0;}); };
            check(app.breachTurns_==kBreachTurns-1 && riftborn()>=1 && app.breachAt_.x==app.landmarkAltar_.x,"Touching the rift opens the breach");
            const auto before=riftborn();
            for (int i=0;i<4;++i) app.tickBreach();
            check(riftborn()>before+3 && app.breachRadius()>2,"Each turn the rift widens and more pour out");
            app.updateFieldOfView(); snapshot("ui-breach-open.png");
            roundTrip();
            check(app.breachTurns_==kBreachTurns-5 && riftborn()>before+3,"Save/load keeps the open breach and its creatures");
            for (auto& m:app.monsters_) if (m->rift==1 && m->stats().hp>0) { m->stats().hp=0; app.checkAndHandleDeath(*m); }
            app.removeDeadMonsters();
            check(app.breachKills_>=5,"Riftborn kills are counted");
            for (int i=0;i<3;++i) app.tickBreach();
            Monster* keeper=nullptr;
            for (auto& m:app.monsters_) if (m->rift==2) keeper=m.get();
            check(keeper && keeper->name()=="Rift-keeper" && keeper->tier()==MonsterTier::Nightmare,"The Rift-keeper comes through");
            const auto ground=app.groundItems_.size();
            if (keeper) { keeper->stats().hp=0; app.checkAndHandleDeath(*keeper); }
            app.removeDeadMonsters();
            check(app.breachTurns_==0 && riftborn()==0,"Slaying the keeper seals the breach, and its creatures vanish");
            check(app.groundItems_.size()>=ground+3,"...and spills two rare items, plus something for the kills");
            // Left to run its course, it closes and takes its creatures back.
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.landmarkUsed_=false;
            app.pickupItem();
            for (int i=0;i<kBreachTurns;++i) app.tickBreach();
            app.removeDeadMonsters();
            check(app.breachTurns_==0 && riftborn()==0,"Left alone, the breach closes and drags its creatures back");
        }

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
        snapshot("ui-ascendancy-choice.png");
        clickOn(screen::ascendRow(0)); clickOn(screen::kAscendLearn);
        check(app.player_.ascendancy.empty() && app.ascendancyChoice_,"An ascendancy your colours don't allow can't be taken");
        app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::Escape});
        check(!app.ascendancyMenu_ && app.player_.ascendancy.empty(),"The choice can wait until your build allows one");
        app.openAscendancy();
        check(app.ascendancyMenu_ && app.ascendancyChoice_,"It comes back when you open the ascendancy screen");
        // (Qualifying is checked above; here the choice is made as if it were allowed.)
        app.player_.ascendancy="juggernaut"; app.ascendancyChoice_=false; app.openAscendancy();
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
        clickRect(app.actionButtonRect(0)); check(app.inventoryOpen_ && app.player_.stats().hp==100,"Action-bar Bag is free");
        click(1190,35); clickRect(app.actionButtonRect(3));
        check(app.player_.stats().hp==99 && app.player_.statusEffects().active()[0].turnsRemaining==2,"Action-bar Wait advances exactly one turn");
        app.player_.statusEffects().active().clear(); clickRect(app.actionButtonRect(4));
        check(app.restTurns_>0,"Action-bar Rest starts resting");
        clickRect(app.actionButtonRect(0)); check(app.restTurns_==0 && !app.inventoryOpen_,"First click during rest only stops resting");
        app.requestTalent(1); clickRect(app.cancelButtonRect());
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
            app.player_.level()=level; app.player_.abilityPoints()=earnedAbilityPoints(level)-3; app.player_.utilityPoints()=earnedUtilityPoints(level);
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
        const auto deepEvents=app.extraLandmarks_.size()+(app.landmark_!=LandmarkKind::None);
        app.mode_=GameMode::Town; app.travelFloor(1,true);
        check(app.extraLandmarks_.empty(),"A floor's extra events stay on that floor (found in testing)");
        check(app.currentFloor_==1 && app.groundItems_.size()==1 && app.groundItems_[0]->instanceId()==homeItem,"Switching dungeons preserves existing ground loot");
        raiseLevel(18); app.mode_=GameMode::Town; app.travelFloor(12,true);
        check(app.monsters_.size()==population && app.monsters_[0]->stats().hp==originalHp-1,
            "Revisiting after level-ups preserves damaged enemy population");
        check(app.extraLandmarks_.size()+(app.landmark_!=LandmarkKind::None)==deepEvents,"Returning keeps every event on the floor");
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

        bool packsSafe=true, varied=false, rares=false, named=false, humans=false;
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
                if (floorTheme(floor).region!=FloorRegion::Crypts) humans |= spawn.type==MonsterType::Archer || spawn.type==MonsterType::Torchbearer;
            }
            packsSafe &= rareCount<=1 && uniqueCount<=1;
            if(!spawns.empty()) packsSafe &= spawns[0].tier==MonsterTier::Base && !isUniqueMonster(spawns[0].type);
            rares |= rareCount>0; named |= uniqueCount>0;
        }
        check(packsSafe && varied && rares && named,"500 generated floors: safe unique positions, opening packs, rare caps and new roster");
        check(!humans,"Goblins hold the Ruins: no human archers or torchbearers before the Crypts");

        // --- Keywords: mechanics named in descriptions, explained on hover.
    {
        check(keywordFor("burns,") && std::string(keywordFor("burns,")->name)=="Burn" && keywordFor("Stunned.") && !keywordFor("sword"),
              "A keyword is found in its word forms, past the punctuation around it");
        const auto rally=keywordsIn(findTalentDefinition("warbanner.rally")->ranks[0].description);
        check(!rally.empty() && std::string(rally.front()->name)=="Shaken","Rally Cry's description names Shaken");
        const auto pommel=keywordsIn(findTalentDefinition("one_handed.pommel")->ranks[0].description);
        check(pommel.size()==1 && std::string(pommel[0]->name)=="Stun","Each keyword is listed once");
    }
        // The Encounter Lab: every class and build starts level 6 with the
        // same budget spent, in the same seeded rooms, and logs the attempt.
        {
            bool budgets=true, rooms=true, hybrids=true, same=true;
            std::vector<Position> firstLayout;
            for (auto cls:{PlayerClass::Warrior,PlayerClass::Mage,PlayerClass::Thief})
                for (int build=0; build<3; ++build) {
                    app.labBuild_=build; app.labSeed_=3; app.startLab(cls);
                    budgets=budgets && app.labRun_ && app.player_.level()==6 && app.player_.abilityPoints()==0 && app.player_.utilityPoints()==0 &&
                            app.mode_==GameMode::Playing && app.player_.unspentAttributePoints()==0;
                    rooms=rooms && app.monsters_.size()==8 && app.map_.tileAt(app.floorExit_.x,app.floorExit_.y).type==TileType::Door;
                    std::vector<Position> layout;
                    for (const auto& m:app.monsters_) layout.push_back(m->position());
                    if (firstLayout.empty()) firstLayout=layout;
                    for (std::size_t i=0;i<layout.size() && i<firstLayout.size();++i) same=same && layout[i].x==firstLayout[i].x && layout[i].y==firstLayout[i].y;
                    if (build==1) {
                        bool resonant=false;
                        for (const auto* r:resonances()) resonant=resonant || app.player_.talents().rankOf(r->id)>0;
                        hybrids=hybrids && resonant;
                    }
                }
            check(budgets && rooms,"Encounter Lab: every class and build starts at level 6, its points all spent, in three rooms of eight foes");
            check(hybrids,"Encounter Lab: a hybrid build wakes its resonance");
            check(same,"Encounter Lab: the same seed lays out the same rooms for every build");
            bool placed=true;
            for (unsigned seed=1; seed<=20; ++seed) {
                app.labSeed_=seed; app.startLab(PlayerClass::Warrior);
                std::set<std::pair<int,int>> taken{{app.player_.position().x,app.player_.position().y}};
                for (const auto& m:app.monsters_) placed=placed && app.map_.isWalkable(m->position().x,m->position().y) && taken.emplace(m->position().x,m->position().y).second;
                const auto route=findPath(app.map_,app.player_.position(),app.floorExit_);
                placed=placed && route && !route->empty();
            }
            check(placed,"Encounter Lab: across 20 seeds every foe stands on open floor of its own, and the stairs can be reached");
            app.labSeed_=4; app.startLab(PlayerClass::Warrior);
            bool moved=false;
            for (std::size_t i=0;i<app.monsters_.size() && i<firstLayout.size();++i) moved=moved || app.monsters_[i]->position().x!=firstLayout[i].x || app.monsters_[i]->position().y!=firstLayout[i].y;
            check(moved,"Encounter Lab: another seed shifts the rooms");
            app.updateFieldOfView(); snapshot("encounter-lab.png");
            // Only the files this test writes are read and removed; real attempts stay.
            const auto files=[&]{ std::set<std::filesystem::path> found; std::error_code e;
                for (const auto& f:std::filesystem::directory_iterator("encounter-lab",e)) if (f.path().extension()==".txt") found.insert(f.path());
                return found; };
            const auto existing=files();
            const auto logs=[&]{ int n=0; for (const auto& f:files()) n+=!existing.count(f); return n; };
            const int before=0;
            app.processMonsterTurns(); app.advanceTurnsUntilPlayerCanAct();
            app.player_.stats().hp=0; app.checkAndHandleDeath(app.player_);
            check(app.mode_==GameMode::GameOver && !app.labRun_ && logs()==before+1,"Encounter Lab: a death writes the attempt's log");
            app.startLab(PlayerClass::Mage);
            app.monsters_.clear(); app.scheduler_=TurnScheduler{}; app.scheduler_.add(app.player_); app.currentActor_=&app.player_;
            app.player_.setPosition({app.floorExit_.x-1,app.floorExit_.y});
            app.tryMovePlayer(1,0);
            check(app.mode_==GameMode::ClassSelection && !app.labRun_ && logs()==before+2,"Encounter Lab: the stairs finish it, log written, back to the class screen");
            std::string text;
            for (const auto& f:files()) {
                if (existing.count(f)) continue;
                { std::ifstream in(f); std::stringstream all; all<<in.rdbuf();
                  if (all.str().find("Outcome: died")!=std::string::npos) text=all.str(); }
                std::filesystem::remove(f);
            }
            check(text.find("Trees:")!=std::string::npos && text.find("T1  you  life")!=std::string::npos && text.find("Outcome: died")!=std::string::npos,
                  "Encounter Lab: the log holds the build, each turn's state and the outcome");
            app.labMode_=false;
        }

        // The sandbox: spawn anything, level up, respec, open any tree, never die, never save.
        {
            app.startSandbox(PlayerClass::Warrior);
            check(app.sandboxRun_ && app.player_.sandbox,"Sandbox: a sandbox run starts");
            purchaseTree(app.player_,PlayerClass::Warrior,*findTree("one_handed"));
            app.closeTalentTrees(); app.mode_=GameMode::Playing;
            const auto foes=app.monsters_.size();
            app.sandboxTier_=1; app.sandboxSpawn(static_cast<int>(MonsterType::Lich));
            const auto& spawned=*app.monsters_.back();
            check(app.monsters_.size()==foes+1 && spawned.type()==MonsterType::Lich && spawned.tier()==MonsterTier::Elite &&
                  std::max(std::abs(spawned.position().x-app.player_.position().x),std::abs(spawned.position().y-app.player_.position().y))<=5,
                  "Sandbox: any foe, at any tier, appears beside you");
            app.sandboxRarity_=2; const auto& sword=*findItemDefinition("iron_sword");
            const auto bag=app.player_.inventory().items().size();
            app.sandboxItem(sword);
            check(app.player_.inventory().items().size()==bag+1 && app.player_.inventory().items().back()->definition()==&sword &&
                  app.player_.inventory().items().back()->rarity()==ItemRarity::Rare,"Sandbox: any base, at any rarity, lands in your bag");
            app.sandboxCharacter(1);
            check(app.player_.level()==6,"Sandbox: +5 levels");
            app.sandboxRespec();
            check(app.player_.trees().empty() && app.player_.abilityPoints()==earnedAbilityPoints(6) && app.player_.utilityPoints()==earnedUtilityPoints(6) &&
                  app.player_.treePoints()==earnedTreePoints(6) && app.player_.talents().rankOf("basic.attack")==1,"Sandbox: respec returns every point and keeps the basics");
            check(treePurchaseReason(app.player_,PlayerClass::Warrior,*findTree("shadow_archer")).empty() &&
                  treePurchaseReason(app.player_,PlayerClass::Warrior,*findTree("fire")).empty(),"Sandbox: any tree can be opened, hidden ones included");
            app.sandboxCharacter(10); app.player_.stats().hp=0; app.checkAndHandleDeath(app.player_);
            check(app.mode_==GameMode::Playing && app.player_.stats().hp==app.player_.stats().maxHp,"Sandbox: god mode refuses death");
            app.sandboxCharacter(10);
            app.logMessages_.clear(); app.saveGame();
            check(!app.logMessages_.empty() && app.logMessages_.back().find("isn't saved")!=std::string::npos,"Sandbox: nothing is saved");
            app.sandboxWorld(4);
            check(app.chestExists_ && app.chestMimic_ && !app.map_.isWalkable(app.chestPosition_.x,app.chestPosition_.y),"Sandbox: a mimic on demand");
            // Max out: level 30, every lore, every dungeon and trial open.
            app.sandboxCharacter(12);
            check(app.player_.level()==30 && app.player_.knowsLore("winter_road") && app.thornwoodOpen() && app.rimeholtOpen() &&
                  app.player_.trialsCleared==(1<<kTrialCount)-1 && app.player_.ascendancyPoints==kTrialCount && app.player_.unspentAttributePoints()>0,
                  "Sandbox: Max out gives level 30, every lore, every trial won, and points to spend");
            check(ascendancyQualified(app.player_,"gravelord") && ascendancyQualified(app.player_,"forgeknight"),"Sandbox: any ascendancy can be chosen");
            app.sandboxCharacter(13);
            check(app.ascendancyMenu_ && app.ascendancyChoice_,"Sandbox: Choose ascendancy opens the choice");
            app.chooseAscendancy("gravelord");
            check(app.player_.ascendancy=="gravelord","Sandbox: and you take one");
            app.ascendancyMenu_=false; app.ascendancyChoice_=false;
            // Travel: any depth of any dungeon, at once.
            app.sandboxTravel(5,10);
            check(app.mode_==GameMode::Playing && app.currentFloor_==kRimeLast && app.boss_ && app.boss_->type()==MonsterType::WinterKing,
                  "Sandbox: Travel takes you straight to the Winter King");
            app.mode_=GameMode::Town;
            app.sandboxTravel(4,2);
            check(app.mode_==GameMode::Playing && app.currentFloor_==kThornFirst+1,"Sandbox: even from town, straight to Thornwood depth 2");
            app.sandboxMenu_=true;
            for (int tab=0; tab<5; ++tab) { app.sandboxTab_=tab; snapshot(("sandbox-tab"+std::to_string(tab)+".png").c_str()); }
            app.sandboxMenu_=false;
            app.selectClass(PlayerClass::Mage);
            check(!app.sandboxRun_ && !app.player_.sandbox,"Sandbox: a normal run leaves it behind");
        }

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

        // The Ashen Foundry: locked until Grik's key, five floors of furnaces,
        // heat, splitting golems and the Forgemaster at the bottom.
        {
            app.selectClass(PlayerClass::Warrior);
            app.mode_=GameMode::Town;
            app.travelFloor(kFoundryFirst,true);
            check(app.mode_==GameMode::Town && app.currentFloor_!=kFoundryFirst,"The Foundry is locked without the Foreman's key");
            app.mode_=GameMode::Playing; app.currentFloor_=3; app.regenerateLevel(77);
            auto made=createMonster(MonsterType::GoblinCaptain,{app.player_.position().x+1,app.player_.position().y}); auto* grik=made.get();
            app.scheduler_.add(*grik); app.monsters_.push_back(std::move(made));
            grik->stats().hp=0; app.checkAndHandleDeath(*grik); app.removeDeadMonsters();
            check(app.loreDrops_.size()==1 && app.loreDrops_[0].id=="foreman_key","Grik drops the Foreman's key");
            app.player_.setPosition(app.loreDrops_[0].at); app.pickupItem();
            check(app.foundryOpen(),"Taking the key opens the Foundry");
            bool furnaces=true, saves=true, boss=false;
            for (int floor=kFoundryFirst; floor<=kFoundryLast; ++floor) {
                app.currentFloor_=floor; app.regenerateLevel(900+floor);
                int count=0; for (const auto& p:app.props_) count+=p.kind==PropKind::Furnace;
                furnaces=furnaces && count>=3;
                for (const auto& m:app.monsters_) boss=boss || m->type()==MonsterType::Forgemaster;
                if (floor==kFoundryFirst+1 || floor==kFoundryLast) {
                    // Stand by a furnace, with a foe in view, for the snapshot.
                    for (const auto& p:app.props_) if (p.kind==PropKind::Furnace) {
                        for (int d=-1; d<=1; ++d) if (app.map_.isWalkable(p.pos.x+d,p.pos.y+1) && !app.isOccupied({p.pos.x+d,p.pos.y+1},nullptr)) { app.player_.setPosition({p.pos.x+d,p.pos.y+1}); break; }
                        break;
                    }
                    if (floor==kFoundryLast && app.boss_) app.player_.setPosition({app.boss_->position().x,app.boss_->position().y+3});
                    app.updateFieldOfView();
                    snapshot(floor==kFoundryLast?"foundry-boss.png":"foundry.png");
                }
                const auto path=(output/"foundry.txt").string();
                saves=saves && saveGame(app.captureState(false),path) && loadGame(path).has_value();
            }
            check(furnaces,"Every Foundry floor has its furnaces");
            check(boss && app.boss_ && app.boss_->type()==MonsterType::Forgemaster,"The Forgemaster waits on the Foundry's last floor");
            check(saves,"Every Foundry floor saves and loads");
            // Heat: beside a furnace it builds, away it fades, water quenches, too much burns.
            setup(PlayerClass::Warrior);
            app.setProps({{PropKind::Furnace,{11,10}}}); app.map_.setTile(11,10,Tile{TileType::Wall,false,true});
            for (int i=0;i<3;++i) app.tickHeat();
            check(app.player_.statusEffects().magnitudeOf(StatusEffectType::Heat)==3,"Beside a furnace, Heat builds by 1 a turn");
            app.player_.setPosition({5,10}); app.tickHeat();
            check(app.player_.statusEffects().magnitudeOf(StatusEffectType::Heat)==2,"Away from it, Heat fades");
            app.addHeat(10); const int hp=app.player_.stats().hp; app.tickHeat();
            check(app.player_.stats().hp==hp-2,"At 10 Heat or more, it burns");
            app.setSurface({5,10},SurfaceType::Water,0); app.tickHeat();
            check(!app.player_.statusEffects().has(StatusEffectType::Heat),"Water quenches Heat at once");
            app.clearSurfaces(); app.setProps({});
            // Golems split; slaglings burn where they fall.
            app.currentFloor_=kFoundryFirst;
            auto golem=createMonster(MonsterType::SlagGolem,{12,10}); auto* g=golem.get(); app.scheduler_.add(*g); app.monsters_.push_back(std::move(golem));
            g->stats().hp=0; app.checkAndHandleDeath(*g); app.removeDeadMonsters();
            int slaglings=0; Monster* slag=nullptr;
            for (auto& m:app.monsters_) if (m->type()==MonsterType::Slagling && m->stats().hp>0) { ++slaglings; slag=m.get(); }
            check(slaglings==2,"A slain Slag Golem splits into two Slaglings");
            if (slag) {
                const auto at=slag->position();
                slag->stats().hp=0; app.checkAndHandleDeath(*slag); app.removeDeadMonsters();
                check(app.surfaceAt(at)==SurfaceType::Fire,"A slain Slagling leaves the ground burning");
            }
            app.currentFloor_=1;
        }

        // The Trial of the Forge and the Forgeknight.
        {
            setup(PlayerClass::Warrior);
            auto made=createMonster(MonsterType::Forgemaster,{12,10}); auto* smith=made.get(); app.monsters_.push_back(std::move(made));
            app.onBossDefeated(*smith);
            for (auto& m:app.monsters_) app.scheduler_.remove(*m); app.monsters_.clear(); app.loreDrops_.clear();
            check((app.player_.trialKeys & 4) && app.trialAvailability(3).empty(),"The Forgemaster's sigil opens the Trial of the Forge, on its own");
            app.mode_=GameMode::Town;
            check(app.enterTrial(3) && app.boss_ && app.boss_->type()==MonsterType::Forgemaster && app.boss_->name()=="The Anvil-Born",
                  "The Trial of the Forge pits you against the Anvil-Born");
            int furnaces=0; for (const auto& p:app.props_) furnaces+=p.kind==PropKind::Furnace;
            check(furnaces>0,"Its arena burns with furnaces");
            check(!ascendancyQualified(app.player_,"forgeknight"),"The Forgeknight waits for its colours");
            giveColour(app.player_,Affinity::Steel,6); giveColour(app.player_,Affinity::Flame,6);
            check(ascendancyQualified(app.player_,"forgeknight"),"Steel 6 and Flame 6 allow the Forgeknight; any trial won lets you take it");
            app.boss_->stats().hp=0; app.checkAndHandleDeath(*app.boss_); app.removeDeadMonsters();
            check((app.player_.trialsCleared & 4) && app.player_.ascendancyPoints==1 && ascendancyQualified(app.player_,"forgeknight"),
                  "Winning the trial gives a point, and the Forgeknight can be taken");
            snapshot("ui-ascendancy-forge.png");
            app.chooseAscendancy("forgeknight");
            check(app.player_.ascendancy=="forgeknight","You become a Forgeknight");
            app.ascendancyMenu_=false; app.leaveTrialState(); app.mode_=GameMode::Playing;
            // Its nodes.
            const auto learn=[&](const char* id) { app.player_.talents().learnTalent(findTalentDefinition(id)->ranks[0]); return app.player_.talents().knownTalents().size()-1; };
            learn("forgeknight.heat_engine"); app.addHeat(8);
            check(ascendancyGuardBonus(app.player_)>=4,"Heat Engine: every 2 Heat is 1 armour");
            const auto q=learn("forgeknight.quench");
            app.player_.stats().hp=10; app.player_.talents().resetCooldowns(); app.player_.stats().mana=app.player_.stats().maxMana;
            app.tryUseTalent(q,app.player_.position());
            check(!app.player_.statusEffects().has(StatusEffectType::Heat) && app.player_.stats().hp>=10+8*3-2,"Quench: all Heat gone, 3 life healed for each point");
            const auto anvil=learn("forgeknight.anvil");
            app.player_.talents().resetCooldowns(); app.tryUseTalent(anvil,app.player_.position());
            check(app.player_.statusEffects().has(StatusEffectType::Anvil),"Anvil Stance plants you");
            learn("forgeknight.overheat"); app.addHeat(12); const int hp=app.player_.stats().hp; app.tickHeat();
            check(app.player_.stats().hp==hp,"Overheat: Heat no longer burns you");
            app.player_.statusEffects().active().clear(); app.player_.ascendancy.clear();
        }

        // Thornwood Hollow: Veyra's map opens it; thorns cut and slow; the
        // witches grow them, the spiders lay eggs, and the Hollow Mother waits.
        {
            setup(PlayerClass::Warrior);
            app.mode_=GameMode::Town;
            app.travelFloor(kThornFirst,true);
            check(app.mode_==GameMode::Town && app.currentFloor_!=kThornFirst,"Thornwood Hollow is lost without the map");
            app.mode_=GameMode::Playing; app.currentFloor_=10; app.regenerateLevel(78);
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.boss_=nullptr; app.loreDrops_.clear();
            auto made=createMonster(MonsterType::OssuaryWarden,{app.player_.position().x+1,app.player_.position().y}); auto* veyra=made.get();
            app.scheduler_.add(*veyra); app.monsters_.push_back(std::move(made));
            veyra->stats().hp=0; app.checkAndHandleDeath(*veyra); app.removeDeadMonsters();
            bool map=false; for (const auto& d:app.loreDrops_) map=map || d.id=="hollow_map";
            check(map,"Veyra drops a map drawn on bone");
            for (const auto& d:app.loreDrops_) if (d.id=="hollow_map") { app.player_.setPosition(d.at); app.pickupItem(); break; }
            check(app.thornwoodOpen(),"Taking the map opens Thornwood Hollow");
            bool thorny=true, saves=true, boss=false;
            for (int floor=kThornFirst; floor<=kThornLast; ++floor) {
                app.currentFloor_=floor; app.regenerateLevel(950+floor);
                int thorns=0;
                for (int y=0;y<app.map_.height();++y) for (int x=0;x<app.map_.width();++x) thorns+=app.surfaceAt({x,y})==SurfaceType::Thorns;
                thorny=thorny && thorns>=6;
                for (const auto& m:app.monsters_) boss=boss || m->type()==MonsterType::HollowMother;
                if (floor==kThornFirst+1 || floor==kThornLast) {
                    if (floor==kThornLast && app.boss_) app.player_.setPosition({app.boss_->position().x,app.boss_->position().y+3});
                    if (floor==kThornFirst+1) {
                        // A witch's patch beside you, so the snapshot shows the thorns.
                        const auto at=app.player_.position();
                        for (int dx=1;dx<=3;++dx) for (int dy=0;dy<=1;++dy)
                            if (app.map_.isWalkable(at.x+dx,at.y+dy) && app.surfaceAt({at.x+dx,at.y+dy})==SurfaceType::None) app.setSurface({at.x+dx,at.y+dy},SurfaceType::Thorns,0);
                    }
                    app.updateFieldOfView();
                    snapshot(floor==kThornLast?"thornwood-boss.png":"thornwood.png");
                }
                const auto path=(output/"thornwood.txt").string();
                saves=saves && saveGame(app.captureState(false),path) && loadGame(path).has_value();
            }
            check(thorny,"Every Hollow floor is overgrown with thorns");
            check(boss && app.boss_ && app.boss_->type()==MonsterType::HollowMother,"The Hollow Mother waits on the Hollow's last floor");
            check(saves,"Every Hollow floor saves and loads");
            app.mode_=GameMode::Town; app.travelFloor(kThornFirst,true);
            check(app.mode_==GameMode::Playing && app.currentFloor_==kThornFirst,"With the map, the Hollow can be entered from town");

            // Thorns cut and slow you; the Hollow's own walk through unharmed; fire burns them.
            setup(PlayerClass::Warrior);
            app.currentFloor_=kThornFirst;
            app.setSurface(app.player_.position(),SurfaceType::Thorns,0);
            auto houndMade=createMonster(MonsterType::BriarHound,{14,10}); auto* hound=houndMade.get();
            app.scheduler_.add(*hound); app.monsters_.push_back(std::move(houndMade));
            app.setSurface({14,10},SurfaceType::Thorns,0);
            app.tickSurfaces();
            check(app.player_.statusEffects().has(StatusEffectType::Bleed) && app.player_.statusEffects().has(StatusEffectType::Slowed),
                  "Standing in thorns, you bleed and slow");
            check(!hound->statusEffects().has(StatusEffectType::Bleed),"A Briar Hound runs through thorns unharmed");
            app.player_.statusEffects().active().clear();
            app.setSurface({16,10},SurfaceType::Thorns,0); app.setSurface({17,10},SurfaceType::Fire,3);
            app.tickSurfaces(); app.tickSurfaces();
            check(app.surfaceAt({16,10})!=SurfaceType::Thorns,"Fire burns thorns away");
            app.clearSurfaces();
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear();

            // A Rot Witch grows thorns around her.
            app.growThorns({8,10});
            int grown=0; for (int y=7;y<=13;++y) for (int x=5;x<=11;++x) grown+=app.surfaceAt({x,y})==SurfaceType::Thorns;
            check(grown==3,"A Rot Witch grows 3 tiles of thorns a turn around her");
            for (int i=0;i<6;++i) app.growThorns({8,10});
            grown=0; bool far=false;
            for (int y=0;y<app.map_.height();++y) for (int x=0;x<app.map_.width();++x) if (app.surfaceAt({x,y})==SurfaceType::Thorns) {
                ++grown; far=far || std::max(std::abs(x-8),std::abs(y-10))>3; }
            check(grown>3 && !far,"Her thorns creep outward, but never past 3 tiles from her");
            app.clearSurfaces();

            // Brood spiders lay eggs; eggs hatch unless broken.
            auto spiderMade=createMonster(MonsterType::BroodSpider,{8,6}); auto* spider=spiderMade.get();
            spider->tactics.alert=8; // it lays while it hunts you
            app.scheduler_.add(*spider); app.monsters_.push_back(std::move(spiderMade));
            const auto eggs=[&]{ std::vector<Monster*> out; for (auto& m:app.monsters_) if (m->type()==MonsterType::EggSac && m->stats().hp>0) out.push_back(m.get()); return out; };
            const auto spiderlings=[&]{ int n=0; for (auto& m:app.monsters_) n+=m->type()==MonsterType::Spiderling && m->stats().hp>0; return n; };
            for (int i=0;i<4;++i) app.broodTurn(*spider);
            check(eggs().empty(),"A Brood Spider doesn't lay every turn");
            app.broodTurn(*spider);
            check(eggs().size()==1,"Every 5 turns, a Brood Spider lays an egg sac");
            if (!eggs().empty()) {
                auto* egg=eggs().front();
                const int level=app.player_.level(), xp=app.player_.xp();
                for (int i=0;i<3;++i) app.hatchTurn(*egg);
                check(egg->stats().hp>0 && spiderlings()==0,"An egg sac waits a few turns");
                app.hatchTurn(*egg); app.removeDeadMonsters();
                check(eggs().empty() && spiderlings()==2,"Then it splits into two spiderlings");
                check(app.player_.level()==level && app.player_.xp()==xp,"A hatched egg gives nothing");
            }
            for (int i=0;i<5;++i) app.broodTurn(*spider);
            if (!eggs().empty()) {
                auto* second=eggs().front(); second->stats().hp=0; app.checkAndHandleDeath(*second); app.removeDeadMonsters();
            }
            check(eggs().empty() && spiderlings()==2,"An egg broken before it hatches is gone for good");
            // The brood has a limit: no more than 8 eggs and spiderlings on a floor.
            for (int i=0;i<100;++i) { app.broodTurn(*spider); for (auto* e:eggs()) app.hatchTurn(*e); app.removeDeadMonsters(); }
            check(static_cast<int>(eggs().size())+spiderlings()<=9,"A floor never holds more than 8 eggs and spiderlings (a hatching can tip it one over)");
            spider->tactics.alert=0; const auto brood=eggs().size();
            for (int i=0;i<10;++i) app.broodTurn(*spider);
            check(eggs().size()==brood,"A spider that isn't hunting you doesn't lay");
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear();

            // A Thornback's spines cut whoever strikes it in melee.
            auto backMade=createMonster(MonsterType::Thornback,{app.player_.position().x+1,app.player_.position().y}); auto* back=backMade.get();
            back->stats().hp=back->stats().maxHp=999;
            app.scheduler_.add(*back); app.monsters_.push_back(std::move(backMade));
            // Strike until a blow lands (a swing can miss).
            int hp=0; std::size_t since=0;
            for (int i=0;i<10 && back->stats().hp==999;++i) {
                app.player_.stats().hp=app.player_.stats().maxHp; hp=app.player_.stats().hp; since=app.logTotal_;
                app.player_.talents().resetCooldowns(); app.player_.stats().mana=app.player_.stats().maxMana;
                app.currentActor_=&app.player_; app.tryUseTalent(0,back->position());
            }
            bool spines=false;
            const std::size_t fresh=std::min(app.logMessages_.size(),app.logTotal_-since);
            for (std::size_t i=app.logMessages_.size()-fresh;i<app.logMessages_.size();++i) {
                spines=spines || app.logMessages_[i].find("Its spines cut you for 3")!=std::string::npos;
            }
            check(back->stats().hp<999 && spines && app.player_.stats().hp<=hp-3,"Striking a Thornback in melee, its spines cut you for 3");

            // At half life the Hollow Mother calls her brood.
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear();
            auto motherMade=createMonster(MonsterType::HollowMother,{app.player_.position().x+4,app.player_.position().y}); auto* mother=motherMade.get();
            mother->stats().hp=mother->stats().maxHp/2; app.boss_=mother;
            app.scheduler_.add(*mother); app.monsters_.push_back(std::move(motherMade));
            app.broodCalled_=false;
            mother->tactics.alert=8; mother->tactics.lastKnown=app.player_.position();
            for (int i=0;i<4 && !app.broodCalled_;++i) { app.player_.stats().hp=app.player_.stats().maxHp; app.currentActor_=mother; app.processMonsterTurns(); }
            check(app.broodCalled_ && spiderlings()>=3,"At half her life the Hollow Mother calls her brood");
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.boss_=nullptr;
            app.clearSurfaces(); app.player_.statusEffects().active().clear(); app.currentFloor_=1;
        }

        // The Trial of the Hollow and the Beastwarden.
        {
            setup(PlayerClass::Warrior);
            auto made=createMonster(MonsterType::HollowMother,{12,10}); auto* mother=made.get(); app.monsters_.push_back(std::move(made));
            app.onBossDefeated(*mother);
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.groundItems_.clear(); app.loreDrops_.clear();
            check((app.player_.trialKeys & 8) && app.trialAvailability(4).empty(),"The Hollow Mother's sigil opens the Trial of the Hollow, on its own");
            app.mode_=GameMode::Town; app.trialMenu_=true;
            snapshot("ui-trials.png");
            app.trialMenu_=false;
            check(app.enterTrial(4) && app.boss_ && app.boss_->type()==MonsterType::HollowMother && app.boss_->name()=="The Thorn Queen",
                  "The Trial of the Hollow pits you against the Thorn Queen");
            int thorns=0; for (int y=0;y<app.map_.height();++y) for (int x=0;x<app.map_.width();++x) thorns+=app.surfaceAt({x,y})==SurfaceType::Thorns;
            check(thorns>0,"Its arena is overgrown with thorns");
            check(!ascendancyQualified(app.player_,"beastwarden"),"The Beastwarden waits for its colours");
            giveColour(app.player_,Affinity::Hunt,6); giveColour(app.player_,Affinity::Motion,6);
            check(ascendancyQualified(app.player_,"beastwarden"),"Hunt 6 and Motion 6 allow the Beastwarden");
            app.boss_->stats().hp=0; app.checkAndHandleDeath(*app.boss_); app.removeDeadMonsters();
            check((app.player_.trialsCleared & 8) && app.player_.ascendancyPoints==1,"Winning the trial gives a point");
            snapshot("ui-ascendancy-hollow.png");
            app.chooseAscendancy("beastwarden");
            check(app.player_.ascendancy=="beastwarden","You become a Beastwarden");
            app.ascendancyMenu_=false; app.leaveTrialState(); app.mode_=GameMode::Playing;
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.boss_=nullptr; app.clearSurfaces();
            for (int y=1;y<app.map_.height()-1;++y) for (int x=1;x<app.map_.width()-1;++x) app.map_.setTile(x,y,Tile{TileType::Floor,true,true});
            app.player_.setPosition({10,10}); app.updateFieldOfView();
            const auto learn=[&](const char* id) { app.player_.talents().learnTalent(findTalentDefinition(id)->ranks[0]); return app.player_.talents().knownTalents().size()-1; };
            const auto ready=[&] { app.player_.talents().resetCooldowns(); app.player_.stats().mana=app.player_.stats().maxMana; app.currentActor_=&app.player_; };
            const auto foeAt=[&](Position p) { auto f=createMonster(MonsterType::Goblin,p); f->stats().hp=f->stats().maxHp=90; f->stats().dexterity=0; f->setXpReward(0);
                auto* g=f.get(); app.scheduler_.add(*g); app.monsters_.push_back(std::move(f)); return g; };
            // Thornmaw.
            learn("beastwarden.thornmaw"); app.tickStormcall();
            auto* maw=app.thornmaw();
            check(maw && maw->allied && maw->name()=="Thornmaw" && maw->stats().maxHp==30+8*app.player_.level(),"Thornmaw comes to your side, its life set by your level");
            if (maw) {
                const auto call=learn("packmaster.call"); ready(); app.tryUseTalent(call,app.player_.position());
                int hounds=0; for (auto& m:app.monsters_) hounds+=app.packBeast(*m) && !app.isThornmaw(*m) && m->stats().hp>0;
                check(hounds==1,"Thornmaw doesn't take a hound's place");
                for (auto& m:app.monsters_) if (app.packBeast(*m) && !app.isThornmaw(*m)) { m->stats().hp=0; app.scheduler_.remove(*m); }
                app.removeDeadMonsters(); maw=app.thornmaw();
            }
            // Point.
            if (maw) {
                auto* far=foeAt({18,10});
                const auto point=learn("beastwarden.point"); ready(); app.tryUseTalent(point,{18,10});
                // Thornmaw may already have bitten (spending the doubled bite) before your next turn.
                check(std::max(std::abs(maw->position().x-18),std::abs(maw->position().y-10))==1 && (maw->doubleBite || far->stats().hp<90),
                      "Point: Thornmaw leaps beside the foe and goes for it");
                far->stats().hp=far->stats().maxHp=500; maw->doubleBite=true; app.wardenFoe_=far;
                // One action at a time (Thornmaw is fast enough to act twice in a round):
                // bite until one lands (a bite can miss), then six plain bites to average.
                for (int i=0;i<10 && far->stats().hp==500;++i) app.actMinion(*maw);
                const int doubled=500-far->stats().hp;
                int plain=0, landed=0;
                for (int i=0;i<40 && landed<6;++i) {
                    const int before=far->stats().hp=500;
                    app.actMinion(*maw);
                    if (far->stats().hp<before) { plain+=before-far->stats().hp; ++landed; }
                }
                check(!maw->doubleBite && landed>0 && doubled*landed*10>=plain*15,"Its next bite on that foe deals double");
                check(app.nearestOpponent(*maw,false)==far,"Thornmaw goes for the foe you pointed at");
                for (auto& m:app.monsters_) if (!m->allied) { m->stats().hp=0; app.scheduler_.remove(*m); }
                app.removeDeadMonsters(); maw->doubleBite=false; app.wardenFoe_=nullptr;
            }
            // Pack of Two.
            if (maw) {
                learn("beastwarden.pack_of_two");
                maw->setPosition({14,10});
                auto* beside=foeAt({15,10}); auto* apart=foeAt({15,14});
                const auto& any=findTalentDefinition("beastwarden.point")->ranks[0];
                check(app.situationalBonus(any,*beside)>=app.situationalBonus(any,*apart)+3,"Pack of Two: +3 to your hits on a foe beside Thornmaw");
                for (auto& m:app.monsters_) if (!m->allied) { m->stats().hp=0; app.scheduler_.remove(*m); }
                app.removeDeadMonsters();
            }
            // Running Mate.
            if (maw) {
                learn("beastwarden.running_mate");
                maw->setPosition({4,4}); app.lastMoveDirection_={1,0};
                const auto roll=learn("acrobatics.somersault"); ready(); app.tryUseTalent(roll,{12,10});
                const auto me=app.player_.position();
                check(std::max(std::abs(maw->position().x-me.x),std::abs(maw->position().y-me.y))<=1,"Running Mate: Thornmaw lands beside you when you move");
            }
            // Guardian Instinct.
            if (maw) {
                learn("beastwarden.guardian");
                const auto me=app.player_.position();
                auto* brute=foeAt({me.x,me.y+1}); brute->stats().strength=30; brute->stats().dexterity=10;
                const int life=app.player_.stats().hp;
                for (int i=0;i<10 && app.player_.stats().hp==life;++i) { app.currentActor_=brute; app.processMonsterTurns(); }
                check(app.wardenFoe_==brute,"Guardian Instinct: Thornmaw turns on whatever hits you");
                app.wardenPin_=true; maw->setPosition({brute->position().x+1,brute->position().y});
                for (int i=0;i<10 && app.wardenPin_;++i) app.actMinion(*maw);
                check(!app.wardenPin_ && brute->statusEffects().has(StatusEffectType::Pinned),"and its next bite on it pins it");
                // Call of the Wild.
                const auto wild=learn("beastwarden.call_wild"); ready(); app.tryUseTalent(wild,app.player_.position());
                app.tickStormcall();
                check(app.player_.statusEffects().has(StatusEffectType::Hasted) && maw->statusEffects().has(StatusEffectType::Hasted),"Call of the Wild hastens you both");
                app.player_.stats().hp=50; maw->stats().hp=20;
                brute->stats().hp=0; app.checkAndHandleDeath(*brute); app.removeDeadMonsters();
                check(app.player_.stats().hp==60 && maw->stats().hp==30,"Every foe that falls heals you both 10");
                // It falls, and finds you again on the next floor.
                maw->stats().hp=0; app.checkAndHandleDeath(*maw); app.removeDeadMonsters();
                app.tickStormcall();
                check(!app.thornmaw() && app.thornmawDown_,"When Thornmaw falls, it stays down on this floor");
                app.callPackBack(); app.tickStormcall();
                check(app.thornmaw()!=nullptr,"On the next floor it finds you again");
            }
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.player_.statusEffects().active().clear(); app.player_.ascendancy.clear(); app.wardenFoe_=nullptr;
        }

        // The Plaguebringer.
        {
            setup(PlayerClass::Mage);
            app.player_.trialKeys=app.player_.trialsCleared=1; app.player_.ascendancyPoints=1;
            check(!ascendancyQualified(app.player_,"plaguebringer"),"The Plaguebringer waits for its colours");
            giveColour(app.player_,Affinity::Rot,6); giveColour(app.player_,Affinity::Dark,6);
            check(ascendancyQualified(app.player_,"plaguebringer"),"Rot 6 and Dark 6 allow the Plaguebringer");
            app.chooseAscendancy("plaguebringer"); app.ascendancyMenu_=false; app.ascendancyChoice_=false;
            check(app.player_.ascendancy=="plaguebringer","You become a Plaguebringer");
            app.player_.statusEffects().active().clear(); app.updateFieldOfView();
            const auto learn=[&](const char* id) { app.player_.talents().learnTalent(findTalentDefinition(id)->ranks[0]); return app.player_.talents().knownTalents().size()-1; };
            const auto ready=[&] { app.player_.talents().resetCooldowns(); app.player_.stats().mana=app.player_.stats().maxMana; app.currentActor_=&app.player_; };
            const auto foeAt=[&](Position p) { auto f=createMonster(MonsterType::Goblin,p); f->stats().hp=f->stats().maxHp=200; f->stats().dexterity=0; f->setXpReward(0);
                auto* g=f.get(); app.scheduler_.add(*g); app.monsters_.push_back(std::move(f)); return g; };
            const auto clear=[&] { for (auto& m:app.monsters_) app.scheduler_.remove(*m); app.monsters_.clear(); app.clearSurfaces(); };
            const auto plague=[](Monster* m) { return m->statusEffects().magnitudeOf(StatusEffectType::Plague); };
            // Patient Zero.
            app.player_.stats().intelligence=20;
            auto* zero=foeAt({14,10});
            const auto infect=learn("plaguebringer.patient_zero"); ready(); app.tryUseTalent(infect,{14,10});
            check(plague(zero)==5,"Patient Zero: the plague grows with Intelligence (3, +1 per 10)");
            // Contagion.
            learn("plaguebringer.contagion");
            auto* beside=foeAt({15,10}); auto* apart=foeAt({18,10});
            app.tickStormcall();
            check(plague(beside)==5 && plague(apart)==0,"Contagion: the foe beside a plagued one catches it, the one apart doesn't");
            clear();
            // Outbreak: further, and stronger with each jump.
            learn("plaguebringer.outbreak");
            auto* carrier=foeAt({14,10}); carrier->statusEffects().apply({StatusEffectType::Plague,5,4});
            auto* near=foeAt({17,10}); auto* far=foeAt({18,10});
            carrier->stats().hp=0; app.checkAndHandleDeath(*carrier); app.removeDeadMonsters();
            check(plague(near)==5 && plague(far)==0,"Outbreak: the plague reaches 3 tiles, and grows 1 stronger");
            auto* top=foeAt({14,14}); top->statusEffects().apply({StatusEffectType::Plague,5,10});
            auto* next=foeAt({15,14});
            top->stats().hp=0; app.checkAndHandleDeath(*top); app.removeDeadMonsters();
            check(plague(next)==10,"It never grows past 10");
            clear();
            // Miasma.
            learn("plaguebringer.miasma");
            auto* sick=foeAt({14,10}); sick->statusEffects().apply({StatusEffectType::Plague,5,3});
            sick->stats().hp=0; app.checkAndHandleDeath(*sick); app.removeDeadMonsters();
            check(app.surfaceAt({14,10})==SurfaceType::Gas && app.surfaceAt({15,11})==SurfaceType::Gas,"Miasma: the plagued dead leave poison gas on the 8 tiles around");
            clear();
            // Wasting.
            learn("plaguebringer.wasting");
            const auto blows=[&](bool sickly) {
                auto* brute=foeAt({11,10}); brute->stats().strength=30; brute->stats().dexterity=10;
                if (sickly) brute->statusEffects().apply({StatusEffectType::Plague,99,1});
                int total=0;
                for (int i=0;i<40;++i) { // enough blows that dodges and criticals even out
                    app.player_.stats().hp=app.player_.stats().maxHp=500; app.player_.statusEffects().active().clear();
                    brute->stats().hp=200;
                    app.currentActor_=brute; app.processMonsterTurns();
                    total+=500-app.player_.stats().hp;
                }
                clear(); return total;
            };
            const int healthy=blows(false), wasted=blows(true);
            check(healthy>0 && wasted*100<=healthy*90,"Wasting: plagued foes deal less damage");
            // Pandemic.
            auto* a=foeAt({13,10}); a->statusEffects().apply({StatusEffectType::Plague,3,3});
            auto* b=foeAt({13,12}); b->statusEffects().apply({StatusEffectType::Plague,3,6});
            auto* healthyFoe=foeAt({13,8});
            app.updateFieldOfView();
            const auto pandemic=learn("plaguebringer.pandemic"); ready(); app.tryUseTalent(pandemic,app.player_.position());
            check(plague(a)==6 && plague(b)==10 && plague(healthyFoe)==0,"Pandemic: every plague in sight doubles, up to 10");
            check(std::any_of(a->statusEffects().active().begin(),a->statusEffects().active().end(),
                  [](const StatusEffectInstance& e){ return e.type==StatusEffectType::Plague && e.turnsRemaining>=6; }),"and lasts 5 more turns");
            // Your allies never catch it.
            clear();
            auto* hound=app.spawnHound(0);
            auto* host=foeAt({hound->position().x+1,hound->position().y}); host->statusEffects().apply({StatusEffectType::Plague,5,5});
            app.tickStormcall();
            host->stats().hp=0; app.checkAndHandleDeath(*host); app.removeDeadMonsters();
            check(!hound->statusEffects().has(StatusEffectType::Plague),"Your own beasts never catch the plague");
            clear(); app.player_.statusEffects().active().clear(); app.player_.ascendancy.clear();
        }

        // Vampirism: the Vampire Lord's bite curses you.
        {
            setup(PlayerClass::Warrior);
            app.player_.stats().hp=app.player_.stats().maxHp=500;
            auto made=createMonster(MonsterType::Gloomstalker,{11,10}); auto* lord=made.get();
            lord->eventChampion=kChampionVampire; lord->setName(championName(kChampionVampire));
            lord->stats().hp=lord->stats().maxHp=500; lord->tactics.alert=8; lord->tactics.lastKnown=app.player_.position();
            app.scheduler_.add(*lord); app.monsters_.push_back(std::move(made));
            for (int i=0;i<20 && !app.player_.statusEffects().has(StatusEffectType::Vampirism);++i) {
                app.player_.stats().hp=500; app.currentActor_=lord; app.processMonsterTurns();
            }
            check(app.player_.statusEffects().has(StatusEffectType::Vampirism),"The Vampire Lord's bite leaves his curse in your blood");
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear();
            // Night eyes: with your torch out, you see 6 tiles into the dark.
            app.darknessEnabled_=true; app.player_.lightLit=false;
            app.updateFieldOfView();
            const bool nearSeen=app.exploredMap_.at(14,10)==Visibility::Visible, farSeen=app.exploredMap_.at(18,10)==Visibility::Visible;
            check(nearSeen && !farSeen,"Cursed, you see in the dark up to 6 tiles away");
            auto cured=app.player_.statusEffects().active();
            app.player_.statusEffects().remove(StatusEffectType::Vampirism); app.updateFieldOfView();
            check(app.exploredMap_.at(14,10)!=Visibility::Visible,"Without the curse, the dark stays dark");
            app.player_.statusEffects().active()=cured; app.updateFieldOfView();
            // The light burns; blood heals; water hurts.
            app.player_.stats().hp=100; app.tickStormcall();
            check(app.player_.stats().hp==100,"In the dark, nothing burns");
            app.player_.lightLit=true; app.updateFieldOfView(); app.tickStormcall();
            check(app.player_.stats().hp==99,"Your own torchlight burns you 1 a turn");
            app.player_.lightLit=false; app.updateFieldOfView();
            app.setSurface(app.player_.position(),SurfaceType::Blood,0); app.player_.stats().hp=50; app.tickStormcall();
            check(app.player_.stats().hp==52,"A blood pool heals you 2 a turn");
            app.setSurface(app.player_.position(),SurfaceType::Water,0); app.player_.stats().hp=50; app.tickStormcall();
            check(app.player_.stats().hp==48,"Water hurts you 2 a turn");
            app.clearSurfaces();
            // Stronger in the dark, weaker in the light; and the thirst.
            const auto strike=[&](bool dark) {
                app.darknessEnabled_=dark; app.player_.lightLit=!dark; app.updateFieldOfView();
                auto f=createMonster(MonsterType::Goblin,{11,10}); f->stats().hp=f->stats().maxHp=5000; f->stats().dexterity=0; f->setXpReward(0);
                auto* foe=f.get(); app.scheduler_.add(*foe); app.monsters_.push_back(std::move(f));
                int dealt=0, healed=0;
                for (int i=0;i<12;++i) {
                    app.player_.talents().resetCooldowns(); app.player_.stats().mana=app.player_.stats().maxMana;
                    app.player_.stats().hp=50; app.currentActor_=&app.player_;
                    const int before=foe->stats().hp; app.tryUseTalent(0,{11,10});
                    dealt+=before-foe->stats().hp; healed+=std::max(0,app.player_.stats().hp-50);
                }
                for (auto& m:app.monsters_) app.scheduler_.remove(*m);
                app.monsters_.clear();
                return std::pair<int,int>{dealt,healed};
            };
            const auto [inDark,drunk]=strike(true);
            const auto [inLight,unused]=strike(false); (void)unused;
            check(inLight>0 && inDark*100>=inLight*140,"Cursed, your hits land harder in the dark than in the light");
            check(drunk>0,"Your melee hits drink some of the blood they spill");
            // It fades.
            app.darknessEnabled_=false;
            for (auto& e:app.player_.statusEffects().active()) if (e.type==StatusEffectType::Vampirism) e.turnsRemaining=1;
            app.tickStormcall();
            check(!app.player_.statusEffects().has(StatusEffectType::Vampirism),"In time, the curse fades");
            app.player_.statusEffects().active().clear(); app.player_.lightLit=true;
        }

        // The journal (J): the lore you've found, kept.
        {
            setup(PlayerClass::Warrior);
            const auto sawLog=[&](const char* text) { for (const auto& line:app.logMessages_) if (line.find(text)!=std::string::npos) return true; return false; };
            app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::J});
            check(app.journalOpen_,"J opens the journal");
            snapshot("ui-journal-empty.png");
            app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::J});
            check(!app.journalOpen_,"J closes it again");
            // Lore you read goes into it.
            app.loreDrops_.push_back({app.player_.position(),"hollow_map"});
            app.logMessages_.clear(); app.pickupItem();
            check(app.player_.knowsLore("hollow_map") && sawLog("A map scratched into a flat bone") && sawLog("journal (J)"),"Reading lore tells you it's kept in your journal");
            for (const char* id:{"warlord_standard","foreman_key","forgemaster_brand","slag_formula","chorister_hymn","bonecaller_journal","acolyte_catechism","witch_seed","hound_collar"})
                app.player_.lore.push_back(id);
            check(loreEntries().size()>=11 && std::all_of(app.player_.lore.begin(),app.player_.lore.end(),[](const std::string& id){ return loreEntry(id)!=nullptr; }),
                  "Every piece of lore has a journal entry");
            app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::J});
            snapshot("ui-journal.png");
            app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::Down});
            check(app.journalScroll_==1,"Down turns to the next page");
            snapshot("ui-journal-page2.png");
            app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::Escape});
            check(!app.journalOpen_,"Esc closes the journal");
            app.player_.lore.clear();
        }

        // Nemeses: the foe you flee follows you down, named.
        {
            setup(PlayerClass::Warrior);
            app.mode_=GameMode::Playing; app.currentFloor_=2; app.regenerateLevel(91);
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.boss_=nullptr;
            const auto farFrom=[&](Position from) {
                Position best=from; int bestD=-1;
                for (int y=0;y<app.map_.height();++y) for (int x=0;x<app.map_.width();++x) {
                    const int d=std::max(std::abs(x-from.x),std::abs(y-from.y));
                    if (app.map_.isWalkable(x,y) && !app.isOccupied({x,y},nullptr) && d>bestD) { bestD=d; best={x,y}; }
                }
                return best;
            };
            const auto addFoe=[&](Position p,int harm) {
                auto f=createMonster(MonsterType::Goblin,p); auto* g=f.get();
                g->tactics.alert=8; g->harmToPlayer=harm;
                app.scheduler_.add(*g); app.monsters_.push_back(std::move(f)); return g;
            };
            const auto nemesisHere=[&]()->Monster* { for (auto& m:app.monsters_) if (m->roam==Roam::Nemesis && m->stats().hp>0) return m.get(); return nullptr; };
            const auto leave=[&](int to) { app.player_.setPosition(app.floorExit_); app.combatThisTurn_=false; app.travelFloor(to,false); };
            // A scratch is forgotten.
            addFoe(farFrom(app.floorExit_),2);
            leave(3);
            check(app.currentFloor_==3 && app.player_.nemesis.type<0,"A foe that barely touched you doesn't remember you");
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.boss_=nullptr;
            // A foe that hurt you badly, and still hunts you, does.
            addFoe(farFrom(app.floorExit_),40);
            leave(4);
            check(app.currentFloor_==4 && app.player_.nemesis.type==static_cast<int>(MonsterType::Goblin) && !app.player_.nemesis.name.empty() &&
                  app.player_.nemesis.depth==3,"Flee a foe that hurt you badly, and it becomes your nemesis");
            auto* nemesis=nemesisHere();
            check(nemesis && nemesis->name()==app.player_.nemesis.name && nemesis->tier()==MonsterTier::Nightmare && nemesis->tactics.alert>0,
                  "On the next floor down it is waiting, named, and it hunts you");
            check(app.floorNotice_.find(app.player_.nemesis.name+" has followed you here")!=std::string::npos,"The floor tells you it has followed you");
            if (nemesis) {
                const int life=nemesis->stats().maxHp;
                const auto path=(output/"nemesis.txt").string();
                check(saveGame(app.captureState(false),path),"A floor with your nemesis saves");
                const auto loaded=loadGame(path);
                check(loaded && app.restoreState(*loaded) && nemesisHere() && nemesisHere()->name()==app.player_.nemesis.name,"And loads with it, still named");
                // Flee it again: it grows bolder.
                for (auto& m:app.monsters_) if (m->roam!=Roam::Nemesis && !m->vaultGuard) { m->stats().hp=0; app.scheduler_.remove(*m); }
                app.removeDeadMonsters();
                nemesisHere()->setPosition(farFrom(app.floorExit_)); nemesisHere()->tactics.alert=8;
                leave(5); // the Warlord's floor: a nemesis never intrudes on a boss
                check(app.currentFloor_==5 && app.player_.nemesis.rank==2 && !nemesisHere(),"Flee it again: it grows bolder, but leaves a boss's floor alone");
                for (auto& m:app.monsters_) app.scheduler_.remove(*m);
                app.monsters_.clear(); app.boss_=nullptr;
                leave(6);
                nemesis=nemesisHere();
                check(app.currentFloor_==6 && nemesis && nemesis->stats().maxHp>life,"On the floor after, it comes back stronger");
                // Kill it, and it's over.
                if (nemesis) {
                    const auto drops=app.groundItems_.size();
                    nemesis->stats().hp=0; app.checkAndHandleDeath(*nemesis); app.removeDeadMonsters();
                    check(app.player_.nemesis.type<0 && app.groundItems_.size()>drops,"Kill it and the grudge is over, and it leaves you something fine");
                }
            }
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.player_.nemesis={}; app.currentFloor_=1; app.boss_=nullptr;
        }

        // Rimeholt: beyond the Lich, the run goes on.
        {
            setup(PlayerClass::Warrior);
            app.mode_=GameMode::Town;
            app.travelFloor(kRimeFirst,true);
            check(app.mode_==GameMode::Town && app.currentFloor_!=kRimeFirst,"Rimeholt is buried in snow until the Lich falls");
            // The Lich's victory screen offers to go on.
            app.mode_=GameMode::GameOver; app.wonGame_=true; app.defeatedBossName_="Lich"; app.currentFloor_=20;
            snapshot("ui-victory-go-on.png");
            app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::G});
            check(app.mode_==GameMode::Playing && !app.wonGame_ && app.rimeholtOpen() && app.player_.knowsLore("winter_road"),
                  "After the Lich, Go on (G): the run continues, and the winter road opens");
            app.currentFloor_=1;
            // Its floors.
            bool icy=true, saves=true, boss=false;
            for (int floor=kRimeFirst; floor<=kRimeLast; ++floor) {
                app.currentFloor_=floor; app.regenerateLevel(970+floor);
                int ice=0;
                for (int y=0;y<app.map_.height();++y) for (int x=0;x<app.map_.width();++x) ice+=app.surfaceAt({x,y})==SurfaceType::Ice;
                icy=icy && ice>=8;
                for (const auto& m:app.monsters_) boss=boss || m->type()==MonsterType::WinterKing;
                if (floor==kRimeFirst+2 || floor==kRimeLast) {
                    if (floor==kRimeLast && app.boss_) app.player_.setPosition({app.boss_->position().x,app.boss_->position().y+3});
                    app.updateFieldOfView();
                    snapshot(floor==kRimeLast?"rimeholt-boss.png":"rimeholt.png");
                }
                const auto path=(output/"rimeholt.txt").string();
                saves=saves && saveGame(app.captureState(false),path) && loadGame(path).has_value();
            }
            check(icy,"Every Rimeholt floor has its sheets of ice");
            check(boss && app.boss_ && app.boss_->type()==MonsterType::WinterKing,"The Winter King waits on Rimeholt's last floor");
            check(saves,"Every Rimeholt floor saves and loads");
            check(floorDepth(kRimeFirst)==21 && floorDepth(kRimeLast)==30,"Rimeholt is as deep as floors 21 to 30");
            app.mode_=GameMode::Town; app.travelFloor(kRimeFirst,true);
            check(app.mode_==GameMode::Playing && app.currentFloor_==kRimeFirst,"With the road open, Rimeholt can be entered from town");

            // The cold: water freezes and the ice creeps.
            setup(PlayerClass::Warrior);
            app.currentFloor_=kRimeFirst;
            app.setSurface({14,10},SurfaceType::Water,0);
            for (int x=4;x<=8;++x) app.setSurface({x,15},SurfaceType::Ice,0);
            int before=0, after=0;
            for (int y=0;y<22;++y) for (int x=0;x<32;++x) before+=app.surfaceAt({x,y})==SurfaceType::Ice;
            app.tickStormcall(); app.floorTurns_=1; app.tickStormcall();
            for (int y=0;y<22;++y) for (int x=0;x<32;++x) after+=app.surfaceAt({x,y})==SurfaceType::Ice;
            check(app.surfaceAt({14,10})==SurfaceType::Ice,"In Rimeholt, standing water freezes");
            check(after>before+1,"and the ice creeps across the floor");
            app.clearSurfaces();
            // The winter's own walk on ice unchilled.
            const auto spawn=[&](MonsterType type,Position p) { auto made=createMonster(type,p); auto* m=made.get();
                m->tactics.alert=8; m->tactics.lastKnown=app.player_.position(); app.scheduler_.add(*m); app.monsters_.push_back(std::move(made)); return m; };
            auto* wight=spawn(MonsterType::RimeWight,{15,10});
            app.setSurface({15,10},SurfaceType::Ice,0); app.setSurface(app.player_.position(),SurfaceType::Ice,0);
            app.tickSurfaces();
            check(!wight->statusEffects().has(StatusEffectType::Chill) && app.player_.statusEffects().has(StatusEffectType::Chill),"Ice chills you, but not the winter's own");
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.clearSurfaces(); app.player_.statusEffects().active().clear();
            // A frost bear leaves ice where it walks.
            auto* bear=spawn(MonsterType::FrostBear,{16,10});
            app.currentActor_=bear; app.processMonsterTurns();
            check(bear->position().x!=16 && app.surfaceAt({16,10})==SurfaceType::Ice,"A Frost Bear leaves ice where it walks");
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.clearSurfaces();
            // A frozen thrall shatters.
            auto* thrall=spawn(MonsterType::FrozenThrall,{11,10});
            thrall->setXpReward(0); // a level-up would heal you and hide the cut
            const int life=app.player_.stats().hp;
            thrall->stats().hp=0; app.checkAndHandleDeath(*thrall); app.removeDeadMonsters();
            check(app.player_.stats().hp==life-6 && app.surfaceAt({12,11})==SurfaceType::Ice,"A slain Frozen Thrall shatters: ice around it, and shards that cut you");
            app.clearSurfaces();
            // The Winter King raises his court at half his life.
            auto* king=spawn(MonsterType::WinterKing,{16,10});
            king->stats().hp=king->stats().maxHp/2; app.boss_=king; app.courtCalled_=false;
            for (int i=0;i<4 && !app.courtCalled_;++i) { app.player_.stats().hp=app.player_.stats().maxHp; app.currentActor_=king; app.processMonsterTurns(); }
            int court=0; for (auto& m:app.monsters_) court+=m->type()==MonsterType::RimeWight && m->stats().hp>0;
            check(app.courtCalled_ && court==3,"At half his life the Winter King raises three of his court");
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.boss_=nullptr; app.clearSurfaces(); app.player_.statusEffects().active().clear();
            // Beyond the Lich, levels go on to 30, with tree points at 25 and 30.
            check(kRunMaxLevel==30 && earnedTreePoints(20)==4 && earnedTreePoints(25)==5 && earnedTreePoints(30)==6,"Levels go on to 30, with tree points at 25 and 30");
            app.player_.lore.clear();
            check(app.player_.levelCap()==20,"Until you go on past the Lich, the cap stays 20");
            app.player_.lore.push_back("winter_road");
            check(app.player_.levelCap()==30,"Past him, it is 30");
            app.player_.lore.clear(); app.currentFloor_=1;
        }

        // The Trial of Winter and the Wintercaller.
        {
            setup(PlayerClass::Mage);
            auto made=createMonster(MonsterType::WinterKing,{12,10}); auto* king=made.get(); app.monsters_.push_back(std::move(made));
            app.onBossDefeated(*king);
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.groundItems_.clear(); app.loreDrops_.clear();
            check((app.player_.trialKeys & 16) && app.trialAvailability(5).empty(),"The Winter King's sigil opens the Trial of Winter, on its own");
            app.mode_=GameMode::Town; app.trialMenu_=true;
            snapshot("ui-trials-five.png");
            app.trialMenu_=false;
            check(app.enterTrial(5) && app.boss_ && app.boss_->type()==MonsterType::WinterKing && app.boss_->name()=="The Frost Regent",
                  "The Trial of Winter pits you against the Frost Regent");
            int ice=0; for (int y=0;y<app.map_.height();++y) for (int x=0;x<app.map_.width();++x) ice+=app.surfaceAt({x,y})==SurfaceType::Ice;
            check(ice>20,"Its arena is sheeted in ice");
            check(!ascendancyQualified(app.player_,"wintercaller"),"The Wintercaller waits for its colours");
            giveColour(app.player_,Affinity::Frost,6); giveColour(app.player_,Affinity::Water,6);
            check(ascendancyQualified(app.player_,"wintercaller"),"Frost 6 and Water 6 allow the Wintercaller");
            app.boss_->stats().hp=0; app.checkAndHandleDeath(*app.boss_); app.removeDeadMonsters();
            check((app.player_.trialsCleared & 16) && app.player_.ascendancyPoints==1,"Winning the trial gives a point");
            snapshot("ui-ascendancy-twelve.png");
            app.chooseAscendancy("wintercaller");
            check(app.player_.ascendancy=="wintercaller","You become a Wintercaller");
            app.ascendancyMenu_=false; app.leaveTrialState(); app.mode_=GameMode::Playing;
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.boss_=nullptr; app.clearSurfaces();
            for (int y=1;y<app.map_.height()-1;++y) for (int x=1;x<app.map_.width()-1;++x) app.map_.setTile(x,y,Tile{TileType::Floor,true,true});
            app.player_.setPosition({10,10}); app.updateFieldOfView();
            const auto learn=[&](const char* id) { app.player_.talents().learnTalent(findTalentDefinition(id)->ranks[0]); return app.player_.talents().knownTalents().size()-1; };
            const auto ready=[&] { app.player_.talents().resetCooldowns(); app.player_.stats().mana=app.player_.stats().maxMana; app.currentActor_=&app.player_; };
            const auto foeAt=[&](Position p) { auto f=createMonster(MonsterType::Goblin,p); f->stats().hp=f->stats().maxHp=300; f->stats().dexterity=0; f->setXpReward(0);
                auto* g=f.get(); app.scheduler_.add(*g); app.monsters_.push_back(std::move(f)); return g; };
            const auto clear=[&] { for (auto& m:app.monsters_) app.scheduler_.remove(*m); app.monsters_.clear(); app.clearSurfaces(); };
            // Rime Tide.
            const auto tide=learn("wintercaller.rime_tide"); ready(); app.tryUseTalent(tide,app.player_.position());
            check(app.surfaceAt({9,9})==SurfaceType::Ice && app.surfaceAt({11,11})==SurfaceType::Ice,"Rime Tide: the 8 tiles around you flood and freeze");
            app.clearSurfaces();
            // Flash Freeze.
            auto* onIce=foeAt({12,10}); auto* dry=foeAt({10,13});
            app.setSurface({12,10},SurfaceType::Ice,0); app.setSurface({13,12},SurfaceType::Water,0);
            const auto freeze=learn("wintercaller.flash_freeze"); ready(); app.tryUseTalent(freeze,app.player_.position());
            check(app.surfaceAt({13,12})==SurfaceType::Ice && onIce->statusEffects().has(StatusEffectType::Stun) && !dry->statusEffects().has(StatusEffectType::Stun),
                  "Flash Freeze: the water freezes, and the foe on ice is frozen where it stands");
            clear();
            // Shatterpoint.
            learn("wintercaller.shatterpoint");
            auto* target=foeAt({11,10}); target->stats().hp=target->stats().maxHp=3000;
            int plain=0, shattered=0;
            for (int i=0;i<4;++i) { ready(); const int b=target->stats().hp; app.tryUseTalent(0,{11,10}); plain+=b-target->stats().hp; }
            app.setSurface({11,10},SurfaceType::Ice,0);
            for (int i=0;i<4;++i) { ready(); const int b=target->stats().hp; app.tryUseTalent(0,{11,10}); shattered+=b-target->stats().hp; }
            check(plain>0 && shattered*10>=plain*12,"Shatterpoint: your hits land harder on a foe standing on ice");
            clear();
            // Cold Blood and Glacial Armour.
            learn("wintercaller.cold_blood"); learn("wintercaller.glacial_armour");
            app.player_.statusEffects().apply({StatusEffectType::Chill,3,30});
            app.setSurface(app.player_.position(),SurfaceType::Ice,0);
            app.tickStormcall();
            check(!app.player_.statusEffects().has(StatusEffectType::Chill) && app.player_.statusEffects().has(StatusEffectType::Hasted),"Cold Blood: no chill takes you, and on ice you are quick");
            check(ascendancyGuardBonus(app.player_)>=3,"Glacial Armour: on ice, hits on you deal 3 less");
            clear(); app.player_.statusEffects().active().clear();
            // Absolute Zero.
            auto* frozen=foeAt({13,10}); auto* warm=foeAt({13,12});
            app.setSurface({13,10},SurfaceType::Ice,0); app.updateFieldOfView();
            const auto zero=learn("wintercaller.absolute_zero"); ready(); app.tryUseTalent(zero,app.player_.position());
            check(frozen->stats().hp<300 && frozen->statusEffects().has(StatusEffectType::Stun) && warm->stats().hp==300,"Absolute Zero: every foe on ice in sight is struck and frozen");
            clear(); app.player_.statusEffects().active().clear(); app.player_.ascendancy.clear();
        }

        // The Gravelord.
        {
            setup(PlayerClass::Mage);
            app.player_.trialKeys=app.player_.trialsCleared=16; app.player_.ascendancyPoints=1;
            check(!ascendancyQualified(app.player_,"gravelord"),"The Gravelord waits for its colours");
            giveColour(app.player_,Affinity::Death,6); giveColour(app.player_,Affinity::Dark,6);
            check(ascendancyQualified(app.player_,"gravelord"),"Death 6 and Dark 6 allow the Gravelord");
            app.chooseAscendancy("gravelord"); app.ascendancyMenu_=false; app.ascendancyChoice_=false;
            check(app.player_.ascendancy=="gravelord","You become a Gravelord");
            app.player_.statusEffects().active().clear(); app.updateFieldOfView();
            const auto learn=[&](const char* id) { app.player_.talents().learnTalent(findTalentDefinition(id)->ranks[0]); return app.player_.talents().knownTalents().size()-1; };
            const auto ready=[&] { app.player_.talents().resetCooldowns(); app.player_.stats().mana=app.player_.stats().maxMana; app.currentActor_=&app.player_; };
            const auto clear=[&] { for (auto& m:app.monsters_) app.scheduler_.remove(*m); app.monsters_.clear(); app.clearSurfaces(); };
            const auto standing=[&] { int n=0; for (auto& m:app.monsters_) n+=m->allied && raisedDead(m->type()) && m->stats().hp>0; return n; };
            // Without the legion, the risen fade; with it, they stay.
            app.raiseFrozenDead(MonsterType::RimeWight,{12,10},2);
            for (int i=0;i<4;++i) app.advanceEnemyIntents();
            check(standing()==0,"Without Standing Legion, the risen fade in time");
            learn("gravelord.standing_legion");
            app.raiseFrozenDead(MonsterType::RimeWight,{12,10},2);
            for (int i=0;i<6;++i) app.advanceEnemyIntents();
            check(standing()==1,"Standing Legion: they no longer fade");
            for (int i=0;i<7;++i) app.raiseFrozenDead(MonsterType::RimeWight,{3+i,15},5);
            app.tickStormcall();
            check(standing()==6,"The legion holds 6, and the oldest past that crumbles");
            // Shield of the Dead.
            learn("gravelord.shield");
            clear();
            app.raiseFrozenDead(MonsterType::RimeWight,{11,10},5); app.raiseFrozenDead(MonsterType::RimeWight,{9,10},5);
            app.tickStormcall();
            check(ascendancyGuardBonus(app.player_)>=3,"Shield of the Dead: with your dead close, hits on you deal 3 less");
            // Rally the Dead.
            clear(); app.player_.statusEffects().active().clear();
            auto* far=app.raiseFrozenDead(MonsterType::RimeWight,{18,10},5);
            const auto rally=learn("gravelord.rally"); ready(); app.tryUseTalent(rally,app.player_.position());
            check(std::max(std::abs(far->position().x-10),std::abs(far->position().y-10))<=2,"Rally the Dead: they gather around you");
            // Bone Tithe.
            learn("gravelord.tithe");
            app.player_.stats().hp=50; app.player_.stats().mana=10;
            far->stats().hp=0; app.checkAndHandleDeath(*far); app.removeDeadMonsters();
            check(app.player_.stats().hp==55 && app.player_.stats().mana==15,"Bone Tithe: one of your dead falls, and you regain life and mana");
            // Death's Due.
            learn("gravelord.deaths_due");
            auto* risen=app.raiseFrozenDead(MonsterType::RimeWight,{11,10},5);
            auto f=createMonster(MonsterType::Goblin,{12,10}); auto* prey=f.get(); prey->stats().hp=1; prey->stats().dexterity=0; prey->setXpReward(0);
            app.scheduler_.add(*prey); app.monsters_.push_back(std::move(f));
            risen->stats().hp=5;
            for (int i=0;i<10 && prey->stats().hp>0;++i) app.actMinion(*risen);
            check(prey->stats().hp<=0 && risen->stats().hp>=13,"Death's Due: a kill mends your raised dead");
            app.removeDeadMonsters();
            // Last Rites.
            auto g=createMonster(MonsterType::Goblin,{12,11}); auto* near=g.get(); near->stats().hp=near->stats().maxHp=90; near->setXpReward(0);
            app.scheduler_.add(*near); app.monsters_.push_back(std::move(g));
            app.player_.stats().hp=50;
            const auto rites=learn("gravelord.last_rites"); ready(); app.tryUseTalent(rites,app.player_.position());
            check(standing()==0 && near->stats().hp<90 && app.player_.stats().hp>50,"Last Rites: your dead burst, hurting the foes beside them, and you heal");
            clear(); app.player_.statusEffects().active().clear(); app.player_.ascendancy.clear();
        }

        // Soak: on the newest floors, every foe comes for you while you wait out 150
        // turns (unkillable). Any crash or hang in their behaviour shows up here.
        {
            setup(PlayerClass::Warrior);
            app.sandboxGod_=true;
            bool alive=true;
            for (const int floor:{kThornFirst+2,kThornLast,kRimeFirst+3,kRimeLast}) {
                app.mode_=GameMode::Playing; app.currentFloor_=floor; app.regenerateLevel(990+floor);
                for (auto& m:app.monsters_) if (!m->allied) { m->tactics.alert=8; m->tactics.lastKnown=app.player_.position(); }
                for (int turn=0;turn<150 && app.mode_==GameMode::Playing;++turn) {
                    app.currentActor_=&app.player_;
                    app.finishInventoryTurn();
                    if (app.mode_!=GameMode::Playing) break;
                }
                alive=alive && app.mode_==GameMode::Playing && app.currentFloor_==floor;
            }
            check(alive,"150 turns on Thornwood and Rimeholt floors, every foe hunting you, without a crash or a hang");
            app.sandboxGod_=false; app.currentFloor_=1;
            for (auto& m:app.monsters_) app.scheduler_.remove(*m);
            app.monsters_.clear(); app.boss_=nullptr;
        }

        // The title screen: a living scene behind the menu.
        {
            app.enterTitle();
            check(app.mode_==GameMode::Title && !app.map_.isWalkable(-1,-1) && app.map_.width()>0,"The title screen has a real floor behind it");
            bool brazierBeside=false;
            for (const auto& p:app.props_) brazierBeside=brazierBeside || (p.kind==PropKind::Brazier &&
                std::max(std::abs(p.pos.x-app.player_.position().x),std::abs(p.pos.y-app.player_.position().y))<=3);
            check(brazierBeside,"Your character stands by a brazier");
            // Six scenes, one from each dungeon (snapshots after the fade-in).
            for (int i=0;i<6;++i) {
                if (i) app.buildTitleScene();
                sf::sleep(sf::seconds(1.3f));
                snapshot(("ui-title-"+std::to_string(i+1)+".png").c_str());
            }
            const auto items=app.titleItems();
            check(items.back()==Application::TitleItem::Quit && std::find(items.begin(),items.end(),Application::TitleItem::NewGame)!=items.end(),
                  "The menu offers New Game and Quit");
            app.titleSelection_=static_cast<std::size_t>(std::find(items.begin(),items.end(),Application::TitleItem::NewGame)-items.begin());
            app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::Enter});
            check(app.mode_==GameMode::ClassSelection && !app.labMode_ && !app.sandboxMode_,"New Game opens class selection");
            app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::Escape});
            check(app.mode_==GameMode::Title,"Esc on class selection returns to the title");
            app.chooseTitle(Application::TitleItem::Sandbox);
            check(app.mode_==GameMode::ClassSelection && app.sandboxMode_,"Sandbox from the title opens class selection for a sandbox run");
            app.enterTitle(); app.chooseTitle(Application::TitleItem::Lab);
            check(app.mode_==GameMode::ClassSelection && app.labMode_,"Encounter Lab from the title");
            app.enterTitle();
            app.handleEvent(sf::Event::KeyPressed{sf::Keyboard::Key::Down});
            check(app.titleSelection_==1,"Down moves the selection");
            app.labMode_=false; app.sandboxMode_=false; app.mode_=GameMode::Playing;
        }

        app.window_.close();
        std::cout << checks << " reward checks, " << failures << " failures.\n";
        return failures ? 1 : 0;
    }
};
}
int main() { return engine::ApplicationRewardsTestAccess::run(); }
