# Door Visual System — Design Spec

> **Date:** 2026-08-28
> **Status:** Approved
> **Prerequisite:** Batch A (v1.2.4) — DoorState model + collision fix

## Goal

Replace procedural brown-rectangle doors with Kenney Tiny Dungeon tileset sprites.
4 door states (OPEN/CLOSED/LOCKED/SEALED) get 4 distinct visuals + simple 0.3s transition animation.

## §1 DoorRenderer

New files: `src/game/rendering/door_renderer.h`, `src/game/rendering/door_renderer.cpp`

```cpp
struct DoorAnimState {
    DoorState current;
    DoorState previous;
    float progress;  // 0→1, duration 0.3s
};

class DoorRenderer {
    Texture2D tex_open, tex_closed, tex_locked, tex_sealed;
    std::map<uint64_t, DoorAnimState> _anims;
    float _transition_duration = 0.3f;
public:
    static DoorRenderer& inst();
    void init();                           // load 4 PNGs, scale to 32x32
    void update(float dt);                 // advance progress
    void on_state_change(int tx, int ty, DoorState from, DoorState to);
    void draw_door(int tx, int ty, DoorState state, bool bright);
};
```

## §2 State → Sprite Mapping

| State | Base tile | Overlay |
|-------|-----------|---------|
| OPEN | `tile_0003` (open archway) | none |
| CLOSED | `tile_0022` (wooden door + handle) | none |
| LOCKED | `tile_0022` (wooden door + handle) | red lock icon (code-drawn: rect + arc) |
| SEALED | `tile_0018` (dark door) | purple seal cross (code-drawn: lines + circle, pulsing) |

Source tiles from `assets/vendor/kenney_tiny_dungeon/Tiles/`.

Overlays drawn via DrawRectangle/DrawCircle/DrawLineEx in `draw_door()`.
Overlay only drawn when progress >= 1.0 (not during transition).

## §3 Transition Animation

- State change triggers 0.3s transition
- During transition: old tile alpha 1→0, new tile alpha 0→1
- New tile scale: 0.8→1.0
- Lerp: `alpha = Lerp(progress, old, new)`
- `progress += dt / 0.3` per frame

## §4 Integration

Replace `game_map.cpp:363-369` DOOR branch:

```cpp
// Before: procedural brown rect
// After:
DoorRenderer::inst().draw_door(tx, ty, t.door_state, bright);
```

Call `DoorRenderer::inst().on_state_change()` from `GameMap::set_door_state()`.
Call `DoorRenderer::inst().init()` from `GameScene` init.
Call `DoorRenderer::inst().update(dt)` from `GameScene::_update_fov()` or main tick.

## §5 Testing

| Test | Content |
|------|---------|
| `door_renderer_test` | 4 textures loaded; state change triggers anim; progress 0→1; final tile 32x32 |
| Regression | 43+ ctest all pass |
| Manual | Room encounter door close anim; kill clear door open anim; LOCKED shows red lock |

## Files Changed

| File | Change |
|------|--------|
| `src/game/rendering/door_renderer.h` | **NEW** — DoorRenderer class |
| `src/game/rendering/door_renderer.cpp` | **NEW** — load, update, draw, overlay |
| `src/game/world/game_map.cpp` | DOOR branch → `DoorRenderer::inst().draw_door()` |
| `src/game/scenes/game_scene.cpp` | Init + update DoorRenderer |
| `tests/world/door_renderer_test.cpp` | **NEW** — unit tests |
| `tests/CMakeLists.txt` | Register new test |
