#include "core/Application.hpp"
#include "entities/MonsterFactory.hpp"

#include <algorithm>

namespace engine {

// Packmaster: Briar Hounds bound to you. They follow you from floor to floor,
// and grow with Blood, which their kills feed (0-10).

// A hound comes to your side (hp 0: at full life). Null if there's no room.
Monster* Application::spawnHound(int hp) {
    const auto me = player_.position();
    for (int r = 1; r <= 3; ++r)
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                const Position p{me.x + dx, me.y + dy};
                if (std::max(std::abs(dx), std::abs(dy)) != r || !map_.isWalkable(p.x, p.y) || isOccupied(p, nullptr)) continue;
                auto made = createMonster(MonsterType::BriarHound, p);
                configureMinion(*made, 1, 0);
                made->setName("Your Hound");
                auto* hound = made.get();
                scheduler_.add(*hound);
                monsters_.push_back(std::move(made));
                refreshPack();
                hound->stats().hp = hp > 0 ? std::min(hp, hound->stats().maxHp) : hound->stats().maxHp;
                hound->lastObservedHp = hound->stats().hp;
                return hound;
            }
    return nullptr;
}

// A hound's strength: your level and Dexterity, and +1 Strength and +3 life
// for every point of Blood.
void Application::refreshPack() {
    const int blood = player_.packBlood;
    for (auto& m : monsters_) {
        if (!packBeast(*m) || m->stats().hp <= 0) continue;
        auto& s = m->stats();
        const bool maw = isThornmaw(*m); // Thornmaw grows with your level
        const int maxHp = (maw ? 30 + 8 * player_.level() : 24 + 4 * player_.level()) + 3 * blood;
        if (maxHp > s.maxHp) s.hp += maxHp - s.maxHp;
        s.maxHp = maxHp; s.hp = std::min(s.hp, s.maxHp);
        s.strength = (maw ? 6 + player_.level() / 2 : 6 + player_.stats().dexterity / 5) + blood;
        s.dexterity = 14; s.speed = 125;
        const char* name = maw ? "Thornmaw" : "Your Hound";
        if (m->name() != name) m->setName(name); // a reloaded hound keeps its name
    }
}

// Each of your turns: your quarry, if it's gone, is forgotten; Alpha's Howl
// drives your beasts and shakes the foes beside you.
Monster* Application::thornmaw() {
    for (auto& m : monsters_) if (isThornmaw(*m) && m->stats().hp > 0) return m.get();
    return nullptr;
}

// Thornmaw comes to your side (Beastwarden).
void Application::spawnThornmaw() {
    const auto me = player_.position();
    for (int r = 1; r <= 3; ++r)
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                const Position p{me.x + dx, me.y + dy};
                if (std::max(std::abs(dx), std::abs(dy)) != r || !map_.isWalkable(p.x, p.y) || isOccupied(p, nullptr)) continue;
                auto made = createMonster(MonsterType::BriarHound, p);
                configureMinion(*made, 3, 0);
                made->setName("Thornmaw");
                auto* maw = made.get();
                scheduler_.add(*maw);
                monsters_.push_back(std::move(made));
                refreshPack();
                maw->stats().hp = maw->stats().maxHp; maw->lastObservedHp = maw->stats().hp;
                return;
            }
}

// Contagion (Plaguebringer): each turn, the plagued pass it to the foes beside them.
void Application::tickContagion() {
    if (!player_.talents().passiveValue(PassiveKind::Contagion, player_.stats())) return;
    std::vector<std::pair<Monster*, int>> caught;
    for (const auto& carrier : monsters_) {
        if (carrier->allied || carrier->stats().hp <= 0 || !carrier->statusEffects().has(StatusEffectType::Plague)) continue;
        const int plague = carrier->statusEffects().magnitudeOf(StatusEffectType::Plague);
        for (auto& m : monsters_)
            if (m != carrier && !m->allied && m->stats().hp > 0 && !m->statusEffects().has(StatusEffectType::Plague) &&
                std::max(std::abs(m->position().x - carrier->position().x), std::abs(m->position().y - carrier->position().y)) <= 1)
                caught.push_back({m.get(), plague});
    }
    for (const auto& [m, plague] : caught)
        if (!m->statusEffects().has(StatusEffectType::Plague)) m->statusEffects().apply({StatusEffectType::Plague, 5, plague});
}

void Application::tickPack() {
    tickContagion();
    if (wardenFoe_ && std::none_of(monsters_.begin(), monsters_.end(), [&](const auto& m) { return m.get() == wardenFoe_ && m->stats().hp > 0; }))
        wardenFoe_ = nullptr, wardenPin_ = false;
    // Thornmaw walks with you, unless it fell on this floor.
    if (mode_ == GameMode::Playing && !thornmawDown_ && !thornmaw() && player_.talents().passiveValue(PassiveKind::Thornmaw, player_.stats()))
        spawnThornmaw();
    if (player_.statusEffects().has(StatusEffectType::CallOfTheWild)) {
        player_.statusEffects().apply({StatusEffectType::Hasted, 2, 30});
        if (auto* maw = thornmaw()) maw->statusEffects().apply({StatusEffectType::Hasted, 2, 30});
    }
    if (quarry_ && std::none_of(monsters_.begin(), monsters_.end(), [&](const auto& m) { return m.get() == quarry_ && m->stats().hp > 0; }))
        quarry_ = nullptr, quarryPin_ = false;
    refreshPack();
    if (player_.statusEffects().has(StatusEffectType::AlphasHowl)) {
        const auto me = player_.position();
        for (auto& m : monsters_) {
            if (m->stats().hp <= 0) continue;
            if (packBeast(*m)) m->statusEffects().apply({StatusEffectType::Hasted, 2, 30});
            else if (!m->allied && std::max(std::abs(m->position().x - me.x), std::abs(m->position().y - me.y)) <= 1)
                m->statusEffects().apply({StatusEffectType::Shaken, 2, 25});
        }
    }
}

// Leaving a floor: your living hounds wait to follow you down.
void Application::stablePack() {
    for (const auto& m : monsters_)
        if (packBeast(*m) && !isThornmaw(*m) && m->stats().hp > 0 && player_.packHp.size() < 2) player_.packHp.push_back(m->stats().hp);
}

// Arriving on a floor: they come to your side.
void Application::callPackBack() {
    thornmawDown_ = false; // a new floor: Thornmaw finds you again
    if (player_.packHp.empty()) return;
    const auto waiting = std::move(player_.packHp);
    player_.packHp.clear();
    int came = 0;
    for (const int hp : waiting) came += spawnHound(hp) != nullptr;
    if (came) log(came > 1 ? "Your hounds pad in after you." : "Your hound pads in after you.");
}

// A kill by your beasts feeds your Blood; Blooded doubles it.
void Application::packKill() {
    const int gain = player_.talents().passiveValue(PassiveKind::Blooded, player_.stats()) ? 2 : 1;
    player_.packBlood = std::min(10, player_.packBlood + gain);
    refreshPack();
}

// One of your beasts falls: Blood halves (Blooded: you lose only 2).
void Application::packLoss() {
    player_.packBlood = player_.talents().passiveValue(PassiveKind::Blooded, player_.stats()) ? std::max(0, player_.packBlood - 2) : player_.packBlood / 2;
    log("Your hound falls.");
}

} // namespace engine
