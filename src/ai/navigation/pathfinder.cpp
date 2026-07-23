// G8.2: A* grid pathfinder implementation
#include "ai/navigation/pathfinder.h"
#include <queue>
#include <vector>
#include <limits>
#include <cstdlib>

namespace ai {

// ── Internal node ──────────────────────────────────────────
struct AStarNode {
    int idx;       // x*height + y
    float g, f;
    bool operator>(const AStarNode& o) const { return f > o.f; }
};

// Manhattan distance on grid
static inline float heuristic(Vec2i a, Vec2i b) {
    return (float)(std::abs(a.x - b.x) + std::abs(a.y - b.y));
}

static inline int pack(int x, int y, int h) { return x * h + y; }
static inline Vec2i unpack(int idx, int h) { return {idx / h, idx % h}; }

static const int DX[4] = {1, -1, 0,  0};
static const int DY[4] = {0,  0, 1, -1};

PathResult find_path(Vec2i start, Vec2i goal,
                     WalkableFn is_walkable,
                     int map_w, int map_h,
                     int max_steps) {
    PathResult result;
    int size = map_w * map_h;

    // Flat arrays for performance
    std::vector<float> g_cost(size, std::numeric_limits<float>::max());
    std::vector<int> parent(size, -1);
    std::vector<bool> closed(size, false);

    std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> open;

    int si = pack(start.x, start.y, map_h);
    int gi = pack(goal.x, goal.y, map_h);

    g_cost[si] = 0;
    open.push({si, 0, heuristic(start, goal)});

    while (!open.empty() && result.nodes_visited < max_steps) {
        AStarNode cur = open.top(); open.pop();
        int ci = cur.idx;
        if (closed[ci]) continue;
        closed[ci] = true;
        result.nodes_visited++;

        if (ci == gi) {
            // Reconstruct path
            result.reachable = true;
            int p = ci;
            while (p != -1) {
                result.path.push_back(unpack(p, map_h));
                p = parent[p];
            }
            // Reverse so path is start→goal
            for (int i = 0; i < (int)result.path.size() / 2; i++)
                std::swap(result.path[i], result.path[result.path.size() - 1 - i]);
            return result;
        }

        Vec2i cpos = unpack(ci, map_h);
        for (int d = 0; d < 4; d++) {
            int nx = cpos.x + DX[d], ny = cpos.y + DY[d];
            if (nx < 0 || nx >= map_w || ny < 0 || ny >= map_h) continue;
            int ni = pack(nx, ny, map_h);
            if (closed[ni]) continue;
            if (!is_walkable(nx, ny)) continue;

            float ng = g_cost[ci] + 1.0f;
            if (ng < g_cost[ni]) {
                g_cost[ni] = ng;
                parent[ni] = ci;
                float f = ng + heuristic({nx, ny}, goal);
                open.push({ni, ng, f});
            }
        }
    }

    result.reachable = false;
    return result;
}

} // namespace ai
