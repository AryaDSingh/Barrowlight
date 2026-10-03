#include "world/TalentTargeting.hpp"

#include <algorithm>
#include <cmath>
#include "entities/Actor.hpp"
#include "entities/ArmourTalents.hpp"
#include "entities/HiddenCombat.hpp"
#include "world/ExploredMap.hpp"
#include "world/LineOfFire.hpp"

namespace engine {
namespace {
bool same(Position a, Position b) { return a.x == b.x && a.y == b.y; }
int distanceSquared(Position a, Position b) {
    const int dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy;
}

}

std::string talentUnavailableReason(const Actor& caster, std::size_t index) {
    const auto& known = caster.talents().knownTalents();
    if (index >= known.size()) return "No talent in this slot.";
    if (!caster.talents().isReady(index)) return "Ability is on cooldown.";
    const Talent effective = combatTalent(caster,caster.talents().effectiveTalent(index));
    if (effective.id=="spellblade.imbue") return "Bind an Imbue variant in the talent browser (V selects element).";
    if (effective.cleanse && std::none_of(caster.statusEffects().active().begin(),caster.statusEffects().active().end(),
        [](const auto& e){return isCleansable(e.type);})) return "No removable ailments. Cleanse was not spent.";
    if (effective.passive) return "Passive abilities do not need to be cast.";
    if (!armourMatches(caster,effective.armourRequirement)) return armourRequirementText(effective.armourRequirement);
    if (effective.requiresStealth && !caster.statusEffects().has(StatusEffectType::Concealed)) return "Requires Concealment.";
    const auto* weapon=caster.inventory().equipped(EquipmentSlot::Weapon);
    const auto* shield=caster.inventory().equipped(EquipmentSlot::OffHand);
    const auto kind=weapon && weapon->definition() ? weapon->definition()->weaponKind : WeaponKind::None;
    switch (effective.weaponRequirement) {
    case WeaponRequirement::Melee: if (!hasMeleeWeapon(caster)) return "Equip a one- or two-handed melee weapon (not a bow or staff)."; break;
    case WeaponRequirement::OneHanded: if (kind!=WeaponKind::OneHanded) return "Equip a one-handed weapon."; break;
    case WeaponRequirement::TwoHanded: if (kind!=WeaponKind::TwoHanded) return "Equip a two-handed weapon."; break;
    case WeaponRequirement::Bow: if (kind!=WeaponKind::Bow) return "Equip a bow."; break;
    case WeaponRequirement::Whip: if (kind!=WeaponKind::Whip) return "Equip a whip."; break;
    case WeaponRequirement::Shield: if (!shield) return "Equip a shield."; break;
    default: break;
    }
    if (caster.stats().mana < effective.manaCost) return "Not enough mana.";
    if (effective.hpCost > 0 && caster.stats().hp <= effective.hpCost)
        return "Not enough HP to safely cast.";
    return {};
}

TalentTarget resolveTalentTarget(const Map& map, const ExploredMap& vision,
    Actor& caster, const std::vector<Actor*>& enemies, const Talent& talent,
    Position cursor) {
    TalentTarget result;
    const Position start = caster.position();
    result.destination = start;
    auto visible = [&](Position p) {
        return vision.at(p.x, p.y) == Visibility::Visible;
    };
    auto enemyAt = [&](Position p) -> Actor* {
        if (!visible(p)) return nullptr;
        for (Actor* enemy : enemies)
            if (enemy != &caster && enemy->stats().hp > 0 && same(enemy->position(), p))
                return enemy;
        return nullptr;
    };

    auto movementArea = [&] {
        if (!talent.movementBurn || same(result.destination,start)) return;
        for (const Position d:std::vector<Position>{{1,0},{-1,0},{0,1},{0,-1}}) {
            Position p{result.destination.x+d.x,result.destination.y+d.y};
            if (visible(p) && map.isWalkable(p.x,p.y)) result.area.push_back(p);
        }
    };

    if (talent.targeting == TargetingMode::Self &&
        talent.shape == EffectShape::SingleTarget) {
        result.affected.push_back(&caster);
        result.area.push_back(start);
        result.valid = true;
        return result;
    }

    if (talent.shape != EffectShape::AreaAroundSelf && !visible(cursor)) {
        result.message = "Choose a currently visible tile.";
        return result;
    }

    Actor* anchor = nullptr;
    if (talent.shape == EffectShape::Movement || talent.projectile) {
        result.path.push_back(start);
        const auto ray = fireLine(start, cursor);
        for (std::size_t i = 1; i < ray.size(); ++i) {
            const Position p = ray[i];
            if (talent.shape == EffectShape::Movement &&
                i > static_cast<std::size_t>(std::max(0, talent.moveDistance))) break;
            result.path.push_back(p);
            if (!fireTileOpen(map, p) || fireCornerBlocked(map, ray[i - 1], p) ||
                (talent.shape == EffectShape::Movement && !map.isWalkable(p.x, p.y))) {
                result.blockedAt = p;
                result.message = "Path blocked by terrain.";
                break;
            }
            if (Actor* enemy = enemyAt(p)) {
                if (talent.shape == EffectShape::Movement) {
                    result.blockedAt = p;
                    result.message = "Movement stops before an occupied tile.";
                } else {
                    anchor = enemy;
                    if (!same(p, cursor)) result.message = "Intercepted by " + enemy->name();
                }
                break;
            }
            if (talent.shape == EffectShape::Movement) result.destination = p;
        }
        if (talent.shape == EffectShape::Movement) {
            result.valid = !same(result.destination, start);
            if (!result.valid && result.message.empty()) result.message = "Choose a different tile.";
            result.area.push_back(result.destination);
            movementArea();
            return result;
        }
    } else if (talent.shape != EffectShape::AreaAroundSelf) {
        if (!hasLineOfFire(map,start,cursor)) { result.message="Line of fire blocked by terrain."; return result; }
        anchor = enemyAt(cursor);
        const int dx = cursor.x - start.x, dy = cursor.y - start.y, reach = std::abs(dx) + std::abs(dy);
        // Charges and whips reach along a straight, clear line.
        const int span = talent.chargeDistance > 0 ? talent.chargeDistance + 1 : talent.reach;
        if (talent.targeting == TargetingMode::AdjacentEnemy && span > 1 &&
            (dx == 0) != (dy == 0) && reach > 1 && reach <= span) {
            const bool charge = talent.chargeDistance > 0;
            if (!anchor) { result.message = charge ? "Charge at a visible enemy in a straight line." : "Lash at a visible enemy in a straight line."; return result; }
            const Position step{(dx > 0) - (dx < 0), (dy > 0) - (dy < 0)};
            if (charge) result.movementPath.push_back(start);
            for (Position p{start.x + step.x, start.y + step.y}; !same(p, cursor); p = {p.x + step.x, p.y + step.y}) {
                if (!visible(p) || !map.isWalkable(p.x, p.y) || enemyAt(p)) {
                    result.blockedAt = p; result.message = charge ? "The charge needs a clear, straight run." : "Something is in the way.";
                    return result;
                }
                if (charge) { result.movementPath.push_back(p); result.destination = p; }
            }
        } else if (talent.targeting == TargetingMode::AdjacentEnemy && reach != 1) {
            result.message = span > 1 ? "Target must be adjacent, or in a straight line within " +
                std::to_string(span) + " tiles." : "Target must be orthogonally adjacent.";
            return result;
        }
    }

    if (talent.shape != EffectShape::AreaAroundSelf) {
        if (result.blockedAt) return result;
        // Empty ground is a real cast target. Preserve terrain and visibility
        // restrictions, and projectile interception by the first visible enemy.
        if (!anchor && !map.isWalkable(cursor.x,cursor.y)) {
            result.message = "Choose a visible ground tile.";
            return result;
        }
    }
    const Position center = talent.shape==EffectShape::AreaAroundSelf ? start :
        anchor ? anchor->position() : cursor;
    if (talent.shape == EffectShape::AreaAroundSelf ||
        talent.shape == EffectShape::AreaAroundTarget) {
        const int radius = std::max(0, talent.areaRadius);
        for (int y = center.y - radius; y <= center.y + radius; ++y)
            for (int x = center.x - radius; x <= center.x + radius; ++x) {
                const Position p{x, y};
                if (!visible(p) || !map.isWalkable(x, y) ||
                    distanceSquared(center, p) > radius * radius || !hasLineOfFire(map,center,p)) continue;
                result.area.push_back(p);
                if (Actor* enemy = enemyAt(p)) result.affected.push_back(enemy);
            }
    } else {
        result.area.push_back(center);
        if (anchor) result.affected.push_back(anchor);
    }
    result.valid = true;
    if (result.affected.empty()) result.message = "Empty ground: cast will still spend its turn, cost and cooldown.";

    if (talent.chain && anchor && result.valid) {
        // Nearest visible secondary target, stable coordinate tie-break. No RNG.
        std::vector<Actor*> candidates;
        for (Actor* enemy : enemies) {
            if (enemy != anchor && enemy != &caster && enemy->stats().hp > 0 && visible(enemy->position()) &&
                distanceSquared(center, enemy->position()) <= 9) candidates.push_back(enemy);
        }
        std::sort(candidates.begin(), candidates.end(), [&](const Actor* a, const Actor* b) {
            const int da = distanceSquared(center, a->position()), db = distanceSquared(center, b->position());
            if (da != db) return da < db;
            return a->position().y != b->position().y ? a->position().y < b->position().y : a->position().x < b->position().x;
        });
        for (Actor* enemy : candidates) {
            const auto ray = fireLine(center, enemy->position());
            bool clear = true;
            for (std::size_t i = 1; i < ray.size(); ++i) {
                if (!fireTileOpen(map, ray[i]) || fireCornerBlocked(map, ray[i-1], ray[i]) ||
                    (i + 1 < ray.size() && enemyAt(ray[i]))) { clear = false; break; }
            }
            if (!clear) continue;
            result.chainedTarget = enemy; result.chainPath = ray;
            result.affected.push_back(enemy); result.area.push_back(enemy->position());
            break;
        }
    }

    if (talent.retreatDistance > 0) {
        const Position direction{start.x - center.x, start.y - center.y};
        result.movementPath.push_back(start);
        for (int i = 0; i < talent.retreatDistance; ++i) {
            const Position p{result.destination.x + direction.x,
                             result.destination.y + direction.y};
            if (!visible(p) || !map.isWalkable(p.x, p.y) || enemyAt(p)) break;
            result.movementPath.push_back(p);
            result.destination = p;
        }
    }
    if (talent.retreatDistance>0) movementArea();
    return result;
}
} // namespace engine
