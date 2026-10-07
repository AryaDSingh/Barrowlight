#include "core/Application.hpp"
#include "entities/MonsterFactory.hpp"
#include "entities/TalentEffects.hpp"

#include <algorithm>

namespace engine {

// The Ashen Foundry: heat, the fires the imps fan, and what its monsters
// leave behind when they die.

namespace {
constexpr int kMaxHeat = 15, kBurningHeat = 10;
}

void Application::addHeat(int amount) {
    const int heat = std::clamp(player_.statusEffects().magnitudeOf(StatusEffectType::Heat) + amount, 0, kMaxHeat);
    player_.statusEffects().remove(StatusEffectType::Heat);
    if (heat > 0) player_.statusEffects().apply({StatusEffectType::Heat, 10000, heat});
    if (amount > 0) heatFresh_ = true;
}

// As your turn begins: furnaces in the 8 tiles around you, or fire under your
// feet, heat you; away from them you cool; water quenches you at once. Too
// hot, and it burns.
void Application::tickHeat() {
    const auto me = player_.position();
    const int heat = player_.statusEffects().magnitudeOf(StatusEffectType::Heat);
    if (surfaceAt(me) == SurfaceType::Water) {
        if (heat > 0) { addHeat(-heat); log("The water hisses as you cool."); }
        return;
    }
    bool hot = surfaceAt(me) == SurfaceType::Fire;
    for (const auto& prop : props_)
        if (prop.kind == PropKind::Furnace && std::max(std::abs(prop.pos.x - me.x), std::abs(prop.pos.y - me.y)) <= 1) hot = true;
    const auto held = std::find_if(player_.statusEffects().active().begin(), player_.statusEffects().active().end(),
                                    [](const StatusEffectInstance& e) { return e.type == StatusEffectType::Forgeheart; });
    const bool holding = held != player_.statusEffects().active().end();
    const int heldTurns = holding ? held->turnsRemaining : 0; // read now: changing Heat rewrites the list
    const bool fresh = heatFresh_;
    heatFresh_ = false;
    const bool anvil = player_.statusEffects().has(StatusEffectType::Anvil);
    if (hot) { addHeat(1); heatFresh_ = false; } // the furnace's heat follows the usual rule
    else if (heat > 0 && !holding && !fresh && !anvil) addHeat(-1);
    if (anvil) { addHeat(2); heatFresh_ = false; } // Anvil Stance: the stance itself heats you
    // Forgeheart's last turn: the heat it held comes out all at once.
    if (holding && heldTurns <= 1) {
        const int vent = player_.statusEffects().magnitudeOf(StatusEffectType::Heat);
        addHeat(-vent);
        if (vent) {
            log("Your forgeheart bursts!");
            for (auto& m : monsters_)
                if (!m->allied && m->stats().hp > 0 && std::max(std::abs(m->position().x - me.x), std::abs(m->position().y - me.y)) <= 2) {
                    m->stats().hp -= 2 * vent; flashActor(*m); checkAndHandleDeath(*m);
                }
            for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx)
                if ((dx || dy) && map_.isWalkable(me.x + dx, me.y + dy)) setSurface({me.x + dx, me.y + dy}, SurfaceType::Fire, kSpilledFireTurns);
            removeDeadMonsters();
        }
    }
    if (player_.statusEffects().magnitudeOf(StatusEffectType::Heat) >= kBurningHeat &&
        !player_.talents().passiveValue(PassiveKind::HeatSink, player_.stats()) &&
        !player_.talents().passiveValue(PassiveKind::Overheat, player_.stats())) {
        player_.stats().hp -= 2;
        flashActor(player_);
        harmSource_ = "the heat";
        log("The heat sears you.");
        checkAndHandleDeath(player_);
        harmSource_.clear();
    }
}

// A Bellows Imp's turn: every fire within three tiles of it creeps a tile.
void Application::fanFires(Position imp) {
    std::vector<Position> burning;
    for (int dy = -3; dy <= 3; ++dy)
        for (int dx = -3; dx <= 3; ++dx)
            if (surfaceAt({imp.x + dx, imp.y + dy}) == SurfaceType::Fire) burning.push_back({imp.x + dx, imp.y + dy});
    int spread = 0;
    for (const auto& p : burning) {
        for (const Position n : {Position{p.x + 1, p.y}, Position{p.x - 1, p.y}, Position{p.x, p.y + 1}, Position{p.x, p.y - 1}}) {
            if (!map_.isWalkable(n.x, n.y) || surfaceAt(n) == SurfaceType::Fire || surfaceAt(n) == SurfaceType::Water) continue;
            setSurface(n, SurfaceType::Fire, kSpilledFireTurns);
            if (++spread >= 3) return;
            break;
        }
    }
}

// What the Foundry's monsters leave behind: a Slag Golem splits into two
// Slaglings, a Slagling leaves the ground burning, and Grik drops the
// Foreman's key. (Called for the Forgemaster at half life, too: its slaglings.)
void Application::foundryDeath(Monster& monster) {
    const auto at = monster.position();
    const auto spawnSlaglings = [&](int count, Position near) {
        for (int i = 0; i < count; ++i) {
            std::optional<Position> spot;
            for (int r = 1; r <= 3 && !spot; ++r)
                for (int dy = -r; dy <= r && !spot; ++dy)
                    for (int dx = -r; dx <= r && !spot; ++dx)
                        if (std::max(std::abs(dx), std::abs(dy)) == r && map_.isWalkable(near.x + dx, near.y + dy) && !isOccupied({near.x + dx, near.y + dy}, nullptr))
                            spot = Position{near.x + dx, near.y + dy};
            if (!spot) return;
            auto slag = createMonster(MonsterType::Slagling, *spot);
            scaleDungeonMonster(*slag, floorDepth(currentFloor_));
            slag->lastObservedHp = slag->stats().hp;
            slag->tactics.alert = 8; slag->tactics.lastKnown = player_.position(); slag->voicedAlert = true;
            scheduler_.add(*slag);
            monsters_.push_back(std::move(slag));
        }
    };
    if (monster.type() == MonsterType::SlagGolem && monster.stats().hp <= 0) {
        log("The golem cracks apart, and its molten heart crawls out in pieces!");
        spawnSlaglings(2, at);
    }
    if (monster.type() == MonsterType::Slagling && monster.stats().hp <= 0) setSurface(at, SurfaceType::Fire, kSpilledFireTurns);
    if (monster.type() == MonsterType::Forgemaster && monster.stats().hp > 0) {
        // Out of the two furnaces nearest you.
        std::vector<Position> furnaces;
        for (const auto& prop : props_) if (prop.kind == PropKind::Furnace) furnaces.push_back(prop.pos);
        const auto me = player_.position();
        std::sort(furnaces.begin(), furnaces.end(), [&](Position a, Position b) {
            return std::abs(a.x - me.x) + std::abs(a.y - me.y) < std::abs(b.x - me.x) + std::abs(b.y - me.y); });
        log("The Forgemaster roars, and the furnaces spill out their slag!");
        if (furnaces.empty()) spawnSlaglings(2, at);
        for (std::size_t i = 0; i < std::min<std::size_t>(2, furnaces.size()); ++i) spawnSlaglings(1, furnaces[i]);
    }
    // Deep-tree lore carried by the monsters whose powers it teaches: the first one slain drops it.
    const auto carries = [&](MonsterType type, const char* lore, const char* line) {
        if (monster.type() != type || monster.stats().hp > 0 || player_.knowsLore(lore) ||
            std::any_of(loreDrops_.begin(), loreDrops_.end(), [&](const LoreDrop& d) { return d.id == lore; })) return;
        loreDrops_.push_back({at, lore});
        log(line);
    };
    carries(MonsterType::DrownedChorister, "chorister_hymn", "A sodden hymn-sheet drifts down where the Chorister fell.");
    carries(MonsterType::Bonecaller, "bonecaller_journal", "A Bonecaller's journal slips from its robes.");
    carries(MonsterType::FrostAcolyte, "acolyte_catechism", "A frost-rimed catechism falls from the Acolyte's hands.");
    carries(MonsterType::OssuaryWarden, "hollow_map", "A map, drawn on bone, slips from the Warden's ashes.");
    carries(MonsterType::RotWitch, "witch_seed", "A black seed rolls from the witch's hand, still warm.");
    carries(MonsterType::BriarHound, "hound_collar", "A braided collar slips from the hound's neck.");
    carries(MonsterType::RimeWight, "wight_oath", "A shield of black ice falls from the wight's arm, an oath cut into it.");
    carries(MonsterType::FrozenThrall, "thrall_binding", "Among the shards lies a ring of black iron.");
    // A frozen thrall shatters: ice across the 8 tiles around it, and the shards cut whoever stands there.
    if (monster.type() == MonsterType::FrozenThrall) {
        const auto at = monster.position();
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if (map_.isWalkable(at.x + dx, at.y + dy)) setSurface({at.x + dx, at.y + dy}, SurfaceType::Ice, 12);
        if (visibleTile(at)) log(monster.name(), " shatters!");
        const auto cut = [&](Actor& a) {
            if (&a == &monster || a.stats().hp <= 0 || std::max(std::abs(a.position().x - at.x), std::abs(a.position().y - at.y)) > 1) return;
            a.stats().hp -= 6; flashActor(a);
        };
        cut(player_);
        if (player_.stats().hp <= 0) { harmSource_ = "a shattering thrall"; checkAndHandleDeath(player_); harmSource_.clear(); }
        for (auto& m : monsters_) if (m.get() != &monster && m->stats().hp > 0) { cut(*m); if (m->stats().hp <= 0) checkAndHandleDeath(*m); }
    }
    if (monster.type() == MonsterType::GoblinCaptain && monster.stats().hp <= 0 && !player_.knowsLore("foreman_key") &&
        std::none_of(loreDrops_.begin(), loreDrops_.end(), [](const LoreDrop& d) { return d.id == "foreman_key"; })) {
        loreDrops_.push_back({at, "foreman_key"});
        log("An iron key clatters from Grik's belt.");
    }
}

// Tempest: the storm over you strikes the nearest foe within 3 tiles; and the
// Eye of the Storm quickens you on electrified ground.
void Application::tickStormcall() {
    const auto me = player_.position();
    if (const int eye = player_.talents().passiveValue(PassiveKind::EyeOfTheStorm, player_.stats()); eye && surfaceAt(me) == SurfaceType::Electrified)
        player_.statusEffects().apply({StatusEffectType::Hasted, 2, eye});
    tickBriars();
    tickPack();
    // Rimeheart: ice that creeps; and inside Winter's Heart, nothing reaches you.
    if (!frostCreep_.empty()) {
        auto creeping = std::move(frostCreep_); frostCreep_.clear();
        for (const auto& [p, turns] : creeping) {
            for (const Position n : {Position{p.x + 1, p.y}, Position{p.x - 1, p.y}, Position{p.x, p.y + 1}, Position{p.x, p.y - 1}})
                if (map_.isWalkable(n.x, n.y) && surfaceAt(n) != SurfaceType::Ice && !(n.x == me.x && n.y == me.y)) { freezeGround(n, turns - 1); break; }
        }
    }
    const auto ice = std::find_if(player_.statusEffects().active().begin(), player_.statusEffects().active().end(),
                                  [](const StatusEffectInstance& e) { return e.type == StatusEffectType::Encased; });
    if (ice != player_.statusEffects().active().end()) {
        const int turnsLeft = ice->turnsRemaining, reach = ice->magnitude;
        player_.stats().hp = std::max(player_.stats().hp, encasedHp_);
        for (const auto type : {StatusEffectType::Poison, StatusEffectType::Burn, StatusEffectType::Bleed, StatusEffectType::Chill, StatusEffectType::Slowed,
                                StatusEffectType::Stun, StatusEffectType::Doom, StatusEffectType::Plague, StatusEffectType::Marked, StatusEffectType::Shock})
            player_.statusEffects().remove(type);
        if (turnsLeft <= 1) { player_.statusEffects().remove(StatusEffectType::Encased); winterBurst(reach, reach > 2); }
    }
    // Bonewright: the bone storm cuts the foes beside you; Bone Lord hastens your minions.
    if (const int shards = player_.statusEffects().magnitudeOf(StatusEffectType::BoneStorm)) {
        for (auto& m : monsters_)
            if (!m->allied && m->stats().hp > 0 && std::max(std::abs(m->position().x - me.x), std::abs(m->position().y - me.y)) <= 1) {
                m->stats().hp -= shards; m->statusEffects().apply({StatusEffectType::Bleed, 3, 2}); flashActor(*m); checkAndHandleDeath(*m);
                combatThisTurn_ = true;
            }
        removeDeadMonsters();
    }
    if (player_.statusEffects().has(StatusEffectType::BoneLord))
        for (auto& m : monsters_) if (m->allied && m->stats().hp > 0) m->statusEffects().apply({StatusEffectType::Hasted, 2, 25});
    // Grown Guard: your bone guardian is as strong as your mind, kept current
    // as your Intelligence changes (bounded by it, never by kills).
    for (auto& m : monsters_) {
        if (!m->allied || m->stats().hp <= 0 || m->type() != MonsterType::SkeletonGuard) continue;
        const int intelligence = player_.stats().intelligence;
        const bool grown = player_.talents().passiveValue(PassiveKind::GrownGuard, player_.stats()) > 0;
        const int maxHp = 12 + 4 * m->summonRank + intelligence + 10 + (grown ? 2 * (intelligence / 5) : 0);
        m->stats().strength = 2 + m->summonRank + intelligence / 5 + 2 + (grown ? intelligence / 5 : 0);
        if (m->stats().maxHp != maxHp) {
            m->stats().hp = std::max(1, m->stats().hp + maxHp - m->stats().maxHp);
            m->stats().maxHp = maxHp;
        }
    }
    if (!player_.statusEffects().has(StatusEffectType::Stormcall)) return;
    Monster* nearest = nullptr; int best = 100;
    for (auto& m : monsters_) {
        if (m->allied || m->stats().hp <= 0 || !visibleTile(m->position())) continue;
        const int d = std::max(std::abs(m->position().x - me.x), std::abs(m->position().y - me.y));
        if (d <= 3 && d < best) { best = d; nearest = m.get(); }
    }
    if (!nearest) return;
    Talent bolt; bolt.name = "Stormcall"; bolt.id = "tempest.stormcall.strike"; bolt.tree = TalentTree::Tempest;
    bolt.effectKind = TalentEffectKind::Damage; bolt.power = 4; bolt.scalingStat = ScalingStat::Intelligence;
    combatThisTurn_ = true;
    spawnVfx({Vfx::Kind::Lightning, {me.x + .5f, me.y - 1.f}, {nearest->position().x + .5f, nearest->position().y + .5f}, sf::Color(170, 200, 255), 0, .25f, 1.f});
    if (applyTalentDamage(bolt, player_, *nearest)) {
        flashActor(*nearest);
        nearest->statusEffects().apply({StatusEffectType::Shock, 3, 0});
        soundManager_.playHit(HitSound::Lightning, lastHitWasCritical(), bleeds(nearest->type()));
    }
    checkAndHandleDeath(*nearest);
    removeDeadMonsters();
}

// Ice you make (Rimeheart); with Creeping Frost it spreads a tile a turn.
void Application::freezeGround(Position p, int creep) {
    if (!map_.isWalkable(p.x, p.y)) return;
    setSurface(p, SurfaceType::Ice, 12);
    if (creep > 0 && frostCreep_.size() < 40) frostCreep_.push_back({p, creep});
}

// Winter's Heart bursts: frost through everything near you, and ice underfoot.
void Application::winterBurst(int radius, bool freeze) {
    const auto me = player_.position();
    Talent burst; burst.name = "Winter's Heart"; burst.id = "rimeheart.heart.burst"; burst.tree = TalentTree::Rimeheart;
    burst.effectKind = TalentEffectKind::Damage; burst.power = 10; burst.scalingStat = ScalingStat::Intelligence;
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx)
            if ((dx || dy) && map_.isWalkable(me.x + dx, me.y + dy)) setSurface({me.x + dx, me.y + dy}, SurfaceType::Ice, 12);
    for (auto& m : monsters_) {
        if (m->allied || m->stats().hp <= 0 || std::max(std::abs(m->position().x - me.x), std::abs(m->position().y - me.y)) > radius) continue;
        if (applyTalentDamage(burst, player_, *m)) {
            flashActor(*m);
            m->statusEffects().apply({StatusEffectType::Chill, 3, 30});
            if (freeze && m->stats().hp > 0 && m->statusEffects().canReceiveStun()) m->statusEffects().apply({StatusEffectType::Stun, 1, 0});
        }
        checkAndHandleDeath(*m);
    }
    combatThisTurn_ = true;
    log("The ice around you bursts!");
    removeDeadMonsters();
}

} // namespace engine
