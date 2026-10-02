#include "core/Application.hpp"

#include <algorithm>
#include <cmath>
#include <queue>
#include <random>

#include "core/PlayLayout.hpp"

namespace engine {

// Surfaces and light fixtures (world/Surfaces.hpp): oil, water, fire, ice
// and electrified water on the ground; wall torches, braziers and oil
// barrels that the elements light, douse or set off. Surfaces tick once
// per player action, hurting whoever stands in them.

namespace {
constexpr float kTile = static_cast<float>(playLayout::tileSize);
constexpr float kPi = 3.14159265f;

unsigned surfaceHash(int x, int y, unsigned salt) {
    unsigned h = static_cast<unsigned>(x) * 73856093u ^ static_cast<unsigned>(y) * 19349663u ^ salt * 83492791u;
    h ^= h >> 13; h *= 1274126177u; h ^= h >> 16;
    return h;
}
float unit(unsigned h) { return static_cast<float>(h % 1000) / 1000.f; }

void blob(sf::VertexArray& va, sf::Vector2f c, float r, sf::Color inner, sf::Color outer) {
    constexpr int segments = 12;
    for (int i = 0; i < segments; ++i) {
        const float a0 = 2 * kPi * i / segments, a1 = 2 * kPi * (i + 1) / segments;
        va.append(sf::Vertex{c, inner});
        va.append(sf::Vertex{{c.x + std::cos(a0) * r, c.y + std::sin(a0) * r}, outer});
        va.append(sf::Vertex{{c.x + std::cos(a1) * r, c.y + std::sin(a1) * r}, outer});
    }
}
void line(sf::VertexArray& va, sf::Vector2f a, sf::Vector2f b, float w, sf::Color c) {
    const sf::Vector2f d = b - a;
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < .01f) return;
    const sf::Vector2f n{-d.y / len * w / 2, d.x / len * w / 2};
    for (const auto& p : {a + n, b + n, b - n, a + n, b - n, a - n}) va.append(sf::Vertex{p, c});
}
} // namespace

Element talentElement(const Talent& t) {
    if (t.effectKind != TalentEffectKind::Damage || t.shape == EffectShape::Movement) return Element::None;
    if (t.tree == TalentTree::Fire || (t.onHitEffect && t.onHitEffect->type == StatusEffectType::Burn)) return Element::Fire;
    if (t.tree == TalentTree::Ice || (t.onHitEffect && t.onHitEffect->type == StatusEffectType::Chill)) return Element::Ice;
    if (t.tree == TalentTree::Lightning) return Element::Lightning;
    if (t.tree == TalentTree::Bow || t.tree == TalentTree::ShadowArcher) return Element::Arrow;
    return Element::None;
}

SurfaceType Application::surfaceAt(Position p) const {
    if (!map_.inBounds(p.x, p.y) || surfaces_.size() != static_cast<std::size_t>(map_.width() * map_.height())) return SurfaceType::None;
    return surfaces_[static_cast<std::size_t>(p.y * map_.width() + p.x)].type;
}

void Application::setSurface(Position p, SurfaceType type, int turns) {
    if (!map_.inBounds(p.x, p.y)) return;
    if (surfaces_.size() != static_cast<std::size_t>(map_.width() * map_.height()))
        surfaces_.assign(static_cast<std::size_t>(map_.width() * map_.height()), {});
    if (type != SurfaceType::None && !map_.isWalkable(p.x, p.y)) return;
    surfaces_[static_cast<std::size_t>(p.y * map_.width() + p.x)] = {type, turns};
}

void Application::clearSurfaces() {
    surfaces_.assign(static_cast<std::size_t>(std::max(0, map_.width() * map_.height())), {});
    torchToggles_.clear();
}

bool Application::visibleTile(Position p) const {
    return map_.inBounds(p.x, p.y) && exploredMap_.at(p.x, p.y) == Visibility::Visible;
}

// A fire, cold, lightning or arrow landing on these tiles.
void Application::applyElement(Element element, const std::vector<Position>& tiles) {
    if (element == Element::None || tiles.empty()) return;
    bool steam = false, ignited = false, froze = false;
    std::vector<Position> water;
    for (const auto& p : tiles) {
        const auto s = surfaceAt(p);
        if (element == Element::Fire) {
            if (s == SurfaceType::Oil) { setSurface(p, SurfaceType::Fire, kOilFireTurns); ignited = true; }
            else if (s == SurfaceType::Ice) { setSurface(p, SurfaceType::Water, 0); steam = true; }
            else if (s == SurfaceType::Water || s == SurfaceType::Electrified) steam = true;
        } else if (element == Element::Ice) {
            if (s == SurfaceType::Water || s == SurfaceType::Electrified) { setSurface(p, SurfaceType::Ice, 0); froze = true; }
            else if (s == SurfaceType::Fire) { setSurface(p, SurfaceType::None, 0); steam = true; }
        } else if (element == Element::Lightning && (s == SurfaceType::Water || s == SurfaceType::Electrified)) {
            water.push_back(p);
        }
        if (steam && visibleTile(p)) spawnVfx({Vfx::Kind::Puff, {p.x + .5f, p.y + .5f}, {p.x + .5f, p.y + .5f}, sf::Color(200, 210, 220), 0, .6f, .8f});
    }
    if (ignited) log("The oil goes up in flames!");
    if (froze) log("The water freezes solid.");
    if (!water.empty()) electrify(water);

    // Fixtures within reach of where it landed: torches, braziers, oil barrels.
    const auto near = [&](Position f) {
        return std::any_of(tiles.begin(), tiles.end(), [&](Position p) { return std::max(std::abs(p.x - f.x), std::abs(p.y - f.y)) <= 1; });
    };
    for (const auto& front : wallTorches_) {
        const Position wall{front.x, front.y - 1};
        if (!near(front) && !near(wall)) continue;
        const bool lit = torchLit(wall.x, wall.y);
        if (element == Element::Fire && !lit) { setTorchLit(wall.x, wall.y, true); log("The torch flares alight."); }
        else if ((element == Element::Ice || element == Element::Arrow) && lit) {
            setTorchLit(wall.x, wall.y, false);
            log(element == Element::Arrow ? "Your shot snuffs out the torch." : "Frost snuffs out the torch.");
            spawnVfx({Vfx::Kind::Puff, {wall.x + .5f, wall.y + .9f}, {wall.x + .5f, wall.y + .9f}, sf::Color(150, 150, 160), 0, .6f, .5f});
        }
    }
    for (std::size_t i = 0; i < props_.size(); ++i) {
        const Prop& prop = props_[i];
        if (!near(prop.pos)) continue;
        if (prop.kind == PropKind::ColdBrazier && element == Element::Fire) {
            props_[i].kind = PropKind::Brazier; log("The brazier catches and burns.");
        } else if (prop.kind == PropKind::Brazier && element == Element::Ice) {
            props_[i].kind = PropKind::ColdBrazier; log("Frost smothers the brazier.");
        } else if (prop.kind == PropKind::OilBarrel && (element == Element::Fire || element == Element::Lightning)) {
            explodeOilBarrel(i);
            break; // props_ changed
        } else if (prop.kind == PropKind::OilBarrel && element == Element::Arrow) {
            props_[i].kind = PropKind::Barrel; // punctured: it empties onto the floor
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
                if (surfaceAt({prop.pos.x + dx, prop.pos.y + dy}) == SurfaceType::None) setSurface({prop.pos.x + dx, prop.pos.y + dy}, SurfaceType::Oil, 0);
            log("Oil gushes from the punctured barrel.");
        }
    }
    updateFieldOfView();
}

// Lightning runs through a whole connected pool.
void Application::electrify(const std::vector<Position>& seeds) {
    std::queue<Position> open;
    std::vector<std::uint8_t> seen(static_cast<std::size_t>(map_.width() * map_.height()), 0);
    for (const auto& p : seeds) { open.push(p); seen[static_cast<std::size_t>(p.y * map_.width() + p.x)] = 1; }
    int count = 0;
    std::vector<Position> pool;
    while (!open.empty() && count < 60) {
        const auto p = open.front(); open.pop(); ++count;
        pool.push_back(p);
        setSurface(p, SurfaceType::Electrified, kElectrifiedTurns);
        for (const Position d : {Position{1, 0}, Position{-1, 0}, Position{0, 1}, Position{0, -1}}) {
            const Position n{p.x + d.x, p.y + d.y};
            if (!map_.inBounds(n.x, n.y) || seen[static_cast<std::size_t>(n.y * map_.width() + n.x)]) continue;
            const auto s = surfaceAt(n);
            if (s != SurfaceType::Water && s != SurfaceType::Electrified) continue;
            seen[static_cast<std::size_t>(n.y * map_.width() + n.x)] = 1;
            open.push(n);
        }
    }
    log("Lightning crackles through the water!");
    for (const auto& p : pool)
        if (visibleTile(p) && (p.x + p.y) % 2 == 0)
            spawnVfx({Vfx::Kind::Burst, {p.x + .5f, p.y + .5f}, {p.x + .5f, p.y + .5f}, sf::Color(175, 205, 255), 0, .35f, .5f});
    shockStanding(pool);
}

void Application::shockStanding(const std::vector<Position>& pool) {
    const auto inPool = [&](Position p) { return std::any_of(pool.begin(), pool.end(), [&](Position q) { return q.x == p.x && q.y == p.y; }); };
    std::vector<Actor*> victims;
    if (inPool(player_.position())) victims.push_back(&player_);
    for (auto& m : monsters_) if (m->stats().hp > 0 && inPool(m->position())) victims.push_back(m.get());
    for (auto* a : victims) {
        a->statusEffects().apply({StatusEffectType::Shock, 2, 0});
        a->stats().hp -= 3;
        flashActor(*a);
        if (a == &player_) log("The water shocks you for 3!");
        checkAndHandleDeath(*a);
    }
}

void Application::explodeOilBarrel(std::size_t index) {
    if (index >= props_.size()) return;
    const Position at = props_[index].pos;
    auto rest = props_;
    rest.erase(rest.begin() + static_cast<std::ptrdiff_t>(index));
    map_.setTile(at.x, at.y, Tile{TileType::Floor, true, true});
    setProps(rest);
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) setSurface({at.x + dx, at.y + dy}, SurfaceType::Fire, kOilFireTurns);
    if (visibleTile(at)) {
        spawnVfx({Vfx::Kind::Burst, {at.x + .5f, at.y + .5f}, {at.x + .5f, at.y + .5f}, sf::Color(255, 140, 50), 0, .5f, 1.8f});
        spawnVfx({Vfx::Kind::Ring, {at.x + .5f, at.y + .5f}, {at.x + .5f, at.y + .5f}, sf::Color(255, 180, 80), 0, .5f, 2.f});
    }
    log("The oil barrel explodes!");
    std::vector<Actor*> caught;
    const auto inBlast = [&](Position p) { return std::max(std::abs(p.x - at.x), std::abs(p.y - at.y)) <= 1; };
    if (inBlast(player_.position())) caught.push_back(&player_);
    for (auto& m : monsters_) if (m->stats().hp > 0 && inBlast(m->position())) caught.push_back(m.get());
    for (auto* a : caught) {
        a->stats().hp -= 6;
        a->statusEffects().apply({StatusEffectType::Burn, 3, 2});
        flashActor(*a);
        if (a == &player_) log("The blast burns you for 6!");
        checkAndHandleDeath(*a);
    }
}

// Walking into a brazier or an oil barrel knocks it over.
bool Application::knockOver(Position tile, Position direction) {
    const int index = propIndexAt(tile.x, tile.y);
    if (index < 0) return false;
    const Prop prop = props_[static_cast<std::size_t>(index)];
    if (prop.kind != PropKind::Brazier && prop.kind != PropKind::ColdBrazier && prop.kind != PropKind::OilBarrel) return false;
    auto rest = props_;
    rest.erase(rest.begin() + index);
    map_.setTile(tile.x, tile.y, Tile{TileType::Floor, true, true});
    setProps(rest);
    const Position beyond{tile.x + direction.x, tile.y + direction.y};
    if (prop.kind == PropKind::Brazier) {
        for (const Position p : {tile, beyond}) setSurface(p, SurfaceType::Fire, kSpilledFireTurns);
        if (surfaceAt(beyond) == SurfaceType::Oil) setSurface(beyond, SurfaceType::Fire, kOilFireTurns);
        log("You kick over the brazier. Burning coals spill across the floor!");
        if (visibleTile(tile)) spawnVfx({Vfx::Kind::Burst, {beyond.x + .5f, beyond.y + .5f}, {beyond.x + .5f, beyond.y + .5f}, sf::Color(255, 140, 50), 0, .4f, 1.f});
    } else if (prop.kind == PropKind::ColdBrazier) {
        log("You knock the cold brazier over.");
    } else {
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if (surfaceAt({tile.x + dx + direction.x, tile.y + dy + direction.y}) == SurfaceType::None)
                    setSurface({tile.x + dx + direction.x, tile.y + dy + direction.y}, SurfaceType::Oil, 0);
        setSurface(tile, SurfaceType::Oil, 0);
        log("You topple the barrel. Oil floods the floor.");
    }
    updateFieldOfView();
    return true;
}

// Once per player action: fire spreads along oil and burns out, sparks
// fade, and whoever stands in something suffers it.
void Application::tickSurfaces() {
    if (surfaces_.size() != static_cast<std::size_t>(map_.width() * map_.height())) return;
    const int w = map_.width(), h = map_.height();
    std::vector<Position> spreadTo;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            auto& s = surfaces_[static_cast<std::size_t>(y * w + x)];
            if (s.type != SurfaceType::Fire) continue;
            for (const Position d : {Position{1, 0}, Position{-1, 0}, Position{0, 1}, Position{0, -1}})
                if (surfaceAt({x + d.x, y + d.y}) == SurfaceType::Oil) spreadTo.push_back({x + d.x, y + d.y});
        }
    for (auto& s : surfaces_) {
        if (s.type == SurfaceType::Fire && --s.turns <= 0) s = {};
        else if (s.type == SurfaceType::Electrified && --s.turns <= 0) s = {SurfaceType::Water, 0};
    }
    for (const auto& p : spreadTo) setSurface(p, SurfaceType::Fire, kOilFireTurns);

    std::vector<Position> shocked;
    const auto suffer = [&](Actor& a) {
        switch (surfaceAt(a.position())) {
            case SurfaceType::Fire:
                if (!a.statusEffects().has(StatusEffectType::Burn) && &a == &player_) log("You are standing in flames!");
                a.statusEffects().apply({StatusEffectType::Burn, 3, 2});
                break;
            case SurfaceType::Ice: a.statusEffects().apply({StatusEffectType::Chill, 2, 20}); break;
            case SurfaceType::Electrified: shocked.push_back(a.position()); break;
            default: break;
        }
    };
    suffer(player_);
    for (auto& m : monsters_) if (m->stats().hp > 0) suffer(*m);
    if (!shocked.empty()) shockStanding(shocked);
}

// Dangerous ground monsters won't walk into on purpose.
bool Application::hazardousSurface(Position p) const {
    const auto s = surfaceAt(p);
    return s == SurfaceType::Fire || s == SurfaceType::Electrified;
}

// Scatter puddles, oil slicks, braziers and oil barrels over a new floor.
void Application::seedSurfaces(unsigned seed) {
    clearSurfaces();
    std::mt19937 rng(seed ^ 0x5eedu);
    const auto region = floorTheme(currentFloor_).region;
    const auto clearOf = [&](Position p, int dist) {
        const auto far = [&](Position q) { return std::abs(p.x - q.x) + std::abs(p.y - q.y) > dist; };
        return far(floorEntrance_) && (!map_.inBounds(floorExit_.x, floorExit_.y) || far(floorExit_)) &&
               (landmark_ == LandmarkKind::None || far(landmarkAltar_)) && (!vaultExists_ || far(vaultCenter_));
    };
    std::vector<Position> floor;
    for (int y = 1; y + 1 < map_.height(); ++y)
        for (int x = 1; x + 1 < map_.width(); ++x)
            if (map_.isWalkable(x, y) && map_.tileAt(x, y).type == TileType::Floor) floor.push_back({x, y});
    if (floor.empty()) return;
    const auto pool = [&](SurfaceType type, int size) {
        for (int attempt = 0; attempt < 20; ++attempt) {
            const Position start = floor[std::uniform_int_distribution<std::size_t>(0, floor.size() - 1)(rng)];
            if (!clearOf(start, 4) || surfaceAt(start) != SurfaceType::None) continue;
            std::vector<Position> grown{start};
            setSurface(start, type, 0);
            while (static_cast<int>(grown.size()) < size) {
                const Position from = grown[std::uniform_int_distribution<std::size_t>(0, grown.size() - 1)(rng)];
                const Position d = std::array<Position, 4>{Position{1, 0}, Position{-1, 0}, Position{0, 1}, Position{0, -1}}
                    [std::uniform_int_distribution<int>(0, 3)(rng)];
                const Position n{from.x + d.x, from.y + d.y};
                if (!map_.isWalkable(n.x, n.y) || surfaceAt(n) != SurfaceType::None || !clearOf(n, 2)) {
                    if (std::uniform_int_distribution<int>(0, 9)(rng) == 0) break;
                    continue;
                }
                setSurface(n, type, 0);
                grown.push_back(n);
            }
            return;
        }
    };
    const int puddles = region == FloorRegion::Crypts ? 4 : region == FloorRegion::Sanctum ? 3 : 2;
    for (int i = 0; i < puddles; ++i) pool(SurfaceType::Water, std::uniform_int_distribution<int>(4, 10)(rng));
    const int slicks = region == FloorRegion::Barracks ? 2 : 1;
    for (int i = 0; i < slicks; ++i) pool(SurfaceType::Oil, std::uniform_int_distribution<int>(3, 7)(rng));

    // Braziers stand in open floor (all eight neighbours open), so they never block a route.
    auto props = props_;
    const auto occupiedByActor = [&](Position p) {
        if (p.x == player_.position().x && p.y == player_.position().y) return true;
        return std::any_of(monsters_.begin(), monsters_.end(), [&](const auto& m) { return m->position().x == p.x && m->position().y == p.y; });
    };
    int braziers = std::uniform_int_distribution<int>(3, 5)(rng);
    for (int attempt = 0; attempt < 400 && braziers > 0; ++attempt) {
        const Position p = floor[std::uniform_int_distribution<std::size_t>(0, floor.size() - 1)(rng)];
        bool open = clearOf(p, 3) && !occupiedByActor(p) && surfaceAt(p) == SurfaceType::None;
        for (int dy = -1; dy <= 1 && open; ++dy)
            for (int dx = -1; dx <= 1 && open; ++dx)
                open = map_.isWalkable(p.x + dx, p.y + dy) && propIndexAt(p.x + dx, p.y + dy) < 0;
        if (!open) continue;
        props.push_back({std::uniform_int_distribution<int>(0, 9)(rng) < 7 ? PropKind::Brazier : PropKind::ColdBrazier, p});
        map_.setTile(p.x, p.y, Tile{TileType::Wall, false, true});
        setProps(props);
        --braziers;
    }
    // About half the barrels hold oil.
    for (auto& prop : props) if (prop.kind == PropKind::Barrel && std::uniform_int_distribution<int>(0, 1)(rng)) prop.kind = PropKind::OilBarrel;
    setProps(props);
}

// Ground surfaces and fixtures, under the lighting.
void Application::renderSurfaces(std::vector<std::pair<sf::Vector2f, sf::Color>>& lights, int x0, int y0, int x1, int y1) {
    const float now = animNow();
    sf::VertexArray ground(sf::PrimitiveType::Triangles), detail(sf::PrimitiveType::Triangles);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            const auto type = surfaceAt({x, y});
            if (type == SurfaceType::None) continue;
            const auto vis = exploredMap_.at(x, y);
            if (vis == Visibility::Hidden) continue;
            const float dim = vis == Visibility::Visible ? 1.f : .45f;
            const auto at = worldToScreen(x, y);
            const sf::Vector2f c{at.x + kTile / 2, at.y + kTile / 2};
            const auto shade = [&](sf::Color col) {
                return sf::Color(static_cast<std::uint8_t>(col.r * dim), static_cast<std::uint8_t>(col.g * dim),
                                 static_cast<std::uint8_t>(col.b * dim), col.a);
            };
            sf::Color body;
            switch (type) {
                case SurfaceType::Water: case SurfaceType::Electrified: body = sf::Color(40, 90, 150, 150); break;
                case SurfaceType::Oil: body = sf::Color(18, 14, 10, 185); break;
                case SurfaceType::Ice: body = sf::Color(185, 225, 250, 165); break;
                case SurfaceType::Fire: body = sf::Color(60, 25, 10, 170); break; // scorched ground under the flames
                default: break;
            }
            // Three overlapping blobs per tile merge with the neighbours into a puddle.
            for (unsigned i = 0; i < 3; ++i) {
                const unsigned hsh = surfaceHash(x, y, i);
                const sf::Vector2f o{(unit(hsh) - .5f) * kTile * .35f, (unit(hsh >> 10) - .5f) * kTile * .35f};
                const sf::Color in = shade(body);
                sf::Color out = in; out.a = static_cast<std::uint8_t>(body.a * .55f);
                blob(ground, c + o, kTile * (.6f + .1f * unit(hsh >> 20)), in, out);
            }
            if (vis != Visibility::Visible) continue;
            if (type == SurfaceType::Water || type == SurfaceType::Electrified) {
                const float shimmer = std::sin(now * 2.f + x * .9f + y * 1.3f);
                blob(detail, {c.x - 3 + shimmer * 3, c.y - 4}, 4.f, sf::Color(180, 215, 255, 55), sf::Color(180, 215, 255, 0));
            } else if (type == SurfaceType::Oil) {
                const float sheen = .5f + .5f * std::sin(now * 1.5f + x + y);
                blob(detail, {c.x + 3, c.y - 2}, 3.f, sf::Color(120, 60, 160, static_cast<std::uint8_t>(60 * sheen)), sf::Color(40, 160, 120, 0));
            } else if (type == SurfaceType::Ice) {
                const unsigned hsh = surfaceHash(x, y, 9);
                line(detail, {c.x - 7, c.y - 4 + unit(hsh) * 6}, {c.x + 6, c.y + 3 - unit(hsh >> 8) * 6}, 1.f, sf::Color(255, 255, 255, 150));
            } else if (type == SurfaceType::Fire) {
                lights.push_back({c, sf::Color(255, 140, 60)});
            }
        }
    window_.draw(ground);
    window_.draw(detail);

    // Fixtures: braziers (iron bowls on legs) and oil barrels' dark stain.
    sf::VertexArray iron(sf::PrimitiveType::Triangles);
    for (const auto& prop : props_) {
        if (prop.kind != PropKind::Brazier && prop.kind != PropKind::ColdBrazier && prop.kind != PropKind::OilBarrel) continue;
        if (prop.pos.x < x0 || prop.pos.x >= x1 || prop.pos.y < y0 || prop.pos.y >= y1) continue;
        const auto vis = exploredMap_.at(prop.pos.x, prop.pos.y);
        if (vis == Visibility::Hidden) continue;
        const auto at = worldToScreen(prop.pos.x, prop.pos.y);
        const sf::Vector2f c{at.x + kTile / 2, at.y + kTile / 2};
        if (prop.kind == PropKind::OilBarrel) {
            blob(iron, {c.x + 5, c.y + 9}, 4.f, sf::Color(10, 8, 5, 200), sf::Color(10, 8, 5, 0));
            continue;
        }
        const sf::Color metal = vis == Visibility::Visible ? sf::Color(70, 64, 60) : sf::Color(35, 32, 30);
        line(iron, {c.x - 6, c.y + 2}, {c.x - 8, c.y + 12}, 2.f, metal);
        line(iron, {c.x + 6, c.y + 2}, {c.x + 8, c.y + 12}, 2.f, metal);
        line(iron, {c.x, c.y + 2}, {c.x, c.y + 12}, 2.f, metal);
        blob(iron, {c.x, c.y}, 9.f, sf::Color(45, 40, 38), metal);
        blob(iron, {c.x, c.y - 1}, 6.f, prop.kind == PropKind::Brazier ? sf::Color(255, 120, 40) : sf::Color(30, 28, 28),
             prop.kind == PropKind::Brazier ? sf::Color(140, 40, 10) : sf::Color(40, 38, 36));
        if (prop.kind == PropKind::Brazier && vis == Visibility::Visible) lights.push_back({{c.x, c.y - 6}, sf::Color(255, 150, 70)});
    }
    window_.draw(iron);
    for (const auto& prop : props_)
        if (prop.kind == PropKind::Brazier && exploredMap_.at(prop.pos.x, prop.pos.y) == Visibility::Visible &&
            prop.pos.x >= x0 && prop.pos.x < x1 && prop.pos.y >= y0 && prop.pos.y < y1) {
            const auto at = worldToScreen(prop.pos.x, prop.pos.y);
            const int frame = (static_cast<int>(now * 8.f) + prop.pos.x) % 3;
            sprites_.draw(window_, {"calciumtrice/tiles/dungeon_tileset_calciumtrice.png", sf::IntRect({224 + frame * 16, 512}, {16, 16})},
                          {at.x + kTile * .15f, at.y - kTile * .35f}, kTile * .7f);
        }
}

// Flames and sparks over the ground, drawn after the lighting so they glow.
void Application::renderSurfaceGlow(int x0, int y0, int x1, int y1) {
    const float now = animNow();
    sf::VertexArray glow(sf::PrimitiveType::Triangles);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            const auto type = surfaceAt({x, y});
            if ((type != SurfaceType::Fire && type != SurfaceType::Electrified) || exploredMap_.at(x, y) != Visibility::Visible) continue;
            const auto at = worldToScreen(x, y);
            const sf::Vector2f c{at.x + kTile / 2, at.y + kTile / 2};
            if (type == SurfaceType::Fire) {
                const int frame = (static_cast<int>(now * 9.f) + x * 3 + y) % 3;
                sprites_.draw(window_, {"calciumtrice/tiles/dungeon_tileset_calciumtrice.png", sf::IntRect({224 + frame * 16, 512}, {16, 16})},
                              {at.x + kTile * .1f, at.y + kTile * .05f}, kTile * .8f);
                blob(glow, c, kTile * .6f, sf::Color(255, 120, 40, 70), sf::Color(255, 80, 20, 0));
                for (int i = 0; i < 3; ++i) {
                    const float phase = std::fmod(now * 1.4f + unit(surfaceHash(x, y, 20 + i)), 1.f);
                    blob(glow, {c.x + (unit(surfaceHash(x, y, 30 + i)) - .5f) * kTile * .8f, c.y - phase * kTile * .8f}, 1.8f,
                         sf::Color(255, 220, 120, static_cast<std::uint8_t>(220 * (1 - phase))), sf::Color(255, 120, 40, 0));
                }
            } else {
                const unsigned frame = static_cast<unsigned>(now / .08f);
                if (surfaceHash(x, y, frame) % 3 == 0) {
                    const float a = unit(surfaceHash(x, y, frame + 1)) * 2 * kPi;
                    const sf::Vector2f p0{c.x + std::cos(a) * kTile * .35f, c.y + std::sin(a) * kTile * .25f};
                    line(glow, p0, {c.x - (p0.x - c.x) * .6f, c.y - (p0.y - c.y) * .6f + 3}, 1.5f, sf::Color(220, 235, 255, 230));
                }
                blob(glow, c, kTile * .5f, sf::Color(120, 160, 255, 40), sf::Color(120, 160, 255, 0));
            }
        }
    window_.draw(glow, sf::BlendAdd);
}

} // namespace engine
