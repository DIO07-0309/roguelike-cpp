#pragma once
// ============================================================
// P0-M2: SimAI teleport target contract (room-domain determinism)
//
// Root cause locked by tests/sim/p0_teleport_test.cpp:
//   old _teleport_player_to_nearest dropped the player on any walkable
//   tile in r=1 ring around the monster — including corridor/door
//   tiles. v1.2.3 room boundary (monster IDLE when rooms differ) then
//   froze the target monster: player at room -1 (corridor) could never
//   attack, monster never approached -> 900s per-run timeout, whole
//   sim batch stuck at F1.
//
// Contract (approved P0-M2):
//   1. Prefer landing tiles whose room_at() equals the target
//      monster's room — player and monster must share a room domain
//      after teleport.
//   2. Never land on: walls, LOCKED/SEALED doors, tiles overlapping
//      another living monster's rect.
//   3. Fallback (no same-room free tile): any walkable tile is
//      acceptable; honest failure (found=false) is also acceptable —
//      caller keeps the old position.
// ============================================================
#include "raylib.h"
#include <vector>

class GameMap;
class RoomManager;
class Monster;

struct TeleportQuery {
    Rectangle player_rect{};               // player collision rect (px)
    Monster* target = nullptr;             // monster to reach
    const GameMap* map = nullptr;
    const RoomManager* rooms = nullptr;
    std::vector<Monster*> extra_monsters;  // other living monsters (occupancy)
};

struct TeleportResult {
    bool found = false;
    int tile_x = -1, tile_y = -1;
};

// Pure target selection — no position mutation, deterministic.
TeleportResult sim_ai_teleport_target(const TeleportQuery& q);
