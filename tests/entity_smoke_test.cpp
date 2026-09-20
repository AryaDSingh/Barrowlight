// Standalone sanity check for the Entity/Actor/Item/Feature hierarchy.
//
// Deliberately has nothing to do with SFML or the game window -- this
// build target links no rendering library at all. That's not just for
// speed: if this ever stops compiling because these headers pulled in
// <SFML/...> somewhere, that's the "Application owns all sf:: types"
// boundary (ARCHITECTURE_DECISIONS.md) being broken, caught immediately
// instead of discovered later.
//
// Not a unit test framework -- just constructs some entities, prints what
// it built, and exits 0. A real framework can replace this later if it
// ever earns its overhead (see ROADMAP.md, Prompt 4 already plans another
// one of these for the turn scheduler).

#include <iostream>
#include <memory>

#include "entities/Feature.hpp"
#include "entities/Item.hpp"
#include "entities/Monster.hpp"
#include "entities/Player.hpp"
#include "ai/NullAIBehavior.hpp"

int main() {
    using namespace engine;

    Stats playerStats;
    playerStats.maxHp = 20;
    playerStats.hp = 20;
    Player player({5, 5}, playerStats);

    Stats goblinStats;
    goblinStats.maxHp = 8;
    goblinStats.hp = 8;
    goblinStats.speed = 120; // faster than the 100 baseline
    Monster goblin("Goblin", 'g', Position{8, 5}, goblinStats,
                    std::make_unique<NullAIBehavior>());

    Item potion("Healing Potion", '!', Position{5, 6});
    Feature door("Door", '+', Position{6, 5});

    std::cout << "Entity/Actor smoke test\n";
    std::cout << "-----------------------\n";

    std::cout << player.name() << " '" << player.glyph() << "' at ("
              << player.position().x << ',' << player.position().y
              << ") hp=" << player.stats().hp << '/' << player.stats().maxHp
              << " ai=" << (player.ai() ? "yes" : "none (player-controlled)")
              << '\n';

    std::cout << goblin.name() << " '" << goblin.glyph() << "' at ("
              << goblin.position().x << ',' << goblin.position().y
              << ") hp=" << goblin.stats().hp << '/' << goblin.stats().maxHp
              << " speed=" << goblin.stats().speed
              << " ai=" << (goblin.ai() ? "yes" : "none") << '\n';

    std::cout << potion.name() << " '" << potion.glyph() << "' at ("
              << potion.position().x << ',' << potion.position().y << ")\n";

    std::cout << door.name() << " '" << door.glyph() << "' at ("
              << door.position().x << ',' << door.position().y << ")\n";

    // Prove Inventory/TalentSet/StatusEffects are actually wired into
    // Actor and usable, even though two of them are placeholder types.
    player.inventory().add(
        std::make_unique<Item>("Rusty Dagger", '/', player.position()));
    std::cout << "\nPlayer inventory size: " << player.inventory().items().size()
              << '\n';
    std::cout << std::boolalpha;
    std::cout << "Player talents empty: " << player.talents().empty() << '\n';
    std::cout << "Player status effects empty: "
              << player.statusEffects().empty() << '\n';

    std::cout << "\nAll good -- hierarchy compiles and composes correctly.\n";
    return 0;
}
