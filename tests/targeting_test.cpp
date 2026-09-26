#include <algorithm>
#include <iostream>
#include "entities/Monster.hpp"
#include "entities/MonsterInspection.hpp"
#include "entities/TalentEffects.hpp"
#include "world/ExploredMap.hpp"
#include "world/TalentTargeting.hpp"

using namespace engine;
namespace {
int failures = 0;
void check(bool condition, const char* message) {
    std::cout << (condition ? "[ok] " : "[FAIL] ") << message << '\n';
    if (!condition) ++failures;
}
bool has(const std::vector<Position>& positions, Position p) {
    return std::any_of(positions.begin(), positions.end(), [&](Position q) { return p.x==q.x && p.y==q.y; });
}
bool mentions(const std::vector<std::string>& lines, const std::string& text) {
    return std::any_of(lines.begin(), lines.end(), [&](const auto& line) { return line.find(text) != std::string::npos; });
}
}
int main() {
    Position start;
    Map map = parseAsciiMap({"##########", "#........#", "#.@......#", "#........#",
        "#........#", "#........#", "##########"}, start);
    ExploredMap vision(map);
    std::vector<Position> visible;
    for (int y=0; y<map.height(); ++y) for (int x=0; x<map.width(); ++x) visible.push_back({x,y});
    vision.update(visible);
    Stats stats; stats.hp=stats.maxHp=100; stats.mana=stats.maxMana=50; stats.strength=10;
    Actor caster("Player", '@', start, stats);
    Monster first(MonsterType::Goblin, "First", 'g', {4,2}, stats, nullptr);
    Monster second(MonsterType::Goblin, "Second", 'g', {7,2}, stats, nullptr);
    Monster side(MonsterType::Goblin, "Side", 'g', {4,3}, stats, nullptr);
    std::vector<Actor*> enemies{&first, &second, &side};
    Talent shot; shot.name="Shot"; shot.targeting=TargetingMode::RangedEnemyInSight;
    shot.power=8; shot.manaCost=3; shot.cooldownTurns=4;
    caster.talents() = TalentSet({shot});
    auto result = resolveTalentTarget(map, vision, caster, enemies, shot, second.position());
    check(result.valid && result.affected.size()==1 && result.affected[0]==&second,
        "A direct spell selects the requested enemy, not the nearest");
    shot.projectile=true;
    result = resolveTalentTarget(map, vision, caster, enemies, shot, second.position());
    check(result.valid && result.affected[0]==&first && result.path.back().x==4,
        "Projectile stops on its first victim and its preview ends there");
    map.setTile(3,2, Tile{TileType::Wall,false,false});
    result = resolveTalentTarget(map, vision, caster, enemies, shot, first.position());
    check(!result.valid && result.blockedAt && result.blockedAt->x==3 && result.affected.empty(),
        "A wall blocks the projectile before it reaches the target");
    map.setTile(3,2, Tile{TileType::Floor,true,true});
    map.setTile(2,3, Tile{TileType::Wall,false,false});
    result = resolveTalentTarget(map, vision, caster, enemies, shot, side.position());
    check(!result.valid, "A diagonal projectile cannot squeeze past a blocked corner");
    map.setTile(2,3, Tile{TileType::Floor,true,true});
    Talent melee=shot; melee.projectile=false; melee.targeting=TargetingMode::AdjacentEnemy;
    check(!resolveTalentTarget(map, vision, caster, enemies, melee, first.position()).valid,
        "Melee rejects a visible but nonadjacent enemy");
    first.setPosition({3,2});
    check(resolveTalentTarget(map, vision, caster, enemies, melee, first.position()).valid,
        "Melee accepts an orthogonally adjacent enemy");
    first.setPosition({3,3});
    check(!resolveTalentTarget(map, vision, caster, enemies, melee, first.position()).valid,
        "Melee rejects a diagonal neighbor");
    first.setPosition({4,2});
    Talent blast=shot; blast.shape=EffectShape::AreaAroundTarget; blast.areaRadius=1;
    result = resolveTalentTarget(map, vision, caster, enemies, blast, second.position());
    check(result.valid && result.affected.size()==2 && has(result.area, first.position()) &&
        has(result.area, side.position()) && !has(result.area, second.position()),
        "Projectile splash preview and victims use the first impact as their center");
    auto partial=visible;
    partial.erase(std::remove_if(partial.begin(), partial.end(), [&](Position p) {
        return p.x==side.position().x && p.y==side.position().y;
    }), partial.end());
    vision.update(partial);
    result = resolveTalentTarget(map, vision, caster, enemies, blast, first.position());
    check(result.affected.size()==1 && !has(result.area, side.position()),
        "Remembered enemies and tiles never leak through splash previews or damage");
    check(!resolveTalentTarget(map, vision, caster, enemies, shot, side.position()).valid,
        "A remembered enemy cannot be aimed at");
    vision.update(visible);
    Talent blink; blink.shape=EffectShape::Movement; blink.targeting=TargetingMode::Self; blink.moveDistance=4;
    result = resolveTalentTarget(map, vision, caster, enemies, blink, {8,2});
    check(result.valid && result.destination.x==3 && result.blockedAt && result.blockedAt->x==4,
        "Aimed movement stops before an occupied tile");
    result = resolveTalentTarget(map, vision, caster, enemies, blink, {2,5});
    check(result.valid && result.destination.y==5, "Blink follows the selected direction");
    check(!resolveTalentTarget(map, vision, caster, enemies, blink, start).valid,
        "Blinking in place is invalid");
    check(!resolveTalentTarget(map, vision, caster, enemies, blink, {-1,2}).valid,
        "Out-of-map aim is rejected safely");
    first.setPosition({3,2}); melee.retreatDistance=3;
    result = resolveTalentTarget(map, vision, caster, enemies, melee, first.position());
    check(result.valid && result.destination.x==1 && result.movementPath.size()==2,
        "Vault retreat previews stop at terrain");
    Talent heal; heal.targeting=TargetingMode::Self; heal.effectKind=TalentEffectKind::Heal;
    result = resolveTalentTarget(map, vision, caster, enemies, heal, {-1,-1});
    check(result.valid && result.affected[0]==&caster, "Self effects do not need a cursor target");

    shot.conditionalHpFraction=0.5f; shot.conditionalMultiplier=3;
    caster.statusEffects().apply({StatusEffectType::Empowered,3,4});
    first.stats().hp=40;
    const auto damage=estimateTalentDamage(shot,caster,first);
    check(damage.normal==48 && damage.critical==72,
        "Preview includes cooldown-tier scaling, Empowered, execute and crit truncation");
    for(int i=0;i<100;++i) { estimateTalentDamage(shot,caster,first); resolveTalentTarget(map,vision,caster,enemies,shot,first.position()); }
    check(caster.stats().mana==50 && caster.stats().hp==100 && first.stats().hp==40 &&
        caster.talents().cooldownRemaining(0)==0 && caster.statusEffects().active()[0].turnsRemaining==3,
        "Repeated previews leave resources, HP, effects and cooldowns unchanged");
    first.talents()=TalentSet({shot}); first.talents().setCooldownRemaining(0,3);
    check(mentions(inspectMonster(first,vision),"Shot") && !mentions(inspectMonster(first,vision),"Cooldown: 3"),
        "Ordinary inspection names abilities without disclosing their remaining cooldown");
    check(mentions(inspectMonster(first,vision,{true}),"Cooldown: 3 enemy turns"),
        "Explicit information access reveals cooldowns in enemy turns");
    vision.update({start});
    check(inspectMonster(first,vision,{true}).empty(),
        "Even enhanced inspection cannot read a hidden or remembered enemy");
    vision.update(visible); first.stats().hp=0;
    check(inspectMonster(first,vision).empty(), "Dead enemies cannot be inspected");
    caster.talents().setCooldownRemaining(0,2);
    check(!talentUnavailableReason(caster,0).empty(), "Preflight rejects an active cooldown");
    caster.talents().resetCooldowns(); caster.stats().mana=0;
    check(!talentUnavailableReason(caster,0).empty(), "Preflight rejects insufficient mana");
    return failures ? 1 : 0;
}
