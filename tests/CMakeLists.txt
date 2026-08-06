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

# Helper: copy runtime DLLs (raylib + MinGW runtime) next to a test exe
function(copy_test_runtime_dlls target)
    if(WIN32)
        get_filename_component(MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${RAYLIB_DLL}" "$<TARGET_FILE_DIR:${target}>/raylib.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MINGW_BIN}/libgcc_s_seh-1.dll" "$<TARGET_FILE_DIR:${target}>/libgcc_s_seh-1.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MINGW_BIN}/libstdc++-6.dll" "$<TARGET_FILE_DIR:${target}>/libstdc++-6.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MINGW_BIN}/libwinpthread-1.dll" "$<TARGET_FILE_DIR:${target}>/libwinpthread-1.dll")
    endif()
endfunction()

# Helper: define a test executable
function(add_roguelike_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE roguelike_lib gtest_main gtest)
    if(MSVC)
        target_link_libraries(${name} PRIVATE "${RAYLIB_IMPLIB}" winmm opengl32)
    elseif(WIN32)
        target_link_libraries(${name} PRIVATE "${RAYLIB_LIB}" winmm gdi32 opengl32)
    else()
        target_link_libraries(${name} PRIVATE raylib m pthread dl)
    endif()
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
    copy_test_runtime_dlls(${name})
    add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction()

# ── Test targets ──────────────────────────────────────────────

add_roguelike_test(registry_test    registry/registry_test.cpp)
add_roguelike_test(damage_test      combat/damage_test.cpp)
add_roguelike_test(damage_type_test combat/damage_type_test.cpp)
add_roguelike_test(projectile_test  combat/projectile_test.cpp)
add_roguelike_test(buff_test        combat/buff_test.cpp)
add_roguelike_test(biome_test       world/biome_test.cpp)
add_roguelike_test(encounter_test   world/encounter_test.cpp)
add_roguelike_test(condition_test   world/condition_test.cpp)
add_roguelike_test(action_test      combat/action_test.cpp)
add_roguelike_test(save_test        save/save_test.cpp)
# G7.3: Simulation test (data-structure only, no game deps)
add_executable(simulation_test sim/simulation_test.cpp
    ${CMAKE_SOURCE_DIR}/src/core/sim/sim_runner.cpp)
target_link_libraries(simulation_test PRIVATE gtest_main gtest)
target_include_directories(simulation_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
copy_test_runtime_dlls(simulation_test)
add_test(NAME simulation_test COMMAND simulation_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# G8.1: Behavior Tree test (pure C++, no game deps)
add_executable(behavior_tree_test ai/behavior_tree_test.cpp)
target_link_libraries(behavior_tree_test PRIVATE gtest_main gtest)
target_include_directories(behavior_tree_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
copy_test_runtime_dlls(behavior_tree_test)
add_test(NAME behavior_tree_test COMMAND behavior_tree_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# G8.2: A* Pathfinder test (pure C++, no game deps)
add_executable(astar_test ai/astar_test.cpp ${CMAKE_SOURCE_DIR}/src/ai/navigation/pathfinder.cpp)
target_link_libraries(astar_test PRIVATE gtest_main gtest)
target_include_directories(astar_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
copy_test_runtime_dlls(astar_test)
add_test(NAME astar_test COMMAND astar_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# G8.3: MCTS tests (pure C++, no game deps)
add_executable(sim_state_test mcts/simulation_state_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/simulation_state.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/combat_evaluator.cpp)
target_link_libraries(sim_state_test PRIVATE gtest_main gtest)
target_include_directories(sim_state_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
copy_test_runtime_dlls(sim_state_test)
add_test(NAME sim_state_test COMMAND sim_state_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(mcts_search_test mcts/mcts_search_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/mcts_search.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/simulation_state.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/combat_evaluator.cpp)
target_link_libraries(mcts_search_test PRIVATE gtest_main gtest)
target_include_directories(mcts_search_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
copy_test_runtime_dlls(mcts_search_test)
add_test(NAME mcts_search_test COMMAND mcts_search_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# G8.4: RL tests (pure C++, no game deps)
add_executable(env_test rl/environment_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/rl/environment.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/rl/observation.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/simulation_state.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/combat_evaluator.cpp)
target_link_libraries(env_test PRIVATE gtest_main gtest)
target_include_directories(env_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
copy_test_runtime_dlls(env_test)
add_test(NAME env_test COMMAND env_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(obs_test rl/observation_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/rl/observation.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/simulation_state.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/combat_evaluator.cpp)
target_link_libraries(obs_test PRIVATE gtest_main gtest)
target_include_directories(obs_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
copy_test_runtime_dlls(obs_test)
add_test(NAME obs_test COMMAND obs_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(q_test rl/q_agent_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/rl/q_agent.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/rl/observation.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/simulation_state.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/mcts/combat_evaluator.cpp)
target_link_libraries(q_test PRIVATE gtest_main gtest)
target_include_directories(q_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
copy_test_runtime_dlls(q_test)
add_test(NAME q_test COMMAND q_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_roguelike_test(boot_test        integration/boot_test.cpp)

# G9: Weapon system tests
add_roguelike_test(weapon_test      weapon/weapon_test.cpp)
add_roguelike_test(weapon_special_test weapon/weapon_special_test.cpp)
add_roguelike_test(equipment_test   weapon/equipment_test.cpp)
add_roguelike_test(synergy_test     weapon/synergy_test.cpp)

# G10.1: Element Core tests
add_roguelike_test(element_test      element/element_test.cpp)
add_roguelike_test(element_combat    element/element_combat_test.cpp)
add_roguelike_test(element_vfx       element/element_vfx_test.cpp)

# M1: Behavior-clone table + MirrorAgent clone layer tests
add_roguelike_test(clone_test        mirror/behavior_clone_test.cpp)
add_roguelike_test(mirror_phase_test mirror/mirror_phase_test.cpp)
add_roguelike_test(mirror_arb_test    mirror/mirror_arbitration_test.cpp)


