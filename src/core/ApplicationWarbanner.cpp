#include "core/Application.hpp"
#include "entities/TalentProgression.hpp"

#include <algorithm>

namespace engine {

// Warbanner's standard, and the lore that reveals deep trees.

bool Application::nearBanner(Position p) const {
    if (!banner_) return false;
    const int reach = banner_->great ? 3 : 2;
    return std::max(std::abs(p.x - banner_->at.x), std::abs(p.y - banner_->at.y)) <= reach;
}

// Planted on a tile beside `near`: the way you last moved if it's open,
// otherwise the first open tile around it.
void Application::plantBanner(int turns, bool great, Position near) {
    std::optional<Position> spot;
    const Position ahead{near.x + lastMoveDirection_.x, near.y + lastMoveDirection_.y};
    if ((lastMoveDirection_.x || lastMoveDirection_.y) && map_.isWalkable(ahead.x, ahead.y)) spot = ahead;
    for (int dy = -1; dy <= 1 && !spot; ++dy)
        for (int dx = -1; dx <= 1 && !spot; ++dx)
            if ((dx || dy) && map_.isWalkable(near.x + dx, near.y + dy)) spot = Position{near.x + dx, near.y + dy};
    banner_ = Banner{spot.value_or(near), turns, great};
    spawnVfx({Vfx::Kind::Ring, {banner_->at.x + .5f, banner_->at.y + .5f}, {banner_->at.x + .5f, banner_->at.y + .5f},
              sf::Color(200, 60, 50), 0, .5f, great ? 3.5f : 2.5f});
    log(great ? "You raise the great standard." : "You plant your standard.");
    tickBanner(); // it steadies you at once
    ++banner_->turns; // ...without costing a turn of its life
}

// As each of your turns begins: the banner steadies you while you stay near
// it, and a great standard slows the foes around it and speeds you.
void Application::tickBanner() {
    if (!banner_) return;
    if (--banner_->turns <= 0) { banner_.reset(); log("Your standard falls."); return; }
    if (nearBanner(player_.position())) {
        player_.statusEffects().apply({StatusEffectType::Guard, 2, 3});
        player_.statusEffects().remove(StatusEffectType::Slowed);
        if (const int hold = player_.talents().passiveValue(PassiveKind::HoldTheLine, player_.stats()))
            player_.statusEffects().apply({StatusEffectType::Steadfast, 2, hold});
        if (banner_->great) player_.statusEffects().apply({StatusEffectType::Hasted, 2, 25});
    }
    if (banner_->great)
        for (auto& m : monsters_)
            if (!m->allied && m->stats().hp > 0 && nearBanner(m->position())) m->statusEffects().apply({StatusEffectType::Slowed, 2, 30});
}

void Application::readLore(std::size_t index) {
    if (index >= loreDrops_.size()) return;
    const auto drop = loreDrops_[index];
    loreDrops_.erase(loreDrops_.begin() + static_cast<std::ptrdiff_t>(index));
    if (player_.knowsLore(drop.id)) return;
    player_.lore.push_back(drop.id);
    soundManager_.playFamily("levelup", 70.f);
    if (drop.id == "hollow_map") {
        log("A map scratched into a flat bone: a way down to a cloister the forest swallowed.");
        log("It leads to Thornwood Hollow. Choose it from the dungeon menu in town.");
    }
    if (drop.id == "foreman_key") {
        log("An iron key on a chain, stamped with a hammer and a flame.");
        log("It opens the Ashen Foundry. Choose it from the dungeon menu in town.");
    }
    if (drop.id == "acolyte_catechism") {
        log("A catechism of the cold, its pages stiff with frost: the winter does not end, it only waits.");
    }
    if (drop.id == "bonecaller_journal") {
        log("A journal bound in skin: how bones remember their shape, and how to ask them to take a new one.");
    }
    if (drop.id == "chorister_hymn") {
        log("Water-stained notes of a hymn no living throat could sing. Reading them, you hear the thunder in it.");
    }
    if (drop.id == "slag_formula") {
        log("A slab of cooled slag, scratched with a smith's formula: how to wake the slag, and how to make it stand.");
    }
    if (drop.id == "forgemaster_brand") {
        log("A branding iron, still hot, its mark a hammer inside a flame. The heat runs up your arm and stays.");
    }
    if (drop.id == "warlord_standard") {
        log("Torn goblin silk on a broken spear, stitched under the Warlord's mark:");
        log("\"Plant it where you stand. Let them break on it.\"");
    }
    for (std::size_t t = 0; t < kTalentTrees.size(); ++t)
        if (const auto g = deepGate(kTalentTrees[t].id); g && drop.id == g->lore)
            log("A new path shows among your talents: ", kTalentTrees[t].name, ".");
}

} // namespace engine
