#include "core/Application.hpp"
#include "entities/MonsterFactory.hpp"

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
    if (t.tree == TalentTree::Tide) return Element::None; // its chill must not freeze the water it brings
    if (t.tree == TalentTree::Stormlance) return Element::Lightning;
    if (t.tree == TalentTree::Fire || (t.onHitEffect && t.onHitEffect->type == StatusEffectType::Burn)) return Element::Fire;
    if (t.tree == TalentTree::Ice || (t.onHitEffect && t.onHitEffect->type == StatusEffectType::Chill)) return Element::Ice;
    if (t.tree == TalentTree::Lightning) return Element::Lightning;
    if (t.tree == TalentTree::Bow || t.tree == TalentTree::ShadowArcher || t.tree == TalentTree::Crossbow) return Element::Arrow;
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
            if (s == SurfaceType::Gas) { explodeGas(p); continue; }
            if (s == SurfaceType::Oil) { setSurface(p, SurfaceType::Fire, kOilFireTurns); ignited = true; }
            else if (s == SurfaceType::Ice) { setSurface(p, SurfaceType::Water, 0); steam = true; }
            else if (s == SurfaceType::Water || s == SurfaceType::Electrified) steam = true;
        } else if (element == Element::Ice) {
            if (conducts(s)) { setSurface(p, SurfaceType::Ice, 0); froze = true; }
            else if (s == SurfaceType::Fire) { setSurface(p, SurfaceType::None, 0); steam = true; }
        } else if (element == Element::Lightning && conducts(s)) {
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
            if (!conducts(surfaceAt(n))) continue;
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
bool Application::knockOver(Position tile, Position direction, const Actor* kicker) {
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
        // A boss's kick sends the coals further, in a line.
        const int reach = kicker ? 3 : 1;
        for (int step = 0; step <= reach; ++step) {
            const Position p{tile.x + direction.x * step, tile.y + direction.y * step};
            if (step && !map_.isWalkable(p.x, p.y)) break;
            setSurface(p, SurfaceType::Fire, surfaceAt(p) == SurfaceType::Oil ? kOilFireTurns : kSpilledFireTurns);
        }
        log(kicker ? kicker->name() + " kicks a brazier at you! Burning coals scatter!" : std::string("You kick over the brazier. Burning coals spill across the floor!"));
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

void Application::placeBraziers(Position centre, const std::vector<Position>& offsets) {
    auto props = props_;
    for (const auto& o : offsets) {
        const Position p{centre.x + o.x, centre.y + o.y};
        bool open = map_.isWalkable(p.x, p.y) && !isOccupied(p, nullptr);
        for (int dy = -1; dy <= 1 && open; ++dy)
            for (int dx = -1; dx <= 1 && open; ++dx) open = map_.isWalkable(p.x + dx, p.y + dy) && propIndexAt(p.x + dx, p.y + dy) < 0;
        if (!open) continue;
        props.push_back({PropKind::Brazier, p});
        setSurface(p, SurfaceType::None, 0); // a puddle under it would be left on a solid tile
        map_.setTile(p.x, p.y, Tile{TileType::Wall, false, true});
        setProps(props);
    }
}

// Boss tricks. The Warlord kicks a lit brazier beside it at the player;
// the Lich breathes out every light near it and, badly hurt, floods its
// sanctum. Returns true when the trick took the boss's turn.
bool Application::bossSurfaceAction(Monster& boss) {
    const auto at = boss.position(), you = player_.position();
    const int distance = std::max(std::abs(at.x - you.x), std::abs(at.y - you.y));
    if (boss.type() == MonsterType::TheSleeper && boss.tactics.alert > 0 && distance <= 9) {
        // Badly hurt, once: it calls the Drowned.
        if (!boss.flooded && boss.stats().hp * 2 <= boss.stats().maxHp) {
            boss.flooded = true;
            int called = 0;
            for (int dy = -2; dy <= 2 && called < 2; ++dy)
                for (int dx = -2; dx <= 2 && called < 2; ++dx) {
                    const Position p{at.x + dx, at.y + dy};
                    if ((!dx && !dy) || !map_.isWalkable(p.x, p.y) || isOccupied(p, nullptr)) continue;
                    auto drowned = createMonster(MonsterType::DrownedOne, p);
                    scaleDungeonMonster(*drowned, floorDepth(currentFloor_));
                    drowned->setRewardsEligible(false);
                    drowned->tactics.alert = 8; drowned->tactics.lastKnown = you;
                    scheduler_.add(*drowned);
                    monsters_.push_back(std::move(drowned));
                    ++called;
                }
            log(boss.name(), " calls the Drowned from the deep!");
            return true;
        }
        // A cycle: flood the room around you, then a warning, then the lightning.
        const int phase = ++boss.bossTimer % 7;
        if (phase == 3) {
            for (const Position centre : {at, you})
                for (int dy = -2; dy <= 2; ++dy)
                    for (int dx = -2; dx <= 2; ++dx) {
                        const Position p{centre.x + dx, centre.y + dy};
                        if (dx * dx + dy * dy <= 5 && map_.isWalkable(p.x, p.y) && surfaceAt(p) == SurfaceType::None) setSurface(p, SurfaceType::Water, 0);
                    }
            log(boss.name(), " stirs: black water wells up around you!");
            return true;
        }
        if (phase == 5) {
            log(boss.name(), "'s eye kindles with lightning.");
            spawnVfx({Vfx::Kind::Ring, {at.x + .5f, at.y + .5f}, {at.x + .5f, at.y + .5f}, sf::Color(170, 230, 255), 0, .8f, 6.f});
            return true;
        }
        if (phase == 6) {
            std::vector<Position> pools;
            for (int dy = -9; dy <= 9; ++dy)
                for (int dx = -9; dx <= 9; ++dx)
                    if (surfaceAt({at.x + dx, at.y + dy}) == SurfaceType::Water) pools.push_back({at.x + dx, at.y + dy});
            log("Lightning races through the water!");
            if (!pools.empty()) applyElement(Element::Lightning, pools);
            return true;
        }
    }
    if (boss.type() == MonsterType::GoblinWarlord && boss.tactics.alert > 0 && distance <= 5) {
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const Position p{at.x + dx, at.y + dy};
                const int index = propIndexAt(p.x, p.y);
                if (index < 0 || props_[static_cast<std::size_t>(index)].kind != PropKind::Brazier) continue;
                const Position toward{(you.x > p.x) - (you.x < p.x), (you.y > p.y) - (you.y < p.y)};
                if (!toward.x && !toward.y) continue;
                notifyAttack(boss, you);
                knockOver(p, toward, &boss);
                return true;
            }
    }
    if (boss.type() == MonsterType::Lich) {
        if (!boss.flooded && boss.stats().hp * 10 <= boss.stats().maxHp * 6) {
            boss.flooded = true;
            for (int dy = -3; dy <= 3; ++dy)
                for (int dx = -3; dx <= 3; ++dx)
                    if (dx * dx + dy * dy <= 10 && surfaceAt({at.x + dx, at.y + dy}) == SurfaceType::None)
                        setSurface({at.x + dx, at.y + dy}, SurfaceType::Water, 0);
            log(boss.name(), " floods its sanctum! Dark water spreads across the floor.");
            if (visibleTile(at)) spawnVfx({Vfx::Kind::Ring, {at.x + .5f, at.y + .5f}, {at.x + .5f, at.y + .5f}, sf::Color(80, 140, 220), 0, .7f, 3.5f});
        }
        if (boss.tactics.alert > 0 && distance <= 8 && ++boss.bossTimer % 7 == 0) {
            const auto close = [&](Position p) { return std::max(std::abs(p.x - at.x), std::abs(p.y - at.y)) <= 8; };
            for (const auto& front : wallTorches_)
                if (close(front) && torchLit(front.x, front.y - 1)) setTorchLit(front.x, front.y - 1, false);
            for (auto& prop : props_) if (prop.kind == PropKind::Brazier && close(prop.pos)) prop.kind = PropKind::ColdBrazier;
            lightOrbs_.erase(std::remove_if(lightOrbs_.begin(), lightOrbs_.end(), [&](const LightOrb& o) { return close(o.at); }), lightOrbs_.end());
            // Pitch black: your own light can't burn for a few turns, then returns by itself.
            player_.statusEffects().apply({StatusEffectType::Smothered, 4, 0});
            log(boss.name(), " breathes out the light. Darkness swallows you.");
            spawnVfx({Vfx::Kind::Ring, {at.x + .5f, at.y + .5f}, {at.x + .5f, at.y + .5f}, sf::Color(150, 80, 220), 0, .8f, 8.f});
            updateFieldOfView();
            return true;
        }
    }
    return false;
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
                if (surfaceAt({x + d.x, y + d.y}) == SurfaceType::Oil || surfaceAt({x + d.x, y + d.y}) == SurfaceType::Gas) spreadTo.push_back({x + d.x, y + d.y});
        }
    for (auto& orb : lightOrbs_) --orb.turns;
    lightOrbs_.erase(std::remove_if(lightOrbs_.begin(), lightOrbs_.end(), [](const LightOrb& o) { return o.turns <= 0; }), lightOrbs_.end());
    for (auto& s : surfaces_) {
        if (s.type == SurfaceType::Fire && --s.turns <= 0) s = {};
        else if (s.type == SurfaceType::Electrified && --s.turns <= 0) s = {SurfaceType::Water, 0};
        else if (s.type == SurfaceType::Acid && --s.turns <= 0) s = {};
        else if (s.type == SurfaceType::Gas && --s.turns <= 0) s = {};
        else if (s.type == SurfaceType::Ice && s.turns > 0 && --s.turns <= 0) s = {}; // a spell's ice melts; frozen water stays
    }
    for (const auto& p : spreadTo) {
        if (surfaceAt(p) == SurfaceType::Gas) explodeGas(p);
        else setSurface(p, SurfaceType::Fire, kOilFireTurns);
    }
    for (auto& trap : traps_) --trap.turns;
    traps_.erase(std::remove_if(traps_.begin(), traps_.end(), [](const Trap& t) { return t.turns <= 0; }), traps_.end());
    if (const int ward = player_.talents().passiveValue(PassiveKind::LanternWard,player_.stats()); ward && playerLightRadius() > 0) {
        const auto me = player_.position();
        for (auto& m : monsters_)
            if (!m->allied && m->stats().hp > 0 && std::max(std::abs(m->position().x - me.x), std::abs(m->position().y - me.y)) <= 1) {
                m->stats().hp -= ward; flashActor(*m);
                if (visibleTile(m->position())) log("Your lantern sears ", m->name(), " for ", ward, ".");
                checkAndHandleDeath(*m);
            }
    }
    // Raised pillars crumble in time.
    for (auto it = pillarTurns_.begin(); it != pillarTurns_.end();) {
        if (--it->second > 0) { ++it; continue; }
        const Position p{it->first.first, it->first.second};
        auto props = props_;
        props.erase(std::remove_if(props.begin(), props.end(), [&](const Prop& q) { return q.kind == PropKind::StonePillar && q.pos.x == p.x && q.pos.y == p.y; }), props.end());
        map_.setTile(p.x, p.y, Tile{TileType::Floor, true, true});
        setProps(props);
        if (visibleTile(p)) log("The stone pillar crumbles.");
        it = pillarTurns_.erase(it);
    }

    std::vector<Position> shocked;
    const auto suffer = [&](Actor& a) {
        if (surfaceAt(a.position()) == SurfaceType::Water) {
            // Tidecaller: the water mends you. Foul Water: it poisons your foes.
            if (&a == &player_) {
                if (const int mend = player_.talents().passiveValue(PassiveKind::Tidecaller,player_.stats()))
                    player_.stats().hp = std::min(player_.stats().maxHp, player_.stats().hp + mend);
            } else if (const int foul = player_.talents().passiveValue(PassiveKind::FoulWater,player_.stats())) {
                const auto* m = dynamic_cast<const Monster*>(&a);
                if (m && !m->allied) a.statusEffects().apply({StatusEffectType::Poison, 2, foul});
            }
        }
        switch (surfaceAt(a.position())) {
            case SurfaceType::Fire:
                if (&a == &player_ && patronBoon(Patron::AshSaint)) break; // the Ash Saint's own walk through fire
                if (!a.statusEffects().has(StatusEffectType::Burn) && &a == &player_) log("You are standing in flames!");
                a.statusEffects().apply({StatusEffectType::Burn, 3, std::max(2, a.statusEffects().magnitudeOf(StatusEffectType::Burn))}); // never weakens a fiercer burn
                break;
            case SurfaceType::Ice: a.statusEffects().apply({StatusEffectType::Chill, 2, 20}); break;
            case SurfaceType::Electrified: shocked.push_back(a.position()); break;
            case SurfaceType::Gas:
                a.statusEffects().apply({StatusEffectType::Poison, 2, 2});
                if (&a == &player_) log("You choke on the poison gas!");
                break;
            case SurfaceType::Acid:
                // Acid eats at whatever stands in it, and leaves it exposed.
                a.stats().hp -= 2; flashActor(a);
                a.statusEffects().apply({StatusEffectType::Marked, 3, 1});
                if (&a == &player_) log("Acid eats at you!");
                checkAndHandleDeath(a);
                break;
            default: break;
        }
    };
    // Regeneration gear: a little life every fifth action.
    if (const int regen = player_.inventory().affixTotal(BonusStat::Regeneration); regen && ++regenTicks_ % 5 == 0)
        player_.stats().hp = std::min(player_.stats().maxHp, player_.stats().hp + regen);
    suffer(player_);
    for (auto& m : monsters_) if (m->stats().hp > 0) suffer(*m);
    for (auto& m : monsters_)
        if (m->stats().hp > 0 && bleeds(m->type()) && m->statusEffects().has(StatusEffectType::Bleed) && surfaceAt(m->position()) == SurfaceType::None)
            setSurface(m->position(), SurfaceType::Blood, 0);
    // The Sleeper's boon: water and blood close your wounds.
    if (patronBoon(Patron::Sleeper) && conducts(surfaceAt(player_.position())) && player_.stats().hp > 0)
        player_.stats().hp = std::min(player_.stats().maxHp, player_.stats().hp + 2);

    // The creatures that live by light, dark, fire and water.
    for (auto& m : monsters_) {
        if (m->stats().hp <= 0 || m->allied) continue;
        const auto at = m->position();
        if (m->eventChampion == kChampionVampire && surfaceAt(at) == SurfaceType::Blood && m->stats().hp < m->stats().maxHp) {
            m->stats().hp = std::min(m->stats().maxHp, m->stats().hp + 3);
            if (visibleTile(at)) log(m->name(), " drinks from the blood at its feet.");
        }
        if (m->type() == MonsterType::DeepLurker) {
            const auto me = player_.position();
            const bool close = std::max(std::abs(at.x - me.x), std::abs(at.y - me.y)) <= 2;
            m->tactics.concealed = conducts(surfaceAt(at)) && !close;
        }
        if (m->type() == MonsterType::TheSleeper && conducts(surfaceAt(at)) && m->stats().hp < m->stats().maxHp && !patronBoon(Patron::Sleeper))
            m->stats().hp = std::min(m->stats().maxHp, m->stats().hp + 4);
        if (m->type() == MonsterType::DrownedOne) {
            if (surfaceAt(at) == SurfaceType::None) setSurface(at, SurfaceType::Water, 0);
            if (conducts(surfaceAt(at))) m->stats().hp = std::min(m->stats().maxHp, m->stats().hp + 2);
        } else if (m->type() == MonsterType::Gloomstalker && m->tactics.alert > 0 && tileLit(at)) {
            // Only in the fight: asleep or wandering, a Gloomstalker (and the
            // Vampire Lord) lurks in torchlight without burning to death.
            m->stats().hp -= 2;
            if (visibleTile(at)) log(m->name(), " shrinks from the light!");
            checkAndHandleDeath(*m);
        } else if (m->type() == MonsterType::Torchbearer) {
            // Its torch rekindles fixtures and catches oil beside it.
            std::vector<Position> near;
            bool kindling = false;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    const Position p{at.x + dx, at.y + dy};
                    near.push_back(p);
                    kindling = kindling || surfaceAt(p) == SurfaceType::Oil;
                    if (const int index = propIndexAt(p.x, p.y); index >= 0 && props_[static_cast<std::size_t>(index)].kind == PropKind::ColdBrazier) kindling = true;
                }
            for (const auto& front : wallTorches_)
                if (!torchLit(front.x, front.y - 1) && std::max(std::abs(front.x - at.x), std::abs(front.y - at.y)) <= 1) kindling = true;
            if (kindling) applyElement(Element::Fire, near);
        }
    }
    if (!shocked.empty()) shockStanding(shocked);
}

// Dangerous ground monsters won't walk into on purpose.
bool Application::hazardousSurface(Position p) const {
    const auto s = surfaceAt(p);
    return s == SurfaceType::Fire || s == SurfaceType::Electrified || s == SurfaceType::Acid || s == SurfaceType::Gas;
}

// A push, step by step. Whatever stops it hurts: walls (3), another
// creature (2 each), a brazier or oil barrel (tipped over onto the far
// side). Ground it lands on acts at once, and a chasm takes it for good.
bool Application::immovable(const Actor& actor) const {
    const auto* monster = dynamic_cast<const Monster*>(&actor);
    return monster && (monster == boss_ || isUniqueMonster(monster->type()) || monster->eventChampion);
}

void Application::enterSurface(Actor& actor, Position tile) {
    switch (surfaceAt(tile)) {
        case SurfaceType::Fire:
            actor.statusEffects().apply({StatusEffectType::Burn, 3, 2}); log(actor.name(), " lands in the flames!"); break;
        case SurfaceType::Electrified: shockStanding({tile}); break;
        case SurfaceType::Ice: actor.statusEffects().apply({StatusEffectType::Chill, 2, 20}); break;
        default: break;
    }
}

void Application::pushActor(Actor& target, Position direction, int distance, const Actor& pusher) {
    if (&target == &player_ && player_.talents().passiveValue(PassiveKind::Bedrock,player_.stats()) && player_.statusEffects().has(StatusEffectType::Guard)) {
        log("You stand your ground.");
        return;
    }
    auto* monster = dynamic_cast<Monster*>(&target);
    const bool anchored = immovable(target);
    // Hard Landing (Brawling): your collisions hit harder.
    const int hard = &pusher == &player_ ? player_.talents().passiveValue(PassiveKind::HardLanding,player_.stats()) : 0;
    for (int step = 0; step < distance && target.stats().hp > 0; ++step) {
        const auto pos = target.position();
        const Position dest{pos.x + direction.x, pos.y + direction.y};
        const bool diagonalBlocked = dest.x != pos.x && dest.y != pos.y &&
            (!map_.isWalkable(dest.x, pos.y) || !map_.isWalkable(pos.x, dest.y));
        if (map_.inBounds(dest.x, dest.y) && map_.tileAt(dest.x, dest.y).type == TileType::Chasm && !diagonalBlocked) {
            if (!monster && &target == &player_ && canFall()) {
                log("You are knocked over the edge and fall into the chasm!");
                pendingFall_ = true;
                break;
            }
            if (anchored || !monster) {
                target.stats().hp -= 5; flashActor(target);
                log(target.name(), " teeters on the edge of the chasm!");
                break;
            }
            // Gone: the XP is yours, the loot falls with it.
            const int xp = monster->xpReward();
            monster->setRewardsEligible(false);
            target.setPosition(dest);
            log(target.name(), " is pushed into the chasm and falls into darkness!");
            if (visibleTile(dest)) spawnVfx({Vfx::Kind::Puff, {dest.x + .5f, dest.y + .5f}, {dest.x + .5f, dest.y + .5f}, sf::Color(90, 90, 110), 0, .5f, .6f});
            if (xp > 0) grantXpAndAnnounce(xp);
            target.stats().hp = 0;
            checkAndHandleDeath(target);
            return;
        }
        if (const int index = propIndexAt(dest.x, dest.y); index >= 0) {
            const auto kind = props_[static_cast<std::size_t>(index)].kind;
            if (kind == PropKind::Brazier || kind == PropKind::ColdBrazier || kind == PropKind::OilBarrel) {
                log(target.name(), " crashes into the ", propName(kind), "!");
                knockOver(dest, direction, kind == PropKind::Brazier ? &target : nullptr);
                target.stats().hp -= 2 + hard; flashActor(target);
                break;
            }
        }
        if (!map_.isWalkable(dest.x, dest.y) || diagonalBlocked) {
            target.stats().hp -= 3 + hard; flashActor(target);
            log(target.name(), " slams into the wall for ", 3 + hard, "!");
            break;
        }
        if (Actor* other = actorAt(dest, &target)) {
            target.stats().hp -= 2 + hard; other->stats().hp -= 2 + hard; flashActor(target); flashActor(*other);
            log(target.name(), " crashes into ", other->name(), "! Both take ", 2 + hard, ".");
            // Domino (Hurl mastery): the one it hits goes flying too.
            if (dominoPush_ && other->stats().hp > 0 && other != &player_ && !immovable(*other)) {
                dominoPush_ = false;
                pushActor(*other, direction, 1, pusher);
                dominoPush_ = true;
            }
            checkAndHandleDeath(*other);
            break;
        }
        target.setPosition(dest);
        enterSurface(target, dest); // the ground acts at once
    }
    checkAndHandleDeath(target);
}

// Hurl: lift the enemy over your head and let the throw carry it on from
// your own tile. If the very first tile behind you is blocked, it crashes
// into that and comes down where it stood.
void Application::hurlActor(Actor& target, int distance, bool domino) {
    const Position me = player_.position(), from = target.position();
    const Position direction{me.x - from.x, me.y - from.y};
    target.statusEffects().remove(StatusEffectType::Grappled);
    log("You heave ", target.name(), " over your shoulder!");
    target.setPosition(me);
    dominoPush_ = domino;
    pushActor(target, direction, distance, player_);
    dominoPush_ = false;
    const Position at = target.position();
    if (at.x == me.x && at.y == me.y) target.setPosition(from);
    else if (visibleTile(at)) spawnVfx({Vfx::Kind::Puff, {at.x + .5f, at.y + .5f}, {at.x + .5f, at.y + .5f}, sf::Color(150, 130, 110), 0, .45f, .7f});
}

void Application::addLightOrb(Position at, int turns) {
    if (lightOrbs_.size() >= 4) lightOrbs_.erase(lightOrbs_.begin());
    lightOrbs_.push_back({at, turns});
}

// A trap springs on the foe that stepped on it.
void Application::triggerTrap(std::size_t index, Monster& victim, Position heading) {
    if (index >= traps_.size()) return;
    const Trap trap = traps_[index];
    traps_.erase(traps_.begin() + static_cast<std::ptrdiff_t>(index));
    const int bonus = player_.talents().passiveValue(PassiveKind::Trapper,player_.stats());
    const auto hurt = [&](Actor& a, int amount) { a.stats().hp -= amount; flashActor(a); };
    switch (trap.kind) {
        case 1: // snare
            hurt(victim, 4 + bonus);
            victim.statusEffects().apply({StatusEffectType::Pinned, 3, 0});
            log(victim.name(), " is caught in a snare!");
            break;
        case 2: // tripwire
            hurt(victim, 2 + bonus);
            log(victim.name(), " trips over the wire!");
            if ((heading.x || heading.y) && !immovable(victim)) pushActor(victim, heading, 2, player_);
            break;
        case 3: { // rigged charge
            std::vector<Position> blast;
            for (int dy = -trap.radius; dy <= trap.radius; ++dy)
                for (int dx = -trap.radius; dx <= trap.radius; ++dx) blast.push_back({trap.at.x + dx, trap.at.y + dy});
            log("A rigged charge goes off!");
            for (auto& m : monsters_)
                if (m->stats().hp > 0 && !m->allied && std::max(std::abs(m->position().x - trap.at.x), std::abs(m->position().y - trap.at.y)) <= trap.radius) {
                    hurt(*m, 8 + bonus); m->statusEffects().apply({StatusEffectType::Burn, 3, 2});
                }
            for (const auto& p : blast) if (map_.isWalkable(p.x, p.y) && surfaceAt(p) == SurfaceType::None) setSurface(p, SurfaceType::Fire, kSpilledFireTurns);
            applyElement(Element::Fire, blast);
            spawnVfx({Vfx::Kind::Burst, {trap.at.x + .5f, trap.at.y + .5f}, {trap.at.x + .5f, trap.at.y + .5f}, sf::Color(255, 160, 60), 0, .4f, trap.radius + .6f});
            break;
        }
        case 4: // caltrops
            hurt(victim, 2 + bonus);
            victim.statusEffects().apply({StatusEffectType::Bleed, 3, 2});
            log(victim.name(), " steps on caltrops!");
            break;
        case 5: { // booby trap: a gas cloud that catches fire
            log("A booby trap bursts into poison gas, and the gas catches fire!");
            hurt(victim, 4 + bonus);
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if (map_.isWalkable(trap.at.x + dx, trap.at.y + dy)) setSurface({trap.at.x + dx, trap.at.y + dy}, SurfaceType::Gas, 6);
            explodeGas(trap.at);
            break;
        }
        case 6: case 7: // bear trap; its third rank also bleeds
            hurt(victim, 8 + bonus);
            if (victim.statusEffects().canReceiveStun()) victim.statusEffects().apply({StatusEffectType::Stun, 2, 0});
            if (trap.kind == 7) victim.statusEffects().apply({StatusEffectType::Bleed, 3, 2});
            log("Iron jaws snap shut on ", victim.name(), "!");
            break;
        case 8: { // net trap: everything within reach is pinned
            log("A net drops!");
            for (auto& m : monsters_)
                if (m->stats().hp > 0 && !m->allied && std::max(std::abs(m->position().x - trap.at.x), std::abs(m->position().y - trap.at.y)) <= std::max(1, trap.radius))
                    m->statusEffects().apply({StatusEffectType::Pinned, 3, 0});
            break;
        }
        default: break;
    }
    // Ambusher marks what the trap caught; Incendiary sets it burning.
    if (victim.stats().hp > 0) {
        if (player_.talents().passiveValue(PassiveKind::Ambusher,player_.stats())) victim.statusEffects().apply({StatusEffectType::Marked, 3, 1});
        if (const int burn = player_.talents().passiveValue(PassiveKind::Incendiary,player_.stats())) victim.statusEffects().apply({StatusEffectType::Burn, 3, burn});
    }
    for (auto& m : monsters_) if (m->stats().hp <= 0) checkAndHandleDeath(*m);
}

void Application::renderTraps() {
    static constexpr const char* kTrapIcons[]{"", "wolf-trap", "tripwire", "time-bomb", "caltrops", "rolling-bomb", "wolf-trap", "wolf-trap", "lasso"};
    for (const auto& trap : traps_) {
        if (!visibleTile(trap.at) || trap.kind < 1 || trap.kind > 8) continue;
        const auto at = worldToScreen(trap.at.x, trap.at.y);
        const float tile = kTile;
        ui_.icon(window_, kTrapIcons[trap.kind], {{at.x + tile * .2f, at.y + tile * .2f}, {tile * .6f, tile * .6f}}, sf::Color(205, 190, 160, 200));
    }
}

// Poison gas meets fire: it bursts into flame, burning whoever stands in it.
void Application::explodeGas(Position tile) {
    setSurface(tile, SurfaceType::Fire, kSpilledFireTurns);
    if (Actor* caught = actorAt(tile, nullptr); caught && caught->stats().hp > 0) {
        caught->stats().hp -= 6; flashActor(*caught);
        caught->statusEffects().apply({StatusEffectType::Burn, 3, 2});
        checkAndHandleDeath(*caught);
    }
    if (visibleTile(tile)) {
        spawnVfx({Vfx::Kind::Burst, {tile.x + .5f, tile.y + .5f}, {tile.x + .5f, tile.y + .5f}, sf::Color(255, 170, 60), 0, .35f, .9f});
        log("The gas explodes!");
    }
}

bool Application::raisePillarAt(Position tile) {
    if (!map_.isWalkable(tile.x, tile.y) || isOccupied(tile, nullptr) || propIndexAt(tile.x, tile.y) >= 0 || (tile.x == floorExit_.x && tile.y == floorExit_.y)) return false;
    auto props = props_;
    props.push_back({PropKind::StonePillar, tile});
    setSurface(tile, SurfaceType::None, 0);
    map_.setTile(tile.x, tile.y, Tile{TileType::Wall, false, true});
    setProps(props);
    pillarTurns_[{tile.x, tile.y}] = 12;
    return true;
}

void Application::snuffLights(Position centre, int radius) {
    const auto close = [&](Position p) { return std::max(std::abs(p.x - centre.x), std::abs(p.y - centre.y)) <= radius; };
    for (const auto& front : wallTorches_)
        if (close(front) && torchLit(front.x, front.y - 1)) setTorchLit(front.x, front.y - 1, false);
    for (auto& prop : props_) if (prop.kind == PropKind::Brazier && close(prop.pos)) prop.kind = PropKind::ColdBrazier;
    lightOrbs_.erase(std::remove_if(lightOrbs_.begin(), lightOrbs_.end(), [&](const LightOrb& o) { return close(o.at); }), lightOrbs_.end());
    for (int y = centre.y - radius; y <= centre.y + radius; ++y)
        for (int x = centre.x - radius; x <= centre.x + radius; ++x)
            if (surfaceAt({x, y}) == SurfaceType::Fire) setSurface({x, y}, SurfaceType::None, 0);
    updateFieldOfView();
}

void Application::relightLights(Position centre, int radius) {
    const auto close = [&](Position p) { return std::max(std::abs(p.x - centre.x), std::abs(p.y - centre.y)) <= radius; };
    for (const auto& front : wallTorches_)
        if (close(front)) setTorchLit(front.x, front.y - 1, true);
    for (auto& prop : props_) if (prop.kind == PropKind::ColdBrazier && close(prop.pos)) prop.kind = PropKind::Brazier;
    updateFieldOfView();
}

// A grappled enemy follows you into the tile you just left; one that is no
// longer beside you has slipped free.
void Application::dragGrappled(Position vacated) {
    const Position me = player_.position();
    for (auto& m : monsters_) {
        if (m->stats().hp <= 0 || !m->statusEffects().has(StatusEffectType::Grappled)) continue;
        const Position p = m->position();
        if (std::max(std::abs(p.x - vacated.x), std::abs(p.y - vacated.y)) > 1 || (p.x == me.x && p.y == me.y)) {
            m->statusEffects().remove(StatusEffectType::Grappled);
            log(m->name(), " slips free of your grip.");
            continue;
        }
        m->setPosition(vacated);
        log("You drag ", m->name(), " along.");
        enterSurface(*m, vacated);
        checkAndHandleDeath(*m);
        break; // you only ever hold one
    }
}

// One chasm on about half the floors: grown in open floor, and undone if it
// would cut any part of the floor off from the stairs.
void Application::carveChasms(std::mt19937& rng) {
    if (std::uniform_int_distribution<int>(0, 1)(rng) == 0) return;
    const auto reachable = [&] {
        std::vector<std::uint8_t> seen(static_cast<std::size_t>(map_.width() * map_.height()), 0);
        std::queue<Position> open; open.push(floorEntrance_);
        seen[static_cast<std::size_t>(floorEntrance_.y * map_.width() + floorEntrance_.x)] = 1;
        int count = 0;
        while (!open.empty()) {
            const auto p = open.front(); open.pop(); ++count;
            for (const Position d : {Position{1, 0}, Position{-1, 0}, Position{0, 1}, Position{0, -1}}) {
                const Position n{p.x + d.x, p.y + d.y};
                const auto i = static_cast<std::size_t>(n.y * map_.width() + n.x);
                if (!map_.inBounds(n.x, n.y) || seen[i] || !map_.isWalkable(n.x, n.y)) continue;
                seen[i] = 1; open.push(n);
            }
        }
        return count;
    };
    const auto actorOn = [&](Position p) {
        return (p.x == player_.position().x && p.y == player_.position().y) ||
               std::any_of(monsters_.begin(), monsters_.end(), [&](const auto& m) { return m->position().x == p.x && m->position().y == p.y; });
    };
    const auto open8 = [&](Position p) {
        for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
            if (!map_.isWalkable(p.x + dx, p.y + dy) && map_.tileAt(p.x + dx, p.y + dy).type != TileType::Chasm) return false;
        return true;
    };
    const int before = reachable();
    for (int attempt = 0; attempt < 60; ++attempt) {
        const Position seed{std::uniform_int_distribution<int>(2, map_.width() - 3)(rng), std::uniform_int_distribution<int>(2, map_.height() - 3)(rng)};
        const auto far = [&](Position p, Position q) { return std::abs(p.x - q.x) + std::abs(p.y - q.y) > 5; };
        if (!map_.isWalkable(seed.x, seed.y) || !open8(seed) || actorOn(seed) || !far(seed, floorEntrance_) ||
            (map_.inBounds(floorExit_.x, floorExit_.y) && !far(seed, floorExit_)) ||
            (landmark_ != LandmarkKind::None && !far(seed, landmarkAltar_)) || (vaultExists_ && !far(seed, vaultCenter_))) continue;
        std::vector<Position> pit{seed};
        const int size = std::uniform_int_distribution<int>(3, 7)(rng);
        for (int tries = 0; tries < 40 && static_cast<int>(pit.size()) < size; ++tries) {
            const Position from = pit[std::uniform_int_distribution<std::size_t>(0, pit.size() - 1)(rng)];
            const Position d = std::array<Position, 4>{Position{1, 0}, Position{-1, 0}, Position{0, 1}, Position{0, -1}}[std::uniform_int_distribution<int>(0, 3)(rng)];
            const Position n{from.x + d.x, from.y + d.y};
            if (std::any_of(pit.begin(), pit.end(), [&](Position q) { return q.x == n.x && q.y == n.y; })) continue;
            if (!map_.isWalkable(n.x, n.y) || !open8(n) || actorOn(n) || propIndexAt(n.x, n.y) >= 0) continue;
            pit.push_back(n);
        }
        for (const auto& p : pit) { map_.setTile(p.x, p.y, Tile{TileType::Chasm, false, true}); setSurface(p, SurfaceType::None, 0); }
        if (reachable() == before - static_cast<int>(pit.size())) return; // nothing cut off
        for (const auto& p : pit) map_.setTile(p.x, p.y, Tile{TileType::Floor, true, true});
    }
}

// Scatter puddles, oil slicks, braziers and oil barrels over a new floor.
void Application::seedSurfaces(unsigned seed) {
    clearSurfaces();
    std::mt19937 rng(seed ^ 0x5eedu);
    const auto region = floorTheme(currentFloor_).region;
    const auto clearOf = [&](Position p, int dist) {
        const auto far = [&](Position q) { return std::abs(p.x - q.x) + std::abs(p.y - q.y) > dist; };
        return far(floorEntrance_) && (!map_.inBounds(floorExit_.x, floorExit_.y) || far(floorExit_)) &&
               (landmark_ == LandmarkKind::None || far(landmarkAltar_)) && (!vaultExists_ || far(vaultCenter_)) &&
               std::all_of(extraLandmarks_.begin(), extraLandmarks_.end(), [&](const ExtraLandmark& e) { return far(e.altar); });
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
    const bool cathedral = cathedralFloor(currentFloor_);
    const int puddles = cathedral ? (currentFloor_ % 2 ? 10 : 7) : region == FloorRegion::Crypts ? 4 : region == FloorRegion::Sanctum ? 3 : 2;
    for (int i = 0; i < puddles; ++i) pool(SurfaceType::Water, cathedral ? std::uniform_int_distribution<int>(8, 18)(rng) : std::uniform_int_distribution<int>(4, 10)(rng));
    const int slicks = cathedral ? 0 : region == FloorRegion::Barracks ? 2 : 1;
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
    carveChasms(rng);
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
            // Water and blood use real tiles (assets/sprites/dcss): a dimmed
            // shallow-water texture with foam toward dry ground, and splatters.
            if (type == SurfaceType::Water || type == SurfaceType::Electrified) {
                const bool charged = type == SurfaceType::Electrified;
                const auto wet = [&](int nx, int ny) { const auto t = surfaceAt({nx, ny}); return t == SurfaceType::Water || t == SurfaceType::Electrified || t == SurfaceType::Ice; };
                const int frame = (static_cast<int>(now * 1.2f) + x * 7 + y * 3) % 2;
                const sf::Color tint = shade(charged ? sf::Color(175, 205, 240, 225) : sf::Color(105, 135, 150, 215));
                sprites_.draw(window_, {frame ? "dcss/dngn_shallow_water2.png" : "dcss/dngn_shallow_water.png", sf::IntRect({0, 0}, {32, 32})}, at, kTile, tint);
                const sf::Color foam = shade(sf::Color(150, 170, 175, 170));
                const auto edge = [&](const char* sheet) { sprites_.draw(window_, {sheet, sf::IntRect({0, 0}, {32, 32})}, at, kTile, foam); };
                const bool n = wet(x, y - 1), sth = wet(x, y + 1), w = wet(x - 1, y), e = wet(x + 1, y);
                if (!n) edge("dcss/shallow_water_wave_N.png");
                if (!sth) edge("dcss/shallow_water_wave_S.png");
                if (!w) edge("dcss/shallow_water_wave_W.png");
                if (!e) edge("dcss/shallow_water_wave_E.png");
                if (n && w && !wet(x - 1, y - 1)) edge("dcss/shallow_water_wave_corner_NW.png");
                if (n && e && !wet(x + 1, y - 1)) edge("dcss/shallow_water_wave_corner_NE.png");
                if (sth && w && !wet(x - 1, y + 1)) edge("dcss/shallow_water_wave_corner_SW.png");
                if (sth && e && !wet(x + 1, y + 1)) edge("dcss/shallow_water_wave_corner_SE.png");
                continue;
            }
            if (type == SurfaceType::Blood) {
                static constexpr const char* kSplatters[]{"dcss/blood_red.png", "dcss/blood_red1.png", "dcss/blood_red2.png", "dcss/blood_red3.png", "dcss/blood_red4.png"};
                const unsigned hsh = surfaceHash(x, y, 4);
                sprites_.draw(window_, {kSplatters[hsh % 5], sf::IntRect({0, 0}, {32, 32})}, at, kTile, shade(sf::Color(170, 120, 120, 235)), (hsh >> 4) % 2 == 0);
                continue;
            }
            // Oil: the same splatters in grey (dcss/oil*.png), tinted black-brown, with a slick sheen.
            if (type == SurfaceType::Oil) {
                static constexpr const char* kSlicks[]{"dcss/oil.png", "dcss/oil1.png", "dcss/oil2.png", "dcss/oil3.png", "dcss/oil4.png"};
                const unsigned hsh = surfaceHash(x, y, 4);
                sprites_.draw(window_, {kSlicks[hsh % 5], sf::IntRect({0, 0}, {32, 32})}, at, kTile, shade(sf::Color(34, 28, 24, 245)), (hsh >> 4) % 2 == 0);
                if (vis == Visibility::Visible) {
                    const float sheen = .5f + .5f * std::sin(now * 1.5f + x + y);
                    blob(detail, {c.x + 3, c.y - 2}, 4.f, sf::Color(110, 80, 150, static_cast<std::uint8_t>(45 * sheen)), sf::Color(110, 80, 150, 0));
                }
                continue;
            }
            sf::Color body;
            switch (type) {
                case SurfaceType::Water: case SurfaceType::Electrified: body = sf::Color(40, 90, 150, 150); break;
                case SurfaceType::Oil: body = sf::Color(18, 14, 10, 185); break;
                case SurfaceType::Ice: body = sf::Color(185, 225, 250, 165); break;
                case SurfaceType::Fire: body = sf::Color(60, 25, 10, 170); break; // scorched ground under the flames
                case SurfaceType::Blood: body = sf::Color(105, 10, 14, 175); break;
                case SurfaceType::Acid: body = sf::Color(110, 170, 30, 165); break;
                case SurfaceType::Gas: body = sf::Color(120, 190, 60, 95); break;
                default: break;
            }
            // Ice, acid and scorched ground: the grey splatters (dcss/oil*.png), tinted.
            if (type == SurfaceType::Ice || type == SurfaceType::Acid || type == SurfaceType::Fire) {
                static constexpr const char* kSplats[]{"dcss/oil.png", "dcss/oil1.png", "dcss/oil2.png", "dcss/oil3.png", "dcss/oil4.png"};
                const unsigned hsh = surfaceHash(x, y, 4);
                const sf::Color tint = type == SurfaceType::Ice ? sf::Color(205, 236, 255, 235)
                                     : type == SurfaceType::Acid ? sf::Color(150, 225, 60, 235)
                                                                 : sf::Color(46, 34, 26, 230); // char under the flames
                sprites_.draw(window_, {kSplats[hsh % 5], sf::IntRect({0, 0}, {32, 32})}, at, kTile, shade(tint), (hsh >> 4) % 2 == 0);
            } else
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
    for (const auto& orb : lightOrbs_) {
        if (orb.at.x < x0 || orb.at.x >= x1 || orb.at.y < y0 || orb.at.y >= y1 || exploredMap_.at(orb.at.x, orb.at.y) != Visibility::Visible) continue;
        const auto at = worldToScreen(orb.at.x, orb.at.y);
        const sf::Vector2f c{at.x + kTile / 2, at.y + kTile * .1f + std::sin(now * 2.f + orb.at.x) * 3.f};
        const float fade = orb.turns <= 5 ? orb.turns / 5.f : 1.f;
        blob(glow, c, kTile * .7f, sf::Color(150, 190, 255, static_cast<std::uint8_t>(70 * fade)), sf::Color(150, 190, 255, 0));
        blob(glow, c, 4.f, sf::Color(255, 255, 255, static_cast<std::uint8_t>(240 * fade)), sf::Color(170, 200, 255, 0));
    }
    window_.draw(glow, sf::BlendAdd);
}

} // namespace engine
