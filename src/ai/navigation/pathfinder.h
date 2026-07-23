#pragma once
// G8.2: A* Pathfinder — grid-based shortest path with obstacle avoidance.
// Pure C++ module, zero game dependencies (takes a walkable callback).

#include <vector>
#include <functional>
#include <cstdint>

namespace ai {

struct Vec2i { int x, y; };

struct PathResult {
    std::vector<Vec2i> path;     // from start to goal (inclusive)
    int nodes_visited = 0;
    bool reachable = false;
};

// Callback: return true if tile (x,y) is walkable
using WalkableFn = std::function<bool(int x, int y)>;

// A* search. Returns shortest path or empty if unreachable.
// max_steps limits search to prevent runaway on large maps.
PathResult find_path(Vec2i start, Vec2i goal,
                     WalkableFn is_walkable,
                     int map_w, int map_h,
                     int max_steps = 400);

// Convenience: path from tile to tile on a GameMap
// (declared but requires game_map.h at call site)
// PathResult find_path_on_map(Vec2i start, Vec2i goal, const class GameMap& map);

} // namespace ai
