#include "world/TalentTargeting.hpp"

#include <algorithm>
#include <cmath>
#include "entities/Actor.hpp"
#include "world/ExploredMap.hpp"

namespace engine {
namespace {
bool same(Position a, Position b) { return a.x == b.x && a.y == b.y; }
int distanceSquared(Position a, Position b) {
    const int dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy;
}
std::vector<Position> line(Position from, Position to) {
    std::vector<Position> result{from};
    const int dx = std::abs(to.x - from.x), dy = -std::abs(to.y - from.y);
    const int sx = from.x < to.x ? 1 : -1, sy = from.y < to.y ? 1 : -1;
    int error = dx + dy;
    while (!same(from, to)) {
        const int twice = 2 * error;
        if (twice >= dy) { error += dy; from.x += sx; }
        if (twice <= dx) { error += dx; from.y += sy; }
        result.push_back(from);
    }
    return result;
}
bool open(const Map& map, Position p) {
    return map.inBounds(p.x, p.y) && map.tileAt(p.x, p.y).transparent;
}
bool cornerBlocked(const Map& map, Position a, Position b) {
    return a.x != b.x && a.y != b.y &&
        (!open(map, {a.x, b.y}) || !open(map, {b.x, a.y}));
}
}

std::string talentUnavailableReason(const Actor& caster, std::size_t index) {
    const auto& known = caster.talents().knownTalents();
    if (index >= known.size()) return "No talent in this slot.";
    if (!caster.talents().isReady(index)) return "Ability is on cooldown.";
    const Talent effective = caster.talents().effectiveTalent(index);
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
        const auto ray = line(start, cursor);
        for (std::size_t i = 1; i < ray.size(); ++i) {
            const Position p = ray[i];
            if (talent.shape == EffectShape::Movement &&
                i > static_cast<std::size_t>(std::max(0, talent.moveDistance))) break;
            if (!visible(p)) { result.message = "Path leaves your field of view."; break; }
            result.path.push_back(p);
            if (!open(map, p) || cornerBlocked(map, ray[i - 1], p) ||
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
            return result;
        }
    } else if (talent.shape != EffectShape::AreaAroundSelf) {
        anchor = enemyAt(cursor);
        if (talent.targeting == TargetingMode::AdjacentEnemy &&
            std::abs(cursor.x - start.x) + std::abs(cursor.y - start.y) != 1) {
            result.message = "Target must be orthogonally adjacent.";
            return result;
        }
    }

    if (talent.shape != EffectShape::AreaAroundSelf && !anchor) {
        if (result.message.empty()) result.message = "Choose a visible enemy.";
        return result;
    }
    const Position center = anchor ? anchor->position() : start;
    if (talent.shape == EffectShape::AreaAroundSelf ||
        talent.shape == EffectShape::AreaAroundTarget) {
        const int radius = std::max(0, talent.areaRadius);
        for (int y = center.y - radius; y <= center.y + radius; ++y)
            for (int x = center.x - radius; x <= center.x + radius; ++x) {
                const Position p{x, y};
                if (!visible(p) || !map.isWalkable(x, y) ||
                    distanceSquared(center, p) > radius * radius) continue;
                result.area.push_back(p);
                if (Actor* enemy = enemyAt(p)) result.affected.push_back(enemy);
            }
    } else {
        result.area.push_back(center);
        result.affected.push_back(anchor);
    }
    result.valid = !result.affected.empty();
    if (!result.valid) result.message = "No enemies in the affected area.";

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
            const auto ray = line(center, enemy->position());
            bool clear = true;
            for (std::size_t i = 1; i < ray.size(); ++i) {
                if (!visible(ray[i]) || !open(map, ray[i]) || cornerBlocked(map, ray[i-1], ray[i]) ||
                    (i + 1 < ray.size() && enemyAt(ray[i]))) { clear = false; break; }
            }
            if (!clear) continue;
            result.chainedTarget = enemy; result.chainPath = ray;
            result.affected.push_back(enemy); result.area.push_back(enemy->position());
            break;
        }
    }

    if (anchor && talent.retreatDistance > 0) {
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
    return result;
}
} // namespace engine
