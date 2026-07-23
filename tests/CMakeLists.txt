# G7.2: Test target — links game sources as library + GoogleTest
# Excludes main.cpp (defines main() which would clash with gtest_main)

set(TEST_SOURCES "")
file(GLOB_RECURSE SRC_ALL "${CMAKE_SOURCE_DIR}/src/*.cpp")
foreach(f ${SRC_ALL})
    if(NOT f MATCHES "main\.cpp$" AND NOT f MATCHES "seh_handler\.cpp$")
        list(APPEND TEST_SOURCES ${f})
    endif()
endforeach()

# Build game sources as a static library (tests link against it)
add_library(roguelike_lib STATIC ${TEST_SOURCES})
target_include_directories(roguelike_lib PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/game
    ${CMAKE_SOURCE_DIR}/src/game/entities
    ${CMAKE_SOURCE_DIR}/src/game/systems
    ${CMAKE_SOURCE_DIR}/src/game/world
    ${CMAKE_SOURCE_DIR}/src/game/scenes
    ${CMAKE_SOURCE_DIR}/src/game/audio
    ${CMAKE_SOURCE_DIR}/src/game/save
    ${CMAKE_SOURCE_DIR}/src/game/scene
    ${CMAKE_SOURCE_DIR}/src/game/types
    ${CMAKE_SOURCE_DIR}/src/game/resources
    ${CMAKE_SOURCE_DIR}/src/game/core
    ${CMAKE_SOURCE_DIR}/src/game/boss
    ${CMAKE_SOURCE_DIR}/src/game/director
    ${CMAKE_SOURCE_DIR}/src/data
    ${CMAKE_SOURCE_DIR}/src/core/replay
    ${CMAKE_SOURCE_DIR}/src/core/sim
    ${JSON_DIR}
    ${RAYLIB_DIR}/include
)
if(NOT MSVC)
    target_compile_options(roguelike_lib PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# Helper: define a test executable
function(add_roguelike_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE roguelike_lib gtest_main gtest)
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/game
        ${CMAKE_SOURCE_DIR}/src/game/world
        ${CMAKE_SOURCE_DIR}/src/game/entities
        ${CMAKE_SOURCE_DIR}/src/game/systems
        ${CMAKE_SOURCE_DIR}/src/game/scenes
        ${CMAKE_SOURCE_DIR}/src/data
        ${JSON_DIR}
        ${RAYLIB_DIR}/include
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# ── Test targets ──────────────────────────────────────────────

add_roguelike_test(registry_test    registry/registry_test.cpp)
add_roguelike_test(damage_test      combat/damage_test.cpp)
add_roguelike_test(buff_test        combat/buff_test.cpp)
add_roguelike_test(biome_test       world/biome_test.cpp)
add_roguelike_test(encounter_test   world/encounter_test.cpp)
add_roguelike_test(condition_test   world/condition_test.cpp)
add_roguelike_test(action_test      combat/action_test.cpp)
add_roguelike_test(save_test        save/save_test.cpp)
# G7.3: Simulation test (data-structure only, no game deps)
add_executable(simulation_test sim/simulation_test.cpp)
target_link_libraries(simulation_test PRIVATE gtest_main gtest)
target_include_directories(simulation_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME simulation_test COMMAND simulation_test)

# G8.1: Behavior Tree test (pure C++, no game deps)
add_executable(behavior_tree_test ai/behavior_tree_test.cpp)
target_link_libraries(behavior_tree_test PRIVATE gtest_main gtest)
target_include_directories(behavior_tree_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME behavior_tree_test COMMAND behavior_tree_test)

# G8.2: A* Pathfinder test (pure C++, no game deps)
add_executable(astar_test ai/astar_test.cpp)
target_link_libraries(astar_test PRIVATE gtest_main gtest)
target_include_directories(astar_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME astar_test COMMAND astar_test)

# G8.3: MCTS tests (pure C++, no game deps)
add_executable(sim_state_test mcts/simulation_state_test.cpp)
target_link_libraries(sim_state_test PRIVATE gtest_main gtest)
target_include_directories(sim_state_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME sim_state_test COMMAND sim_state_test)

add_executable(mcts_search_test mcts/mcts_search_test.cpp)
target_link_libraries(mcts_search_test PRIVATE gtest_main gtest)
target_include_directories(mcts_search_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME mcts_search_test COMMAND mcts_search_test)

# G8.4: RL tests (pure C++, no game deps)
add_executable(env_test rl/environment_test.cpp)
target_link_libraries(env_test PRIVATE gtest_main gtest)
target_include_directories(env_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME env_test COMMAND env_test)

add_executable(obs_test rl/observation_test.cpp)
target_link_libraries(obs_test PRIVATE gtest_main gtest)
target_include_directories(obs_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME obs_test COMMAND obs_test)

add_executable(q_test rl/q_agent_test.cpp)
target_link_libraries(q_test PRIVATE gtest_main gtest)
target_include_directories(q_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME q_test COMMAND q_test)

add_roguelike_test(boot_test        integration/boot_test.cpp)
