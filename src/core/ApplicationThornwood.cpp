#include "core/Application.hpp"
#include "entities/MonsterFactory.hpp"

#include <algorithm>

namespace engine {

// Thornwood Hollow: thorns that cut and slow, the witches they grow from, and
// the spiders' eggs.

// Walking through thorns: you bleed and slow. The Hollow's own pass unharmed.
void Application::thornsCut(Actor& actor) {
    if (const auto* m = dynamic_cast<const Monster*>(&actor); m && thornNative(m->type())) return;
    if (&actor == &player_ && player_.talents().passiveValue(PassiveKind::Thornborn, player_.stats())) return; // Thornborn
    actor.statusEffects().apply({StatusEffectType::Bleed, 3, std::max(2, actor.statusEffects().magnitudeOf(StatusEffectType::Bleed))});
    actor.statusEffects().apply({StatusEffectType::Slowed, 2, 20});
}

// A Rot Witch's turn: thorns take root around her and creep outward, a few
// tiles a turn, within 3 tiles of where she stands.
void Application::growThorns(Position witch) {
    const auto open = [&](Position p) {
        return map_.isWalkable(p.x, p.y) && surfaceAt(p) == SurfaceType::None && !(p.x == player_.position().x && p.y == player_.position().y);
    };
    int grown = 0;
    for (int r = 1; r <= 3 && grown < 3; ++r)
        for (int dy = -r; dy <= r && grown < 3; ++dy)
            for (int dx = -r; dx <= r && grown < 3; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                const Position p{witch.x + dx, witch.y + dy};
                // Thorns spread from thorns (or from the witch herself).
                bool rooted = r == 1;
                for (const Position n : {Position{p.x + 1, p.y}, Position{p.x - 1, p.y}, Position{p.x, p.y + 1}, Position{p.x, p.y - 1}})
                    rooted = rooted || surfaceAt(n) == SurfaceType::Thorns;
                if (rooted && open(p)) { setSurface(p, SurfaceType::Thorns, 0); ++grown; }
            }
}

// A Brood Spider lays an egg sac beside it every few turns.
void Application::broodTurn(Monster& spider) {
    if (spider.type() == MonsterType::BroodSpider && ++spider.eggTimer < 5) return;
    spider.eggTimer = 0;
    const auto at = spider.position();
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            const Position p{at.x + dx, at.y + dy};
            if ((!dx && !dy) || !map_.isWalkable(p.x, p.y) || isOccupied(p, nullptr)) continue;
            const bool mother = spider.type() == MonsterType::HollowMother;
            auto made = createMonster(mother ? MonsterType::Spiderling : MonsterType::EggSac, p);
            scaleDungeonMonster(*made, floorDepth(currentFloor_));
            made->lastObservedHp = made->stats().hp;
            made->eggTimer = 4;
            made->tactics.alert = 8; made->tactics.lastKnown = player_.position(); made->voicedAlert = true;
            if (visibleTile(p)) log(mother ? "A spiderling drops from the dark." : "The spider lays a pale egg sac.");
            scheduler_.add(*made);
            monsters_.push_back(std::move(made));
            return;
        }
}

// An egg sac only waits; break it before it hatches into two spiderlings.
void Application::hatchTurn(Monster& egg) {
    if (--egg.eggTimer > 0) return;
    const auto at = egg.position();
    egg.setRewardsEligible(false);
    egg.stats().hp = 0;
    int hatched = 0;
    for (int dy = -1; dy <= 1 && hatched < 2; ++dy)
        for (int dx = -1; dx <= 1 && hatched < 2; ++dx) {
            const Position p{at.x + dx, at.y + dy};
            if (!map_.isWalkable(p.x, p.y) || (isOccupied(p, &egg) && !(p.x == at.x && p.y == at.y))) continue;
            auto made = createMonster(MonsterType::Spiderling, p);
            scaleDungeonMonster(*made, floorDepth(currentFloor_));
            made->lastObservedHp = made->stats().hp;
            made->tactics.alert = 8; made->tactics.lastKnown = player_.position(); made->voicedAlert = true;
            scheduler_.add(*made);
            monsters_.push_back(std::move(made));
            ++hatched;
        }
    if (visibleTile(at)) log("The egg sac splits, and spiderlings spill out!");
    checkAndHandleDeath(egg);
}

// Thorns you grow (Briarheart): on bare ground, water or blood, never on fire or ice.
void Application::growBriar(Position p) {
    if (!map_.isWalkable(p.x, p.y)) return;
    const auto s = surfaceAt(p);
    if (s != SurfaceType::None && s != SurfaceType::Water && s != SurfaceType::Blood && s != SurfaceType::Thorns) return;
    setSurface(p, SurfaceType::Thorns, 0);
    if (briarTiles_.size() < 200 && std::none_of(briarTiles_.begin(), briarTiles_.end(), [&](Position b) { return b.x == p.x && b.y == p.y; }))
        briarTiles_.push_back(p);
}

// Briarheart, each of your turns: Thornborn guards you among thorns; under
// Overgrowth your thorns creep outward; Heart of Briars feeds on their bleeding.
void Application::tickBriars() {
    const auto me = player_.position();
    if (const int guard = player_.talents().passiveValue(PassiveKind::Thornborn, player_.stats()); guard && surfaceAt(me) == SurfaceType::Thorns)
        player_.statusEffects().apply({StatusEffectType::Thornguard, 2, guard});
    briarTiles_.erase(std::remove_if(briarTiles_.begin(), briarTiles_.end(), [&](Position b) { return surfaceAt(b) != SurfaceType::Thorns; }), briarTiles_.end());
    if (const int reach = player_.statusEffects().magnitudeOf(StatusEffectType::Overgrowth)) {
        const auto near = [&](Position p) { return std::max(std::abs(p.x - me.x), std::abs(p.y - me.y)) <= reach; };
        const auto grown = briarTiles_;
        int added = 0;
        for (const auto& b : grown) {
            if (added >= 8 || !near(b)) continue;
            for (const Position n : {Position{b.x + 1, b.y}, Position{b.x - 1, b.y}, Position{b.x, b.y + 1}, Position{b.x, b.y - 1}}) {
                if (!near(n) || (n.x == me.x && n.y == me.y) || !map_.isWalkable(n.x, n.y) || surfaceAt(n) != SurfaceType::None) continue;
                growBriar(n); ++added;
                for (auto& m : monsters_)
                    if (!m->allied && m->stats().hp > 0 && m->position().x == n.x && m->position().y == n.y &&
                        std::find(overgrowthPinned_.begin(), overgrowthPinned_.end(), m.get()) == overgrowthPinned_.end()) {
                        m->statusEffects().apply({StatusEffectType::Pinned, 2, 0});
                        overgrowthPinned_.push_back(m.get());
                        if (visibleTile(n)) log("The briars close around ", m->name(), ".");
                    }
                break;
            }
        }
    }
    if (const int heal = player_.statusEffects().magnitudeOf(StatusEffectType::BriarHeart)) {
        int bleeding = 0;
        for (const auto& m : monsters_)
            bleeding += !m->allied && m->stats().hp > 0 && m->statusEffects().has(StatusEffectType::Bleed) && visibleTile(m->position());
        if (bleeding) player_.stats().hp = std::min(player_.stats().maxHp, player_.stats().hp + heal * bleeding);
    }
}

} // namespace engine
