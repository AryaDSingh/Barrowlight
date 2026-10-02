cmake_minimum_required(VERSION 3.28)
project(RoguelikeEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

include(FetchContent)

# We only needed windowing + 2D rendering at first. Network stays off --
# this project has no multiplayer/networking need. Audio (Prompt 25) is
# now on: sound effects, generated as plain WAV (see assets/sounds/),
# don't need the FLAC/Vorbis compressed-format decoders SFML's audio
# module can optionally pull in, so this shouldn't meaningfully slow the
# build even though it's now enabled.
set(SFML_BUILD_AUDIO ON CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)

# SFML's own default for this varies by platform -- system libraries on
# Linux, its own fetched/bundled ones on Windows/macOS (see SFML's
# CMakeLists.txt). Forced OFF explicitly so this project's own "one
# cmake command builds everything, no manual dependency installation"
# story (true since Prompt 2) stays true on every platform, Linux
# included, rather than only working by accident on whichever platform
# happens to default the other way. SFML fetches and builds its own
# Vorbis/FLAC from source the same way it fetches itself -- verified by
# actually removing every system audio library from the build
# environment and confirming a full rebuild still succeeds, not just by
# reading what this flag is supposed to do.
set(SFML_USE_SYSTEM_DEPS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.1.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SFML)

# ---------------------------------------------------------------------------
# Entity/Actor headers
# ---------------------------------------------------------------------------
# Header-only for now -- every class here is intentionally trivial (see
# ARCHITECTURE_DECISIONS.md / ROADMAP.md), so there's no real implementation
# to hide behind a .cpp file yet. Listed explicitly (rather than only
# #include-d) so they show up as real files in IDEs like Visual Studio's
# Solution Explorer instead of being invisible until referenced.

set(COMMON_HEADERS
    src/core/Position.hpp
)

set(ENTITY_HEADERS
    src/entities/Entity.hpp
    src/entities/Item.hpp
    src/entities/LootGenerator.hpp
    src/entities/EnemyIntent.hpp
    src/entities/Stealth.hpp
    src/entities/TalentCatalog.hpp
    src/entities/TalentProgression.hpp
    src/entities/Feature.hpp
    src/entities/Stats.hpp
    src/entities/AttributeFormulas.hpp
    src/entities/AIBehavior.hpp
    src/entities/Inventory.hpp
    src/entities/Talent.hpp
    src/entities/TalentSet.hpp
    src/entities/TalentEffects.hpp
    src/entities/SpellbladeTalents.hpp
    src/entities/StatusEffects.hpp
    src/entities/StatusEffectLogic.hpp
    src/entities/MonsterAttackProfile.hpp
    src/entities/MonsterType.hpp
    src/entities/MonsterInspection.hpp
    src/entities/MonsterTier.hpp
    src/entities/HybridSpec.hpp
    src/entities/MonsterFactory.hpp
    src/entities/PlayerClass.hpp
    src/entities/PlayerClassFactory.hpp
    src/entities/PlayerLeveling.hpp
    src/entities/WarriorTalents.hpp
    src/entities/ThiefTalents.hpp
    src/entities/MageTalents.hpp
    src/entities/Actor.hpp
    src/entities/Player.hpp
    src/entities/Monster.hpp
)

set(CORE_HEADERS
    src/core/Application.hpp
    src/core/PlayLayout.hpp
    src/core/TurnScheduler.hpp
    src/core/SaveGame.hpp
    src/core/SoundManager.hpp
    src/core/SpriteAtlas.hpp
    src/core/ScreenLayout.hpp
    src/core/UiKit.hpp
    src/core/GameIcons.hpp
)

set(WORLD_HEADERS
    src/world/FloorTheme.hpp
    src/world/EncounterPlan.hpp
    src/world/Tile.hpp
    src/world/Map.hpp
    src/world/TalentTargeting.hpp
    src/world/FieldOfView.hpp
    src/world/ExploredMap.hpp
    src/world/Pathfinder.hpp
    src/world/DungeonGenerator.hpp
    src/world/DungeonModules.hpp
    src/world/ProceduralModules.hpp
    src/world/Landmark.hpp
    src/world/Props.hpp
)

set(AI_HEADERS
    src/ai/NullAIBehavior.hpp
    src/ai/AIUtils.hpp
    src/ai/Chaser.hpp
    src/ai/Kiter.hpp
    src/ai/Support.hpp
    src/ai/AoEBomber.hpp
    src/ai/BossBehavior.hpp
    src/ai/LichBehavior.hpp
)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
# Prompt 5 is the first point where the game actually uses TurnScheduler and
# Map for real (previously TurnScheduler.cpp only compiled into its own
# test), so both now compile into the roguelike target too.

add_executable(roguelike
    src/main.cpp
    src/core/Application.cpp
    src/core/ApplicationTargeting.cpp
    src/core/ApplicationInventory.cpp
    src/core/ApplicationTravel.cpp
    src/core/ApplicationExplore.cpp
    src/core/ApplicationLoot.cpp
    src/core/ApplicationLandmark.cpp
    src/core/ApplicationAnimation.cpp
    src/core/ApplicationTown.cpp
    src/core/ApplicationAscendancy.cpp
    src/core/ApplicationHidden.cpp
    src/core/ApplicationTalents.cpp
    src/world/TalentTargeting.cpp
    src/entities/MonsterInspection.cpp
    src/core/TurnScheduler.cpp
    src/world/Map.cpp
    src/world/FieldOfView.cpp
    src/world/ExploredMap.cpp
    src/world/Pathfinder.cpp
    src/world/DungeonGenerator.cpp
    src/world/DungeonModules.cpp
    src/world/ProceduralModules.cpp
    src/ai/Chaser.cpp
    src/ai/Kiter.cpp
    src/ai/Support.cpp
    src/ai/AoEBomber.cpp
    src/ai/BossBehavior.cpp
    src/ai/LichBehavior.cpp
    src/entities/TalentSet.cpp
    src/entities/TalentEffects.cpp
    src/entities/AttributeFormulas.cpp
    src/entities/SpellbladeTalents.cpp
    src/entities/WarriorTalents.cpp
    src/entities/MageTalents.cpp
    src/entities/ThiefTalents.cpp
    src/entities/PlayerClassFactory.cpp
    src/entities/PlayerLeveling.cpp
    src/entities/HybridSpec.cpp
    src/entities/StatusEffects.cpp
    src/entities/StatusEffectLogic.cpp
    src/entities/MonsterFactory.cpp
    src/entities/MonsterTier.cpp
    src/core/SaveGame.cpp
    src/core/SoundManager.cpp
    src/core/SpriteAtlas.cpp
    src/core/UiKit.cpp
    ${COMMON_HEADERS}
    ${ENTITY_HEADERS}
    ${CORE_HEADERS}
    ${WORLD_HEADERS}
    ${AI_HEADERS}
)

target_include_directories(roguelike PRIVATE src)
target_compile_features(roguelike PRIVATE cxx_std_17)
target_link_libraries(roguelike PRIVATE SFML::Graphics SFML::Audio)

# ---------------------------------------------------------------------------
# Entity smoke test
# ---------------------------------------------------------------------------
# Standalone, console-only sanity check for the entity hierarchy -- no SFML
# linked, deliberately. If this target ever stops compiling because
# something under src/entities pulled in <SFML/...>, that's the
# "Application owns all sf:: types" boundary being broken, caught here
# instead of somewhere harder to trace.

add_executable(entity_smoke_test
    tests/entity_smoke_test.cpp
    src/entities/TalentSet.cpp
    src/entities/SpellbladeTalents.cpp
    src/entities/WarriorTalents.cpp
    src/entities/MageTalents.cpp
    src/entities/ThiefTalents.cpp
    src/entities/PlayerClassFactory.cpp
    src/entities/AttributeFormulas.cpp
    src/entities/StatusEffects.cpp
    ${COMMON_HEADERS}
    ${ENTITY_HEADERS}
    ${AI_HEADERS}
)

target_include_directories(entity_smoke_test PRIVATE src)
target_compile_features(entity_smoke_test PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Turn scheduler test
# ---------------------------------------------------------------------------
# Also console-only, no SFML -- TurnScheduler only touches Actor's public
# interface (Stats::speed), never rendering. Same enforcement pattern as
# entity_smoke_test above.

add_executable(turn_scheduler_test
    tests/turn_scheduler_test.cpp
    src/core/TurnScheduler.cpp
    src/entities/StatusEffects.cpp
    ${COMMON_HEADERS}
    ${ENTITY_HEADERS}
    ${CORE_HEADERS}
    ${AI_HEADERS}
)

target_include_directories(turn_scheduler_test PRIVATE src)
target_compile_features(turn_scheduler_test PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# FOV test
# ---------------------------------------------------------------------------
# Also console-only, no SFML -- shadowcasting only touches Map/Tile, never
# rendering. Same enforcement pattern as the other two test targets above.

add_executable(fov_test
    tests/fov_test.cpp
    src/world/Map.cpp
    src/world/FieldOfView.cpp
    ${COMMON_HEADERS}
    ${WORLD_HEADERS}
)

target_include_directories(fov_test PRIVATE src)
target_compile_features(fov_test PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Pathfinder test
# ---------------------------------------------------------------------------
# Also console-only, no SFML -- A* only touches Map/Tile, never rendering.
# Same enforcement pattern as the other test targets above.

add_executable(pathfinder_test
    tests/pathfinder_test.cpp
    src/world/Map.cpp
    src/world/Pathfinder.cpp
    ${COMMON_HEADERS}
    ${WORLD_HEADERS}
)

target_include_directories(pathfinder_test PRIVATE src)
target_compile_features(pathfinder_test PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Chaser test
# ---------------------------------------------------------------------------
# Also console-only, no SFML -- tests Chaser's own visibility-gate/
# pathfind/stop-when-adjacent logic directly, on top of its two
# dependencies (computeFieldOfView, findPath) which each have their own
# test above.

add_executable(chaser_test
    tests/chaser_test.cpp
    src/world/Map.cpp
    src/world/FieldOfView.cpp
    src/world/Pathfinder.cpp
    src/ai/Chaser.cpp
    src/entities/TalentSet.cpp
    src/entities/SpellbladeTalents.cpp
    src/entities/WarriorTalents.cpp
    src/entities/MageTalents.cpp
    src/entities/ThiefTalents.cpp
    src/entities/PlayerClassFactory.cpp
    src/entities/AttributeFormulas.cpp
    src/entities/StatusEffects.cpp
    ${COMMON_HEADERS}
    ${ENTITY_HEADERS}
    ${WORLD_HEADERS}
    ${AI_HEADERS}
)

target_include_directories(chaser_test PRIVATE src)
target_compile_features(chaser_test PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Dungeon generator test
# ---------------------------------------------------------------------------
# Also console-only, no SFML -- verifies connectivity (the one property
# that actually matters for "a connected, playable Map") across multiple
# seeds via flood-fill, plus prints one example layout.

add_executable(dungeon_test
    tests/dungeon_test.cpp
    src/world/Map.cpp
    src/world/DungeonGenerator.cpp
    src/world/DungeonModules.cpp
    src/world/ProceduralModules.cpp
    ${COMMON_HEADERS}
    ${WORLD_HEADERS}
)

target_include_directories(dungeon_test PRIVATE src)
target_compile_features(dungeon_test PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Talent test
# ---------------------------------------------------------------------------
# Also console-only, no SFML -- tests Talent/TalentSet/applyTalentDamage
# directly against hand-computed expected values.

add_executable(talent_test
    tests/talent_test.cpp
    src/entities/TalentSet.cpp
    src/entities/TalentEffects.cpp
    src/entities/AttributeFormulas.cpp
    src/entities/SpellbladeTalents.cpp
    src/entities/WarriorTalents.cpp
    src/entities/MageTalents.cpp
    src/entities/ThiefTalents.cpp
    src/entities/PlayerClassFactory.cpp
    src/entities/StatusEffects.cpp
    ${COMMON_HEADERS}
    ${ENTITY_HEADERS}
    ${AI_HEADERS}
)

target_include_directories(talent_test PRIVATE src)
target_compile_features(talent_test PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Warrior test
# ---------------------------------------------------------------------------
# Mirrors talent_test.cpp's structure exactly, for the Warrior's kit
# (originally "Marauder," renamed to Fighter at Prompt 19, then Warrior
# at Prompt 26) and PlayerClassFactory instead of the Spellblade's.

add_executable(warrior_test
    tests/warrior_test.cpp
    src/entities/TalentSet.cpp
    src/entities/TalentEffects.cpp
    src/entities/AttributeFormulas.cpp
    src/entities/SpellbladeTalents.cpp
    src/entities/WarriorTalents.cpp
    src/entities/MageTalents.cpp
    src/entities/ThiefTalents.cpp
    src/entities/PlayerClassFactory.cpp
    src/entities/StatusEffects.cpp
    ${COMMON_HEADERS}
    ${ENTITY_HEADERS}
    ${AI_HEADERS}
)

target_include_directories(warrior_test PRIVATE src)
target_compile_features(warrior_test PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Thief test
# ---------------------------------------------------------------------------
# Mirrors warrior_test.cpp's structure exactly, for the Thief's kit
# (originally "Archer," renamed at Prompt 19) and PlayerClassFactory
# instead of the Warrior's.

add_executable(thief_test
    tests/thief_test.cpp
    src/entities/TalentSet.cpp
    src/entities/TalentEffects.cpp
    src/entities/AttributeFormulas.cpp
    src/entities/SpellbladeTalents.cpp
    src/entities/WarriorTalents.cpp
    src/entities/MageTalents.cpp
    src/entities/ThiefTalents.cpp
    src/entities/PlayerClassFactory.cpp
    src/entities/StatusEffects.cpp
    ${COMMON_HEADERS}
    ${ENTITY_HEADERS}
    ${AI_HEADERS}
)

target_include_directories(thief_test PRIVATE src)
target_compile_features(thief_test PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Mage test
# ---------------------------------------------------------------------------
# Mirrors warrior_test.cpp/thief_test.cpp's structure exactly, for the
# third base class's kit and PlayerClassFactory.

add_executable(mage_test
    tests/mage_test.cpp
    src/entities/TalentSet.cpp
    src/entities/TalentEffects.cpp
    src/entities/AttributeFormulas.cpp
    src/entities/SpellbladeTalents.cpp
    src/entities/WarriorTalents.cpp
    src/entities/MageTalents.cpp
    src/entities/ThiefTalents.cpp
    src/entities/PlayerClassFactory.cpp
    src/entities/StatusEffects.cpp
    ${COMMON_HEADERS}
    ${ENTITY_HEADERS}
    ${AI_HEADERS}
)

target_include_directories(mage_test PRIVATE src)
target_compile_features(mage_test PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Talent unlock test
# ---------------------------------------------------------------------------
# Covers Prompt 23's level-gated talent unlocks: TalentSet::learnTalent()
# and PlayerClassFactory::talentUnlockedAtLevel()'s dispatch to each
# base class's own per-level unlock function.

add_executable(talent_unlock_test
    tests/talent_unlock_test.cpp
    src/entities/TalentSet.cpp
    src/entities/TalentEffects.cpp
    src/entities/AttributeFormulas.cpp
    src/entities/SpellbladeTalents.cpp
    src/entities/WarriorTalents.cpp
    src/entities/MageTalents.cpp
    src/entities/ThiefTalents.cpp
    src/entities/PlayerClassFactory.cpp
    src/entities/StatusEffects.cpp
    ${COMMON_HEADERS}
    ${ENTITY_HEADERS}
    ${AI_HEADERS}
)

target_include_directories(talent_unlock_test PRIVATE src)
target_compile_features(talent_unlock_test PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Hybrid spec test
# ---------------------------------------------------------------------------
# Covers Prompt 24's Warrior/Mage hybrid path (Fighter/Sorcerer at the
# time it was built): eligibility, pool class, and availableHybridPicks()
# shrinking correctly as abilities are picked.

add_executable(hybrid_spec_test
    tests/hybrid_spec_test.cpp
    src/entities/HybridSpec.cpp
    src/entities/TalentSet.cpp
    src/entities/TalentEffects.cpp
    src/entities/AttributeFormulas.cpp
    src/entities/SpellbladeTalents.cpp
    src/entities/WarriorTalents.cpp
    src/entities/MageTalents.cpp
    src/entities/ThiefTalents.cpp
    src/entities/PlayerClassFactory.cpp
    src/entities/StatusEffects.cpp
    ${COMMON_HEADERS}
    ${ENTITY_HEADERS}
    ${AI_HEADERS}
)

target_include_directories(hybrid_spec_test PRIVATE src)
target_compile_features(hybrid_spec_test PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Player leveling test
# ---------------------------------------------------------------------------
# No SFML, no window -- same pattern as every other standalone test.
# Covers the XP curve, the multi-level-up-in-one-grant loop (the exact
# scenario the boss's XP reward triggers), and the level-10 cap.

add_executable(player_leveling_test
    tests/player_leveling_test.cpp
    src/entities/TalentSet.cpp
    src/entities/PlayerLeveling.cpp
    src/entities/StatusEffects.cpp
    ${COMMON_HEADERS}
    ${ENTITY_HEADERS}
    ${AI_HEADERS}
)

target_include_directories(player_leveling_test PRIVATE src)
target_compile_features(player_leveling_test PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Monster tier test
# ---------------------------------------------------------------------------
# Unlike the other test targets, this one exercises MonsterFactory's
# real createMonster() (not a test-local AIBehavior construction), so it
# needs the full behavior roster it can build, not just TalentSet/
# AttributeFormulas.

add_executable(monster_tier_test
    tests/monster_tier_test.cpp
    src/entities/MonsterFactory.cpp
    src/entities/MonsterTier.cpp
    src/entities/AttributeFormulas.cpp
    src/entities/TalentSet.cpp
    src/entities/StatusEffects.cpp
    src/world/Map.cpp
    src/world/FieldOfView.cpp
    src/world/Pathfinder.cpp
    src/ai/Chaser.cpp
    src/ai/Kiter.cpp
    src/ai/Support.cpp
    src/ai/AoEBomber.cpp
    src/ai/BossBehavior.cpp
    src/ai/LichBehavior.cpp
    ${COMMON_HEADERS}
    ${ENTITY_HEADERS}
    ${AI_HEADERS}
)

target_include_directories(monster_tier_test PRIVATE src)
target_compile_features(monster_tier_test PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Monster AI test
# ---------------------------------------------------------------------------
# Also console-only, no SFML -- tests StatusEffects/tickStatusEffects and
# the Kiter/Support/AoEBomber decision logic directly against
# hand-computed expected values.

add_executable(monster_ai_test
    tests/monster_ai_test.cpp
    src/world/Map.cpp
    src/world/FieldOfView.cpp
    src/world/Pathfinder.cpp
    src/ai/Kiter.cpp
    src/ai/Support.cpp
    src/ai/AoEBomber.cpp
    src/entities/TalentSet.cpp
    src/entities/TalentEffects.cpp
    src/entities/AttributeFormulas.cpp
    src/entities/SpellbladeTalents.cpp
    src/entities/WarriorTalents.cpp
    src/entities/MageTalents.cpp
    src/entities/ThiefTalents.cpp
    src/entities/PlayerClassFactory.cpp
    src/entities/StatusEffects.cpp
    src/entities/StatusEffectLogic.cpp
    ${COMMON_HEADERS}
    ${ENTITY_HEADERS}
    ${WORLD_HEADERS}
    ${AI_HEADERS}
)

target_include_directories(monster_ai_test PRIVATE src)
target_compile_features(monster_ai_test PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Boss test
# ---------------------------------------------------------------------------
# Also console-only, no SFML -- verifies BossBehavior's phase transitions
# and per-phase decision logic against hand-computed expected values,
# using MonsterFactory's real configured numbers.

add_executable(boss_test
    tests/boss_test.cpp
    src/world/Map.cpp
    src/world/FieldOfView.cpp
    src/world/Pathfinder.cpp
    src/ai/BossBehavior.cpp
    src/entities/TalentSet.cpp
    src/entities/TalentEffects.cpp
    src/entities/AttributeFormulas.cpp
    src/entities/SpellbladeTalents.cpp
    src/entities/WarriorTalents.cpp
    src/entities/MageTalents.cpp
    src/entities/ThiefTalents.cpp
    src/entities/PlayerClassFactory.cpp
    src/entities/StatusEffects.cpp
    ${COMMON_HEADERS}
    ${ENTITY_HEADERS}
    ${WORLD_HEADERS}
    ${AI_HEADERS}
)

target_include_directories(boss_test PRIVATE src)
target_compile_features(boss_test PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Lich test
# ---------------------------------------------------------------------------
# Also console-only, no SFML -- verifies LichBehavior's Kiter-like
# movement and, more importantly, the summon mechanic itself (cooldown
# gating, the maxSummons cap held across multiple ready cooldowns, and
# falling back to a bolt when no open tile exists), using MonsterFactory's
# real configured numbers.

add_executable(lich_test
    tests/lich_test.cpp
    src/world/Map.cpp
    src/world/FieldOfView.cpp
    src/world/Pathfinder.cpp
    src/ai/LichBehavior.cpp
    src/entities/TalentSet.cpp
    src/entities/TalentEffects.cpp
    src/entities/AttributeFormulas.cpp
    src/entities/SpellbladeTalents.cpp
    src/entities/WarriorTalents.cpp
    src/entities/MageTalents.cpp
    src/entities/ThiefTalents.cpp
    src/entities/PlayerClassFactory.cpp
    src/entities/StatusEffects.cpp
    ${COMMON_HEADERS}
    ${ENTITY_HEADERS}
    ${WORLD_HEADERS}
    ${AI_HEADERS}
)

target_include_directories(lich_test PRIVATE src)
target_compile_features(lich_test PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Save/load test
# ---------------------------------------------------------------------------
# Also console-only, no SFML -- SaveGame is entirely SFML-independent by
# design. Round-trips a hand-built SaveGameState and verifies every field
# matches exactly, plus missing/corrupt file handling.

add_executable(savegame_test
    tests/savegame_test.cpp
    src/world/Map.cpp
    src/world/ExploredMap.cpp
    src/core/SaveGame.cpp
    ${COMMON_HEADERS}
    ${ENTITY_HEADERS}
    ${WORLD_HEADERS}
    ${CORE_HEADERS}
)

target_include_directories(savegame_test PRIVATE src)
target_compile_features(savegame_test PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Attribute formulas test
# ---------------------------------------------------------------------------
# The lightest test target in the project -- AttributeFormulas has no
# dependency on anything else (Actor, Talent, Monster, none of it), so
# this needs nothing beyond the formulas module itself.

add_executable(attribute_formulas_test
    tests/attribute_formulas_test.cpp
    src/entities/AttributeFormulas.cpp
    ${ENTITY_HEADERS}
)

target_include_directories(attribute_formulas_test PRIVATE src)
target_compile_features(attribute_formulas_test PRIVATE cxx_std_17)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT roguelike)

# Pure targeting/inspection rules, with no SFML dependency.
add_executable(targeting_test
    tests/targeting_test.cpp
    src/world/TalentTargeting.cpp
    src/world/Map.cpp
    src/world/ExploredMap.cpp
    src/entities/MonsterInspection.cpp
    src/entities/TalentSet.cpp
    src/entities/TalentEffects.cpp
    src/entities/AttributeFormulas.cpp
    src/entities/StatusEffects.cpp
)
target_include_directories(targeting_test PRIVATE src)
target_compile_features(targeting_test PRIVATE cxx_std_17)

# Real input/casting integration and render snapshots using a hidden SFML window.
get_target_property(APPLICATION_TEST_SOURCES roguelike SOURCES)
list(REMOVE_ITEM APPLICATION_TEST_SOURCES src/main.cpp)
add_executable(application_targeting_test tests/application_targeting_test.cpp ${APPLICATION_TEST_SOURCES})
target_include_directories(application_targeting_test PRIVATE src)
target_compile_features(application_targeting_test PRIVATE cxx_std_17)
target_link_libraries(application_targeting_test PRIVATE SFML::Graphics SFML::Audio)

add_executable(application_rewards_test tests/application_rewards_test.cpp ${APPLICATION_TEST_SOURCES})
target_include_directories(application_rewards_test PRIVATE src)
target_compile_features(application_rewards_test PRIVATE cxx_std_17)
target_link_libraries(application_rewards_test PRIVATE SFML::Graphics SFML::Audio)
