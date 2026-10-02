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
#include "entities/RunProgression.hpp"
#include "world/EncounterPlan.hpp"
#include "world/LineOfFire.hpp"
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
            app.autoExploring_=false; app.restTurns_=0; app.quietTurns_=0; app.combatThisTurn_=false;
            app.vaultExists_=app.vaultOpened_=app.vaultClaimed_=false; app.vaultMenu_=0; app.exitMenu_=false;
            app.landmark_=LandmarkKind::None; app.landmarkUsed_=false; app.shrineMenu_=false;
            app.player_.ascendancy.clear(); app.player_.ascendancyPoints=app.player_.trialKeys=app.player_.trialsCleared=0;
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
            app.player_.trees()={{tree,false}}; app.player_.treePoints()=0; app.player_.abilityPoints()=0;
            app.player_.talents()=TalentSet({basicAttack(),findTalentDefinition(id)->ranks[0],basicCleanse()});
            app.player_.talents().setRank(1,3);
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
        app.player_.talents().setRank(1,1); app.player_.abilityPoints()=2;
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
        check(app.playerClass_==PlayerClass::Mage && app.mode_==GameMode::AbilityChoice && app.player_.treePoints()==1 && app.player_.abilityPoints()==3,
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
        check(app.player_.abilityPoints()==2 && app.player_.talents().rankOf(talentCatalog()[fire*4].id)==1,"Learn button spends exactly one ability point");
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
        check(app.player_.inventory().equipped(EquipmentSlot::Ring2) && !app.inventoryOpen_ && app.player_.position().x==10,
            "Dragging a ring to Ring 2 equips and closes without moving the player");
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

        // Landmark shrine: kneel beside the altar, pick one blessing, then it's spent.
        setup(PlayerClass::Mage);
        app.landmark_=LandmarkKind::Shrine; app.landmarkAltar_={11,10};
        app.map_.setTile(11,10,Tile{TileType::Wall,false,false}); app.updateFieldOfView();
        app.player_.stats().hp=40; app.gold_=0;
        app.pickupItem();
        check(app.shrineMenu_,"G beside the altar opens the shrine");
        snapshot("ui-shrine.png");
        clickOn(screen::shrineChoice(2));
        check(app.shrineMenu_ && !app.landmarkUsed_,"An unaffordable blessing is refused without spending the shrine");
        clickOn(screen::shrineChoice(0));
        check(!app.shrineMenu_ && app.landmarkUsed_ && app.player_.stats().hp==app.player_.stats().maxHp,
            "Restoration heals fully and spends the shrine");
        app.pickupItem();
        check(!app.shrineMenu_,"A spent shrine stays closed");
        roundTrip();
        check(app.landmark_==LandmarkKind::Shrine && app.landmarkUsed_ && app.landmarkAltar_.x==11,"Save/load keeps the landmark and its state");

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
        check(app.player_.trialsCleared==1 && app.player_.ascendancy=="juggernaut" && app.player_.ascendancyPoints==1 && app.ascendancyMenu_,
              "Winning the trial grants the Juggernaut ascendancy, a point, and opens the choice");
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
            app.player_.level()=level; app.player_.abilityPoints()=level-1;
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
