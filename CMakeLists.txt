cmake_minimum_required(VERSION 3.28)
project(Barrowlight VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
# SFML 3, fetched and built from source, so a single CMake command builds
# everything on any platform with no libraries to install by hand. Audio is
# on (the sound effects are Ogg Vorbis); networking is off.
# ---------------------------------------------------------------------------
# BARROWLIGHT_PORTABLE links the C++ runtime statically (MSVC), so the
# packaged game runs on a machine with no Visual C++ redistributable.
option(BARROWLIGHT_PORTABLE "Statically link the C++ runtime for a standalone build" OFF)
if(BARROWLIGHT_PORTABLE)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(SFML_USE_STATIC_STD_LIBS ON CACHE BOOL "" FORCE)
endif()

include(FetchContent)
set(SFML_BUILD_AUDIO ON CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
set(SFML_USE_SYSTEM_DEPS OFF CACHE BOOL "" FORCE) # bundle Vorbis/FLAC too, on Linux as well
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.1.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SFML)

# Warnings for this project's own code (SFML, added above, is unaffected).
# missing-field-initializers is off because talents are built with aggregate
# initialisation that deliberately leaves trailing fields at their defaults.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-missing-field-initializers)
endif()

# ---------------------------------------------------------------------------
# The code is three layers, and the build enforces the boundary between them:
#
#   engine_core  rules: actors, talents, monsters, items, dungeon generation,
#                field of view, pathfinding, AI, the turn scheduler and saving.
#                No SFML at all, so it can be tested in plain console programs.
#   engine_app   the game itself: Application (input, rendering, the turn
#                loop and every system that touches the screen), UI, sprites.
#   sound        SoundManager, built twice: with sound for the game, and
#                silent (ROGUELIKE_SILENT) for the tests and the playtest bot.
#
# engine_core never links SFML: if a rule ever reaches for sf::, the console
# tests stop building. Sources are picked up by directory.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    src/entities/*.cpp src/entities/*.hpp
    src/world/*.cpp src/world/*.hpp
    src/ai/*.cpp src/ai/*.hpp)
list(APPEND CORE_SOURCES
    src/core/Position.hpp
    src/core/TurnScheduler.cpp src/core/TurnScheduler.hpp
    src/core/SaveGame.cpp src/core/SaveGame.hpp)

file(GLOB APP_SOURCES CONFIGURE_DEPENDS src/core/*.cpp src/core/*.hpp)
list(REMOVE_ITEM APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Position.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/TurnScheduler.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/core/TurnScheduler.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/SaveGame.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/core/SaveGame.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/SoundManager.cpp)

add_library(engine_core STATIC ${CORE_SOURCES})
target_include_directories(engine_core PUBLIC src)
target_compile_features(engine_core PUBLIC cxx_std_17)

add_library(engine_app STATIC ${APP_SOURCES})
target_link_libraries(engine_app PUBLIC engine_core SFML::Graphics SFML::Audio)

add_library(sound STATIC src/core/SoundManager.cpp)
target_link_libraries(sound PUBLIC engine_core SFML::Audio)
add_library(sound_silent STATIC src/core/SoundManager.cpp)
target_link_libraries(sound_silent PUBLIC engine_core SFML::Audio)
target_compile_definitions(sound_silent PRIVATE ROGUELIKE_SILENT)

# ---------------------------------------------------------------------------
# The game
# ---------------------------------------------------------------------------
add_executable(barrowlight src/main.cpp)
target_link_libraries(barrowlight PRIVATE engine_app sound)
if(WIN32)
    target_sources(barrowlight PRIVATE src/barrowlight.rc) # the .exe icon
endif()
if(MSVC)
    # A Release build is a windowed app with no console; Debug keeps the console for logs.
    target_link_options(barrowlight PRIVATE $<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup>)
endif()
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT barrowlight)
# The game finds assets/ relative to where it starts: run it from the project root.
set_property(TARGET barrowlight PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# The playable package: `cmake --install` or `cpack` puts the game, its
# assets and the README in one folder (or zip) that runs from anywhere.
install(TARGETS barrowlight RUNTIME DESTINATION .)
install(DIRECTORY assets DESTINATION .)
install(FILES README.md DESTINATION .)
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_FILE_NAME "Barrowlight-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
include(CPack)

# ---------------------------------------------------------------------------
# Tests. Each is a small console program that prints [ok]/[FAIL] per check
# and exits non-zero on any failure; `ctest` runs them all.
#
#   Console tests link engine_core only (no SFML, no window).
#   Application tests drive the real game through a hidden window: real
#   input, real turns, real saves, and render snapshots written to
#   build/rewards-checks/ for eyeballing the UI.
# ---------------------------------------------------------------------------
enable_testing()

# Fixed talent kits from before the talent trees: stable data for the
# isolated combat tests (see tests/fixtures/ClassKits.hpp).
file(GLOB FIXTURE_SOURCES CONFIGURE_DEPENDS tests/fixtures/*.cpp tests/fixtures/*.hpp)
add_library(test_fixtures STATIC ${FIXTURE_SOURCES})
target_include_directories(test_fixtures PUBLIC tests)
target_link_libraries(test_fixtures PUBLIC engine_core)

foreach(name
        attribute_formulas_test entity_smoke_test turn_scheduler_test fov_test pathfinder_test chaser_test
        dungeon_test talent_test talent_tree_test warrior_test thief_test mage_test player_leveling_test
        monster_tier_test monster_ai_test boss_test lich_test savegame_test targeting_test)
    add_executable(${name} tests/${name}.cpp)
    target_link_libraries(${name} PRIVATE engine_core test_fixtures)
    add_test(NAME ${name} COMMAND ${name})
endforeach()

foreach(name application_targeting_test application_rewards_test)
    add_executable(${name} tests/${name}.cpp)
    target_link_libraries(${name} PRIVATE engine_app sound_silent test_fixtures)
    add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}) # assets/ is found from the project root
endforeach()
set_tests_properties(application_rewards_test PROPERTIES LABELS slow TIMEOUT 3600)

# The playtest bot plays whole runs with a hidden window and writes a report
# to build/playtest/. Not a test: it takes minutes. Use it as a crash check.
add_executable(playtest_bot tests/playtest_bot.cpp)
target_link_libraries(playtest_bot PRIVATE engine_app sound_silent)
