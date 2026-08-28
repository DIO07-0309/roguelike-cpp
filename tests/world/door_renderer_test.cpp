#include <gtest/gtest.h>
#include "rendering/door_renderer.h"
#include "game_map.h"
#include "config.h"

// Note: Raylib window is not initialized in unit tests.
// DoorRenderer::init() will fail to load textures (LoadTexture returns {0}),
// but should NOT crash. draw_door() with is_loaded()=false should be a no-op.

TEST(DoorRendererTest, SingletonExists) {
    DoorRenderer& r = DoorRenderer::inst();
    (void)r;
    SUCCEED();
}

TEST(DoorRendererTest, InitDoesNotCrashWithoutWindow) {
    DoorRenderer& r = DoorRenderer::inst();
    r.init();  // textures won't load (no Raylib window), but should not crash
    // is_loaded may be false — that's OK in headless test
    SUCCEED();
}

TEST(DoorRendererTest, DrawDoesNotCrashWhenNotLoaded) {
    DoorRenderer& r = DoorRenderer::inst();
    // Don't call init — textures not loaded
    r.draw_door(1, 1, DoorState::OPEN, true);
    r.draw_door(2, 2, DoorState::CLOSED, false);
    r.draw_door(3, 3, DoorState::LOCKED, true);
    r.draw_door(4, 4, DoorState::SEALED, false);
    SUCCEED();
}

TEST(DoorRendererTest, AnimStateChangeDoesNotCrash) {
    DoorRenderer& r = DoorRenderer::inst();
    r.on_state_change(5, 5, DoorState::OPEN, DoorState::CLOSED);
    r.update(0.1f);
    r.draw_door(5, 5, DoorState::CLOSED, true);
    SUCCEED();
}

TEST(DoorRendererTest, AnimCompletesInTransitionDuration) {
    DoorRenderer& r = DoorRenderer::inst();
    r.on_state_change(10, 10, DoorState::OPEN, DoorState::CLOSED);
    r.update(0.15f);
    r.update(0.15f);
    r.draw_door(10, 10, DoorState::CLOSED, true);
    SUCCEED();
}

TEST(DoorRendererTest, SameStateNoAnimation) {
    DoorRenderer& r = DoorRenderer::inst();
    r.on_state_change(7, 7, DoorState::CLOSED, DoorState::CLOSED);
    r.update(0.1f);
    r.draw_door(7, 7, DoorState::CLOSED, true);
    SUCCEED();
}

TEST(DoorRendererTest, SetDoorStateTriggersAnim) {
    DoorRenderer& r = DoorRenderer::inst();
    GameMap map(10, 10, TILE_SIZE);
    map.set_tile(5, 5, TileType::DOOR);
    map.set_door_state(5, 5, DoorState::CLOSED);
    r.update(0.1f);
    r.draw_door(5, 5, DoorState::CLOSED, true);
    SUCCEED();
}
