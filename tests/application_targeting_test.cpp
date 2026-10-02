// Integration checks use a hidden SFML window and real Application handlers.
// Screenshots land in build/targeting-checks; no user save is touched.
#include <filesystem>
#include <iostream>
#include "core/Application.hpp"
#include "core/PlayLayout.hpp"
#include "entities/HybridSpec.hpp"
#include "entities/MonsterFactory.hpp"
#include "entities/PlayerClassFactory.hpp"
#include "entities/TalentEffects.hpp"

namespace engine {
struct ApplicationTargetingTestAccess {
    static int run() {
        int failures = 0;
        auto check = [&](bool condition, const char* message) {
            std::cout << (condition ? "[ok] " : "[FAIL] ") << message << '\n';
            if (!condition) ++failures;
        };
        Application app;
        app.window_.setVisible(false);
        app.window_.setFramerateLimit(0);
        const auto originalPath = std::filesystem::current_path();
        const auto outputPath = originalPath / "build" / "targeting-checks";
        std::filesystem::create_directories(outputPath);
        auto snapshot = [&](const char* filename) {
            app.render();
            app.render(); // Both swap buffers must contain this scene before capture.
            sf::Texture texture(app.window_.getSize());
            texture.update(app.window_);
            check(texture.copyToImage().saveToFile(outputPath / filename), "Save UI snapshot");
        };
        auto setup = [&] {
            app.cancelTargeting(); app.mousePixel_.reset(); app.talentPage_=0;
            app.mode_=GameMode::Playing; app.playerClass_=PlayerClass::Mage;
            app.floorEntrance_={1,1}; app.floorExit_={48,28};
            app.currentFloor_=1; app.boss_=nullptr; app.pendingFinalVictory_=false;
            app.player_.level()=1; app.player_.xp()=0;
            app.player_.unspentAttributePoints()=0; app.progressionReviewPending_=false;
            app.darknessEnabled_=false; // darkness is checked in the rewards tests
            app.player_.setPosition({20,12});
            app.player_.stats()=statsForClass(PlayerClass::Mage);
            app.player_.stats().hp=app.player_.stats().maxHp=100;
            app.player_.stats().mana=80; app.player_.stats().maxMana=100;
            app.player_.baseStats()=app.player_.stats(); // fixture's permanent stats, before equipment
            app.player_.talents()=talentSetForClass(PlayerClass::Mage);
            app.player_.statusEffects().active().clear();
            app.map_=Map(50,30);
            app.setProps({}); app.landmark_=LandmarkKind::None;
            for(int y=0;y<30;++y) for(int x=0;x<50;++x) {
                const bool edge=x==0||y==0||x==49||y==29;
                app.map_.setTile(x,y,Tile{edge?TileType::Wall:TileType::Floor,!edge,!edge});
            }
            app.monsters_.clear();
            Stats stats; stats.hp=stats.maxHp=100;
            app.monsters_.push_back(std::make_unique<Monster>(MonsterType::Goblin,
                "East target",'g',Position{23,12},stats,nullptr));
            app.monsters_.push_back(std::make_unique<Monster>(MonsterType::Goblin,
                "South target",'g',Position{20,15},stats,nullptr));
            app.exploredMap_=ExploredMap(app.map_); app.updateFieldOfView();
            app.scheduler_=TurnScheduler{}; app.scheduler_.add(app.player_);
            // Dummy enemies never act: the player is the only scheduled actor.
            app.currentActor_=&app.scheduler_.nextTurn();
            app.updateCamera();
        };
        auto pixelAt = [&](Position p) {
            auto screen=app.worldToScreen(p.x,p.y);
            return app.window_.mapCoordsToPixel(screen+sf::Vector2f(
                playLayout::tileSize/2.f,playLayout::tileSize/2.f));
        };
        setup();
        const Position position=app.player_.position();
        app.player_.talents().setCooldownRemaining(1,3);
        app.player_.statusEffects().apply({StatusEffectType::Poison,3,2});
        app.requestTalent(0);
        check(app.aimingTalent_ && app.player_.stats().mana==80 && app.player_.stats().hp==100,
            "Selecting an aimed talent spends nothing");
        const auto firstCursor=app.targetCursor_;
        app.handleTargetingKey(sf::Keyboard::Key::Tab,false);
        check(app.targetCursor_.x!=firstCursor.x || app.targetCursor_.y!=firstCursor.y,
            "Tab cycles between independently targetable enemies");
        app.handleTargetingKey(sf::Keyboard::Key::Right,false);
        check(app.player_.position().x==position.x && app.player_.position().y==position.y,
            "Arrows move the aim cursor instead of the player");
        for(int i=0;i<50;++i) app.targetPreview(0,app.targetCursor_);
        app.cancelTargeting();
        check(!app.aimingTalent_ && app.player_.stats().mana==80 && app.player_.stats().hp==100 &&
            app.player_.talents().cooldownRemaining(1)==3 &&
            app.player_.statusEffects().active()[0].turnsRemaining==3,
            "Cancellation and repeated previews consume no turns, effects or resources");
        app.requestTalent(0);
        check(!app.tryUseTalent(0,{1,1}) && app.aimingTalent_ && app.player_.stats().mana==80 &&
            app.player_.talents().cooldownRemaining(1)==3,
            "Invalid confirmation retains aim without consuming a turn");
        const auto beforeDamage=estimateTalentDamage(app.player_.talents().knownTalents()[0],
            app.player_,*app.monsters_[1]);
        app.targetCursor_=app.monsters_[1]->position();
        snapshot("aim-projectile.png");
        check(app.tryUseTalent(0,app.targetCursor_), "Confirming a valid target casts");
        const int damage=100-app.monsters_[1]->stats().hp;
        check((damage==beforeDamage.normal || damage==beforeDamage.critical) &&
            app.monsters_[0]->stats().hp==100 && !app.aimingTalent_,
            "Actual damage matches the preview and hits only the chosen enemy");
        check(app.player_.talents().cooldownRemaining(1)==2 && app.player_.stats().hp==98 &&
            app.player_.statusEffects().active()[0].turnsRemaining==2,
            "A confirmed cast advances exactly one player turn");

        setup();
        app.map_.setTile(22,12,Tile{TileType::Wall,false,false});
        app.requestTalent(0); app.targetCursor_={23,12};
        // Keep target visible in this fixture to independently exercise ray blocking.
        snapshot("aim-blocked.png");
        check(!app.tryUseTalent(0,{23,12}) && app.monsters_[0]->stats().hp==100 &&
            app.player_.stats().mana==80,"Blocked shots do not spend resources or hit through walls");
        app.handleTargetingMouse(sf::Event::MouseButtonPressed{sf::Mouse::Button::Right,{0,0}});
        check(!app.aimingTalent_,"Right-click cancels aiming");

        setup();
        const auto eastPixel=pixelAt({23,12});
        const auto tile=app.screenToWorld(eastPixel);
        check(tile && tile->x==23 && tile->y==12 && app.cameraX_>0 && app.cameraY_>0,
            "Mouse coordinates account for camera offsets and the map's top margin");
        check(!app.screenToWorld({100,300}) && !app.screenToWorld({400,5}) &&
            !app.screenToWorld({400,620}),"Character column, map frame and log cannot be clicked through");
        app.requestTalent(0);
        app.handleTargetingMouse(sf::Event::MouseButtonPressed{sf::Mouse::Button::Left,{400,620}});
        check(app.aimingTalent_ && app.monsters_[0]->stats().hp==100,
            "Clicking the message log does not confirm the previous target");
        app.handleTargetingMouse(sf::Event::MouseMoved{eastPixel});
        check(app.targetCursor_.x==23 && app.targetCursor_.y==12,"Mouse movement updates the aim");
        app.handleTargetingMouse(sf::Event::MouseButtonPressed{sf::Mouse::Button::Left,eastPixel});
        check(!app.aimingTalent_ && app.monsters_[0]->stats().hp<100,
            "Left-click confirms the enemy under the cursor");

        setup();
        app.monsters_[1]->setPosition({24,13});
        app.requestTalent(1); app.targetCursor_={23,12};
        const auto area=app.targetPreview(1,app.targetCursor_);
        snapshot("aim-area.png");
        check(area.affected.size()==2 && app.tryUseTalent(1,app.targetCursor_) &&
            app.monsters_[0]->stats().hp<100 && app.monsters_[1]->stats().hp<100,
            "Both highlighted area victims take damage on confirmation");
        setup();
        app.requestTalent(2);
        check(!app.aimingTalent_ && app.player_.statusEffects().has(StatusEffectType::Empowered),
            "Unambiguous self-buffs cast immediately");

        setup();
        app.playerClass_=PlayerClass::Warrior;
        app.player_.level()=10;
        app.player_.talents()=TalentSet(fullKitForClass(PlayerClass::Warrior));
        for(const auto& talent:fullKitForClass(PlayerClass::Mage)) app.player_.talents().learnTalent(talent);
        app.handleTargetingKey(sf::Keyboard::Key::PageDown,false);
        const auto slotPixel=[](int slot) {
            return sf::Vector2i(static_cast<int>(playLayout::hotbarX+playLayout::hotbarStride*slot+20),static_cast<int>(playLayout::hotbarY+20));
        };
        check(app.talentPage_==1 && app.talentAtPixel(slotPixel(0))==std::optional<std::size_t>(9) &&
            app.talentAtPixel(slotPixel(2))==std::optional<std::size_t>(11),
            "Second talent page maps to learned abilities 10 through 12");
        app.requestTalent(11); app.targetCursor_={23,12};
        snapshot("hybrid-page-two.png");
        check(app.aimingTalent_==std::optional<std::size_t>(11) &&
            app.tryUseTalent(11,app.targetCursor_),"An ability beyond key nine can actually be cast");
        app.requestTalent(12);
        check(app.player_.statusEffects().has(StatusEffectType::Empowered),
            "A hybrid self-buff on the second page remains usable");
        app.player_.talents().resetCooldowns(); app.requestTalent(6);
        app.player_.trees()={{"one_handed",false},{"arcane",false}};
        app.player_.treePoints()=2; app.player_.abilityPoints()=6;
        app.player_.talents()=TalentSet({basicAttack(),findTalentDefinition("one_handed.quick_strike")->ranks[0],findTalentDefinition("arcane.bolt")->ranks[0],basicCleanse()});
        app.player_.talents().setRank(1,3); app.player_.talents().setRank(2,3);
        std::filesystem::current_path(outputPath);
        app.saveGame();
        check(loadGame("savegame.txt").has_value(),"Targeting fixture is a valid loadable save");
        app.loadGame();
        std::filesystem::current_path(originalPath);
        check(!app.aimingTalent_ && !app.inspecting_ && app.talentPage_==0 &&
            app.player_.talents().knownTalents().size()==4,
            "Loading clears transient selection while preserving learned tree abilities");
        setup();
        app.monsters_.clear();
        auto lich=createMonster(MonsterType::Lich,{23,12});
        lich->talents().setCooldownRemaining(0,3);
        app.monsters_.push_back(std::move(lich));
        app.mousePixel_=pixelAt({23,12});
        snapshot("enemy-inspection.png");
        app.handleTargetingKey(sf::Keyboard::Key::I,false);
        check(app.inspecting_ && app.player_.stats().mana==80,"Keyboard inspection is free");
        app.requestTalent(0); app.regenerateLevel(1337);
        check(!app.aimingTalent_ && !app.inspecting_,"Regeneration clears stale targeting state");

        // Click-to-walk: with no danger in sight, clicking far ground walks there.
        setup();
        app.monsters_.clear(); app.scheduler_=TurnScheduler{}; app.scheduler_.add(app.player_);
        app.currentActor_=&app.scheduler_.nextTurn(); app.updateFieldOfView();
        const auto leftClick=[&](sf::Vector2i pixel) {
            app.handleTargetingMouse(sf::Event::MouseButtonPressed{sf::Mouse::Button::Left,pixel});
        };
        leftClick(pixelAt({24,14}));
        check(app.autoExploring_ && app.travelGoal_ && app.travelGoal_->x==24 && app.travelGoal_->y==14,
            "Clicking distant known ground starts walking there");
        for(int i=0;i<40 && app.autoExploring_;++i) { sf::sleep(sf::milliseconds(105)); app.stepAutoExplore(); }
        check(!app.autoExploring_ && app.player_.position().x==24 && app.player_.position().y==14,
            "Walking stops exactly at the clicked tile");
        // Clicking the portrait opens the bag; the name opens the talent trees.
        leftClick({40,40});
        check(app.inventoryOpen_,"Clicking the portrait opens the inventory");
        app.inventoryOpen_=false; leftClick({150,40});
        check(app.mode_==GameMode::AbilityChoice,"Clicking the character name opens the talent trees");
        app.window_.close();
        return failures ? 1 : 0;
    }
};
}
int main() { return engine::ApplicationTargetingTestAccess::run(); }
