#pragma once
#include "raylib.h"
#include "game/world/game_map.h"
#include <map>
#include <cstdint>

// ============================================================
// DoorRenderer — Kenney tile-based door sprites + transition anim
// 4 states: OPEN(archway) / CLOSED(wood door) / LOCKED(door+lock) / SEALED(door+seal)
// ============================================================

struct DoorAnimState {
    DoorState current  = DoorState::OPEN;
    DoorState previous = DoorState::OPEN;
    float progress     = 1.0f;  // 0→1
};

class DoorRenderer {
public:
    static DoorRenderer& inst();

    void init();
    void update(float dt);
    void on_state_change(int tx, int ty, DoorState from, DoorState to);
    void draw_door(int tx, int ty, DoorState state, bool bright);

    bool is_loaded() const { return _loaded; }

private:
    DoorRenderer() = default;

    static uint64_t _key(int tx, int ty);
    Texture2D _tex_for(DoorState s) const;
    void _draw_lock_icon(float cx, float cy, bool bright);
    void _draw_seal_icon(float cx, float cy, bool bright);

    Texture2D _tex_open   = {0};
    Texture2D _tex_closed = {0};
    Texture2D _tex_locked = {0};
    Texture2D _tex_sealed = {0};
    bool _loaded = false;
    float _transition_dur = 0.3f;
    std::map<uint64_t, DoorAnimState> _anims;
};
