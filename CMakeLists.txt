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

# We only need windowing + 2D rendering right now. Turning Audio and Network
# off keeps the first build faster and avoids pulling in system deps
# (FLAC/Vorbis/mbedTLS/libssh2) that this project doesn't use yet. Flip these
# back on later if/when the engine needs sound.
set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)

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
    src/entities/Feature.hpp
    src/entities/Stats.hpp
    src/entities/AIBehavior.hpp
    src/entities/Inventory.hpp
    src/entities/TalentSet.hpp
    src/entities/StatusEffects.hpp
    src/entities/Actor.hpp
    src/entities/Player.hpp
    src/entities/Monster.hpp
)

set(CORE_HEADERS
    src/core/Application.hpp
    src/core/TurnScheduler.hpp
)

set(WORLD_HEADERS
    src/world/Tile.hpp
    src/world/Map.hpp
    src/world/FieldOfView.hpp
    src/world/ExploredMap.hpp
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
    src/core/TurnScheduler.cpp
    src/world/Map.cpp
    src/world/FieldOfView.cpp
    src/world/ExploredMap.cpp
    ${COMMON_HEADERS}
    ${ENTITY_HEADERS}
    ${CORE_HEADERS}
    ${WORLD_HEADERS}
)

target_include_directories(roguelike PRIVATE src)
target_compile_features(roguelike PRIVATE cxx_std_17)
target_link_libraries(roguelike PRIVATE SFML::Graphics)

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
    ${COMMON_HEADERS}
    ${ENTITY_HEADERS}
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
    ${COMMON_HEADERS}
    ${ENTITY_HEADERS}
    ${CORE_HEADERS}
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

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT roguelike)
