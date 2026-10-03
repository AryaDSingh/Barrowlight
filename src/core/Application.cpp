#include "entities/ArmourTalents.hpp"
#include "entities/RunProgression.hpp"
#include "entities/HiddenCombat.hpp"
#include "entities/HiddenTrees.hpp"
#include "core/Application.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "core/SaveGame.hpp"
#include "core/GameIcons.hpp"
#include "core/ScreenLayout.hpp"
#include "entities/TalentCatalog.hpp"
#include "ai/BossBehavior.hpp"
#include "core/PlayLayout.hpp"
#include "entities/AttributeFormulas.hpp"
#include "entities/MonsterFactory.hpp"
#include "entities/TalentProgression.hpp"
#include "entities/PlayerClassFactory.hpp"
#include "entities/PlayerLeveling.hpp"
#include "entities/StatusEffectLogic.hpp"
#include "entities/Stealth.hpp"
#include "entities/TalentEffects.hpp"
#include "world/DungeonGenerator.hpp"
#include "world/EncounterPlan.hpp"
#include "world/FloorTheme.hpp"
#include "world/FieldOfView.hpp"
#include "world/LineOfFire.hpp"

namespace engine {

namespace {
constexpr unsigned int kWindowWidth = playLayout::windowWidth;
constexpr unsigned int kWindowHeight = playLayout::windowHeight;
constexpr char kWindowTitle[] = "Roguelike Engine - Dev Window";
constexpr float kTileSize = static_cast<float>(playLayout::tileSize);
constexpr float kMapLeft = static_cast<float>(playLayout::mapLeft);
constexpr unsigned int kMapWidth = playLayout::mapWidth;
constexpr unsigned int kMapHeight = playLayout::mapHeight;
constexpr float kMapTop = static_cast<float>(playLayout::mapTop);
constexpr int kSightRadius = 8;
constexpr unsigned int kInitialSeed = 1337;

// The multi-floor dungeon progression: floors 1 through kFinalFloor,
// each a separate generated dungeon reached by walking through the
// previous floor's door. Only kFirstBossFloor and kFinalFloor generate
// with a boss room at all -- see regenerateLevel(). kFinalFloor's boss
// currently reuses GoblinWarlord as a placeholder (see that function's
// own comment) -- the actual Lich is separate, later work.
constexpr int kFirstBossFloor = 5;
constexpr int kFinalFloor = kRunFinalFloor;

// Passive regen, applied once per player turn (see
// advanceTurnsUntilPlayerCanAct) regardless of what action was taken --
// without it, a fight that outlasts the player's starting mana pool
// leaves them with talents they can see are "off cooldown" but can never
// actually afford again. Modest on purpose: enough to matter over a
// multi-turn fight, not enough to make mana cost meaningless.
constexpr int kManaRegenOutsideCombat = 2;
constexpr int kManaRegenInCombat = 1;

// Relative to wherever the executable is launched from -- same
// reasoning as avoiding data/ file loading elsewhere in this project
// (see ARCHITECTURE_DECISIONS.md): resolving the executable's own
// directory needs platform-specific APIs this project has deliberately
// avoided needing so far.
constexpr const char* kSaveFilePath = "savegame.txt";

// Menu hit areas live in core/ScreenLayout.hpp (shared with the UI tests).

// Remembered (explored, not currently seen) tiles: darker and cooler, so
// they read as memory rather than as the lit present.
sf::Color dim(sf::Color c) {
    return sf::Color(static_cast<std::uint8_t>(c.r * 0.30f), static_cast<std::uint8_t>(c.g * 0.33f),
                     static_cast<std::uint8_t>(std::min(255.f, c.b * 0.46f)), c.a);
}

// Fallback when a monster's sprite sheet fails to load (see monsterLook())
// -- each enemy type gets a distinct flat color so the roster is at least
// visually distinguishable at a glance.
// Keyed by MonsterType, not the display name string -- Prompt 22's
// Elite/Nightmare tiers prefix the name ("Elite Goblin", "Nightmare
// Goblin"), which would silently fail an exact-string match like
// `name == "Goblin"` and fall through to the default gray for every
// tiered monster. MonsterType is stable regardless of tier or display
// name, so this can't have the same failure mode again.
sf::Color monsterColor(MonsterType type) {
    switch (type) {
        case MonsterType::Goblin: return sf::Color(200, 60, 60);
        case MonsterType::Spider: return sf::Color(120, 200, 60);
        case MonsterType::Ogre: return sf::Color(140, 90, 50);
        case MonsterType::Archer: return sf::Color(210, 170, 60);
        case MonsterType::Shaman: return sf::Color(170, 70, 210);
        case MonsterType::Bomber: return sf::Color(230, 110, 30);
        case MonsterType::GoblinWarlord: return sf::Color(255, 215, 0);
        case MonsterType::Lich: return sf::Color(140, 220, 210); // pale, ghostly teal
        case MonsterType::GoblinRaider: return sf::Color(235,125,70);
        case MonsterType::SkeletonArcher: return sf::Color(135,210,245);
        case MonsterType::SkeletonGuard: return sf::Color(170,185,205);
        case MonsterType::Bonecaller: return sf::Color(180,125,220);
        case MonsterType::GoblinCaptain: return sf::Color(255,185,60);
        case MonsterType::OssuaryWarden: return sf::Color(255,185,60);
        case MonsterType::GoblinBulwark: case MonsterType::CryptSentinel: return sf::Color(110,150,195);
        case MonsterType::GoblinMedic: case MonsterType::GraveMender: return sf::Color(95,230,140);
        case MonsterType::GoblinStalker: case MonsterType::CryptShade: return sf::Color(180,110,200);
        case MonsterType::GoblinSlinger: return sf::Color(225,155,95);
        case MonsterType::FrostAcolyte: return sf::Color(120,220,250);
        case MonsterType::Skeleton: return sf::Color(220, 220, 200); // bone white
        case MonsterType::Torchbearer: return sf::Color(255, 180, 90);
        case MonsterType::Gloomstalker: return sf::Color(90, 70, 130);
        case MonsterType::OrcFirebrand: return sf::Color(230, 90, 40);
        case MonsterType::DrownedOne: return sf::Color(90, 170, 180);
    }
    return sf::Color(190, 190, 190); // unreachable -- all enum values handled above
}

// A visual border color for Elite/Nightmare monsters (Prompt 22) --
// drawn as a slightly larger square behind the monster's own type-
// colored tile, so a tiered monster is identifiable at a glance without
// needing to read the combat log. Base tier returns no value: nothing
// extra is drawn, a tier-0 monster looks exactly as it always has.
std::optional<sf::Color> tierBorderColor(MonsterTier tier) {
    switch (tier) {
        case MonsterTier::Base:
            return std::nullopt;
        case MonsterTier::Elite:
            return sf::Color(255, 200, 60); // amber
        case MonsterTier::Nightmare:
            // Stark white, not a saturated color -- a deep red border
            // (an earlier attempt) blended almost invisibly into the
            // Goblin's own red tile color when checked against an
            // actual screenshot, not just reasoned about in the
            // abstract. White has to contrast against every monster
            // color in the roster at once (red, green, brown, tan,
            // purple, orange), not just look distinct in isolation.
            return sf::Color(255, 255, 255);
    }
    return std::nullopt; // unreachable
}

// Sprite art (assets/sprites/CREDITS.txt). Character sheets are 10x5
// grids of 32px frames whose first frame is the idle pose facing right;
// the minotaur sheet's frames are 48x52. The dungeon tileset is a 16px grid.
constexpr SpriteFrame idleFrame(const char* sheet, int row = 0, int w = 32, int h = 32) {
    return {sheet, sf::IntRect({0, row * h}, {w, h})};
}
constexpr const char* kTileset = "calciumtrice/tiles/dungeon_tileset_calciumtrice.png";
// Gothic walls and floors (assets/sprites/CREDITS.txt): Sevarihk's Evil
// Dungeon (32px) and Daniel Siegmund's cobblestones (16px).
constexpr const char* kEvilDungeon = "evildungeon/evildungeon_0.png";
constexpr const char* kCobbles = "siegmund/cobbles2.png";

unsigned tileHash(int x, int y) {
    unsigned h = static_cast<unsigned>(x) * 73856093u ^ static_cast<unsigned>(y) * 19349663u;
    h ^= h >> 13;
    return h * 1274126177u;
}

// Brick wall faces from the Evil Dungeon wall strip: mostly plain brick,
// now and then a skull-topped pillar. Tall faces (two tiles) use the lower
// 64px of the strip; short faces its bottom 32px.
SpriteFrame wallFaceFrame(int x, int y, bool tall) {
    constexpr int kPlain[]{0, 32, 160, 192, 224, 0, 32, 192};
    const unsigned h = tileHash(x, y);
    const int column = h % 9 == 0 ? (h % 2 ? 64 : 128) : kPlain[(h >> 4) % std::size(kPlain)];
    return tall ? SpriteFrame{kEvilDungeon, sf::IntRect({column, 128}, {32, 64})}
                : SpriteFrame{kEvilDungeon, sf::IntRect({column, 160}, {32, 32})};
}
// Door tiles are floor transitions, so they use the tileset's stairway.
constexpr SpriteFrame kDoorFrame{kTileset, sf::IntRect({112, 64}, {16, 32})};

// Each region has its own floor: flagstones in the Barracks, purple
// cobbles in the Sanctum, grey cobbles in the Crypts. Variants are picked
// per tile by a position hash, so the floor doesn't visibly repeat but
// doesn't shimmer between frames either.
const char* floorSheet(FloorRegion region) { return region == FloorRegion::Barracks ? kEvilDungeon : kCobbles; }

SpriteFrame floorFrame(int x, int y, FloorRegion region) {
    const unsigned h = tileHash(x, y);
    if (region == FloorRegion::Barracks) {
        // Mostly the plain slab, sometimes cracked, rarely the small bricks.
        const int column = h % 7 == 0 ? 128 : h % 23 == 0 ? 64 : 96;
        return {kEvilDungeon, sf::IntRect({column, 0}, {32, 32})};
    }
    // The cobble sheet's 7x4 interior blocks: purple rows start at 0,
    // grey rows at 160.
    const int top = region == FloorRegion::Sanctum ? 0 : 160;
    const int column = static_cast<int>(h % 7), row = static_cast<int>((h >> 8) % 4);
    return {kCobbles, sf::IntRect({column * 16, top + row * 16}, {16, 16})};
}

// Prop art: thrones and statues from Evil Dungeon (32px), barrels, crates
// and sacks from the Calciumtrice tileset (16px).
std::pair<SpriteFrame, const char*> propFrame(PropKind kind) {
    switch (kind) {
        case PropKind::Barrel: return {{kTileset, sf::IntRect({64, 304}, {16, 16})}, kTileset};
        case PropKind::Crate: return {{kTileset, sf::IntRect({0, 304}, {16, 16})}, kTileset};
        case PropKind::Sacks: return {{kTileset, sf::IntRect({112, 304}, {16, 16})}, kTileset};
        case PropKind::Throne: return {{kEvilDungeon, sf::IntRect({28, 384}, {40, 64})}, kEvilDungeon};
        case PropKind::SkeletonThrone: return {{kEvilDungeon, sf::IntRect({124, 384}, {40, 72})}, kEvilDungeon};
        case PropKind::Statue: return {{kEvilDungeon, sf::IntRect({176, 408}, {72, 112})}, kEvilDungeon};
        case PropKind::OilBarrel: return {{kTileset, sf::IntRect({80, 304}, {16, 16})}, kTileset};
        case PropKind::Brazier: case PropKind::ColdBrazier: return {{kTileset, sf::IntRect({448, 304}, {16, 16})}, kTileset}; // blank: drawn by renderSurfaces
    }
    return {{kTileset, sf::IntRect({64, 304}, {16, 16})}, kTileset};
}

// --- Decorations -------------------------------------------------------
// Purely visual: derived from the map, tile position and floor number at
// draw time, so they never block anything, need no save data, and work on
// any layout (old saves included). Wall decor only goes on a wall face --
// a wall with floor directly below it, which is the side the camera sees.
enum class Decor { None, Torch, Banner, Bones, Rubble, Cobweb, Niche, SkullNiche, Chains };

unsigned decorHash(int x, int y, int floor) {
    unsigned h = static_cast<unsigned>(x) * 374761393u + static_cast<unsigned>(y) * 668265263u +
                 static_cast<unsigned>(floor) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

Decor decorAt(const Map& map, int x, int y, int floor, FloorRegion region) {
    const TileType type = map.tileAt(x, y).type;
    const unsigned h = decorHash(x, y, floor) % 100;
    if (type == TileType::Wall) {
        if (!map.inBounds(x, y + 1) || map.tileAt(x, y + 1).type != TileType::Floor) return Decor::None;
        // Torches spaced out along a wall: never two in neighboring columns.
        const auto wantsTorch = [&](int tx) { return decorHash(tx, y, floor) % 100 < 9; };
        if (h < 9 && !wantsTorch(x - 1)) return Decor::Torch;
        if (h >= 90 && h < 95) return Decor::Banner;
        // Crypt niches and hanging chains, more of them the deeper you go.
        const bool crypt = region == FloorRegion::Crypts;
        if (h >= 60 && h < (crypt ? 66u : 62u)) return h % 2 ? Decor::SkullNiche : Decor::Niche;
        if (h >= 70 && h < (crypt ? 74u : 72u)) return Decor::Chains;
        return Decor::None;
    }
    if (type != TileType::Floor) return Decor::None;
    const auto isWall = [&](int wx, int wy) { return !map.inBounds(wx, wy) || map.tileAt(wx, wy).type == TileType::Wall; };
    const bool nearWall = isWall(x - 1, y) || isWall(x, y - 1);
    // Kept rare: a bone pile at full density reads like a skeleton enemy.
    const unsigned fine = decorHash(y, x, floor) % 1000; // independent of h for finer odds
    if (fine < (region == FloorRegion::Crypts ? 15u : 5u)) return Decor::Bones;
    if (h == 50) return Decor::Rubble;
    if (nearWall && fine >= 500 && fine < 510) return Decor::Cobweb;
    return Decor::None;
}

SpriteFrame bannerFrame(FloorRegion region, int x) {
    // Red banners in the Barracks, blue cross in the Sanctum, black skull in the Crypts.
    const int base = region == FloorRegion::Barracks ? 288 : region == FloorRegion::Sanctum ? 320 : 352;
    return {kTileset, sf::IntRect({base + (x % 2) * 16, 176}, {16, 32})};
}

// The three lit torch frames, cycled for a flicker; each torch is offset
// so they don't all flicker in step.
SpriteFrame torchFrame(int x, int y, float seconds) {
    const int frame = (static_cast<int>(seconds * 6.f) + x * 7 + y * 3) % 3;
    return {kTileset, sf::IntRect({176 + frame * 16, 304}, {16, 16})};
}

constexpr SpriteFrame kBonesFrame{kTileset, sf::IntRect({112, 240}, {16, 16})};
constexpr SpriteFrame kRubbleFrame{kTileset, sf::IntRect({128, 240}, {16, 16})};
constexpr SpriteFrame kCobwebFrame{kTileset, sf::IntRect({96, 240}, {16, 16})};

// The tileset's stone is pale grey, so tinting it with a theme color
// darkens it below that flat color. Brighten the tint by roughly the
// inverse of each texture's average brightness so a textured tile reads
// about as bright as the old flat square did.
sf::Color themeTint(sf::Color c, float boost) {
    auto ch = [boost](std::uint8_t v) {
        return static_cast<std::uint8_t>(std::min(255.f, v * boost));
    };
    return sf::Color(ch(c.r), ch(c.g), ch(c.b));
}

// Most sheets are the standard 10x5 grid; `rows`/`frames` remap the
// animation rows (idle, gesture, walk, attack, death) for sheets laid out
// differently, and `scale` enlarges a creature beyond its frame.
struct MonsterLook {
    SpriteFrame frame;
    sf::Color tint = sf::Color::White;
    float scale = 1.f;
    std::array<int, 5> rows{0, 1, 2, 3, 4};
    std::array<int, 5> frames{10, 10, 10, 10, 10};
};
SpriteFrame lookFrame(const MonsterLook& look, int row, int frame) {
    const int r = std::clamp(row, 0, 4);
    const int w = look.frame.rect.size.x, h = look.frame.rect.size.y;
    return {look.frame.sheet, sf::IntRect({frame * look.frames[r] / 10 * w, look.frame.rect.position.y + look.rows[r] * h}, {w, h})};
}

// Bosses and named encounters get the most distinctive sheets in the
// pack; regular enemies share the generic goblin/skeleton art, tinted
// where two types would otherwise look identical.
MonsterLook monsterLook(MonsterType type);
// The chained demon keeps its own red minotaur art once it breaks free.
MonsterLook monsterLook(const Monster& monster) {
    if (monster.eventChampion == kChampionDemon) return {idleFrame("calciumtrice/monsters/RedMinotaur.png", 0, 48, 52)};
    if (monster.eventChampion == kChampionRevenant) return {idleFrame("calciumtrice/heroes/BlackKnight.png"), sf::Color(200, 190, 255)};
    // The trial guardians: a living statue (64px frames: idle, walk, attack, death rows) and a fallen saint.
    if (monster.eventChampion == kChampionStoneWarden)
        return {idleFrame("calciumtrice/monsters/statue_spritesheet_no_green.png", 0, 64, 64), sf::Color::White, 1.15f,
                {0, 0, 1, 2, 3}, {4, 4, 8, 5, 6}};
    if (monster.eventChampion == kChampionFallenSaint)
        return {idleFrame("calciumtrice/heroes/EvilCleric.png"), sf::Color(205, 175, 255), 1.6f};
    return monsterLook(monster.type());
}

MonsterLook monsterLook(MonsterType type) {
    constexpr const char* goblin = "calciumtrice/monsters/goblin_spritesheet_calciumtrice.png";
    constexpr const char* skeleton = "calciumtrice/monsters/skeleton_spritesheet_calciumtrice.png";
    switch (type) {
        case MonsterType::Goblin: return {idleFrame(goblin)};
        case MonsterType::GoblinSlinger: return {idleFrame(goblin), sf::Color(255, 220, 170)};
        case MonsterType::GoblinRaider: return {idleFrame(goblin, 5)}; // the sheet's armored variant
        case MonsterType::GoblinStalker: return {idleFrame("calciumtrice/monsters/Imp.png")};
        case MonsterType::GoblinBulwark: return {idleFrame("calciumtrice/monsters/ArmourImp.png")};
        case MonsterType::GoblinMedic: return {idleFrame("calciumtrice/monsters/PsionicGoblin.png")};
        case MonsterType::Spider: return {idleFrame("calciumtrice/monsters/snake_spritesheet_calciumtrice.png")};
        case MonsterType::Ogre: return {idleFrame("calciumtrice/monsters/orc_spritesheet_calciumtrice.png")};
        case MonsterType::Archer: return {idleFrame("calciumtrice/heroes/DarkRanger.png")};
        case MonsterType::Shaman: return {idleFrame("calciumtrice/monsters/TricksterOrc.png")};
        case MonsterType::Bomber: return {idleFrame("calciumtrice/monsters/PyromaniacOrc.png")};
        case MonsterType::Skeleton: return {idleFrame(skeleton)};
        case MonsterType::SkeletonArcher: return {idleFrame(skeleton), sf::Color(170, 220, 255)};
        case MonsterType::SkeletonGuard: return {idleFrame(skeleton), sf::Color(190, 190, 205)};
        case MonsterType::Bonecaller: return {idleFrame("calciumtrice/heroes/FoulMonk.png")};
        case MonsterType::CryptSentinel: return {idleFrame("calciumtrice/heroes/BlackKnight.png")};
        case MonsterType::GraveMender: return {idleFrame("calciumtrice/heroes/FutureCleric.png")};
        case MonsterType::CryptShade: return {idleFrame("calciumtrice/monsters/Ghost.png"), sf::Color(255, 255, 255, 190)};
        case MonsterType::FrostAcolyte: return {idleFrame("calciumtrice/heroes/BlueCleric.png")};
        case MonsterType::Torchbearer: return {idleFrame("calciumtrice/heroes/MysteryMonk.png"), sf::Color(255, 215, 180)};
        case MonsterType::Gloomstalker: return {idleFrame("calciumtrice/heroes/Assassin.png"), sf::Color(120, 95, 170, 215)};
        case MonsterType::OrcFirebrand: return {idleFrame("calciumtrice/monsters/RedOrc.png")};
        case MonsterType::DrownedOne: return {idleFrame(skeleton), sf::Color(120, 200, 205)};
        case MonsterType::GoblinWarlord: return {idleFrame("calciumtrice/monsters/GreyMinotaur.png", 0, 48, 52)};
        case MonsterType::Lich: return {idleFrame("calciumtrice/monsters/Death.png")};
        case MonsterType::GoblinCaptain: return {idleFrame("calciumtrice/monsters/ArmourPsionicGoblin.png")};
        case MonsterType::OssuaryWarden: return {idleFrame("calciumtrice/heroes/Psychopath.png")};
    }
    return {idleFrame(goblin)}; // unreachable -- all enum values handled above
}

SpriteFrame playerFrame(PlayerClass playerClass) {
    switch (playerClass) {
        case PlayerClass::Warrior: return idleFrame("calciumtrice/heroes/MitheralKnight.png");
        case PlayerClass::Thief: return idleFrame("calciumtrice/heroes/PrinceRanger.png");
        case PlayerClass::Mage: return idleFrame("calciumtrice/heroes/Mage.png");
        case PlayerClass::Spellblade: return idleFrame("calciumtrice/heroes/warrior_spritesheet_calciumtrice.png");
    }
    return idleFrame("calciumtrice/heroes/MitheralKnight.png"); // unreachable
}
} // namespace

Application::Application()
    : window_(sf::VideoMode({kWindowWidth, kWindowHeight}), kWindowTitle),
      // Spellblade is just this constructor's placeholder starting
      // point -- selectClass() (called once the person picks a class on
      // the selection screen) reconfigures player_'s stats and talents
      // for real before any dungeon is generated. See
      // PlayerClassFactory for what each class's Stats/TalentSet
      // actually are.
      player_(Position{0, 0}, statsForClass(PlayerClass::Spellblade),
              TalentSet({basicAttack(),basicCleanse()})) {
    // Auto-flush cout after every insertion (see ARCHITECTURE_DECISIONS.md,
    // Prompt 9) -- fixes stdout buffering for every diagnostic/combat-log
    // line at once.
    std::cout << std::unitbuf;

    // Fonts load in ui::Kit; a missing one falls back to DejaVu Sans Mono
    // and is logged rather than treated as fatal.
    window_.setFramerateLimit(60);
    window_.setKeyRepeatEnabled(false); // A held confirm key must not cast twice.
    fitView();
    // Deliberately no regenerateLevel() call here -- mode_ starts at
    // ClassSelection (see the member's default), and selectClass()
    // calls regenerateLevel() itself once a real choice is made. Prompt
    // 13's text rendering is what finally makes a real selection screen
    // possible; before that, defaulting straight into the Spellblade
    // (as this constructor did through Prompt 14) was the only option.
}


void Application::run() {
    while (window_.isOpen()) {
        processEvents();
        update();
        updateMusic();
        render();
    }
}

void Application::updateMusic() {
    MusicTrack track = MusicTrack::Title;
    if (mode_ == GameMode::Town) track = MusicTrack::Town;
    else if (mode_ != GameMode::ClassSelection && mode_ != GameMode::GameOver) {
        const auto region = floorTheme(currentFloor_).region;
        track = boss_ ? MusicTrack::Boss : region == FloorRegion::Barracks ? MusicTrack::Barracks
              : region == FloorRegion::Sanctum ? MusicTrack::Sanctum : MusicTrack::Crypts;
    }
    soundManager_.setMusic(track);
    soundManager_.updateMusic(musicClock_.restart().asSeconds());
}

void Application::logImpl(const std::string& message) {
    std::cout << message << std::endl;
    logMessages_.push_back(message);
    while (logMessages_.size() > kMaxLogMessages) {
        logMessages_.pop_front();
    }
}

void Application::drawText(const std::string& text, float x, float y, unsigned int size,
                            sf::Color color) {
    // Older screens were laid out for a monospace font; the proportional
    // body font reads about a point smaller, so nudge it up.
    ui_.text(window_, text, {x, y}, size + 1, color);
}

SpriteFrame Application::playerSpriteFrame() const { return playerFrame(playerClass_); }

void Application::setProps(std::vector<Prop> props) {
    props_ = std::move(props);
    propAt_.assign(static_cast<std::size_t>(std::max(0, map_.width() * map_.height())), -1);
    for (std::size_t i = 0; i < props_.size(); ++i)
        for (int t = 0; t < propWidth(props_[i].kind); ++t)
            if (map_.inBounds(props_[i].pos.x + t, props_[i].pos.y))
                propAt_[static_cast<std::size_t>(props_[i].pos.y * map_.width() + props_[i].pos.x + t)] = static_cast<int>(i);
}

int Application::propIndexAt(int x, int y) const {
    if (!map_.inBounds(x, y) || propAt_.size() != static_cast<std::size_t>(map_.width() * map_.height())) return -1;
    return propAt_[static_cast<std::size_t>(y * map_.width() + x)];
}

// A soft oval at the feet keeps sprites from floating over the floor.
void Application::drawActorShadow(sf::Vector2f tileTopLeft) {
    sf::CircleShape shadow(kTileSize * 0.36f, 18);
    shadow.setOrigin({kTileSize * 0.36f, kTileSize * 0.36f});
    shadow.setScale({1.f, 0.38f});
    shadow.setPosition({tileTopLeft.x + kTileSize / 2, tileTopLeft.y + kTileSize - 3});
    shadow.setFillColor(sf::Color(0, 0, 0, 105));
    window_.draw(shadow);
}

// Darkness with pools of light: the map is multiplied by a light map that
// starts at a dim ambient level, plus additive radial lights (the player's
// own, torches, stairs, an unused shrine). Remembered areas stay readable.
bool Application::ensureLightBlob() {
    if (!lightBlob_) {
        constexpr unsigned kSize = 128;
        sf::Image blob(sf::Vector2u{kSize, kSize}, sf::Color::Transparent);
        for (unsigned y = 0; y < kSize; ++y)
            for (unsigned x = 0; x < kSize; ++x) {
                const float dx = (x + 0.5f) / kSize * 2 - 1, dy = (y + 0.5f) / kSize * 2 - 1;
                const float d = std::min(1.f, std::sqrt(dx * dx + dy * dy));
                const float falloff = (1 - d) * (1 - d);
                blob.setPixel({x, y}, sf::Color(255, 255, 255, static_cast<std::uint8_t>(255 * falloff)));
            }
        lightBlob_.emplace();
        if (!lightBlob_->loadFromImage(blob)) { lightBlob_.reset(); return false; }
        lightBlob_->setSmooth(true);
    }
    return true;
}

void Application::renderLighting(const std::vector<std::pair<sf::Vector2f, sf::Color>>& lights) {
    if (!ensureLightBlob()) return;
    if (!lightMap_) lightMap_.emplace(sf::Vector2u{static_cast<unsigned>(kMapWidth), static_cast<unsigned>(kMapHeight)});
    auto& target = *lightMap_;
    // Darkness proper: unlit areas fall to a deep gloom; light comes from sources.
    const sf::Color ambient = darknessEnabled_ ? sf::Color(92, 88, 112) : sf::Color(170, 160, 182);
    target.clear(ambient);
    const float now = animationClock_.getElapsedTime().asSeconds();
    const auto addLight = [&](sf::Vector2f center, sf::Color color, float radiusTiles) {
        sf::Sprite light(*lightBlob_);
        const float scale = radiusTiles * kTileSize * 2 / 128.f;
        light.setOrigin({64, 64});
        light.setScale({scale, scale});
        light.setPosition({center.x - kMapLeft, center.y - kMapTop});
        light.setColor(color);
        target.draw(light, sf::BlendAdd);
    };
    const auto player = worldToScreen(player_.position().x, player_.position().y);
    if (!darknessEnabled_) addLight({player.x + kTileSize / 2, player.y + kTileSize / 2}, sf::Color(255, 214, 160), 9.2f);
    else if (!playerLightRadius()) addLight({player.x + kTileSize / 2, player.y + kTileSize / 2}, sf::Color(120, 120, 150), 1.4f);
    // Lasting lights follow line of sight: each tile takes light only from
    // sources that can see it, so nothing glows through a wall.
    {
        const int cols = static_cast<int>(kMapWidth / kTileSize) + 3, rows = static_cast<int>(kMapHeight / kTileSize) + 3;
        const int x0 = cameraX_ - 1, y0 = cameraY_ - 1;
        std::vector<sf::Vector3f> grid(static_cast<std::size_t>(cols * rows), sf::Vector3f{});
        for (const auto& source : lightSources_) {
            const float flick = source.flicker ? 0.9f + 0.1f * std::sin(now * 9.f + source.radius * 3.1f + static_cast<float>(source.tiles.empty() ? 0 : source.tiles.front().first)) : 1.f;
            const float reach = source.radius * flick + .5f;
            for (const auto& [index, distance] : source.tiles) {
                const int tx = index % map_.width() - x0, ty = index / map_.width() - y0;
                if (tx < 0 || ty < 0 || tx >= cols || ty >= rows) continue;
                // Only what you can see right now is lit: remembered and unknown
                // ground keeps the gloom, so light never gives away a hidden room.
                if (exploredMap_.at(index % map_.width(), index / map_.width()) != Visibility::Visible) continue;
                float i = std::clamp(1.f - distance / reach, 0.f, 1.f);
                i = .9f * i * i;
                auto& cell = grid[static_cast<std::size_t>(ty * cols + tx)];
                cell += sf::Vector3f{source.color.r * i, source.color.g * i, source.color.b * i};
            }
        }
        // Corners average the open tiles around them, so walls don't smear light across.
        const auto open = [&](int gx, int gy) {
            const int x = gx + x0, y = gy + y0;
            return map_.inBounds(x, y) && map_.tileAt(x, y).type != TileType::Wall;
        };
        const auto corner = [&](int cx, int cy) {
            sf::Vector3f sum{}, any{}; int n = 0, m = 0;
            for (const auto [gx, gy] : {std::pair{cx - 1, cy - 1}, std::pair{cx, cy - 1}, std::pair{cx - 1, cy}, std::pair{cx, cy}}) {
                if (gx < 0 || gy < 0 || gx >= cols || gy >= rows) continue;
                const auto& v = grid[static_cast<std::size_t>(gy * cols + gx)];
                any += v; ++m;
                if (open(gx, gy)) { sum += v; ++n; }
            }
            const sf::Vector3f c = n ? sum / static_cast<float>(n) : (m ? any / static_cast<float>(m) : sf::Vector3f{});
            const auto clip = [](float v) { return static_cast<std::uint8_t>(std::clamp(v, 0.f, 255.f)); };
            return sf::Color(clip(c.x), clip(c.y), clip(c.z));
        };
        sf::VertexArray quads(sf::PrimitiveType::Triangles);
        const auto shift = cameraShift();
        for (int gy = 0; gy < rows; ++gy)
            for (int gx = 0; gx < cols; ++gx) {
                const sf::Vector2f tl{(gx + x0 - cameraX_) * kTileSize + shift.x, (gy + y0 - cameraY_) * kTileSize + shift.y};
                const sf::Vector2f tr{tl.x + kTileSize, tl.y}, br{tl.x + kTileSize, tl.y + kTileSize}, bl{tl.x, tl.y + kTileSize};
                const sf::Color ctl = corner(gx, gy), ctr = corner(gx + 1, gy), cbr = corner(gx + 1, gy + 1), cbl = corner(gx, gy + 1);
                if (!(ctl.r | ctl.g | ctl.b | ctr.r | ctr.g | ctr.b | cbr.r | cbr.g | cbr.b | cbl.r | cbl.g | cbl.b)) continue;
                for (const auto& v : {sf::Vertex{tl, ctl}, sf::Vertex{tr, ctr}, sf::Vertex{br, cbr},
                                      sf::Vertex{tl, ctl}, sf::Vertex{br, cbr}, sf::Vertex{bl, cbl}}) quads.append(v);
            }
        target.draw(quads, sf::BlendAdd);
    }
    for (const auto& [center, color] : lights) {
        // Fire flickers; other lights hold steady.
        const bool fire = color.g < 200 && color.r > 200;
        const float flicker = fire ? 0.9f + 0.1f * std::sin(now * 9.f + center.x * 0.37f) : 1.f;
        addLight(center, color, (fire ? 3.4f : 2.4f) * flicker);
    }
    target.display();
    sf::Sprite overlay(target.getTexture());
    overlay.setPosition({kMapLeft, kMapTop});
    window_.draw(overlay, sf::BlendMultiply);

    // A soft vignette pulls the eye toward the middle of the map.
    sf::VertexArray edges(sf::PrimitiveType::Triangles);
    const float l = kMapLeft, t = kMapTop, r = kMapLeft + kMapWidth, b = kMapTop + kMapHeight, e = 70.f;
    const sf::Color edge(0, 0, 0, 120), none(0, 0, 0, 0);
    const auto strip = [&](sf::Vector2f p0, sf::Vector2f p1, sf::Vector2f p2, sf::Vector2f p3) {
        for (const auto& v : {sf::Vertex{p0, edge}, sf::Vertex{p1, edge}, sf::Vertex{p2, none},
                              sf::Vertex{p0, edge}, sf::Vertex{p2, none}, sf::Vertex{p3, none}}) edges.append(v);
    };
    strip({l, t}, {r, t}, {r, t + e}, {l, t + e});
    strip({l, b}, {r, b}, {r, b - e}, {l, b - e});
    strip({l, t}, {l, b}, {l + e, b}, {l + e, t});
    strip({r, t}, {r, b}, {r - e, b}, {r - e, t});
    window_.draw(edges);
}

void Application::updateCamera() {
    // Viewport size in whole tiles -- derived from the window/tile
    // constants rather than hardcoded again, so this stays correct if
    // either ever changes.
    constexpr int kViewportWidthTiles = static_cast<int>(kMapWidth / kTileSize);
    constexpr int kViewportHeightTiles = static_cast<int>(kMapHeight / kTileSize);

    const int desiredX = player_.position().x - kViewportWidthTiles / 2;
    const int desiredY = player_.position().y - kViewportHeightTiles / 2;

    // Clamped to [0, map dimension - viewport dimension] so the camera
    // never scrolls past the map's own edges and shows empty space
    // beyond it. If the map is smaller than the viewport in either
    // dimension (not expected at the current 60x32 default, but not
    // assumed impossible either), max(0, ...) keeps the clamp range
    // valid instead of inverting.
    const int maxCameraX = std::max(0, map_.width() - kViewportWidthTiles);
    const int maxCameraY = std::max(0, map_.height() - kViewportHeightTiles);

    cameraX_ = std::clamp(desiredX, 0, maxCameraX);
    cameraY_ = std::clamp(desiredY, 0, maxCameraY);
}

sf::Vector2f Application::worldToScreen(int tileX, int tileY) const {
    const auto shift = cameraShift();
    return {kMapLeft + static_cast<float>(tileX - cameraX_) * kTileSize + shift.x,
            kMapTop + static_cast<float>(tileY - cameraY_) * kTileSize + shift.y};
}

sf::FloatRect Application::letterbox() const {
    const auto size = window_.getSize();
    if (!size.x || !size.y) return {{0, 0}, {1, 1}};
    const float windowAspect = static_cast<float>(size.x) / size.y, gameAspect = static_cast<float>(kWindowWidth) / kWindowHeight;
    if (windowAspect > gameAspect) {
        const float w = gameAspect / windowAspect; // bars at the sides
        return {{(1 - w) / 2, 0}, {w, 1}};
    }
    const float h = windowAspect / gameAspect;     // bars above and below
    return {{0, (1 - h) / 2}, {1, h}};
}

void Application::fitView() {
    uiView_ = sf::View(sf::FloatRect({0, 0}, {static_cast<float>(kWindowWidth), static_cast<float>(kWindowHeight)}));
    uiView_.setViewport(letterbox());
    window_.setView(uiView_);
}

// F11: borderless fullscreen at the desktop's resolution, and back.
void Application::toggleFullscreen() {
    fullscreen_ = !fullscreen_;
    if (fullscreen_) window_.create(sf::VideoMode::getDesktopMode(), kWindowTitle, sf::State::Fullscreen);
    else window_.create(sf::VideoMode({kWindowWidth, kWindowHeight}), kWindowTitle);
    window_.setFramerateLimit(60);
    window_.setKeyRepeatEnabled(false);
    fitView();
}

void Application::processEvents() {
    while (const auto event=window_.pollEvent()) handleEvent(*event);
}

void Application::handleEvent(const sf::Event& input) {
    if (input.is<sf::Event::Resized>()) { fitView(); return; }
    if (const auto* key = input.getIf<sf::Event::KeyPressed>(); key && key->code == sf::Keyboard::Key::F11) { toggleFullscreen(); return; }
    // Mouse positions arrive in window pixels; everything below works in the
    // 1280x720 layout, whatever the window's size and shape.
    const auto toLayout = [&](sf::Vector2i pixel) {
        const auto p = window_.mapPixelToCoords(pixel, uiView_);
        return sf::Vector2i{static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y))};
    };
    sf::Event converted = input;
    if (const auto* m = input.getIf<sf::Event::MouseMoved>()) converted = sf::Event::MouseMoved{toLayout(m->position)};
    else if (const auto* m = input.getIf<sf::Event::MouseButtonPressed>()) converted = sf::Event::MouseButtonPressed{m->button, toLayout(m->position)};
    else if (const auto* m = input.getIf<sf::Event::MouseButtonReleased>()) converted = sf::Event::MouseButtonReleased{m->button, toLayout(m->position)};
    else if (const auto* m = input.getIf<sf::Event::MouseWheelScrolled>()) converted = sf::Event::MouseWheelScrolled{m->wheel, m->delta, toLayout(m->position)};
    const auto* event=&converted;
    if (event->is<sf::Event::Closed>()) {
        window_.close();
    }
    if(const auto* moved=event->getIf<sf::Event::MouseMoved>()) mousePixel_=moved->position;
    if (const auto* key=event->getIf<sf::Event::KeyPressed>(); key && key->code==sf::Keyboard::Key::N) {
        soundManager_.toggleMusic();
        log(soundManager_.musicEnabled() ? "Music on (N)." : "Music off (N).");
        return;
    }

    if (autoExploring_ && (event->is<sf::Event::KeyPressed>() ||
        event->is<sf::Event::MouseButtonPressed>() || event->is<sf::Event::FocusLost>())) {
        stopAutoExplore("interrupted by input or focus change.");
        return;
    }
    if (const auto* click=event->getIf<sf::Event::MouseButtonPressed>(); click && click->button==sf::Mouse::Button::Left) {
        const auto point=sf::Vector2f(click->position);
        if(mode_==GameMode::GameOver) {
            if(screen::kRevive.contains(point)) { reviveInTown(); return; }
            if(screen::kRestart.contains(point)) mode_=GameMode::ClassSelection;
            return;
        }
        if(mode_==GameMode::ClassSelection) {
            if(screen::kModeToggle.contains(point)) { adventureMode_=!adventureMode_; return; }
            if(screen::kStartLoad.contains(point)) { loadGame(); return; }
            for(int i=0;i<3;++i) if(screen::classCard(i).contains(point)) {
                selectClass(i==0?PlayerClass::Warrior:i==1?PlayerClass::Mage:PlayerClass::Thief);
                break;
            }
            return;
        }
        if(mode_==GameMode::AttributeAllocation) {
            if(screen::kAttributeClose.contains(point)) { mode_=GameMode::Playing; resumeLevelUpSequence(); return; }
            for(int i=0;i<3;++i) if(screen::attributeChoice(i).contains(point)) {
                allocateAttribute(static_cast<unsigned int>(i));
                break;
            }
            return;
        }
    }

    if (event->is<sf::Event::KeyPressed>() || event->is<sf::Event::MouseButtonPressed>()) {
        if (restTurns_>0) { restTurns_=0; return; }
    }
    if (inventoryOpen_ && !event->is<sf::Event::KeyPressed>()) {
        handleInventoryMouse(*event);
        return; // inventory mouse actions must never click through onto the map
    }
    if(ascendancyMenu_ && !event->is<sf::Event::KeyPressed>()) { handleAscendancyMouse(*event); return; }
    if(mode_==GameMode::Town && !event->is<sf::Event::KeyPressed>()) {
        handleTownMouse(*event);
        return;
    }
    if (mode_ == GameMode::AbilityChoice && !event->is<sf::Event::KeyPressed>()) {
        handleTreeMouse(*event);
        return; // closing a menu must not also activate the map underneath
    }
    if(exitMenu_ && !event->is<sf::Event::KeyPressed>()) { handleTravelMouse(*event); return; }
    if(vaultMenu_ && !event->is<sf::Event::KeyPressed>()) { handleVaultMouse(*event); return; }
    if(shrineMenu_ && !event->is<sf::Event::KeyPressed>()) { handleShrineMouse(*event); return; }
    if (mode_ == GameMode::Playing && !inventoryOpen_ && !vaultMenu_ && !exitMenu_) handleTargetingMouse(*event);

    if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
        if (ascendancyMenu_) { handleAscendancyKey(keyPressed->code); return; }
        if (mode_==GameMode::Town) { handleTownKey(keyPressed->code); return; }
        if (exitMenu_) {
            handleTravelKey(keyPressed->code);
            return;
        }
        if (vaultMenu_) { handleVaultKey(keyPressed->code); return; }
        if (shrineMenu_) { handleShrineKey(keyPressed->code); return; }
        if (keyPressed->code == sf::Keyboard::Key::Escape) {
            if (mode_ == GameMode::AbilityChoice) { handleTreeKey(keyPressed->code,keyPressed->shift); return; }
            if (mode_ == GameMode::AttributeAllocation) { mode_=GameMode::Playing; resumeLevelUpSequence(); return; }
            if (inventoryOpen_) {
                inventoryOpen_ = false;
                return;
            }
            if (mode_ == GameMode::Playing && (aimingTalent_ || inspecting_)) {
                cancelTargeting();
                return;
            }
            window_.close();
            return;
        }

        if (keyPressed->code == sf::Keyboard::Key::F9) {
            // Reachable from either mode -- a person who wants to
            // continue a previous run shouldn't have to pick a class
            // first just to reach a point where loading is possible.
            // loadGame() determines the actual class from the save
            // file itself and switches mode_ to Playing on success.
            loadGame();
            return;
        }

        if (mode_ == GameMode::ClassSelection) {
            // Deliberately only these keys handled here -- this
            // screen doesn't need arrow keys, talent keys, or save,
            // so nothing else in the Playing-mode switch below even
            // applies yet. As of Prompt 19: only the 3 base classes
            // are offered here -- Spellblade still exists in
            // PlayerClassFactory (see PlayerClass.hpp), just not as
            // a starting option anymore.
            if (keyPressed->code == sf::Keyboard::Key::M) { adventureMode_=!adventureMode_; return; }
            if (keyPressed->code == sf::Keyboard::Key::Num1) {
                selectClass(PlayerClass::Warrior);
            } else if (keyPressed->code == sf::Keyboard::Key::Num2) {
                selectClass(PlayerClass::Mage);
            } else if (keyPressed->code == sf::Keyboard::Key::Num3) {
                selectClass(PlayerClass::Thief);
            }
            return;
        }

        if (mode_ == GameMode::GameOver) {
            if (keyPressed->code == sf::Keyboard::Key::R) { reviveInTown(); return; }
            // selectClass() (reached via the ClassSelection screen
            // this leads back to) does the actual reset -- this
            // mode transition alone doesn't need to touch
            // map_/monsters_/player_ itself.
            if (keyPressed->code == sf::Keyboard::Key::Enter) {
                mode_ = GameMode::ClassSelection;
            }
            return;
        }

        if (mode_ == GameMode::AbilityChoice) {
            handleTreeKey(keyPressed->code, keyPressed->shift);
            return;
        }
        if (keyPressed->code == sf::Keyboard::Key::F5) { saveGame(); return; }

        if (mode_ == GameMode::AttributeAllocation) {
            if (keyPressed->code == sf::Keyboard::Key::P) {
                mode_=GameMode::Playing; resumeLevelUpSequence();
            } else if (keyPressed->code == sf::Keyboard::Key::Num1) {
                allocateAttribute(0);
            } else if (keyPressed->code == sf::Keyboard::Key::Num2) {
                allocateAttribute(1);
            } else if (keyPressed->code == sf::Keyboard::Key::Num3) {
                allocateAttribute(2);
            }
            return;
        }

        if (inventoryOpen_) {
            handleInventoryKey(keyPressed->code);
            return;
        }
        if (keyPressed->code == sf::Keyboard::Key::B) {
            openInventory();
            return;
        }
        if (keyPressed->code == sf::Keyboard::Key::T) { openTalentTrees(); return; }
        if (keyPressed->code == sf::Keyboard::Key::P) { openLevelUp(); return; }
        if (keyPressed->code == sf::Keyboard::Key::Y) { openAscendancy(); return; }
        if (keyPressed->code == sf::Keyboard::Key::L) { toggleLight(); return; }
        if (keyPressed->code == sf::Keyboard::Key::C) {
            for (std::size_t i=0;i<player_.talents().knownTalents().size();++i)
                if (player_.talents().knownTalents()[i].id=="basic.cleanse") { requestTalent(i); break; }
            return;
        }
        if (keyPressed->code == sf::Keyboard::Key::Space) {
            player_.statusEffects().apply({StatusEffectType::Opening,2,0});
            finishInventoryTurn(); return;
        }
        if (keyPressed->code == sf::Keyboard::Key::G) {
            pickupItem();
            return;
        }
        if (handleTargetingKey(keyPressed->code, keyPressed->shift)) return;

        switch (keyPressed->code) {
            case sf::Keyboard::Key::Up:
            case sf::Keyboard::Key::W:
                tryMovePlayer(0, -1);
                break;
            case sf::Keyboard::Key::Down:
            case sf::Keyboard::Key::S:
                tryMovePlayer(0, 1);
                break;
            case sf::Keyboard::Key::Left:
            case sf::Keyboard::Key::A:
                tryMovePlayer(-1, 0);
                break;
            case sf::Keyboard::Key::Right:
            case sf::Keyboard::Key::D:
                tryMovePlayer(1, 0);
                break;
            case sf::Keyboard::Key::H:
                returnToTown();
                break;
            case sf::Keyboard::Key::Z:
                startAutoExplore();
                break;
            case sf::Keyboard::Key::R:
                startRest();
                break;
            case sf::Keyboard::Key::Num1:
                requestHotbar(talentPage_ * 9);
                break;
            case sf::Keyboard::Key::Num2:
                requestHotbar(talentPage_ * 9 + 1);
                break;
            case sf::Keyboard::Key::Num3:
                requestHotbar(talentPage_ * 9 + 2);
                break;
            case sf::Keyboard::Key::Num4:
                requestHotbar(talentPage_ * 9 + 3);
                break;
            case sf::Keyboard::Key::Num5:
                requestHotbar(talentPage_ * 9 + 4);
                break;
            case sf::Keyboard::Key::Num6:
                requestHotbar(talentPage_ * 9 + 5);
                break;
            case sf::Keyboard::Key::Num7:
                requestHotbar(talentPage_ * 9 + 6);
                break;
            case sf::Keyboard::Key::Num8:
                requestHotbar(talentPage_ * 9 + 7);
                break;
            case sf::Keyboard::Key::Num9:
                requestHotbar(talentPage_ * 9 + 8);
                break;
            case sf::Keyboard::Key::F5:
                saveGame();
                break;
            default:
                break;
        }
    }
}

bool Application::isOccupied(Position pos, const Actor* exclude) {
    return actorAt(pos, exclude) != nullptr;
}

Actor* Application::actorAt(Position pos, const Actor* exclude) {
    // The integration-pass bug fixed in Prompt 11: movement previously
    // only checked map_.isWalkable() (terrain), never whether another
    // actor already stood on the destination tile. Never surfaced in
    // earlier testing because every scripted test walked to a tile
    // *adjacent* to a target, never onto it -- but nothing stopped a
    // real player (or two monsters converging from different angles)
    // from sharing a tile.
    if (&player_ != exclude && player_.position().x == pos.x && player_.position().y == pos.y) {
        return &player_;
    }
    for (auto& m : monsters_) {
        if (m.get() == exclude || m->stats().hp <= 0) {
            continue;
        }
        if (m->position().x == pos.x && m->position().y == pos.y) {
            return m.get();
        }
    }
    return nullptr;
}

bool Application::tryMovePlayer(int dx, int dy) {
    if (!window_.isOpen()) {
        return false;
    }

    const Position current = player_.position();
    const Position target{current.x + dx, current.y + dy};

    if (!map_.isWalkable(target.x, target.y)) {
        // Walking into a brazier or an oil barrel knocks it over.
        if (!autoExploring_ && knockOver(target, {dx, dy})) { combatThisTurn_ = true; finishInventoryTurn(); return true; }
        return false; // wall or map edge -- a pure input mistake, no turn consumed
    }

    Actor* blocker = actorAt(target, &player_);
    if (auto* ally=dynamic_cast<Monster*>(blocker); ally && ally->allied) {
        ally->setPosition(current); player_.setPosition(target); finishInventoryTurn(); return true;
    }
    if (blocker != nullptr) {
        lastMoveDirection_ = Position{dx, dy};
        for (std::size_t i=0;i<player_.talents().knownTalents().size();++i)
            if (player_.talents().knownTalents()[i].id=="basic.attack") return tryUseTalent(i,target);
        return false;
    }

    player_.setPosition(target);
    for (const auto& item : groundItems_) {
        if (item->position().x == target.x && item->position().y == target.y)
            log("You see ", item->name(), ". G: pick up (1 turn).");
    }
    lastMoveDirection_ = Position{dx, dy};
    advanceEnemyIntents();
    player_.talents().tickCooldowns();
    updateFieldOfView();

    currentActor_ = &scheduler_.nextTurn();
    processMonsterTurns();
    advanceTurnsUntilPlayerCanAct();

    // Stepping onto a door tile advances to the next floor -- checked
    // after the normal move/turn processing above completes, not
    // before, so the move itself still counts as a real turn (monsters
    // still get to act on it) even though regenerateLevel() below
    // immediately replaces them anyway. On a boss floor this tile only
    // becomes reachable once the boss is actually dead -- while alive
    // it occupies (and blocks) this exact position like any other
    // actor, so no separate "is the boss defeated" check is needed
    // here at all.
    if (!autoExploring_ && mode_==GameMode::Playing && target.x==floorExit_.x && target.y==floorExit_.y) {
        cancelTargeting(); exitMenu_=true;
    }

    return true;
}

bool Application::tryUseTalent(std::size_t talentIndex, Position cursor) {
    if (!window_.isOpen()) {
        return false;
    }

    const std::vector<Talent>& talents = player_.talents().knownTalents();
    if (talentIndex >= talents.size()) {
        return false;
    }
    // Own a copy: killing a target can grant a talent and reallocate the kit.
    Talent talent = combatTalent(player_,player_.talents().effectiveTalent(talentIndex));

    const auto unavailable = talentUnavailableReason(player_, talentIndex);
    if (!unavailable.empty()) {
        log(unavailable);
        return false;
    }

    const TalentTarget target = targetPreview(talentIndex, cursor, true);
    if (!target.valid) {
        log(target.message);
        return false;
    }
    const auto& affected = target.affected;
    const Actor* chainedTarget = target.chainedTarget;
    const Position blinkDestination = target.destination;

    const auto* equippedWeapon=player_.inventory().equipped(EquipmentSlot::Weapon);
    talent.committedBloodlust=equippedWeapon && equippedWeapon->definition()->weaponKind==WeaponKind::TwoHanded &&
        player_.stats().hp*2<=player_.stats().maxHp ? player_.talents().passiveValue(PassiveKind::Bloodlust) : 0;
    const Position beforeMovement=player_.position();
    const int wasConcealed=player_.statusEffects().magnitudeOf(StatusEffectType::Concealed);
    bool landedAny=false, killedAny=false;
    // Commit point: validation and preview above are side-effect free.
    if (talent.effectKind==TalentEffectKind::Damage) { combatThisTurn_=true; notifyAttack(player_,cursor); }
    cancelTargeting();

    player_.stats().mana -= talent.manaCost;
    player_.stats().hp -= talent.hpCost;
    spawnTalentVfx(talent, beforeMovement, cursor, target);

    if (talent.boneSwap) {
        if (!affected.empty()) { affected.front()->setPosition(beforeMovement); player_.setPosition(blinkDestination); applyMovementTalents(beforeMovement); }
    } else if (talent.summonCount) {
        summonMinions(talent);
    } else if (talent.shape == EffectShape::Movement) {
        player_.setPosition(blinkDestination);
        log(player_.name(), " uses ", talent.name, ", blinks to (", blinkDestination.x, ',',
            blinkDestination.y, ")");
    } else if (talent.effectKind == TalentEffectKind::Heal) {
        for (Actor* target : affected) {
            applyTalentHeal(talent, player_, *target);
            log(player_.name(), " uses ", talent.name, " (", target->stats().hp, "/",
                target->stats().maxHp, " hp)");
        }
    } else if (talent.effectKind == TalentEffectKind::SelfBuff) {
        for (Actor* target : affected) {
            applyTalentSelfBuff(talent, *target);
        }
        log(player_.name(), " uses ", talent.name, "!");
        if (talent.restoreMana) log("Mana: ",player_.stats().mana,"/",player_.stats().maxMana);
        if (talent.restoreHpPercent) log("HP: ",player_.stats().hp,"/",player_.stats().maxHp);
        if (talent.cleanse) log("Poison, Burn, Chill, Marked and curses removed. Other effects remain.");
        if (talent.conjureLight) {
            lightOrbs_.clear();
            lightOrbs_.push_back({player_.position(), 40});
            log("A wisp of light hangs in the air.");
            updateFieldOfView();
        }
    } else {
        if (affected.empty()) log(player_.name(), " uses ", talent.name, " on empty ground.");
        for (Actor* target : affected) {
            Talent hitTalent = talent;
            if (target == chainedTarget) hitTalent.damagePercent /= 2;
            if (const int momentum=player_.talents().passiveValue(PassiveKind::Momentum)) {
                momentumStreak_=target==momentumTarget_ ? std::min(momentumStreak_+1,3) : 0;
                momentumTarget_=target;
                hitTalent.power+=momentum*momentumStreak_;
            }
            std::string combo;
            if (target->statusEffects().has(StatusEffectType::Marked)) combo+="Marked +25%; charge consumed. ";
            if (talent.consumeBurn && target->statusEffects().has(StatusEffectType::Burn)) combo+="Burn consumed: +50%. ";
            if (talent.consumeShock && target->statusEffects().has(StatusEffectType::Shock)) combo+="Shock consumed: +50%. ";
            if (talent.consumeChill && target->statusEffects().has(StatusEffectType::Chill)) combo+="Chill consumed: +50%. ";
            if (target->statusEffects().has(StatusEffectType::Guard)) combo+="Guard reduces direct damage. ";
            const bool couldStun=target->statusEffects().canReceiveStun();
            const int hpBefore=target->stats().hp;
            if (applyTalentDamage(hitTalent, player_, *target)) {
                landedAny=true; killedAny=killedAny || target->stats().hp<=0;
                if (target->stats().hp<hpBefore) flashActor(*target);
                if (!combo.empty()) log(target->name(), ": ", combo, "Hit dealt ",hpBefore-target->stats().hp," damage.");
                if (!couldStun && (talent.consumeChill || (talent.onHitEffect && talent.onHitEffect->type==StatusEffectType::Stun)))
                    log(target->name(), " is protected from another stun.");
                if (talent.onHitEffect && talent.onHitEffect->type==StatusEffectType::Marked && target->stats().hp>0)
                    log(target->name(), " is Marked: next direct hit +25%.");
                log(player_.name(), " uses ", talent.name, " on ", target->name(), " (",
                    target->stats().hp, "/", target->stats().maxHp, " hp left)");
                soundManager_.play(SoundEffect::Hit);
                if (target->stats().hp>0 && talent.pushDistance>0) {
                    const auto from=player_.position(), origin=target->position();
                    const Position direction{(origin.x>from.x)-(origin.x<from.x),(origin.y>from.y)-(origin.y<from.y)};
                    for (int step=0;step<talent.pushDistance;++step) {
                        const auto pos=target->position();
                        const Position dest{pos.x+direction.x,pos.y+direction.y};
                        if (!map_.isWalkable(dest.x,dest.y) || isOccupied(dest,target) ||
                            (dest.x!=pos.x && dest.y!=pos.y && (!map_.isWalkable(dest.x,pos.y) || !map_.isWalkable(pos.x,dest.y)))) break;
                        target->setPosition(dest);
                    }
                }
                checkAndHandleDeath(*target);
            } else {
                log(target->name(), " dodges ", player_.name(), "'s ", talent.name, "!");
                soundManager_.play(SoundEffect::Dodge);
            }
        }

        // Retreat follows the aimed direction even when the kick hits air.
        if (talent.retreatDistance > 0) {
            const Position targetPos = cursor;
            const Position awayDirection{player_.position().x - targetPos.x,
                                          player_.position().y - targetPos.y};
            const Position retreatDestination = target.destination;
            player_.setPosition(retreatDestination);
            lastMoveDirection_ = awayDirection;
            log(player_.name(), " vaults back to (", retreatDestination.x, ',',
                retreatDestination.y, ")");
        }
    }

    afterHiddenCast(talent,landedAny,killedAny,wasConcealed);
    if (const Element element=talentElement(talent); element!=Element::None) {
        std::vector<Position> touched=target.area;
        if (!target.path.empty()) touched.push_back(target.path.back());
        applyElement(element,touched);
    }
    if (talent.selfBuffEffect && !talent.returnConcealed && talent.effectKind!=TalentEffectKind::SelfBuff) player_.statusEffects().apply(*talent.selfBuffEffect);
    if (talent.shape==EffectShape::Movement || talent.retreatDistance>0) { applyMovementTalents(beforeMovement); }
    player_.talents().startCooldown(talentIndex);
    advanceEnemyIntents();
    player_.talents().tickCooldowns();
    updateFieldOfView(); // in case Blink moved the player

    currentActor_ = &scheduler_.nextTurn();
    processMonsterTurns();
    advanceTurnsUntilPlayerCanAct();
    return true;
}

void Application::advanceEnemyIntents() {
    tickSurfaces();
    for (auto& m:monsters_) {
        if (m->allied && m->remainingLife>0 && --m->remainingLife==0) { m->stats().hp=0; scheduler_.remove(*m); log("A temporary skeleton dissolves."); }
        if (m->intent() && m->intent()->playerActionsRemaining>0) --m->intent()->playerActionsRemaining;
        if (m->recoveryActions>0) --m->recoveryActions;
    }
    removeDeadMonsters();
}

void Application::processMonsterTurns() {
    while (window_.isOpen() && currentActor_ != &player_ && mode_!=GameMode::GameOver) {
        Actor* actor = currentActor_;

        auto* monster=dynamic_cast<Monster*>(actor);
        bool interrupted=false;
        if (monster && monster->intent()) {
            const auto& intent=*monster->intent();
            if (actor->statusEffects().has(StatusEffectType::Stun) ||
                actor->position().x!=intent.origin.x || actor->position().y!=intent.origin.y) {
                log(actor->name(), "'s wind-up is interrupted!");
                monster->intent().reset();
                if (monster->type()==MonsterType::GoblinWarlord || monster->type()==MonsterType::Lich) monster->recoveryActions=1;
                interrupted=true;
            }
        }
        const bool hadPoison = actor->statusEffects().has(StatusEffectType::Poison);
        const int chillMagnitude=actor->statusEffects().magnitudeOf(StatusEffectType::Chill);
        bool chilledMove=false;
        for (const auto& e:actor->statusEffects().active()) if (e.type==StatusEffectType::Chill) chilledMove=e.turnsRemaining%2==1;
        for (const auto& effect:actor->statusEffects().active()) {
            if (isCurse(effect.type) && monster && monster->allied) combatThisTurn_=true;
            if (effect.type==StatusEffectType::Doom && effect.turnsRemaining==1)
                log(actor->name()," suffers ",effect.magnitude," damage from Doom!");
        }
        const bool stunned = tickStatusEffects(*actor);
        if (hadPoison && actor->stats().hp > 0) {
            log(actor->name(), " takes poison damage (", actor->stats().hp, "/",
                actor->stats().maxHp, " hp left)");
        }
        checkAndHandleDeath(*actor);

        if (actor->stats().hp > 0) {
            if (stunned) {
                log(actor->name(), " is stunned and loses a turn!");
            } else if (interrupted) {
                // Displacement cancels the attack and spends this enemy action.
            } else if (monster && monster->recoveryActions>0) {
                // A full player action is guaranteed before the boss acts again.
            } else if (monster && monster->intent()) {
                if (monster->intent()->playerActionsRemaining==0) {
                    const EnemyIntent intent=*monster->intent();
                    monster->intent().reset();
                    if (monster->type()==MonsterType::GoblinWarlord || monster->type()==MonsterType::SkeletonGuard || intent.kind==IntentKind::Summon) {
                        monster->recoveryActions=1;
                        log(actor->name(), " will recover for one player action after this attack.");
                    }
                    if (exploredMap_.at(intent.target.x,intent.target.y)==Visibility::Visible) spawnReleaseVfx(intent);
                    if (intent.kind==IntentKind::MagicStrike && monster->type()==MonsterType::GoblinWarlord) {
                        for (int y=intent.target.y-intent.radius;y<=intent.target.y+intent.radius;++y)
                            for (int x=intent.target.x-intent.radius;x<=intent.target.x+intent.radius;++x)
                                if (intent.contains({x,y}) && hasLineOfFire(map_,intent.target,{x,y}) && (x+y)%2==0)
                                    setSurface({x,y},SurfaceType::Fire,kSpilledFireTurns);
                    }
                    if (intent.kind==IntentKind::MagicStrike && monster->type()==MonsterType::Bomber) {
                        std::vector<Position> blast;
                        for (int y=intent.target.y-intent.radius;y<=intent.target.y+intent.radius;++y)
                            for (int x=intent.target.x-intent.radius;x<=intent.target.x+intent.radius;++x)
                                if (intent.contains({x,y})) blast.push_back({x,y});
                        applyElement(Element::Fire,blast);
                    }
                    suppressAttackVfx_=true;
                    if (intent.kind==IntentKind::Summon) {
                        AIDecision summon;
                        summon.type=AIActionType::Summon; summon.movePosition=intent.target;
                        // The committed attempt count is saved, so the reinforcement
                        // order survives interruption, loading, and floor travel.
                        summon.summonType=monster->summonsCommitted==2?MonsterType::SkeletonArcher:MonsterType::SkeletonGuard;
                        summon.abilityIndex=actor->talents().knownTalents().size(); // cooldown reserved at commitment
                        if (isOccupied(intent.target,actor)) log("The occupied ritual tile disrupts the summon!");
                        else { log(actor->name(), " completes its ritual!"); executeAIDecision(*actor,summon,chillMagnitude); }
                    } else {
                        log(actor->name(), " releases its committed attack!");
                        if (intent.contains(player_.position()) && hasLineOfFire(map_,intent.target,player_.position())) {
                            AIDecision hit;
                            hit.type=AIActionType::Attack; hit.target=&player_; hit.attackPower=intent.attackPower;
                            if (intent.kind==IntentKind::MagicStrike) hit.scalingStat=ScalingStat::Intelligence;
                            else if (intent.kind==IntentKind::StunStrike) hit.effectToApply=StatusEffectInstance{StatusEffectType::Stun,1,0};
                            executeAIDecision(*actor,hit,chillMagnitude);
                        } else log("You escaped the marked area.");
                        std::vector<Actor*> victims;
                        for (auto& m:monsters_) if (m.get()!=actor && m->stats().hp>0 && intent.contains(m->position()) && hasLineOfFire(map_,intent.target,m->position())) victims.push_back(m.get());
                        for (auto* ally:victims) {
                            if (mode_==GameMode::GameOver || actor->stats().hp<=0) break;
                            if (ally->stats().hp<=0) continue;
                            AIDecision hit; hit.type=AIActionType::Attack; hit.target=ally; hit.attackPower=intent.attackPower;
                            if (intent.kind==IntentKind::MagicStrike) hit.scalingStat=ScalingStat::Intelligence;
                            if (intent.kind==IntentKind::StunStrike) hit.effectToApply=StatusEffectInstance{StatusEffectType::Stun,1,0};
                            executeAIDecision(*actor,hit,chillMagnitude);
                        }
                    }
                    suppressAttackVfx_=false;
                }
            } else if (monster && !monster->allied && monster==boss_ && bossSurfaceAction(*monster)) {
                // The boss spent its turn on a brazier or on the light.
            } else if (monster && monster->allied) {
                actMinion(*monster,chilledMove);
            } else if (actor->ai() != nullptr) {
                bool hidden=player_.statusEffects().has(StatusEffectType::Concealed);
                if (hidden) {
                    const float chance=enemyStealthDetectionChance(*actor);
                    // Exactly one check on this enemy's real, non-stunned turn.
                    // Previewing, waiting at a menu and out-of-sight enemies never roll.
                    if (chance>0.f && rollChance(chance)) {
                        player_.statusEffects().remove(StatusEffectType::Concealed);
                        hidden=false;
                        log(actor->name(), " spots you! Concealment breaks.");
                    }
                }
                if (monster) {
                    auto* opponent=nearestOpponent(*actor,hidden);
                    const AIDecision decision=enemyDecision(*monster,opponent);
                    const bool warlord=monster && monster->type()==MonsterType::GoblinWarlord;
                    const bool lich=monster && monster->type()==MonsterType::Lich;
                    const bool blast=monster && (monster->type()==MonsterType::Bomber || monster->type()==MonsterType::OssuaryWarden || warlord) && decision.type==AIActionType::UseAbility;
                    const bool slam=monster && monster->type()==MonsterType::Ogre && decision.type==AIActionType::Attack &&
                        decision.effectToApply && decision.effectToApply->type==StatusEffectType::Stun;
                    const bool guard=monster && monster->type()==MonsterType::SkeletonGuard;
                    const bool heavy=decision.type==AIActionType::Attack && (guard || (warlord && actor->stats().hp*10<=actor->stats().maxHp*3));
                    const bool bolt=lich && decision.type==AIActionType::Attack;
                    const bool summon=lich && decision.type==AIActionType::Summon;
                    if (blast || slam || heavy || bolt || summon) {
                        const Position aim=summon?decision.movePosition:opponent->position();
                        // Both caster and warning tile must be visible before commitment.
                        const auto* allyTarget=dynamic_cast<const Monster*>(opponent);
                        if ((allyTarget && allyTarget->allied) ||
                            (exploredMap_.at(actor->position().x,actor->position().y)==Visibility::Visible &&
                             exploredMap_.at(aim.x,aim.y)==Visibility::Visible)) {
                            const auto kind=summon?IntentKind::Summon:blast||bolt?IntentKind::MagicStrike:heavy?IntentKind::HeavyStrike:IntentKind::StunStrike;
                            const int radius=blast?2:heavy?1:0;
                            const int warning=blast?3:(summon || heavy)?2:1;
                            monster->intent()=EnemyIntent{actor->position(),aim,radius,warning,decision.attackPower,kind};
                            if (!decision.announcement.empty()) log(decision.announcement);
                            if (blast || summon) actor->talents().startCooldown(decision.abilityIndex);
                            if (summon) {
                                ++monster->summonsCommitted;
                                log("Ritual: 2 actions to interrupt or occupy the purple tile. Attempt ",monster->summonsCommitted,"/3.");
                            } else log(actor->name()," prepares a strike: ",warning," action(s) to leave the marked tiles. It can hit other enemies.");
                        }
                    } else if (!(chilledMove && decision.type==AIActionType::Move)) executeAIDecision(*actor,decision,chillMagnitude);
                }
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
        const int manaRegen=combatThisTurn_ || dangerNearby() ? kManaRegenInCombat : kManaRegenOutsideCombat;
        player_.stats().mana =
            std::min(player_.stats().mana + manaRegen, player_.stats().maxMana);

        const bool hadPoison = player_.statusEffects().has(StatusEffectType::Poison);
        if (hadPoison || player_.statusEffects().has(StatusEffectType::Burn) ||
            player_.statusEffects().has(StatusEffectType::ManaDrain) || player_.statusEffects().has(StatusEffectType::Doom)) combatThisTurn_=true;
        for (const auto& effect:player_.statusEffects().active()) {
            if (effect.type==StatusEffectType::Doom && effect.turnsRemaining==1)
                log("Doom erupts for ",effect.magnitude," damage!");
        }
        const bool stunned = tickStatusEffects(player_);
        if (hadPoison && player_.stats().hp > 0) {
            log("Poison deals damage (", player_.stats().hp, "/", player_.stats().maxHp,
                " hp left)");
        }
        checkAndHandleDeath(player_);
        if (!window_.isOpen() || mode_==GameMode::GameOver) {
            return;
        }
        if (!stunned) {
            recordQuietTurn();
            return; // genuinely the player's turn now
        }
        log("You are stunned and lose a turn!");
        player_.talents().tickCooldowns();
        currentActor_ = &scheduler_.nextTurn();
        processMonsterTurns();
    }
}

void Application::executeAIDecision(Actor& actor, const AIDecision& decision, int chillMagnitude) {
    if (decision.type==AIActionType::Attack || decision.type==AIActionType::UseAbility) {
        const auto* attacker=dynamic_cast<const Monster*>(&actor);
        const auto* target=dynamic_cast<const Monster*>(decision.target);
        if (decision.target==&player_ || (attacker && attacker->allied) || (target && target->allied)) combatThisTurn_=true;
    }
    if (!decision.announcement.empty()) {
        log(decision.announcement);
    }

    switch (decision.type) {
        case AIActionType::Move: {
            Position step = decision.movePosition;
            if (hazardousSurface(step) && !hazardousSurface(actor.position())) {
                // Sidestep around the hazard if a safe tile also leads that way, else wait.
                Position best = actor.position();
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        const Position n{actor.position().x + dx, actor.position().y + dy};
                        if ((dx || dy) && std::max(std::abs(n.x - step.x), std::abs(n.y - step.y)) <= 1 && map_.isWalkable(n.x, n.y) &&
                            !hazardousSurface(n) && !isOccupied(n, &actor) && !(n.x == step.x && n.y == step.y)) { best = n; }
                    }
                step = best;
            }
            if (map_.isWalkable(step.x, step.y) && !isOccupied(step, &actor)) actor.setPosition(step);
            break;
        }

        case AIActionType::Attack:
        case AIActionType::UseAbility: {
            if (decision.target == nullptr) {
                break;
            }
            notifyAttack(actor, decision.target->position());

            if(decision.healAmount>0 && decision.target->stats().hp>0) {
                const int healed=std::min(decision.healAmount,decision.target->stats().maxHp-decision.target->stats().hp);
                decision.target->stats().hp+=healed;
                if(auto* patient=dynamic_cast<Monster*>(decision.target)) patient->lastObservedHp=patient->stats().hp;
                if(auto* healer=dynamic_cast<Monster*>(&actor)) --healer->tactics.heals;
                if(exploredMap_.at(actor.position().x,actor.position().y)==Visibility::Visible)
                    log(actor.name()," heals ",decision.target->name()," for ",healed," HP.");
            }
            bool dodged = false;
            if (decision.attackPower > 0 && !suppressAttackVfx_) {
                // Slingers lob oil pots; frost acolytes freeze the ground around their target.
                if (const auto* attacker = dynamic_cast<const Monster*>(&actor); attacker && !attacker->allied) {
                    const Position at = decision.target->position();
                    if (attacker->type() == MonsterType::GoblinSlinger) {
                        bool splashed = false;
                        for (const Position d : {Position{0, 0}, Position{1, 0}, Position{0, 1}, Position{-1, 0}})
                            if (surfaceAt({at.x + d.x, at.y + d.y}) == SurfaceType::None && map_.isWalkable(at.x + d.x, at.y + d.y)) {
                                setSurface({at.x + d.x, at.y + d.y}, SurfaceType::Oil, 0); splashed = true;
                                if (d.x || d.y) break;
                            }
                        if (splashed && visibleTile(at)) log(actor.name(), "'s pot shatters, splashing oil!");
                    } else if (attacker->type() == MonsterType::OrcFirebrand) {
                        std::vector<Position> splash{at};
                        for (const Position d : {Position{1, 0}, Position{0, 1}})
                            if (map_.isWalkable(at.x + d.x, at.y + d.y)) { splash.push_back({at.x + d.x, at.y + d.y}); break; }
                        for (const auto& p : splash) if (surfaceAt(p) == SurfaceType::None) setSurface(p, SurfaceType::Oil, 0);
                        if (visibleTile(at)) log(actor.name(), "'s flask bursts into flame!");
                        applyElement(Element::Fire, splash);
                    } else if (attacker->type() == MonsterType::Lich && conducts(surfaceAt(at))) {
                        applyElement(Element::Ice, {at});
                    } else if (attacker->type() == MonsterType::FrostAcolyte) {
                        std::vector<Position> chill;
                        for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) chill.push_back({at.x + dx, at.y + dy});
                        applyElement(Element::Ice, chill);
                    }
                }
            }
            if (decision.attackPower > 0) {
                dodged = rollChance(std::min(.60f,dodgeChance(decision.target->stats().dexterity)+(decision.target->statusEffects().magnitudeOf(StatusEffectType::Evasion)+armourDodgeBonus(*decision.target)+ascendancyDodgeBonus(*decision.target))/100.f));
                spawnAttackVfx(actor, *decision.target, decision.scalingStat==ScalingStat::Intelligence, dodged);
                if (dodged) {
                    if (decision.target->talents().passiveValue(PassiveKind::Slippery)) decision.target->statusEffects().apply({StatusEffectType::Opening,2,0});
                    const bool adjacent=std::max(std::abs(actor.position().x-decision.target->position().x),
                                                 std::abs(actor.position().y-decision.target->position().y))<=1;
                    if (const int counter=decision.target->talents().passiveValue(PassiveKind::Counter); counter && adjacent && actor.stats().hp>0) {
                        actor.stats().hp-=counter; flashActor(actor);
                        log(decision.target->name()," counters for ",counter,"!");
                        checkAndHandleDeath(actor);
                    }
                    log(decision.target->name(), " dodges ", actor.name(), "'s attack!");
                    soundManager_.play(SoundEffect::Dodge);
                } else {
                    int damage = decision.attackPower;
                    if (const auto* gloom = dynamic_cast<const Monster*>(&actor); gloom && gloom->type() == MonsterType::Gloomstalker)
                        damage = tileLit(actor.position()) ? damage / 2 : damage * 3 / 2;
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
                    if (chillMagnitude>0) damage=damage*(100-chillMagnitude)/100;
                    const bool marked=decision.target->statusEffects().has(StatusEffectType::Marked);
                    if (marked) damage=damage*(100+kMarkedDamagePercent)/100;
                    bool crit = false;
                    if (rollCrit(actor.stats().dexterity)) {
                        crit = true;
                        damage = static_cast<int>(static_cast<float>(damage) * critDamageMultiplier());
                    }
                    int guard=decision.target->statusEffects().magnitudeOf(StatusEffectType::Guard);
                    if (guard && decision.target->inventory().equipped(EquipmentSlot::OffHand)) guard+=decision.target->talents().passiveValue(PassiveKind::ShieldTraining);
                    guard+=armourGuardBonus(*decision.target)+ascendancyGuardBonus(*decision.target);
                    damage=std::max(0,damage-guard);
                    decision.target->stats().hp -= damage;
                    if (damage>0) flashActor(*decision.target);
                    if (const int retribution=decision.target->talents().passiveValue(PassiveKind::Retribution);
                        retribution && guard && actor.stats().hp>0 && std::max(std::abs(actor.position().x-decision.target->position().x),
                                                                               std::abs(actor.position().y-decision.target->position().y))<=1) {
                        actor.stats().hp-=retribution;
                        log(actor.name()," is struck by retribution for ",retribution,".");
                        checkAndHandleDeath(actor);
                    }
                    if (const int thorns=decision.target->inventory().affixTotal(BonusStat::Thorns);
                        thorns && damage>0 && std::max(std::abs(actor.position().x-decision.target->position().x),
                                                       std::abs(actor.position().y-decision.target->position().y))<=1) {
                        actor.stats().hp-=thorns;
                        log(actor.name()," is cut by thorns for ",thorns,".");
                        checkAndHandleDeath(actor);
                    }
                    if (marked) { decision.target->statusEffects().consumeMark(); log("Marked consumed: +25% direct damage before Guard."); }
                    if (guard) log("Guard reduced the incoming hit by up to ",guard," damage.");
                    if (damage>0) decision.target->statusEffects().remove(StatusEffectType::Concealed);
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
                const bool blocked=decision.effectToApply->type==StatusEffectType::Stun && !decision.target->statusEffects().canReceiveStun();
                const bool armourBlocked=decision.effectToApply->type==StatusEffectType::Stun && !blocked && armourResistsStun(*decision.target);
                if (!armourBlocked) decision.target->statusEffects().apply(*decision.effectToApply);
                if (armourBlocked) log(decision.target->name(), " resists stun with Unyielding!");
                else if (blocked) log(decision.target->name(), " resists stun: recovery or an existing stun prevents reapplication.");
                else {
                    log(decision.target->name(), " is affected by ",statusName(decision.effectToApply->type),"!");
                    if (decision.effectToApply->type==StatusEffectType::Doom)
                        log("Doom: ",decision.effectToApply->magnitude," delayed HP damage. C removes it; Guard and dodge do not prevent it.");
                    if (decision.effectToApply->type==StatusEffectType::ManaDrain)
                        log("Mana Drain: loses ",decision.effectToApply->magnitude," mana each tick. C removes it.");
                }
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
                scaleDungeonMonster(*summoned,currentFloor_);
                summoned->setRewardsEligible(false);
                summoned->recoveryActions=1; // no immediate attack before a player response
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
        if(auto* m=dynamic_cast<Monster*>(&actor); m && !m->allied) {
            if(m->stats().hp<m->lastObservedHp) {
                m->tactics.concealed=false;
                if(m->tactics.alert==0) alertEnemyGroup(*m,m->position());
            }
            m->lastObservedHp=m->stats().hp;
        }
        return;
    }

    soundManager_.play(SoundEffect::Death);

    if (&actor == &player_) {
        if (player_.talents().passiveValue(PassiveKind::Deathless) &&
            std::find(player_.deathlessSpentFloors.begin(),player_.deathlessSpentFloors.end(),currentFloor_)==player_.deathlessSpentFloors.end()) {
            player_.deathlessSpentFloors.push_back(currentFloor_); player_.stats().hp=1;
            const int guard=player_.talents().passiveValue(PassiveKind::Deathless)-1;
            if (guard) player_.statusEffects().apply({StatusEffectType::Guard,2,guard});
            log("Deathless saves you at 1 HP! Spent on this floor."); return;
        }
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
    if (defeated && defeated->allied) {
        scheduler_.remove(actor);
        const int explosion=player_.talents().passiveValue(PassiveKind::GravePact);
        if (explosion) for (auto& enemy:monsters_) if (!enemy->allied && enemy->stats().hp>0 &&
            std::abs(enemy->position().x-actor.position().x)+std::abs(enemy->position().y-actor.position().y)<=1) {
            enemy->stats().hp-=explosion; checkAndHandleDeath(*enemy);
        }
        return;
    }
    if (defeated) rewardMonster(*defeated, &actor == boss_);
    if (defeated) {
        if (const int thief=player_.talents().passiveValue(PassiveKind::SpellThief); thief && !defeated->allied) {
            const auto& fx=defeated->statusEffects();
            if (fx.has(StatusEffectType::Burn) || fx.has(StatusEffectType::Chill) || fx.has(StatusEffectType::Shock) ||
                fx.has(StatusEffectType::Poison) || fx.has(StatusEffectType::Marked))
                player_.stats().mana=std::min(player_.stats().maxMana,player_.stats().mana+thief);
        }
        if (const int siphon=player_.inventory().affixTotal(BonusStat::ManaOnKill); siphon && !defeated->allied)
            player_.stats().mana=std::min(player_.stats().maxMana,player_.stats().mana+siphon);
        // Juggernaut's Rampage: every kill shortens running cooldowns.
        if (const int rampage=player_.talents().passiveValue(PassiveKind::Rampage))
            for (std::size_t i=0;i<player_.talents().knownTalents().size();++i)
                player_.talents().setCooldownRemaining(i,std::max(0,player_.talents().cooldownRemaining(i)-rampage));
        if (&actor == boss_) onBossDefeated(*defeated);
    }

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
        log("Level up! You are now level ", player_.level(), ". Spend points any time (P).");
        soundManager_.play(SoundEffect::LevelUp);
    }
}

bool Application::pointsToSpend() const {
    return player_.unspentAttributePoints() > 0 || player_.abilityPoints() > 0 || player_.treePoints() > 0 ||
           player_.ascendancyPoints > 0;
}

// Points are spent at the player's leisure: attributes first if any are
// waiting, otherwise the talent trees.
void Application::openLevelUp() {
    if (mode_ != GameMode::Playing) return;
    cancelTargeting();
    inventoryOpen_ = false;
    if (player_.unspentAttributePoints() > 0) mode_ = GameMode::AttributeAllocation;
    else if (player_.ascendancyPoints > 0 && player_.abilityPoints() <= 0 && player_.treePoints() <= 0) openAscendancy();
    else openTalentTrees();
}

// Level-ups no longer pause the game, so all that's left to resume is a
// final victory that was waiting on an open menu.
void Application::resumeLevelUpSequence() {
    if (mode_==GameMode::GameOver) return;
    if (pendingFinalVictory_ && mode_==GameMode::Playing) {
        pendingFinalVictory_=false; mode_=GameMode::GameOver; wonGame_=true;
    }
}

std::vector<Actor*> Application::aliveAllies(const Actor* exclude) {
    std::vector<Actor*> allies;
    for (auto& m : monsters_) {
        if (m.get() != exclude && m->stats().hp > 0 && !m->allied) {
            allies.push_back(m.get());
        }
    }
    return allies;
}

void Application::removeDeadMonsters() {
    for (const auto& m : monsters_)
        if (m->stats().hp <= 0) {
            const MonsterLook look = monsterLook(*m);
            if (!m->allied && bleeds(m->type()) && surfaceAt(m->position()) == SurfaceType::None)
                setSurface(m->position(), SurfaceType::Blood, 0);
            if (exploredMap_.at(m->position().x, m->position().y) == Visibility::Visible && !m->tactics.concealed)
                spawnVfx({Vfx::Kind::Puff, {m->position().x + .5f, m->position().y + .5f}, {m->position().x + .5f, m->position().y + .5f},
                          m->allied ? sf::Color(120, 200, 220) : sf::Color(200, 50, 40), 0, .5f, .7f});
            recordCorpse(*m, look.frame, m->allied ? sf::Color(120, 235, 235) : look.tint, look.rows[4], look.frames[4], look.scale);
            forgetActor(*m);
        }
    monsters_.erase(std::remove_if(monsters_.begin(), monsters_.end(),
                                    [](const std::unique_ptr<Monster>& m) {
                                        return m->stats().hp <= 0;
                                    }),
                     monsters_.end());
}

void Application::updateFieldOfView() {
    computeLight();
    std::vector<Position> visible = computeFieldOfView(map_, player_.position(), kSightRadius);
    // In the dark only what's lit, or right beside you, can be seen.
    const auto here = player_.position();
    visible.erase(std::remove_if(visible.begin(), visible.end(), [&](Position p) {
        return !tileLit(p) && std::max(std::abs(p.x - here.x), std::abs(p.y - here.y)) > 1;
    }), visible.end());
    exploredMap_.update(visible);
}

int Application::playerLightRadius() const {
    if (!player_.lightLit) return 0;
    const int base = player_.lightSource == 2 ? 6 : player_.lightSource == 1 ? 4 : 0;
    return base ? base + player_.inventory().affixTotal(BonusStat::LightRadius) : 0;
}

bool Application::tileLit(Position p) const {
    if (!darknessEnabled_) return true;
    if (!map_.inBounds(p.x, p.y) || litTiles_.size() != static_cast<std::size_t>(map_.width() * map_.height())) return false;
    return litTiles_[static_cast<std::size_t>(p.y * map_.width() + p.x)] != 0;
}

bool Application::canSee(const Actor& viewer, Position target) const {
    if (!darknessEnabled_) return true;
    const auto* monster = dynamic_cast<const Monster*>(&viewer);
    if (monster && (monster->allied || seesInDark(monster->type()))) return true;
    const auto p = viewer.position();
    return tileLit(target) || std::max(std::abs(p.x - target.x), std::abs(p.y - target.y)) <= 1;
}

// Light sources, each lighting the tiles it can reach within its radius.
void Application::computeLight() {
    litTiles_.assign(static_cast<std::size_t>(std::max(0, map_.width() * map_.height())), 0);
    wallTorches_.clear();
    lightSources_.clear();
    const sf::Color fire(255, 150, 70);
    // Lights what the source can see within `radius`; a coloured source is
    // also drawn (radius `glow` tiles), anything else only counts for sight.
    const auto light = [&](Position source, int radius, sf::Color color = sf::Color::Transparent, float glow = 0.f, bool flicker = false) {
        if (radius <= 0 || !map_.inBounds(source.x, source.y)) return;
        LightSource drawn{color, glow, flicker, {}};
        for (const auto& p : computeFieldOfView(map_, source, radius)) {
            if (!map_.inBounds(p.x, p.y)) continue;
            const int index = p.y * map_.width() + p.x;
            litTiles_[static_cast<std::size_t>(index)] = 1;
            if (color.a) drawn.tiles.push_back({index, std::sqrt(static_cast<float>((p.x - source.x) * (p.x - source.x) + (p.y - source.y) * (p.y - source.y)))});
        }
        if (color.a) lightSources_.push_back(std::move(drawn));
    };
    light(player_.position(), playerLightRadius(), sf::Color(255, 214, 160), playerLightRadius() + 1.2f);
    const auto theme = floorTheme(currentFloor_);
    for (int y = 0; y + 1 < map_.height(); ++y)
        for (int x = 0; x < map_.width(); ++x) {
            const bool altar = landmark_ != LandmarkKind::None && x == landmarkAltar_.x && y == landmarkAltar_.y;
            if (!altar && propIndexAt(x, y) < 0 && decorAt(map_, x, y, currentFloor_, theme.region) == Decor::Torch &&
                map_.isWalkable(x, y + 1))
            { if (torchLit(x, y)) light({x, y + 1}, 3, fire, 3.6f, true); wallTorches_.push_back({x, y + 1}); } // a lit wall torch lights the floor in front of it
        }
    if (map_.inBounds(floorEntrance_.x, floorEntrance_.y)) light(floorEntrance_, 2);
    if (map_.inBounds(floorExit_.x, floorExit_.y)) light(floorExit_, 2);
    if (landmark_ != LandmarkKind::None) {
        for (const Position d : {Position{0, 1}, Position{0, -1}, Position{1, 0}, Position{-1, 0}})
            if (map_.isWalkable(landmarkAltar_.x + d.x, landmarkAltar_.y + d.y)) light({landmarkAltar_.x + d.x, landmarkAltar_.y + d.y}, 2);
        if (landmark_ == LandmarkKind::Shrine)
            for (const Position o : {Position{-3, -1}, Position{3, -1}, Position{-3, 1}, Position{3, 1}})
                for (const Position d : {Position{0, 1}, Position{0, -1}, Position{1, 0}, Position{-1, 0}})
                    if (map_.isWalkable(landmarkAltar_.x + o.x + d.x, landmarkAltar_.y + o.y + d.y))
                        light({landmarkAltar_.x + o.x + d.x, landmarkAltar_.y + o.y + d.y}, 2);
    }
    if (vaultExists_ && map_.inBounds(vaultCenter_.x, vaultCenter_.y)) light(vaultCenter_, 1);
    for (const auto& orb : lightOrbs_) light(orb.at, 5, sf::Color(170, 199, 255), 5.4f);
    for (const auto& item : groundItems_)
        if (item->rarity() >= ItemRarity::Rare && map_.inBounds(item->position().x, item->position().y))
            litTiles_[static_cast<std::size_t>(item->position().y * map_.width() + item->position().x)] = 1;
    // Braziers, and burning ground.
    for (const auto& prop : props_) if (prop.kind == PropKind::Brazier) light(prop.pos, 3, fire, 3.6f, true);
    if (surfaces_.size() == litTiles_.size())
        for (int y = 0; y < map_.height(); ++y)
            for (int x = 0; x < map_.width(); ++x)
                if (surfaces_[static_cast<std::size_t>(y * map_.width() + x)].type == SurfaceType::Fire)
                    light({x, y}, 1, sf::Color(255, 130, 60), 1.8f, true);
    // Torchbearers carry their own light.
    for (const auto& m : monsters_)
        if (m->stats().hp > 0 && m->type() == MonsterType::Torchbearer) light(m->position(), 3, fire, 3.4f, true);
    // Burning creatures are torches too.
    if (player_.statusEffects().has(StatusEffectType::Burn)) light(player_.position(), 2, sf::Color(255, 130, 60), 2.2f, true);
    for (const auto& m : monsters_)
        if (m->stats().hp > 0 && m->statusEffects().has(StatusEffectType::Burn)) light(m->position(), 2, sf::Color(255, 130, 60), 2.2f, true);
}

bool Application::torchLit(int x, int y) const {
    const bool lit = decorHash(x, y, currentFloor_ + 77) % 3 != 0;
    return torchToggles_.count({x, y}) ? !lit : lit;
}

void Application::setTorchLit(int x, int y, bool lit) {
    if (torchLit(x, y) == lit) return;
    if (!torchToggles_.erase({x, y})) torchToggles_.insert({x, y});
}

void Application::toggleLight() {
    if (!player_.lightSource) { log("You carry no light."); return; }
    player_.lightLit = !player_.lightLit;
    const std::string light = player_.lightSource == 2 ? "lantern" : "torch";
    log(player_.lightLit ? "You light your " + light + "." : "You shutter your " + light + ". In the dark, many foes can't see you.");
    updateFieldOfView();
}

void Application::selectClass(PlayerClass cls) {
    soundManager_.play(SoundEffect::Select);
    extraLives_=adventureMode_?2:0;
    playerClass_ = cls;
    player_.inventory() = Inventory{};
    player_.baseStats() = statsForClass(cls);
    player_.stats() = player_.baseStats();
    player_.unspentAttributePoints() = 0;
    player_.statusEffects().active().clear();
    nextItemId_ = 1;
    player_.trees().clear();
    player_.bloodRelic=false; player_.animationRelic=false; player_.deathlessSpentFloors.clear();
    player_.treePoints()=1; player_.abilityPoints()=earnedAbilityPoints(1);
    player_.ascendancy.clear(); player_.ascendancyPoints=0; player_.trialKeys=0; player_.trialsCleared=0;
    player_.lightSource=1; player_.lightLit=true; // everyone starts with a torch
    trial_=0; trialReturnFloor_=0; ascendancyMenu_=false; trialMenu_=false;
    pendingFinalVictory_ = false;

    progressionReviewPending_=true;
    loot_.restore(std::random_device{}());
    player_.talents() = TalentSet({basicAttack(),basicCleanse()});
    if (cls == PlayerClass::Mage) player_.talents().learnTalent(basicLight()); // mages make their own light
    // A fresh class selection is a genuinely new character -- starts at
    // level 1 with 0 XP, same as anyone picking up the game for the
    // first time, regardless of what level a previous run (via this
    // same long-lived player_ object) happened to reach. Loading a save
    // instead (see loadGame()) is the only path that should ever set
    // these to anything else.
    player_.level() = 1;
    player_.xp() = 0;

    floorCache_.clear(); gold_=0; quietTurns_=0; restTurns_=0; exitMenu_=false; combatThisTurn_=false;
    currentFloor_ = 1;
    dungeonMenu_=false;
    mode_ = GameMode::Playing;
    regenerateLevel(kInitialSeed);
    treeSelection_=0;
    while (treeSelection_+1<kTalentTrees.size() && !startingTreeAllowed(cls,kTalentTrees[treeSelection_].tree)) ++treeSelection_;
    abilitySelection_=0; openTalentTrees();
}

void Application::regenerateLevel(unsigned int seed) {
    autoExploring_=false; exploreSeenInterests_.clear();
    inventoryOpen_ = false;
    vaultMenu_=0;
    inventorySelection_ = 0;
    groundItems_.clear();
    cancelTargeting();
    mousePixel_.reset();
    talentPage_ = 0;
    const auto theme=floorTheme(currentFloor_);
    DungeonGenerationParams params;
    params.region=theme.region;
    // Very rare events only stir on deep floors.
    params.rareEventChance=currentFloor_>=kRareEventFloor ? kRareEventChance : 0.f;
    // Only specific floors generate with a boss room at all -- every
    // other floor is a pure "clear it, find the door" dungeon. kFinalFloor
    // (10) is a placeholder using the same GoblinWarlord as
    // kFirstBossFloor (5) for now -- the actual Lich (see ROADMAP.md)
    // is a separate, later piece of work; this gets the full 10-floor
    // structure and victory gating correct end to end first.
    params.includeBossRoom = (currentFloor_ == kFirstBossFloor || currentFloor_ == 10 || currentFloor_ == kFinalFloor);
    params.includeVault=currentFloor_>=3 && !params.includeBossRoom && nextItemId_<=std::numeric_limits<std::uint64_t>::max()-3;
    const GeneratedDungeon dungeon = generateDungeon(params, seed);
    vaultExists_=dungeon.hasVault; vaultOpened_=false; vaultClaimed_=false;
    vaultCenter_=dungeon.vaultCenter; vaultEntrance_=dungeon.vaultEntrance;
    vaultRewards_.clear();
    landmark_=dungeon.landmark; landmarkAltar_=dungeon.landmarkAltar; landmarkUsed_=false; shrineMenu_=false;

    map_ = dungeon.map;
    actorAnims_.clear(); corpses_.clear(); previousCameraX_ = previousCameraY_ = INT_MIN; vfx_.clear(); hitFlash_.clear();
    vfx_.clear(); hitFlash_.clear(); lightOrbs_.clear();
    setProps(dungeon.props);

    player_.setPosition(dungeon.playerStart);
    floorEntrance_=dungeon.playerStart; floorExit_={-1,-1};

    boss_ = nullptr; // clear before repopulating -- see checkAndHandleDeath for why this matters
    monsters_.clear();
    for (const auto& spawn:planEncounters(dungeon,currentFloor_))
        monsters_.push_back(createMonster(spawn.type,spawn.position,spawn.tier));
    if (vaultExists_) {
        auto guard=createMonster(MonsterType::Goblin,{vaultCenter_.x-1,vaultCenter_.y},MonsterTier::Elite);
        guard->vaultGuard=true; monsters_.push_back(std::move(guard));
        guard=createMonster(MonsterType::Archer,{vaultCenter_.x+1,vaultCenter_.y});
        guard->vaultGuard=true; monsters_.push_back(std::move(guard));
        const auto lootTheme=currentFloor_<=3?LootTheme::Barracks:currentFloor_<=6?LootTheme::Sanctum:LootTheme::Crypts;
        for (int i=0;i<3;++i) {
            auto reward=loot_.generate(currentFloor_,1,nextItemId_++,vaultCenter_,ItemRarity::Rare,lootTheme);
            // Prefer different bases without an unbounded generation loop.
            for (int retry=0;retry<16 && std::any_of(vaultRewards_.begin(),vaultRewards_.end(),[&](const auto& item){return item->definition()==reward->definition();});++retry)
                reward=loot_.generate(currentFloor_,1,reward->instanceId(),vaultCenter_,ItemRarity::Rare,lootTheme);
            vaultRewards_.push_back(std::move(reward));
        }
    }
    if (dungeon.hasBossRoom) {
        // The boss always spawns at Base tier regardless of `tier`
        // above -- see createMonster()'s GoblinWarlord/Lich cases for
        // why. Which boss depends on which floor this is: the Lich is
        // the true final fight, not a placeholder like it was before
        // this was actually built.
        const MonsterType bossType =
            (currentFloor_ >= 10) ? MonsterType::Lich : MonsterType::GoblinWarlord;
        Position bossPosition=dungeon.bossRoomCenter;
        auto startDistance=[&](Position p) {
            const int dx=p.x-dungeon.playerStart.x,dy=p.y-dungeon.playerStart.y;
            return dx*dx+dy*dy;
        };
        // The central 7x7 is guaranteed floor in every generated boss room.
        // If its center is near the start, use its far side to prevent an
        // opening attack. Keep the progression door under the boss below.
        if (startDistance(bossPosition)<=64)
            for (int y=dungeon.bossRoomCenter.y-3;y<=dungeon.bossRoomCenter.y+3;++y)
                for (int x=dungeon.bossRoomCenter.x-3;x<=dungeon.bossRoomCenter.x+3;++x)
                    if (map_.isWalkable(x,y) && startDistance({x,y})>startDistance(bossPosition)) bossPosition={x,y};
        monsters_.push_back(createMonster(bossType, bossPosition));
        boss_ = monsters_.back().get();
    }

    // The floor-transition door: on a boss floor it sits exactly where
    // the boss stands, so the player can only reach it (the boss blocks
    // that tile like any other actor, no special "is the boss dead yet"
    // check needed) once the fight is actually won. On a non-boss floor
    // it sits at the last regular room's center -- the same room the
    // shortcut protection above keeps reachable only via the full
    // chain, so reaching the door still means genuinely working through
    // the floor. kFinalFloor has no door at all: reaching it is the
    // end of the run, handled entirely by checkAndHandleDeath's victory
    // branch instead.
    if (currentFloor_ != kFinalFloor) {
        const Position doorPosition =
            dungeon.hasBossRoom ? boss_->position() : dungeon.otherRoomCenters.back();
        floorExit_=doorPosition;
        map_.setTile(doorPosition.x, doorPosition.y, Tile{TileType::Door, true, true});
    }

    for (auto& m:monsters_) scaleDungeonMonster(*m,currentFloor_);
    seedSurfaces(seed);
    if (dungeon.hasBossRoom && boss_) {
        const Position centre=boss_->position();
        placeBraziers(centre,{{-3,-2},{3,-2},{-3,2},{3,2}});
        if (boss_->type()==MonsterType::GoblinWarlord)
            for (const Position o:{Position{-2,0},Position{2,0},Position{0,2}})
                if (map_.isWalkable(centre.x+o.x,centre.y+o.y)) setSurface({centre.x+o.x,centre.y+o.y},SurfaceType::Oil,0);
    }
    spawnFixedItems();
    spawnFloorChest();
    exploredMap_ = ExploredMap(map_);
    log("Entering ", theme.name, ". ", theme.description);

    scheduler_ = TurnScheduler{};
    scheduler_.add(player_);
    for (auto& m : monsters_) {
        scheduler_.add(*m);
    }

    currentActor_ = &scheduler_.nextTurn();
    // Arrival is not a wait action: no healing, cooldown ticks or quiet turns.
    updateFieldOfView();

    std::cout << "Generated dungeon (seed " << seed << ", floor " << currentFloor_
              << "): " << map_.width() << 'x' << map_.height() << ", " << dungeon.roomCount
              << " rooms, " << monsters_.size() << " monsters"
              << (dungeon.hasBossRoom ? " (boss present)" : " (no boss this run)")
              << ", player start (" << dungeon.playerStart.x << ',' << dungeon.playerStart.y
              << ")" << std::endl;
    // Deliberately raw std::cout above, not log() -- logMessages_ is a
    // display buffer for *combat/play* events; this generation summary
    // is a one-time diagnostic that fires before the player has done
    // anything, and would just be a stale, disconnected first line
    // sitting in the on-screen log for the rest of the run.
}

SaveGameState Application::captureState(bool includeFloors) {
    SaveGameState state;
    state.adventureMode=adventureMode_; state.extraLives=extraLives_;
    state.ascendancy=player_.ascendancy; state.ascendancyPoints=player_.ascendancyPoints;
    state.trialKeys=player_.trialKeys; state.trialsCleared=player_.trialsCleared;
    state.trial=trial_; state.trialReturnFloor=trialReturnFloor_;
    state.lightSource=player_.lightSource; state.lightLit=player_.lightLit;
    for (int y=0;y<map_.height();++y) for (int x=0;x<map_.width();++x) {
        const auto s=surfaceAt({x,y});
        if (s!=SurfaceType::None) state.surfaces.push_back({x,y,static_cast<int>(s),surfaces_[static_cast<std::size_t>(y*map_.width()+x)].turns});
    }
    for (const auto& t:torchToggles_) state.torchToggles.push_back({t.first,t.second});
    for (const auto& orb:lightOrbs_) state.lightOrbs.push_back({orb.at.x,orb.at.y,orb.turns});
    state.map = map_;
    state.exploredMap = exploredMap_;
    state.playerPosition = player_.position();
    state.playerClass = playerClass_;
    state.playerLevel = player_.level();
    state.bloodRelic=player_.bloodRelic; state.animationRelic=player_.animationRelic; state.deathlessSpentFloors=player_.deathlessSpentFloors;
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
    state.landmark=static_cast<int>(landmark_); state.landmarkAltar=landmarkAltar_; state.landmarkUsed=landmarkUsed_;
    state.props=props_;
    state.vaultExists=vaultExists_; state.vaultOpened=vaultOpened_; state.vaultClaimed=vaultClaimed_;
    state.vaultCenter=vaultCenter_; state.vaultEntrance=vaultEntrance_;
    const auto saveItem = [&](const Item& item, int location) {
        state.items.push_back({item.definition()->id, item.instanceId(), location,
                              location == -2 ? item.position() : Position{}, item.rollTier(), item.affixes()});
    };
    for (std::size_t i=0;i<vaultRewards_.size();++i) saveItem(*vaultRewards_[i],-3-static_cast<int>(i));
    for (const auto& item : groundItems_) saveItem(*item, -2);
    for (const auto& item : player_.inventory().items()) saveItem(*item, -1);
    for (int slot = 0; slot < kEquipmentSlotCount; ++slot) {
        if (const auto* item = player_.inventory().equipped(static_cast<EquipmentSlot>(slot)))
            saveItem(*item, slot);
    }
    state.lastMoveDirection = lastMoveDirection_;

    const auto& talents = player_.talents().knownTalents();
    for (std::size_t i = 0; i < talents.size(); ++i)
        state.playerTalents.push_back({talents[i].id, player_.talents().cooldownRemaining(i), player_.talents().rank(i)});
    state.treePoints=player_.treePoints(); state.abilityPoints=player_.abilityPoints();
    state.trees=player_.trees(); state.hotbar=player_.talents().hotbar();
    state.progressionReviewPending=progressionReviewPending_; state.pendingFinalVictory=pendingFinalVictory_;
    state.defeatedBossName=defeatedBossName_;
    state.playerStatusEffects = player_.statusEffects().active();

    for (auto& m : monsters_) {
        SaveGameState::MonsterSaveData data;
        data.tactics=m->tactics;
        data.type = m->type();
        data.allied=m->allied; data.summonRank=m->summonRank; data.summonIntelligence=m->summonIntelligence; data.remainingLife=m->remainingLife;
        data.position = m->position();
        data.hp = m->stats().hp;
        data.maxHp = m->stats().maxHp;
        data.isBoss = (m.get() == boss_);
        data.tier = m->tier();
        data.rewardsEligible = m->rewardsEligible();
        data.intent = m->intent();
        data.vaultGuard = m->vaultGuard;
        data.eventChampion = m->eventChampion;
        data.recoveryActions=m->recoveryActions; data.summonsCommitted=m->summonsCommitted;
        if (const auto* behavior=dynamic_cast<const BossBehavior*>(m->ai())) {
            data.announcedPhase=behavior->announcedPhase(); data.enraged=behavior->enraged();
        }
        data.statusEffects = m->statusEffects().active();
        const auto& known = m->talents().knownTalents();
        for (std::size_t i = 0; i < known.size(); ++i)
            data.talents.push_back({known[i].id, m->talents().cooldownRemaining(i)});
        state.monsters.push_back(std::move(data));
    }

    state.floorEntrance=floorEntrance_; state.floorExit=floorExit_;
    state.gold=gold_; state.quietTurns=quietTurns_; state.inTown=mode_==GameMode::Town;
    // In a trial the current map is the arena, so every cached dungeon floor is kept.
    if (includeFloors) for (const auto& entry:floorCache_) if (entry.first!=currentFloor_ || trial_) state.savedFloors.push_back(entry.second);
    return state;
}

void Application::saveGame() {
    if (engine::saveGame(captureState(),kSaveFilePath)) log("Game saved, including visited floors.");
    else log("Failed to save game.");
}

void Application::loadGame() {
    const auto loaded=engine::loadGame(kSaveFilePath);
    if (!loaded) { log("No valid save file found."); return; }
    restoreState(*loaded);
}

bool Application::restoreState(const SaveGameState& state, bool includeFloors) {
    if(state.extraLives<0 || state.extraLives>2 || (!state.adventureMode && state.extraLives!=0)) { log("Invalid death-mode state."); return false; }
    autoExploring_=false; exploreSeenInterests_.clear();
    inventoryDragSource_.reset();
    inventoryOpen_=false; vaultMenu_=0; shrineMenu_=false; exitMenu_=false; restTurns_=0;
    inventorySelection_=0; cancelTargeting(); mousePixel_.reset(); talentPage_=0;
    TalentSet restoredTalents;
    for (const auto& saved:state.playerTalents) {
        const auto* d=findTalentDefinition(saved.id);
        if (!d && saved.id!="basic.attack" && saved.id!="basic.cleanse" && saved.id!="basic.light") { log("Unknown saved ability."); return false; }
        restoredTalents.learnTalent(d ? d->ranks[0] : saved.id=="basic.cleanse" ? basicCleanse() : saved.id=="basic.light" ? basicLight() : basicAttack());
        const auto index=restoredTalents.knownTalents().size()-1;
        restoredTalents.setRank(index,saved.rank);
        restoredTalents.setCooldownRemaining(index,saved.cooldown);
    }
    restoredTalents.hotbar()=state.hotbar;
    std::vector<std::unique_ptr<Monster>> restoredMonsters;
    Monster* restoredBoss = nullptr;
    for (const auto& savedMonster : state.monsters) {
        auto monster = createMonster(savedMonster.type, savedMonster.position, savedMonster.tier);
        if (savedMonster.allied) {
            configureMinion(*monster,savedMonster.summonRank,savedMonster.summonIntelligence);
            monster->remainingLife=savedMonster.remainingLife;
        } else scaleDungeonMonster(*monster,state.currentFloor);
        const auto& known = monster->talents().knownTalents();
        if (savedMonster.talents.size() != known.size()) { log("Save contains an incomplete enemy talent kit."); return false; }
        for (const auto& saved : savedMonster.talents) {
            const auto found = std::find_if(known.begin(), known.end(), [&](const auto& talent) { return talent.id == saved.id; });
            if (found == known.end()) { log("Save contains an unknown enemy talent."); return false; }
            monster->talents().setCooldownRemaining(static_cast<std::size_t>(found-known.begin()), saved.cooldown);
        }
        monster->tactics=savedMonster.tactics;
        monster->setRewardsEligible(savedMonster.rewardsEligible);
        monster->intent() = savedMonster.intent;
        monster->vaultGuard = savedMonster.vaultGuard;
        monster->eventChampion = savedMonster.eventChampion;
        if (monster->eventChampion) monster->setName(championName(monster->eventChampion));
        monster->recoveryActions=savedMonster.recoveryActions; monster->summonsCommitted=savedMonster.summonsCommitted;
        if (auto* behavior=dynamic_cast<BossBehavior*>(monster->ai())) behavior->restoreState(savedMonster.announcedPhase,savedMonster.enraged);
        monster->stats().hp = savedMonster.hp;
        monster->lastObservedHp=savedMonster.hp;
        monster->stats().maxHp = savedMonster.maxHp;
        monster->statusEffects().active() = savedMonster.statusEffects;
        for (auto& effect:monster->statusEffects().active()) if (effect.type==StatusEffectType::Stun)
            effect.turnsRemaining=std::min(effect.turnsRemaining,monster->statusEffects().maxStunDuration());
        if (savedMonster.isBoss) {
            if (restoredBoss) { log("Save contains multiple bosses."); return false; }
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
    actorAnims_.clear(); corpses_.clear(); previousCameraX_ = previousCameraY_ = INT_MIN; vfx_.clear(); hitFlash_.clear();
    exploredMap_ = state.exploredMap;

    player_.setPosition(state.playerPosition);
    playerClass_ = state.playerClass;
    player_.talents() = std::move(restoredTalents);
    player_.trees()=state.trees; player_.treePoints()=state.treePoints; player_.abilityPoints()=state.abilityPoints;
    progressionReviewPending_=state.progressionReviewPending;
    defeatedBossName_=state.defeatedBossName;
    adventureMode_=state.adventureMode; extraLives_=state.extraLives;
    player_.ascendancy=state.ascendancy; player_.ascendancyPoints=state.ascendancyPoints;
    player_.trialKeys=state.trialKeys; player_.trialsCleared=state.trialsCleared;
    trial_=state.trial; trialReturnFloor_=state.trialReturnFloor;
    player_.lightSource=state.lightSource; player_.lightLit=state.lightLit;
    surfaces_.assign(static_cast<std::size_t>(state.map.width()*state.map.height()),{});
    for (const auto& [x,y,type,turns]:state.surfaces)
        surfaces_[static_cast<std::size_t>(y*state.map.width()+x)]={static_cast<SurfaceType>(type),turns};
    torchToggles_.clear();
    for (const auto& t:state.torchToggles) torchToggles_.insert({t.x,t.y});
    lightOrbs_.clear();
    for (const auto& [x,y,turns]:state.lightOrbs) lightOrbs_.push_back({{x,y},turns});
    ascendancyMenu_=false; trialMenu_=false;
    // A trial won before choosing an ascendancy: the choice comes back.
    if (player_.trialsCleared && player_.ascendancy.empty()) { ascendancyChoice_=true; ascendancyMenu_=true; ascendancyChoiceSelection_=0; }
    else ascendancyChoice_=false;
    player_.level() = state.playerLevel;
    player_.bloodRelic=state.bloodRelic; player_.animationRelic=state.animationRelic; player_.deathlessSpentFloors=state.deathlessSpentFloors;
    player_.xp() = state.playerXp;
    currentFloor_ = state.currentFloor;
    dungeonMenu_=false;
    player_.inventory() = Inventory{};
    groundItems_.clear();
    nextItemId_ = state.nextItemId;
    loot_.restore(state.lootRngState);
    chestPosition_ = state.chestPosition; chestExists_ = state.chestExists; chestClaimed_ = state.chestClaimed;
    ordinaryDrops_ = state.ordinaryDrops;
    landmark_=static_cast<LandmarkKind>(state.landmark); landmarkAltar_=state.landmarkAltar; landmarkUsed_=state.landmarkUsed;
    setProps(state.props);
    vaultExists_=state.vaultExists; vaultOpened_=state.vaultOpened; vaultClaimed_=state.vaultClaimed;
    vaultCenter_=state.vaultCenter; vaultEntrance_=state.vaultEntrance;
    vaultRewards_.clear(); if (vaultExists_ && !vaultClaimed_) vaultRewards_.resize(3);
    player_.baseStats() = state.playerStats;
    player_.stats() = state.playerStats;
    player_.unspentAttributePoints() = state.unspentAttributePoints;
    pendingFinalVictory_ = state.pendingFinalVictory;
    // Validation in loadGame guarantees unique IDs, known definitions and slot compatibility.
    for (const auto& savedItem : state.items) {
        auto item = std::make_unique<Item>(*findItemDefinition(savedItem.definitionId),
                                          savedItem.instanceId, savedItem.position, savedItem.affixes, savedItem.rollTier);
        if (savedItem.location<=-3) vaultRewards_[static_cast<std::size_t>(-3-savedItem.location)]=std::move(item);
        else if (savedItem.location == -2) groundItems_.push_back(std::move(item));
        else {
            player_.inventory().add(std::move(item));
            if (savedItem.location >= 0)
                player_.inventory().equip(player_.inventory().items().size() - 1,static_cast<EquipmentSlot>(savedItem.location));
        }
    }
    player_.refreshEquipmentStats();
    lastMoveDirection_ = state.lastMoveDirection;

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

    if (player_.stats().hp<=0) { mode_=GameMode::GameOver; wonGame_=false; }
    else resumeLevelUpSequence();
    log("Game loaded: ", monsters_.size(), " monsters",
        (boss_ != nullptr ? " (boss present)" : ""), ", player at (", player_.position().x, ',',
        player_.position().y, "), ", player_.stats().hp, '/', player_.stats().maxHp, " hp.");
    floorEntrance_=state.floorEntrance; floorExit_=state.floorExit;
    gold_=state.gold; quietTurns_=state.quietTurns; combatThisTurn_=false;
    if (includeFloors) {
        floorCache_.clear();
        for (const auto& floor:state.savedFloors) floorCache_[floor.currentFloor]=floor;
    }
    if (state.inTown && mode_==GameMode::Playing) mode_=GameMode::Town;
    enforceMinionCap();
    updateFieldOfView();
    return true;
}

void Application::update() {
    if (autoExploring_) { stepAutoExplore(); return; }
    if (restTurns_<=0) return;
    if (mode_!=GameMode::Playing || inventoryOpen_ || vaultMenu_ || shrineMenu_ || exitMenu_ || dangerNearby()) {
        restTurns_=0; log("Rest stopped."); return;
    }
    bool ready=quietTurns_>=10 && player_.stats().hp>=player_.stats().maxHp && player_.stats().mana>=player_.stats().maxMana;
    for (std::size_t i=0;i<player_.talents().knownTalents().size();++i) ready=ready && player_.talents().isReady(i);
    if (ready) { restTurns_=0; log("Rest complete: fully recovered. Waystone ready."); return; }
    if (restClock_.getElapsedTime().asMilliseconds()<60) return;
    restClock_.restart(); --restTurns_;
    player_.statusEffects().apply({StatusEffectType::Opening,2,0});
    finishInventoryTurn();
}

void Application::renderClassSelection() {
    using namespace screen;
    const auto mouse=mousePixel_?std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)):std::nullopt;
    const auto hovered=[&](const sf::FloatRect& r){ return mouse && r.contains(*mouse); };
    ui_.panel(window_,{{0,0},{1280,720}},true,sf::Color(140,135,130));
    ui_.textCentered(window_,"Choose your class",{{0,26},{1280,50}},38,ui::kGold,ui::Font::Title);
    ui_.textCentered(window_,"Your class sets your starting attributes and the trees you can begin with.",{{0,78},{1280,26}},17,ui::kMuted);
    struct ClassInfo { const char* name; PlayerClass cls; sf::Color color; const char* stats; const char* pools; const char* blurb; std::vector<std::size_t> trees; };
    const ClassInfo classes[]{
        {"Warrior",PlayerClass::Warrior,sf::Color(232,150,108),"Str 6   Dex 2   Int 2","Life 30   Mana 10",
            "Steel and endurance: heavy blows, a raised shield and a refusal to fall.",{0,1,2}},
        {"Mage",PlayerClass::Mage,sf::Color(142,172,240),"Str 2   Dex 2   Int 6","Life 20   Mana 20",
            "Fire, ice, lightning and raw arcane force, from a safe distance.",{6,7,8,9}},
        {"Thief",PlayerClass::Thief,sf::Color(132,218,160),"Str 2   Dex 6   Int 2","Life 25   Mana 15",
            "Shadows, the bow and quick feet: strike first, then vanish.",{4,3,5}}};
    for (int i=0;i<3;++i) {
        const auto& info=classes[i];
        const auto card=classCard(i);
        const float x=card.position.x, y=card.position.y;
        ui_.inset(window_,card,hovered(card)?info.color:sf::Color(info.color.r,info.color.g,info.color.b,60));
        ui_.text(window_,std::to_string(i+1),{x+12,y+8},16,ui::kMuted,ui::Font::Bold);
        const sf::FloatRect portrait{{x+110,y+22},{140,140}};
        ui_.inset(window_,portrait,sf::Color(140,108,62));
        sprites_.draw(window_,playerFrame(info.cls),{portrait.position.x+6,portrait.position.y+6},128.f);
        ui_.textCentered(window_,info.name,{{x,y+170},{card.size.x,40}},30,info.color,ui::Font::Title);
        ui_.textCentered(window_,info.stats,{{x,y+214},{card.size.x,22}},17,ui::kText,ui::Font::Bold);
        ui_.textCentered(window_,info.pools,{{x,y+238},{card.size.x,22}},16,ui::kMuted,ui::Font::Bold);
        float textY=y+270;
        ui_.paragraph(window_,info.blurb,x+26,textY,card.size.x-52,15,ui::kText);
        ui_.text(window_,"Starting trees",{x+26,y+322},14,ui::kGold,ui::Font::Bold);
        const float slot=(card.size.x-52)/4;
        for (std::size_t t=0;t<info.trees.size();++t) {
            const auto tree=info.trees[t];
            const sf::FloatRect icon{{x+26+slot*t+(slot-46)/2,y+344},{46,46}};
            ui_.inset(window_,icon);
            ui_.icon(window_,talentIcon(talentCatalog()[tree*4].ranks[0]),{{icon.position.x+6,icon.position.y+6},{34,34}},ui::kText);
            ui_.textCentered(window_,kTalentTrees[tree].name,{{x+26+slot*t,y+392},{slot,20}},13,ui::kMuted);
        }
    }
    ui_.textCentered(window_,"You start with 1 tree point and 3 ability points. The other core trees open at level 5.",
        {{0,556},{1280,24}},16,ui::kText);
    ui_.button(window_,kStartLoad,"Load game (F9)",hovered(kStartLoad));
    ui_.button(window_,kModeToggle,adventureMode_?"Mode: Adventure, 2 extra lives (M)":"Mode: Roguelike, one life (M)",hovered(kModeToggle));
    ui_.textCentered(window_,"Click a class or press 1-3. Pick the mode first; revival returns you to town with your gear.",
        {{0,656},{1280,24}},15,ui::kMuted);
    if(!logMessages_.empty()) ui_.textCentered(window_,logMessages_.back(),{{0,684},{1280,24}},15,sf::Color(232,196,130));
}

void Application::allocateAttribute(unsigned int attribute) {
    if(mode_!=GameMode::AttributeAllocation || player_.unspentAttributePoints()<=0 || attribute>2) return;
    if(attribute==0) {
        ++player_.baseStats().strength; ++player_.baseStats().maxHp; log("+1 Strength.");
    } else if(attribute==1) {
        ++player_.baseStats().dexterity; log("+1 Dexterity.");
    } else {
        ++player_.baseStats().intelligence; ++player_.baseStats().maxMana; log("+1 Intelligence.");
    }
    player_.refreshEquipmentStats();
    --player_.unspentAttributePoints();
    soundManager_.play(SoundEffect::Select);
    // The dialog stays open while points remain; it closes on the last one.
    if (player_.unspentAttributePoints()<=0) { mode_=GameMode::Playing; resumeLevelUpSequence(); }
}

void Application::renderGameOver() {
    using namespace screen;
    const auto mouse=mousePixel_?std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)):std::nullopt;
    const auto hovered=[&](const sf::FloatRect& r){ return mouse && r.contains(*mouse); };
    ui_.panel(window_,kGameOverDialog,true,sf::Color(150,140,138));
    const float cx=kGameOverDialog.position.x, w=kGameOverDialog.size.x;
    ui_.icon(window_,wonGame_?"relic-blade":"skull-crossed-bones",{{cx+w/2-36,kGameOverDialog.position.y+24},{72,72}},
        wonGame_?ui::kRare:sf::Color(200,60,52));
    ui_.textCentered(window_,wonGame_?"Victory":"You died",{{cx,kGameOverDialog.position.y+104},{w,56}},46,
        wonGame_?ui::kRare:sf::Color(210,64,56),ui::Font::Title);
    ui_.textCentered(window_,wonGame_?"You have slain the "+defeatedBossName_+".":"The dungeon claims another.",
        {{cx,kGameOverDialog.position.y+160},{w,26}},18,ui::kText);
    ui_.button(window_,kRestart,"New character (Enter)",hovered(kRestart),true,17);
    if(!wonGame_ && adventureMode_ && extraLives_>0) {
        ui_.button(window_,kRevive,"Revive in town (R), "+std::to_string(extraLives_)+" li"+(extraLives_==1?"fe":"ves")+" left",hovered(kRevive),true,17);
        ui_.textCentered(window_,"Full recovery. You keep your gear and progress, and spend one extra life.",
            {{cx,kRevive.position.y+56},{w,22}},15,ui::kMuted);
    } else if(!wonGame_) {
        ui_.textCentered(window_,"No lives remain. This character's story has ended.",{{cx,kRevive.position.y+10},{w,24}},16,ui::kMuted);
    }
}

void Application::renderAttributeAllocation() {
    using namespace screen;
    const auto mouse=mousePixel_?std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)):std::nullopt;
    ui_.panel(window_,kAttributeDialog,true,sf::Color(150,145,140));
    const float x=kAttributeDialog.position.x, w=kAttributeDialog.size.x;
    ui_.textCentered(window_,"Level up",{{x,kAttributeDialog.position.y+22},{w,50}},38,ui::kGold,ui::Font::Title);
    const int points=player_.unspentAttributePoints();
    ui_.textCentered(window_,"You have "+std::to_string(points)+(points==1?" attribute point":" attribute points")+" to spend.",
        {{x,kAttributeDialog.position.y+76},{w,24}},18,ui::kText);
    ui_.textCentered(window_,"Click an attribute or press 1-3. Each choice spends one point; Esc keeps the rest for later.",
        {{x,kAttributeDialog.position.y+102},{w,22}},15,ui::kMuted);
    ui_.button(window_,kAttributeClose,"Later (Esc)",mouse && kAttributeClose.contains(*mouse));
    const Stats& stats = player_.stats();
    struct Choice { const char* name; const char* detail; const char* icon; sf::Color color; int value; };
    const Choice choices[]{
        {"Strength","+1 max life. Strengthens Strength-based abilities.","axe-swing",sf::Color(232,140,100),stats.strength},
        {"Dexterity","+0.5% dodge (up to 25%) and +0.5% critical chance. Strengthens Dexterity-based abilities.","dodging",sf::Color(130,214,140),stats.dexterity},
        {"Intelligence","+1 max mana. Strengthens Intelligence-based abilities.","third-eye",sf::Color(142,172,236),stats.intelligence}};
    for(int i=0;i<3;++i) {
        const auto r=attributeChoice(i);
        const auto& c=choices[i];
        const bool hover=mouse && r.contains(*mouse);
        ui_.inset(window_,r,hover?c.color:sf::Color(c.color.r,c.color.g,c.color.b,60));
        const sf::FloatRect icon{{r.position.x+14,r.position.y+14},{70,70}};
        ui_.inset(window_,icon);
        ui_.icon(window_,c.icon,{{icon.position.x+10,icon.position.y+10},{50,50}},c.color);
        ui_.text(window_,std::to_string(i+1)+".  "+c.name,{r.position.x+100,r.position.y+12},24,c.color,ui::Font::Title);
        float y=r.position.y+48;
        ui_.paragraph(window_,c.detail,r.position.x+100,y,400,15,ui::kText);
        const std::string value=std::to_string(c.value);
        ui_.text(window_,value,{r.position.x+r.size.x-24-ui_.textWidth(value,34,ui::Font::Title),r.position.y+20},34,ui::kText,ui::Font::Title);
        ui_.text(window_,"now",{r.position.x+r.size.x-58,r.position.y+64},13,ui::kMuted);
    }
}

void Application::render() {
    window_.clear(sf::Color(10, 10, 14));
    // Full-screen menus share a dim stone backdrop; screens that paint
    // their own background simply cover it.
    if (mode_ != GameMode::Playing) ui_.stone(window_, {{0, 0}, {1280, 720}}, sf::Color(120, 115, 112));

    if (mode_==GameMode::Town) { renderTown(); window_.display(); return; }
    if (mode_ == GameMode::ClassSelection) {
        renderClassSelection();
        window_.display();
        return;
    }

    if (mode_ == GameMode::GameOver) {
        renderGameOver();
        window_.display();
        return;
    }

    if (mode_ == GameMode::AbilityChoice) {
        renderTalentTrees();
        window_.display();
        return;
    }

    if (mode_ == GameMode::AttributeAllocation) {
        renderAttributeAllocation();
        window_.display();
        return;
    }

    updateCamera();
    updateCameraShift();

    // Only the camera-visible range, not the whole map -- both a real
    // performance win now that the map (60x32, Prompt 18) is bigger
    // than the ~40x22-tile viewport, and it naturally avoids needing a
    // separate "is this tile on screen" check per tile. +1 on each
    // upper bound covers the partially-visible tile at the viewport's
    // trailing edge.
    const auto theme=floorTheme(currentFloor_);
    auto themeColor=[](ThemeColor c) { return sf::Color(c.r,c.g,c.b); };
    const int viewStartX = std::max(0, cameraX_ - 1);
    const int viewEndX = std::min(map_.width(), cameraX_ + static_cast<int>(kMapWidth / kTileSize) + 1);
    const int viewStartY = std::max(0, cameraY_ - 1);
    const int viewEndY = std::min(map_.height(), cameraY_ + static_cast<int>(kMapHeight / kTileSize));

    // Everything on the map is drawn through a view clipped to the map
    // rectangle, so tall wall faces and light never spill onto the HUD.
    sf::View mapView(sf::FloatRect({kMapLeft, kMapTop}, {static_cast<float>(kMapWidth), static_cast<float>(kMapHeight)}));
    const auto box = letterbox();
    mapView.setViewport(sf::FloatRect({box.position.x + box.size.x * kMapLeft / kWindowWidth, box.position.y + box.size.y * kMapTop / kWindowHeight},
                                      {box.size.x * kMapWidth / static_cast<float>(kWindowWidth), box.size.y * kMapHeight / static_cast<float>(kWindowHeight)}));
    window_.setView(mapView);
    // One extra row: a wall just below the view still shows its tall face.
    const int drawEndY = std::min(map_.height(), viewEndY + 1);

    const auto isWallAt = [&](int x, int y) {
        if (!map_.inBounds(x, y)) return true;
        if (landmarkAltarIsFloor(landmark_) && x == landmarkAltar_.x && y == landmarkAltar_.y) return false;
        if (propIndexAt(x, y) >= 0) return false;
        return map_.tileAt(x, y).type == TileType::Wall;
    };
    const auto shadeFor = [](Visibility vis, sf::Color c) { return vis == Visibility::Visible ? c : dim(c); };
    const sf::Color wallTop = themeColor(theme.wall);

    // --- Pass 1: floors, wall tops (seen from above) and contact shadows -------
    sf::VertexArray tops(sf::PrimitiveType::Triangles), shadows(sf::PrimitiveType::Triangles);
    const sf::Texture* wallTexture = sprites_.texture(kEvilDungeon);
    // Floors, wall faces and decorations all come from the dungeon tileset,
    // so each pass is one batched draw call.
    const sf::Texture* tileset = sprites_.texture(kTileset);
    const sf::Texture* floorTexture = sprites_.texture(floorSheet(theme.region));
    sf::VertexArray wallTops(sf::PrimitiveType::Triangles), dais(sf::PrimitiveType::Triangles);
    // The cobble art carries its own colour, so it gets a soft, slightly
    // desaturating tint rather than the full theme colour.
    const sf::Color floorTint = theme.region == FloorRegion::Barracks ? themeTint(themeColor(theme.floor), 2.3f)
                              : theme.region == FloorRegion::Sanctum ? sf::Color(150, 138, 150)
                              : themeTint(themeColor(theme.floor), 1.6f);
    sf::VertexArray floors(sf::PrimitiveType::Triangles), faces(sf::PrimitiveType::Triangles),
        details(sf::PrimitiveType::Triangles), feet(sf::PrimitiveType::Triangles);
    const auto quad = [](sf::VertexArray& va, sf::Vector2f p, sf::Vector2f s, sf::Color a, sf::Color b, bool vertical) {
        // a at the start edge, b at the far edge (top->bottom if vertical, else left->right)
        const sf::Vector2f tl = p, tr{p.x + s.x, p.y}, br{p.x + s.x, p.y + s.y}, bl{p.x, p.y + s.y};
        const sf::Color ctl = a, ctr = vertical ? a : b, cbr = b, cbl = vertical ? b : a;
        for (const auto& v : {sf::Vertex{tl, ctl}, sf::Vertex{tr, ctr}, sf::Vertex{br, cbr},
                              sf::Vertex{tl, ctl}, sf::Vertex{br, cbr}, sf::Vertex{bl, cbl}})
            va.append(v);
    };
    for (int y = viewStartY; y < drawEndY; ++y) {
        for (int x = viewStartX; x < viewEndX; ++x) {
            const Visibility vis = exploredMap_.at(x, y);
            if (vis == Visibility::Hidden) continue;
            const sf::Vector2f at = worldToScreen(x, y);
            if (isWallAt(x, y)) {
                // A dark top, slightly varied, with a lit rim wherever it meets
                // open floor to the side or behind.
                const int vary = static_cast<int>(decorHash(x, y, 7) % 9) - 4;
                const auto top = [&](float f) {
                    return sf::Color(static_cast<std::uint8_t>(std::clamp(wallTop.r * f + vary, 0.f, 255.f)),
                                     static_cast<std::uint8_t>(std::clamp(wallTop.g * f + vary, 0.f, 255.f)),
                                     static_cast<std::uint8_t>(std::clamp(wallTop.b * f + vary, 0.f, 255.f)));
                };
                const sf::Color rim = shadeFor(vis, top(2.0f));
                if (wallTexture) SpriteAtlas::append(wallTops, {kEvilDungeon, sf::IntRect({64, 0}, {32, 32})}, at, kTileSize,
                                                     shadeFor(vis, top(2.6f)));
                else quad(tops, at, {kTileSize, kTileSize}, shadeFor(vis, top(1.05f)), shadeFor(vis, top(1.05f)), true);
                if (!isWallAt(x - 1, y)) quad(tops, at, {2, kTileSize}, rim, rim, true);
                if (!isWallAt(x + 1, y)) quad(tops, {at.x + kTileSize - 2, at.y}, {2, kTileSize}, rim, rim, true);
                if (!isWallAt(x, y - 1)) quad(tops, at, {kTileSize, 2}, rim, rim, true);
                continue;
            }
            if (floorTexture) SpriteAtlas::append(floors, floorFrame(x, y, theme.region), at, kTileSize, shadeFor(vis, floorTint));
            else {
                sf::RectangleShape tileShape({kTileSize - 1.f, kTileSize - 1.f});
                tileShape.setPosition(at);
                tileShape.setFillColor(shadeFor(vis, themeColor(theme.floor)));
                window_.draw(tileShape);
            }
            // The shrine's statue stands on a raised stone dais.
            if (wallTexture && landmark_ == LandmarkKind::Shrine &&
                std::abs(x - landmarkAltar_.x) <= 1 && std::abs(y - landmarkAltar_.y) <= 1)
                SpriteAtlas::append(dais, {kEvilDungeon, sf::IntRect({96, 0}, {32, 32})}, at, kTileSize, shadeFor(vis, sf::Color(235, 215, 185)));
            // Soft shadow along every wall edge: walls feel solid and rooms gain depth.
            const sf::Color dark(0, 0, 0, 120), clear(0, 0, 0, 0);
            if (isWallAt(x, y - 1)) quad(shadows, at, {kTileSize, 11}, dark, clear, true);
            if (isWallAt(x - 1, y)) quad(shadows, at, {8, kTileSize}, sf::Color(0, 0, 0, 90), clear, false);
            if (isWallAt(x + 1, y)) quad(shadows, {at.x + kTileSize - 8, at.y}, {8, kTileSize}, clear, sf::Color(0, 0, 0, 90), false);
        }
    }
    if (floorTexture) window_.draw(floors, sf::RenderStates(floorTexture));
    if (wallTexture) { window_.draw(dais, sf::RenderStates(wallTexture)); window_.draw(wallTops, sf::RenderStates(wallTexture)); }
    window_.draw(tops);
    window_.draw(shadows);

    // --- Pass 2, top to bottom: wall faces, stairs and decorations -------------
    std::vector<std::pair<sf::Vector2f, sf::Color>> lights;
    const float now = animationClock_.getElapsedTime().asSeconds();
    for (int y = viewStartY; y < drawEndY; ++y) {
        for (int x = viewStartX; x < viewEndX; ++x) {
            const Visibility vis = exploredMap_.at(x, y);
            if (vis == Visibility::Hidden) continue;
            const sf::Vector2f at = worldToScreen(x, y);
            const auto shade = [&](sf::Color c) { return shadeFor(vis, c); };
            const bool altarTile = landmark_ != LandmarkKind::None && x == landmarkAltar_.x && y == landmarkAltar_.y;
            const bool wall = isWallAt(x, y);
            const bool face = wall && !isWallAt(x, y + 1);
            if (face) {
                // Walls facing the camera show their bricks: two tiles tall
                // when there's wall behind to rise into, one tile otherwise.
                const auto frame = wallFaceFrame(x, y, isWallAt(x, y - 1));
                const float height = frame.rect.size.y > 16 ? kTileSize * 2 : kTileSize;
                SpriteAtlas::append(faces, frame, {at.x, at.y + kTileSize - height}, height,
                                    shade(themeTint(themeColor(theme.wall), 3.2f)));
                // Darker foot where the wall meets the floor.
                quad(feet, {at.x, at.y + kTileSize - 3}, {kTileSize, 3}, sf::Color(0, 0, 0, 110), sf::Color(0, 0, 0, 110), true);
            }
            if (!wall && map_.tileAt(x, y).type == TileType::Door) {
                if (tileset) SpriteAtlas::append(details, kDoorFrame, at, kTileSize, shade(themeTint(themeColor(theme.door), 1.6f)));
                else {
                    sf::RectangleShape door({kTileSize - 1.f, kTileSize - 1.f}); door.setPosition(at);
                    door.setFillColor(shade(themeColor(theme.door))); window_.draw(door);
                }
                if (vis == Visibility::Visible) lights.push_back({{at.x + kTileSize / 2, at.y + kTileSize / 2}, sf::Color(120, 160, 255)});
            }
            if (const int index = propIndexAt(x, y); index >= 0 && props_[static_cast<std::size_t>(index)].pos.x == x) {
                const Prop& prop = props_[static_cast<std::size_t>(index)];
                const auto [frame, sheet] = propFrame(prop.kind);
                const float width = kTileSize * propWidth(prop.kind);
                const float scale = (sheet == kEvilDungeon ? kTileSize / 32.f : kTileSize / 16.f);
                const float size = std::max(frame.rect.size.x, frame.rect.size.y) * scale;
                drawActorShadow({at.x + (width - kTileSize) / 2, at.y});
                SpriteAtlas::append(sheet == kEvilDungeon ? faces : details, frame,
                                    {at.x + (width - size) / 2, at.y + kTileSize - size + 2}, size, shade(sf::Color(225, 215, 205)));
            }
            switch (altarTile || propIndexAt(x, y) >= 0 ? Decor::None : decorAt(map_, x, y, currentFloor_, theme.region)) {
                case Decor::Torch:
                    SpriteAtlas::append(details, torchLit(x, y) ? torchFrame(x, y, now) : SpriteFrame{kTileset, sf::IntRect({160, 304}, {16, 16})},
                                        {at.x, at.y - kTileSize * 0.45f}, kTileSize, shade(sf::Color::White));
                    break;
                case Decor::Banner:
                    SpriteAtlas::append(details, bannerFrame(theme.region, x), {at.x, at.y - kTileSize * 0.9f}, kTileSize * 1.8f,
                                        shade(sf::Color::White));
                    break;
                case Decor::Bones: SpriteAtlas::append(details, kBonesFrame, at, kTileSize, shade(sf::Color(150, 145, 135, 190))); break;
                case Decor::Rubble: SpriteAtlas::append(details, kRubbleFrame, at, kTileSize, shade(sf::Color(190, 180, 170))); break;
                case Decor::Cobweb: SpriteAtlas::append(details, kCobwebFrame, at, kTileSize, shade(sf::Color(150, 150, 155, 110))); break;
                case Decor::Niche: SpriteAtlas::append(faces, {kEvilDungeon, sf::IntRect({0, 224}, {32, 32})}, at, kTileSize, shade(sf::Color(225, 215, 205))); break;
                case Decor::SkullNiche: SpriteAtlas::append(faces, {kEvilDungeon, sf::IntRect({32, 224}, {32, 32})}, at, kTileSize, shade(sf::Color(225, 215, 205))); break;
                case Decor::Chains: SpriteAtlas::append(faces, {kEvilDungeon, sf::IntRect({64, 224}, {32, 64})}, {at.x, at.y - kTileSize}, kTileSize * 2, shade(sf::Color(200, 195, 190))); break;
                case Decor::None: break;
            }
        }
    }
    if (wallTexture) window_.draw(faces, sf::RenderStates(wallTexture));
    if (tileset) window_.draw(details, sf::RenderStates(tileset));
    window_.draw(feet);

    renderLandmark();
    if (landmark_ != LandmarkKind::None && !landmarkUsed_ &&
        exploredMap_.at(landmarkAltar_.x, landmarkAltar_.y) == Visibility::Visible) {
        const auto at = worldToScreen(landmarkAltar_.x, landmarkAltar_.y);
        // Fountains light the floor they spill onto.
        const float below = landmarkAltarIsFloor(landmark_) ? 0.5f : 1.5f;
        lights.push_back({{at.x + kTileSize / 2, at.y + kTileSize * below}, landmarkLightColor()});
    }
    // Braziers burn on the four pillars that frame a landmark altar.
    if (landmark_ == LandmarkKind::Shrine)
        for (const Position offset : {Position{-3, -1}, Position{3, -1}, Position{-3, 1}, Position{3, 1}}) {
            const Position p{landmarkAltar_.x + offset.x, landmarkAltar_.y + offset.y};
            if (!map_.inBounds(p.x, p.y) || map_.tileAt(p.x, p.y).type != TileType::Wall || propIndexAt(p.x, p.y) >= 0 ||
                exploredMap_.at(p.x, p.y) != Visibility::Visible) continue;
            const auto at = worldToScreen(p.x, p.y);
            sprites_.draw(window_, torchFrame(p.x, p.y, now), {at.x - 2, at.y - kTileSize * 0.75f}, kTileSize + 4);
            lights.push_back({{at.x + kTileSize / 2, at.y - kTileSize * 0.2f}, sf::Color(255, 160, 80)});
        }
    renderSurfaces(lights, viewStartX, viewStartY, viewEndX, viewEndY);
    renderGroundItems();
    renderCorpses();
    // Visible monsters are drawn under the lighting; monsters hunting you
    // from out of sight are drawn after it as faded silhouettes.
    const auto drawMonsters = [&](bool sensedPass) {
    for (auto& m : monsters_) {
        if (m->stats().hp <= 0) {
            continue;
        }
        const bool sensed = sensedMonster(*m);
        if (sensed != sensedPass) continue;
        if (!sensed && (m->tactics.concealed || exploredMap_.at(m->position().x, m->position().y) != Visibility::Visible)) {
            continue; // only draw what the player can currently see -- see Prompt 7 notes
        }

        const ActorPose pose = actorPose(*m);
        const sf::Vector2f screenPos = pose.screen;
        if (!onMap(worldToScreen(m->position().x, m->position().y))) continue;

        // Elite/Nightmare border (Prompt 22): an outline in the tier's
        // color around the monster's tile. An outline rather than the old
        // filled square behind the monster, which a sprite's transparent
        // pixels would show through as a solid block. Base tier returns
        // nullopt -- nothing extra drawn.
        if (const std::optional<sf::Color> borderColor = isUniqueMonster(m->type()) ? std::optional<sf::Color>(sf::Color(255,190,60)) : tierBorderColor(m->tier());
            borderColor.has_value()) {
            constexpr float kBorderThickness = 3.f;
            sf::RectangleShape border({kTileSize - 1.f, kTileSize - 1.f});
            border.setPosition(screenPos);
            border.setFillColor(sf::Color::Transparent);
            border.setOutlineThickness(kBorderThickness);
            border.setOutlineColor(*borderColor);
            window_.draw(border);
        }

        drawActorShadow(screenPos);
        const MonsterLook look = monsterLook(*m);
        // Sized relative to a 32px character frame, so the 48px minotaur
        // stands taller than its tile instead of shrinking to fit it.
        const float spriteSize =
            kTileSize * static_cast<float>(std::max(look.frame.rect.size.x, look.frame.rect.size.y)) / 32.f * look.scale;
        const sf::Vector2f spritePos{screenPos.x + (kTileSize - spriteSize) / 2.f,
                                     screenPos.y + kTileSize - spriteSize};
        // Allies keep their art but are washed cyan, matching the old ally color.
        sf::Color tint = m->allied ? sf::Color(120, 235, 235) : look.tint;
        if (sensed) tint = sf::Color(tint.r * 3 / 5, tint.g * 3 / 5, std::min(255, tint.b * 3 / 4 + 40), 150);
        drawHitFlash(*m, lookFrame(look, pose.row, pose.frame), spritePos, spriteSize, pose.flip);
        if (!sprites_.draw(window_, lookFrame(look, pose.row, pose.frame), spritePos, spriteSize,
                           tint, pose.flip)) {
            sf::RectangleShape monsterShape({kTileSize - 1.f, kTileSize - 1.f});
            monsterShape.setPosition(screenPos);
            monsterShape.setFillColor(m->allied ? sf::Color(90,220,220) : monsterColor(m->type()));
            window_.draw(monsterShape);
        }

        const float hpFraction =
            static_cast<float>(m->stats().hp) / static_cast<float>(m->stats().maxHp);
        // Oversized creatures (the trial guardians) carry their bar above their heads.
        const float barY = screenPos.y - 6.f - std::max(0.f, (spriteSize - kTileSize) * 0.55f);
        sf::RectangleShape hpBack({kTileSize - 1.f, 4.f});
        hpBack.setPosition({screenPos.x, barY});
        hpBack.setFillColor(sf::Color(40, 20, 20));
        window_.draw(hpBack);

        sf::RectangleShape hpFront({(kTileSize - 1.f) * hpFraction, 4.f});
        hpFront.setPosition({screenPos.x, barY});
        hpFront.setFillColor(sf::Color(220, 60, 60));
        window_.draw(hpFront);
        if(m->tactics.retreat>0) drawText("Retreat",screenPos.x-8,screenPos.y+14,10,sf::Color(255,220,90));
        if (sensed) drawText("!", screenPos.x + kTileSize - 9.f, screenPos.y - 4.f, 14, sf::Color(255, 80, 60));
    }
    };
    drawMonsters(false);

    const ActorPose playerPose = actorPose(player_);
    const sf::Vector2f playerPos = playerPose.screen;
    drawActorShadow(playerPos);
    // Concealed, the player is a faint shape in the smoke.
    const bool hidden = player_.statusEffects().has(StatusEffectType::Concealed);
    const SpriteFrame playerSprite = animatedFrame(playerFrame(playerClass_), playerPose.row, playerPose.frame);
    if (!sprites_.draw(window_, playerSprite, playerPos, kTileSize,
                       hidden ? sf::Color(150, 150, 175, 120) : sf::Color::White, playerPose.flip)) {
        sf::RectangleShape playerShape({kTileSize - 1.f, kTileSize - 1.f});
        playerShape.setPosition(playerPos);
        playerShape.setFillColor(sf::Color(240, 200, 60));
        window_.draw(playerShape);
    }

    drawHitFlash(player_, playerSprite, playerPos, kTileSize, playerPose.flip);
    addVfxLights(lights);
    renderLighting(lights);
    drawMonsters(true);
    renderSurfaceGlow(viewStartX, viewStartY, viewEndX, viewEndY);
    renderStatusVfx();
    renderLootBeams();
    renderVfx();

    // Visible committed danger remains visible even if the caster leaves sight.
    renderTelegraphs(viewStartX, viewStartY, viewEndX, viewEndY);

    renderTargetingOverlay();
    window_.setView(uiView_);
    renderBattleHud();

    // Frame around the map, drawn over the tile edges.
    ui_.frame(window_, {{kMapLeft - 4, kMapTop - 4}, {kMapWidth + 8.f, kMapHeight + 8.f}});

    // Boss frame, top centre of the map, while the boss is alive and in sight.
    if (boss_ != nullptr && boss_->stats().hp > 0 &&
        exploredMap_.at(boss_->position().x, boss_->position().y) == Visibility::Visible) {
        const sf::FloatRect box{{kMapLeft + kMapWidth / 2.f - 260, kMapTop + 8}, {520, 50}};
        ui_.panel(window_, box, true, sf::Color(150, 140, 130));
        ui_.textCentered(window_, boss_->name(), {box.position, {box.size.x, 26}}, 18, ui::kRare, ui::Font::Title);
        ui_.bar(window_, {{box.position.x + 16, box.position.y + 28}, {box.size.x - 32, 14}},
                static_cast<float>(boss_->stats().hp) / static_cast<float>(boss_->stats().maxHp), sf::Color(196, 40, 32),
                std::to_string(boss_->stats().hp) + " / " + std::to_string(boss_->stats().maxHp), 12);
    }

    renderTravel();
    renderVault();
    renderShrine();
    renderAscendancy();
    if (vaultMenu_ || shrineMenu_ || exitMenu_) mapHints_.clear();
    renderMapHints();
    if (inventoryOpen_) renderInventory();
    else if (!vaultMenu_ && !shrineMenu_ && !exitMenu_) renderHudTooltips();
    window_.display();
}

} // namespace engine
