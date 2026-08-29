# Challenge Room Portal Redesign — Implementation Plan (Revised)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the challenge room's door-based entrance with a portal system: a glowing portal on the corridor wall teleports the player to a sealed arena; after victory, a return portal appears.

**Architecture (Revised per user directive):**

- **ChallengeRoomController**: ONLY handles challenge state machine, wave spawning, monster spawn requests, rewards. Does NOT handle player teleportation, map switching, or fade effects.
- **GameScene**: Handles `WorldMode` (DUNGEON vs CHALLENGE_ARENA), main map/arena switching, player position save/restore, transition effects, portal rendering, and interaction detection.
- **Map ownership**: Main dungeon map (`game_map`) persists during arena. Challenge arena uses the same map's sealed room — no second map. When in CHALLENGE_ARENA mode, main dungeon monsters are frozen (not ticked).
- **Save/Load**: MVP saves challenge phase state. Challenge in progress = block save (forbid saving during TELEPORTING/ARMED/COMBAT/WAVE phases). Only PORTAL_ACTIVE, CLEARED, and INACTIVE are savable.
- **Portal placement**: "Exit-adjacent optional content" — challenge room is placed near the exit but player can skip it and go directly to stairs. Portal is on the corridor wall, not blocking path.
- **Entry portal**: Blue/cyan, costs 1 Key. **Return portal**: Green/gold, appears after all waves cleared.

**Tech Stack:** C++17, Raylib 5.0, VFXServer primitives, DoorState::SEALED, existing SpecialRoom system.

## Global Constraints
- Functions ≤40 lines, one class = one responsibility, composition > inheritance
- No new third-party libraries
- No comments unless asked
- No >300 lines per generation
- All enum class, no raw enum
- Semantic variable names
- Each `.h` must have `#pragma once`

---

## File Map

| File | Role | Changes |
|------|------|---------|
| `src/game/world/challenge_room.h` | State machine ONLY | Add `PORTAL_ACTIVE` phase, portal/return positions, room rect setters, wave/phase getters. NO teleport, NO fade. |
| `src/game/world/challenge_room.cpp` | Logic ONLY | Portal setup, key consumption, wave management, reward granting. |
| `src/game/scenes/game_scene.h` | WorldMode + transition state | Add `WorldMode` enum, `_world_mode`, `_saved_player_pos`, `_teleport_fade_timer`, `_portal_pulse_timer` |
| `src/game/scenes/game_scene.cpp` | All transition logic | Portal tick, teleport, fade overlay, freeze main dungeon in arena mode |
| `src/game/player_controller.cpp` | Input dispatch | E key at entry/return portal → delegates to GameScene methods |
| `src/game/systems/game_renderer.cpp` | Rendering | Portal VFX drawing, wave HUD, portal labels |
| `src/game/systems/vfx_server.h/cpp` | VFX | `portal_entry()`, `portal_return()` convenience methods |
| `src/game/world/dungeon_generator.cpp` | Map gen | Seal challenge room door, compute portal tile |
| `src/game/world/special_room.h` | Data | Add `portal_tx/ty` fields to SpecialRoom |
| `src/game/save/save_manager.cpp` | Save/Load | Save challenge phase (block save if in-combat) |
| `tests/economy/challenge_room_test.cpp` | Tests | Portal state machine tests |

---

## Task 1: ChallengeRoomController — Portal Phase (State Machine Only)

**Files:**
- Modify: `src/game/world/challenge_room.h`
- Modify: `src/game/world/challenge_room.cpp`

**Responsibility boundary**: This class ONLY manages phase state, wave spawning, rewards. It does NOT touch player position, map, or fade.

- [ ] **Step 1: Add PORTAL_ACTIVE phase to ChallengePhase enum**

In `challenge_room.h`, add `PORTAL_ACTIVE` after `CLEARED`:

```cpp
enum class ChallengePhase : uint8_t {
    INACTIVE, UNLOCKED, ARMED, WAVE_SPAWNING, COMBAT,
    WAIT_NEXT_WAVE, REWARD, CLEARED,
    PORTAL_ACTIVE   // Portal visible, waiting for player interaction
};
```

- [ ] **Step 2: Add portal/room data fields to ChallengeRoomController**

In `challenge_room.h`, add private fields:

```cpp
    int _portal_tx = -1, _portal_ty = -1;
    int _return_portal_tx = -1, _return_portal_ty = -1;
```

Add public getters/setters:

```cpp
    void setup_portal(int tx, int ty);
    bool consume_key_for_challenge(Player& player);
    void set_room_rect(int rx, int ry, int rw, int rh);
    void set_return_portal(int tx, int ty);
    void mark_cleared();

    int portal_tx() const { return _portal_tx; }
    int portal_ty() const { return _portal_ty; }
    int return_portal_tx() const { return _return_portal_tx; }
    int return_portal_ty() const { return _return_portal_ty; }
    int room_rx() const { return _room_rx; }
    int room_ry() const { return _room_ry; }
    int room_rw() const { return _room_rw; }
    int room_rh() const { return _room_rh; }
    ChallengePhase phase() const { return _phase; }
    int current_wave() const { return _current_wave; }
    int total_waves() const { return _total_waves; }
```

- [ ] **Step 3: Implement setup_portal(), consume_key_for_challenge(), set_room_rect(), set_return_portal(), mark_cleared()**

```cpp
void ChallengeRoomController::setup_portal(int tx, int ty) {
    _portal_tx = tx;
    _portal_ty = ty;
    _phase = ChallengePhase::PORTAL_ACTIVE;
}

bool ChallengeRoomController::consume_key_for_challenge(Player& player) {
    if (_phase != ChallengePhase::PORTAL_ACTIVE) return false;
    if (player.key_count <= 0) return false;
    player.spend_key(1);
    return true;
}

void ChallengeRoomController::set_room_rect(int rx, int ry, int rw, int rh) {
    _room_rx = rx; _room_ry = ry; _room_rw = rw; _room_rh = rh;
}

void ChallengeRoomController::set_return_portal(int tx, int ty) {
    _return_portal_tx = tx;
    _return_portal_ty = ty;
}

void ChallengeRoomController::mark_cleared() {
    _phase = ChallengePhase::CLEARED;
}
```

- [ ] **Step 4: Update reset() to clear portal state**

```cpp
void ChallengeRoomController::reset() {
    _phase = ChallengePhase::INACTIVE;
    _current_wave = 0;
    _total_waves = 3;
    _wave_timer = 0.0f;
    _monsters_alive_this_wave = 0;
    _room_rx = _room_ry = _room_rw = _room_rh = 0;
    _deterministic_seed = 0;
    _portal_tx = _portal_ty = -1;
    _return_portal_tx = _return_portal_ty = -1;
}
```

- [ ] **Step 5: Write unit tests**

In `tests/economy/challenge_room_test.cpp`, add:

```cpp
TEST(ChallengeRoomPortal, SetupPortal) {
    ChallengeRoomController ctrl;
    ctrl.setup_portal(10, 5);
    EXPECT_EQ(ctrl.phase(), ChallengePhase::PORTAL_ACTIVE);
    EXPECT_EQ(ctrl.portal_tx(), 10);
    EXPECT_EQ(ctrl.portal_ty(), 5);
}

TEST(ChallengeRoomPortal, ConsumeKeySuccess) {
    ChallengeRoomController ctrl;
    ctrl.setup_portal(10, 5);
    Player p; p.key_count = 1;
    EXPECT_TRUE(ctrl.consume_key_for_challenge(p));
    EXPECT_EQ(p.key_count, 0);
}

TEST(ChallengeRoomPortal, ConsumeKeyFailNoKey) {
    ChallengeRoomController ctrl;
    ctrl.setup_portal(10, 5);
    Player p; p.key_count = 0;
    EXPECT_FALSE(ctrl.consume_key_for_challenge(p));
}

TEST(ChallengeRoomPortal, SetReturnPortal) {
    ChallengeRoomController ctrl;
    ctrl.set_return_portal(8, 12);
    EXPECT_EQ(ctrl.return_portal_tx(), 8);
    EXPECT_EQ(ctrl.return_portal_ty(), 12);
}
```

- [ ] **Step 6: Build and run tests**

Run: `cmake --build build --config Release && ctest --test-dir build --output-on-failure`
Expected: All tests pass.

---

## Task 2: GameScene — WorldMode + Transition State

**Files:**
- Modify: `src/game/scenes/game_scene.h`

**Responsibility**: GameScene owns ALL transition logic: WorldMode switching, player position save/restore, fade, portal VFX, main dungeon freeze.

- [ ] **Step 1: Add WorldMode enum and state fields**

In `game_scene.h`, add before the class definition or in a public section:

```cpp
enum class WorldMode : uint8_t {
    DUNGEON,           // Normal gameplay in main dungeon
    CHALLENGE_ARENA    // Inside challenge arena (main dungeon frozen)
};
```

Add private fields:

```cpp
    WorldMode _world_mode = WorldMode::DUNGEON;
    float _saved_player_x = 0, _saved_player_y = 0;
    float _teleport_fade_timer = 0.0f;
    float _portal_pulse_timer = 0.0f;
    bool _portal_fade_in = false;
```

Add public methods:

```cpp
    WorldMode world_mode() const { return _world_mode; }
    void enter_challenge_arena();
    void exit_challenge_arena();
    bool is_save_blocked() const;
```

- [ ] **Step 2: Implement enter_challenge_arena()**

```cpp
void GameScene::enter_challenge_arena() {
    if (_world_mode != WorldMode::DUNGEON) return;
    _saved_player_x = player->entity.rect.x;
    _saved_player_y = player->entity.rect.y;
    _world_mode = WorldMode::CHALLENGE_ARENA;
    _teleport_fade_timer = 0.5f;
    _portal_fade_in = true;
}
```

- [ ] **Step 3: Implement exit_challenge_arena()**

```cpp
void GameScene::exit_challenge_arena() {
    if (_world_mode != WorldMode::CHALLENGE_ARENA) return;
    player->entity.position = {_saved_player_x, _saved_player_y};
    player->entity.sync_rect();
    game_map->reset_visibility();
    _world_mode = WorldMode::DUNGEON;
    _teleport_fade_timer = 0.5f;
    _portal_fade_in = false;
    _challenge.reset();
}
```

- [ ] **Step 4: Implement is_save_blocked()**

```cpp
bool GameScene::is_save_blocked() const {
    auto ph = _challenge.phase();
    return ph == ChallengePhase::TELEPORTING_IN ||
           ph == ChallengePhase::ARMED ||
           ph == ChallengePhase::WAVE_SPAWNING ||
           ph == ChallengePhase::COMBAT ||
           ph == ChallengePhase::WAIT_NEXT_WAVE;
}
```

- [ ] **Step 5: Build**

Run: `cmake --build build --config Release`
Expected: Compiles cleanly.

---

## Task 3: GameScene — Portal Tick + Teleport Logic in _process()

**Files:**
- Modify: `src/game/scenes/game_scene.cpp`

- [ ] **Step 1: Add portal pulse timer tick**

In `_process()`, after existing challenge tick, add:

```cpp
    // Batch 3I: Portal pulse animation
    if (_challenge.phase() == ChallengePhase::PORTAL_ACTIVE) {
        _portal_pulse_timer += dt;
    }
    // Teleport fade tick
    if (_teleport_fade_timer > 0) {
        _teleport_fade_timer -= dt;
        if (_teleport_fade_timer <= 0) {
            _teleport_fade_timer = 0;
            if (_portal_fade_in && _world_mode == WorldMode::CHALLENGE_ARENA) {
                // Fade complete → teleport player to room center
                int cx = _challenge.room_rx() + _challenge.room_rw() / 2;
                int cy = _challenge.room_ry() + _challenge.room_rh() / 2;
                auto [px, py] = game_map->tile_to_pixel(cx, cy);
                player->entity.position = {px, py};
                player->entity.sync_rect();
                game_map->reset_visibility();
                _challenge.set_phase_for_test(ChallengePhase::ARMED);
            }
        }
    }
```

- [ ] **Step 2: Freeze main dungeon in arena mode**

In `_process()`, after the `if (state != GameState::PLAYING) return;` check, add early return for arena mode:

```cpp
    // Batch 3I: In challenge arena, freeze main dungeon monsters
    if (_world_mode == WorldMode::CHALLENGE_ARENA) {
        // Only tick challenge-related systems, skip main dungeon monster AI
        _challenge.tick(dt);
        // ... existing challenge room tick logic ...
        return;  // Skip main dungeon processing
    }
```

- [ ] **Step 3: Build**

Run: `cmake --build build --config Release`
Expected: Compiles cleanly.

---

## Task 4: PlayerController — Portal Interaction Dispatch

**Files:**
- Modify: `src/game/player_controller.cpp`

- [ ] **Step 1: Replace challenge room E key block with portal dispatch**

Replace the existing challenge room E key block (around line 311-328) with:

```cpp
        // Batch 3I: Challenge Room — Portal interaction
        {
            auto [ptx, pty] = gs.game_map->pixel_to_tile(
                gs.player->entity.rect.x + gs.player->entity.rect.width/2,
                gs.player->entity.rect.y + gs.player->entity.rect.height/2);
            SpecialRoom* room = gs.game_map->get_special_room_at(ptx, pty);
            if (room && room->type == SpecialRoomType::CHALLENGE) {
                // Entry portal: player adjacent to portal tile
                if (gs._challenge.phase() == ChallengePhase::PORTAL_ACTIVE) {
                    int dx = abs(ptx - room->portal_tx);
                    int dy = abs(pty - room->portal_ty);
                    if (dx + dy <= 1) {
                        if (gs._challenge.consume_key_for_challenge(*gs.player)) {
                            gs.enter_challenge_arena();
                            gs.get_tree()->get_audio()->play_sfx("door_open", 0.6f);
                        } else if (gs.player->key_count <= 0) {
                            gs._presentation.room_msg = "需要钥匙才能开启挑战。";
                            gs._presentation.room_msg_timer = 2.0f;
                        }
                        return;
                    }
                }
                // Return portal: player adjacent to return portal inside arena
                if (gs._challenge.phase() == ChallengePhase::CLEARED &&
                    gs._challenge.return_portal_tx() > 0) {
                    int dx = abs(ptx - gs._challenge.return_portal_tx());
                    int dy = abs(pty - gs._challenge.return_portal_ty());
                    if (dx + dy <= 1) {
                        gs.exit_challenge_arena();
                        gs.get_tree()->get_audio()->play_sfx("door_open", 0.6f);
                        return;
                    }
                }
            }
        }
```

- [ ] **Step 2: Block saving during challenge**

In `save_manager.cpp`, before writing save file, check:

```cpp
    if (scene->is_save_blocked()) {
        // Cannot save during challenge
        return false;
    }
```

- [ ] **Step 3: Build**

Run: `cmake --build build --config Release`
Expected: Compiles cleanly.

---

## Task 5: GameRenderer — Portal VFX + Labels

**Files:**
- Modify: `src/game/systems/game_renderer.cpp`

- [ ] **Step 1: Add portal rendering method**

In `game_renderer.h`, add:

```cpp
    void draw_challenge_portal(float cam_x, float cam_y, int portal_tx, int portal_ty,
                               float pulse_timer, bool is_entry);
    void draw_teleport_fade(int sw, int sh, float fade_timer, bool fading_in);
```

In `game_renderer.cpp`, implement:

```cpp
void GameRenderer::draw_challenge_portal(float cam_x, float cam_y,
    int portal_tx, int portal_ty, float pulse_timer, bool is_entry)
{
    float px = portal_tx * TILE_SIZE + TILE_SIZE / 2 - cam_x;
    float py = portal_ty * TILE_SIZE + TILE_SIZE / 2 - cam_y;
    float pulse = 0.8f + 0.2f * sinf(pulse_timer * 3.0f);
    Color outer = is_entry ? Color{80, 180, 255, (unsigned char)(200 * pulse)}
                           : Color{100, 255, 150, (unsigned char)(200 * pulse)};
    Color inner = is_entry ? Color{150, 220, 255, (unsigned char)(220 * pulse)}
                           : Color{180, 255, 200, (unsigned char)(220 * pulse)};
    DrawCircleV({px, py}, 12.0f, outer);
    DrawCircleV({px, py}, 8.0f, inner);
    if (g_font_loaded) {
        const char* label = is_entry ? "按 [E] 开始挑战" : "按 [E] 返回";
        float tw = MeasureTextEx(g_font_small, label, 12, 1).x;
        DrawTextEx(g_font_small, label, {px - tw / 2, py - 24}, 12, 1,
                   is_entry ? Color{200, 220, 255, 220} : Color{180, 255, 200, 220});
    }
}

void GameRenderer::draw_teleport_fade(int sw, int sh, float fade_timer, bool fading_in) {
    if (fade_timer <= 0) return;
    float alpha = fading_in ? (0.5f - fade_timer) / 0.5f : fade_timer / 0.5f;
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, (unsigned char)(255 * alpha)});
}
```

- [ ] **Step 2: Call portal rendering from GameScene _render()**

In `game_scene.cpp` `_render()`, add portal drawing:

```cpp
    // Batch 3I: Draw entry portal (in dungeon mode)
    if (_world_mode == WorldMode::DUNGEON &&
        _challenge.phase() == ChallengePhase::PORTAL_ACTIVE) {
        auto* room = game_map->get_special_room_at(
            _challenge.portal_tx(), _challenge.portal_ty());
        if (room) {
            _renderer.draw_challenge_portal(_cam_x, _cam_y,
                room->portal_tx, room->portal_ty, _portal_pulse_timer, true);
        }
    }
    // Draw return portal (in arena mode)
    if (_world_mode == WorldMode::CHALLENGE_ARENA &&
        _challenge.phase() == ChallengePhase::CLEARED &&
        _challenge.return_portal_tx() > 0) {
        _renderer.draw_challenge_portal(_cam_x, _cam_y,
            _challenge.return_portal_tx(), _challenge.return_portal_ty(),
            _portal_pulse_timer, false);
    }
    // Teleport fade overlay
    _renderer.draw_teleport_fade(sw, sh, _teleport_fade_timer, _portal_fade_in);
```

- [ ] **Step 3: Build**

Run: `cmake --build build --config Release`
Expected: Compiles cleanly.

---

## Task 6: VFXServer — Portal Convenience Methods

**Files:**
- Modify: `src/game/systems/vfx_server.h`
- Modify: `src/game/systems/vfx_server.cpp`

- [ ] **Step 1: Add declarations**

In `vfx_server.h`, add before `};`:

```cpp
    void portal_entry(float cx, float cy);
    void portal_return(float cx, float cy);
```

- [ ] **Step 2: Implement**

In `vfx_server.cpp`, add at end:

```cpp
void VFXServer::portal_entry(float cx, float cy) {
    aura_ring(cx, cy, 18.0f, {80, 180, 255, 200}, 1.2f);
    spark_burst(cx, cy, 6, {120, 200, 255, 180}, 0.4f);
}

void VFXServer::portal_return(float cx, float cy) {
    aura_ring(cx, cy, 18.0f, {100, 255, 150, 200}, 1.2f);
    spark_burst(cx, cy, 6, {150, 255, 180, 180}, 0.4f);
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build --config Release`
Expected: Compiles cleanly.

---

## Task 7: Dungeon Generator — Seal Door + Place Portal

**Files:**
- Modify: `src/game/world/special_room.h`
- Modify: `src/game/world/dungeon_generator.cpp`

- [ ] **Step 1: Add portal fields to SpecialRoom**

In `special_room.h`, add to `SpecialRoom` struct:

```cpp
    int portal_tx = 0, portal_ty = 0;
```

- [ ] **Step 2: Seal door and compute portal position**

In `dungeon_generator.cpp`, after placing challenge room (`sr.type = SpecialRoomType::CHALLENGE`), add:

```cpp
    for (int dy = -1; dy <= sr.rh; dy++) {
        for (int dx = -1; dx <= sr.rw; dx++) {
            int tx = sr.rx + dx, ty = sr.ry + dy;
            if (gm->door_state_at(tx, ty) != DoorState::NONE) {
                gm->set_door_state(tx, ty, DoorState::SEALED);
                if (dx < 0) { sr.portal_tx = tx - 1; sr.portal_ty = ty; }
                else if (dx >= sr.rw) { sr.portal_tx = tx + 1; sr.portal_ty = ty; }
                else if (dy < 0) { sr.portal_tx = tx; sr.portal_ty = ty - 1; }
                else { sr.portal_tx = tx; sr.portal_ty = ty + 1; }
            }
        }
    }
```

- [ ] **Step 3: Setup challenge controller portal in GameScene::enter_floor()**

In `game_scene.cpp` `enter_floor()`, after building room manager, add:

```cpp
    // Batch 3I: Setup challenge room portal
    for (auto& sr : game_map->special_rooms) {
        if (sr.type == SpecialRoomType::CHALLENGE) {
            _challenge.setup_portal(sr.portal_tx, sr.portal_ty);
            _challenge.set_room_rect(sr.rx, sr.ry, sr.rw, sr.rh);
            break;
        }
    }
```

- [ ] **Step 4: Build**

Run: `cmake --build build --config Release`
Expected: Compiles cleanly.

---

## Task 8: Save/Load — Challenge State

**Files:**
- Modify: `src/game/save/save_manager.cpp`

- [ ] **Step 1: Block save during challenge**

In save function, before writing, add:

```cpp
    if (scene->is_save_blocked()) {
        scene->_presentation.show_message("挑战进行中，无法保存。", 2.0f);
        return false;
    }
```

- [ ] **Step 2: Save challenge phase**

After existing save fields, add:

```cpp
    int ch_phase = static_cast<int>(scene->_challenge.phase());
    file << "chal:" << ch_phase << "\n";
```

- [ ] **Step 3: Load challenge phase**

In load function, after restoring special rooms:

```cpp
    auto ch_line = find_save_line(lines, "chal:");
    if (!ch_line.empty()) {
        int ch_phase = std::atoi(ch_line.c_str() + 5);
        // Only restore PORTAL_ACTIVE or CLEARED (in-progress = reset to INACTIVE)
        if (ch_phase == static_cast<int>(ChallengePhase::PORTAL_ACTIVE)) {
            for (auto& sr : game_map->special_rooms) {
                if (sr.type == SpecialRoomType::CHALLENGE) {
                    _challenge.setup_portal(sr.portal_tx, sr.portal_ty);
                    _challenge.set_room_rect(sr.rx, sr.ry, sr.rw, sr.rh);
                    break;
                }
            }
        }
        // CLEARED or in-progress → reset to INACTIVE
    }
```

- [ ] **Step 4: Build**

Run: `cmake --build build --config Release`
Expected: Compiles cleanly.

---

## Task 9: Integration Test + Desktop Sync

- [ ] **Step 1: Write full flow test**

```cpp
TEST(ChallengeRoomPortal, FullFlow) {
    ChallengeRoomController ctrl;
    ctrl.setup_portal(10, 5);
    EXPECT_EQ(ctrl.phase(), ChallengePhase::PORTAL_ACTIVE);

    Player p; p.key_count = 1;
    EXPECT_TRUE(ctrl.consume_key_for_challenge(p));
    EXPECT_EQ(p.key_count, 0);

    // GameScene would now call enter_challenge_arena()
    // ... wave spawning, combat ...
    ctrl.mark_cleared();
    EXPECT_EQ(ctrl.phase(), ChallengePhase::CLEARED);
    EXPECT_GT(ctrl.return_portal_tx(), 0);
}
```

- [ ] **Step 2: Build and run all tests**

Run: `cmake --build build --config Release && ctest --test-dir build --output-on-failure`
Expected: 52+ tests pass.

- [ ] **Step 3: Sync desktop**

Copy exe to `C:\Users\HP\Desktop\Roguelike-CPP版\roguelike_cpp.exe` (root) + build/.
Copy src/, resources/.

---

## Architecture Summary

```
┌─────────────────────────────────────────────────────┐
│                    GameScene                         │
│  WorldMode: DUNGEON │ CHALLENGE_ARENA               │
│                                                     │
│  DUNGEON mode:                                      │
│    - Main dungeon tick (monsters, AI, physics)       │
│    - Player movement, combat                         │
│    - Portal rendering on challenge room wall         │
│    - E key at portal → enter_challenge_arena()       │
│                                                     │
│  CHALLENGE_ARENA mode:                              │
│    - Main dungeon FROZEN (no monster AI/physics)     │
│    - ChallengeRoomController.tick() runs             │
│    - Wave spawning, combat in sealed room            │
│    - Return portal rendering inside room             │
│    - E key at return portal → exit_challenge_arena() │
│                                                     │
│  Transition:                                        │
│    - enter: save player pos, fade out, teleport      │
│    - exit: fade out, restore player pos, fade in     │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│             ChallengeRoomController                  │
│  ONLY:                                              │
│    - Phase state machine                             │
│    - Wave spawning requests                          │
│    - Monster count tracking                          │
│    - Reward granting                                 │
│    - Portal/return position storage                  │
│                                                     │
│  NOT:                                               │
│    - Player teleportation (GameScene)                │
│    - Map switching (GameScene)                       │
│    - Fade effects (GameScene)                        │
│    - VFX rendering (GameRenderer)                    │
└─────────────────────────────────────────────────────┘
```
