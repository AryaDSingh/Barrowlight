#include "core/Application.hpp"

#include <algorithm>
#include <cmath>

#include "entities/MonsterType.hpp"
#include "world/FloorTheme.hpp"

namespace engine {

// Patron gods (entities/Patrons.hpp): favor, boons, prayers and wrath.

Patron Application::shrineGod() const { return godAt(landmarkAltar_); }

Patron Application::godAt(Position altar) const {
    // Fixed per shrine: from where it stands and how deep.
    const unsigned h = static_cast<unsigned>(altar.x * 73856093) ^ static_cast<unsigned>(altar.y * 19349663) ^
                       static_cast<unsigned>(currentFloor_ * 83492791);
    return static_cast<Patron>(1 + static_cast<int>(h % kPatronCount));
}

StrongboxVariant Application::strongboxVariant(Position altar) const {
    const unsigned h = static_cast<unsigned>(altar.x * 19349663) ^ static_cast<unsigned>(altar.y * 83492791) ^
                       static_cast<unsigned>(currentFloor_ * 73856093);
    return static_cast<StrongboxVariant>(h % 3);
}

std::string Application::landmarkLabel(LandmarkKind kind, Position altar) const {
    if (kind == LandmarkKind::Shrine) return std::string("Shrine of ") + patronInfo(godAt(altar)).name;
    if (kind == LandmarkKind::Essence) {
        const auto captive = essenceAt(altar);
        return captive.corrupted ? kWeepingCrystal : essenceInfo(captive.essence).crystal;
    }
    if (kind == LandmarkKind::Strongbox) {
        const auto v = strongboxVariant(altar);
        return v == StrongboxVariant::Armourer ? "Armourer's Strongbox" : v == StrongboxVariant::Arcanist ? "Arcanist's Strongbox" : "Gilded Strongbox";
    }
    return landmarkName(kind, floorTheme(currentFloor_).region);
}

void Application::swapLandmark(std::size_t index) {
    if (index >= extraLandmarks_.size()) return;
    auto& e = extraLandmarks_[index];
    std::swap(landmark_, e.kind);
    std::swap(landmarkAltar_, e.altar);
    std::swap(landmarkUsed_, e.used);
}

// The landmark you're nearest becomes the active one.
void Application::focusNearestLandmark() {
    const auto me = player_.position();
    const auto distance = [&](Position p) { return std::max(std::abs(p.x - me.x), std::abs(p.y - me.y)); };
    // An empty active slot is filled from the extras, never swapped into them.
    if (landmark_ == LandmarkKind::None && !extraLandmarks_.empty()) {
        const auto e = extraLandmarks_.front();
        landmark_ = e.kind; landmarkAltar_ = e.altar; landmarkUsed_ = e.used;
        extraLandmarks_.erase(extraLandmarks_.begin());
    }
    for (std::size_t i = 0; i < extraLandmarks_.size(); ++i)
        if (extraLandmarks_[i].kind != LandmarkKind::None && distance(extraLandmarks_[i].altar) < distance(landmarkAltar_)) swapLandmark(i);
}

// On arrival: everything this floor holds that you'd want to know about.
void Application::announceFloor() {
    std::vector<std::string> events;
    if (landmark_ != LandmarkKind::None && !landmarkUsed_) events.push_back(landmarkLabel(landmark_, landmarkAltar_));
    for (const auto& e : extraLandmarks_) if (!e.used) events.push_back(landmarkLabel(e.kind, e.altar));
    if (vaultExists_ && !vaultClaimed_) events.push_back("a sealed vault");
    bool patrol = false, hunters = false;
    for (const auto& m : monsters_) {
        if (m->stats().hp <= 0 || m->allied) continue;
        if (m.get() == boss_) events.push_back(m->name() + " awaits");
        else if (m->roam == Roam::Nemesis) events.push_back(m->name() + " has followed you here");
        else if (isUniqueMonster(m->type()) || m->eventChampion) events.push_back(m->name() + " lurks here");
        else if (m->roam == Roam::Champion) events.push_back(m->name() + " roams these halls");
        patrol |= m->roam == Roam::Patrol;
        hunters |= m->roam == Roam::Hunter;
    }
    if (patrol) events.push_back("a patrol walks the halls");
    if (hunters) events.push_back("something is hunting you");
    if (events.empty()) { floorNotice_.clear(); return; }
    floorNotice_ = "On this floor: ";
    for (std::size_t i = 0; i < events.size(); ++i) floorNotice_ += (i ? ", " : "") + events[i];
    floorNotice_ += ".";
    floorNoticeClock_.restart();
    log(floorNotice_);
}

std::string Application::landmarkTitle() const { return landmarkLabel(landmark_, landmarkAltar_); }

void Application::gainFavor(Patron god, int amount, const char* why) {
    if (patron() != god || god == Patron::None || !amount) return;
    const int before = player_.favor;
    player_.favor = std::clamp(player_.favor + amount, kFavorMin, kFavorMax);
    const auto& info = patronInfo(god);
    std::string name = info.short_;
    name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
    log(name, amount > 0 ? " is pleased: " : " is displeased: ", amount > 0 ? "+" : "", amount, " favor (", why, ").");
    if (before < kFavorBoon && player_.favor >= kFavorBoon) { log(name, " grants its boon: ", info.boon); updateFieldOfView(); }
    if (before < kFavorPrayer && player_.favor >= kFavorPrayer) log(name, " will answer your prayers now.");
    if (before >= kFavorBoon && player_.favor < kFavorBoon) { log(name, " withdraws its boon."); updateFieldOfView(); }
    if (player_.favor <= kFavorWrath) patronWrath(god);
}

void Application::patronWrath(Patron god) {
    const auto& info = patronInfo(god);
    log("The wrath of ", info.short_, "! ", info.wrath);
    auto& stats = player_.stats();
    switch (god) {
        case Patron::Seraph:
            player_.statusEffects().apply({StatusEffectType::Smothered, 6, 0});
            stats.hp = std::max(1, stats.hp - std::max(1, stats.maxHp / 10));
            flashActor(player_);
            break;
        case Patron::Sleeper:
            player_.statusEffects().apply({StatusEffectType::Doom, 10, std::max(1, stats.maxHp / 4)});
            break;
        case Patron::AshSaint:
            player_.statusEffects().apply({StatusEffectType::Burn, 5, 3});
            break;
        case Patron::Whisperer:
            player_.statusEffects().remove(StatusEffectType::Concealed);
            player_.statusEffects().apply({StatusEffectType::Marked, 3, 1});
            for (auto& m : monsters_) {
                const auto p = m->position(), me = player_.position();
                if (m->allied || m->stats().hp <= 0 || std::max(std::abs(p.x - me.x), std::abs(p.y - me.y)) > 12) continue;
                m->tactics.alert = 8; m->tactics.lastKnown = me;
            }
            break;
        case Patron::None: break;
    }
    if (patron() == god) player_.favor = 0;
    updateFieldOfView();
}

void Application::swearTo(Patron god) {
    if (god == Patron::None || patron() == god) return;
    if (patron() != Patron::None) {
        log("You forsake ", patronInfo(patron()).short_, ".");
        patronWrath(patron());
    }
    player_.patron = static_cast<int>(god);
    player_.favor = kSwornFavor;
    if (!player_.talents().rankOf("basic.pray")) {
        player_.talents().learnTalent(basicPray());
        auto& bar = player_.talents().hotbar();
        bar.resize(18);
        for (std::size_t slot = 0; slot < bar.size(); ++slot)
            if (bar[slot].empty()) { player_.talents().bind("basic.pray", slot); break; }
    }
    const auto& info = patronInfo(god);
    log("You swear yourself to ", info.name, ". It likes: ", info.likes, " It hates: ", info.hates);
}

void Application::pray() {
    const Patron god = patron();
    if (god == Patron::None || player_.favor < kFavorPrayer) return;
    player_.favor -= kPrayerCost;
    const auto& info = patronInfo(god);
    log("You pray to ", info.short_, ", and it answers.");
    const Position me = player_.position();
    const auto near = [&](Position p, int r) { return std::max(std::abs(p.x - me.x), std::abs(p.y - me.y)) <= r; };
    const sf::Color colour(static_cast<std::uint8_t>(info.r), static_cast<std::uint8_t>(info.g), static_cast<std::uint8_t>(info.b));
    spawnVfx({Vfx::Kind::Ring, {me.x + .5f, me.y + .5f}, {me.x + .5f, me.y + .5f}, colour, 0, .7f, 3.5f});
    auto& stats = player_.stats();
    switch (god) {
        case Patron::Seraph: {
            stats.hp = std::min(stats.maxHp, stats.hp + std::max(1, stats.maxHp * 2 / 5));
            auto& effects = player_.statusEffects().active();
            effects.erase(std::remove_if(effects.begin(), effects.end(), [](const StatusEffectInstance& e) { return isCleansable(e.type); }), effects.end());
            player_.statusEffects().apply({StatusEffectType::Guard, 3, 4});
            log("Wings of light fold around you.");
            break;
        }
        case Patron::Sleeper:
            for (int dy = -3; dy <= 3; ++dy)
                for (int dx = -3; dx <= 3; ++dx) {
                    const Position p{me.x + dx, me.y + dy};
                    if (dx * dx + dy * dy <= 10 && map_.isWalkable(p.x, p.y) && surfaceAt(p) != SurfaceType::Fire) setSurface(p, SurfaceType::Water, 0);
                }
            for (auto& m : monsters_)
                if (!m->allied && m->stats().hp > 0 && near(m->position(), 3)) m->statusEffects().apply({StatusEffectType::Chill, 3, 30});
            log("Black water wells up from below.");
            break;
        case Patron::AshSaint:
            for (auto& m : monsters_)
                if (!m->allied && m->stats().hp > 0 && near(m->position(), 3)) {
                    m->statusEffects().apply({StatusEffectType::Burn, 3, 3});
                    setSurface(m->position(), SurfaceType::Fire, kSpilledFireTurns);
                }
            log("Your enemies burst into holy flame.");
            break;
        case Patron::Whisperer:
            player_.statusEffects().apply({StatusEffectType::Concealed, 4, 3});
            for (auto& m : monsters_)
                if (!m->allied && m->stats().hp > 0 && near(m->position(), 6)) m->statusEffects().apply({StatusEffectType::Blinded, 3, 0});
            log("The walls whisper, and every eye near you goes dark.");
            break;
        case Patron::None: break;
    }
    updateFieldOfView();
}

// What your god thinks of a death.
void Application::judgeKill(const Monster& defeated) {
    if (defeated.allied || patron() == Patron::None) return;
    const Position me = player_.position();
    switch (patron()) {
        case Patron::Seraph:
            if (seesInDark(defeated.type()) || !bleeds(defeated.type())) gainFavor(Patron::Seraph, 3, "a creature of the dark slain");
            else if (playerLightRadius() > 0) gainFavor(Patron::Seraph, 1, "a kill made by your light");
            break;
        case Patron::Sleeper:
            if (conducts(surfaceAt(me))) gainFavor(Patron::Sleeper, 3, "a kill in the water");
            break;
        case Patron::AshSaint:
            if (defeated.statusEffects().has(StatusEffectType::Burn)) gainFavor(Patron::AshSaint, 3, "the burning slain");
            break;
        case Patron::Whisperer:
            if (player_.statusEffects().has(StatusEffectType::Concealed) || !tileLit(me)) gainFavor(Patron::Whisperer, 3, "a kill from the dark");
            break;
        case Patron::None: break;
    }
}

// What your god thinks of a spell or ability.
void Application::judgeCast(const Talent& talent) {
    switch (patron()) {
        case Patron::Seraph:
            if (talent.tree == TalentTree::Shadow || talent.tree == TalentTree::BloodMagic) gainFavor(Patron::Seraph, -5, "forbidden magic");
            break;
        case Patron::Sleeper:
            if (talent.tree == TalentTree::Radiance || talentElement(talent) == Element::Fire) gainFavor(Patron::Sleeper, -3, "fire and light");
            break;
        case Patron::AshSaint:
            if (talentElement(talent) == Element::Fire) gainFavor(Patron::AshSaint, 1, "you kindled flame");
            else if (talent.tree == TalentTree::Ice) gainFavor(Patron::AshSaint, -3, "you called the cold");
            break;
        case Patron::Whisperer:
            if (talent.tree == TalentTree::Radiance) gainFavor(Patron::Whisperer, -4, "you made light");
            else if (talent.tree == TalentTree::Shadow) gainFavor(Patron::Whisperer, 1, "you fed the dark");
            break;
        case Patron::None: break;
    }
}

} // namespace engine
