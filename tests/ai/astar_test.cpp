// G8.2: A* Pathfinder unit tests — pure C++, zero game deps
#include <gtest/gtest.h>
#include "ai/navigation/pathfinder.h"

using namespace ai;

// Helper: build a simple walkable grid from a char map
struct TestGrid {
    int w, h;
    std::vector<char> data;
    TestGrid(int w_, int h_) : w(w_), h(h_), data(w * h, '.') {}
    void wall(int x, int y) { data[x * h + y] = '#'; }
    WalkableFn walkable_fn() {
        auto& d = data; int wh = h;
        return [&d, wh](int x, int y) -> bool {
            if (x < 0 || y < 0 || (size_t)(x * wh + y) >= d.size()) return false;
            return d[x * wh + y] != '#';
        };
    }
};

TEST(Pathfinder, OpenRoomDirectPath) {
    TestGrid g(10, 10);
    PathResult r = find_path({0,0}, {5,5}, g.walkable_fn(), 10, 10);
    EXPECT_TRUE(r.reachable);
    EXPECT_GE(r.path.size(), 6u); // at least start→goal with Manhattan steps
    EXPECT_EQ(r.path.front().x, 0);
    EXPECT_EQ(r.path.front().y, 0);
    EXPECT_EQ(r.path.back().x, 5);
    EXPECT_EQ(r.path.back().y, 5);
}

TEST(Pathfinder, WallObstacleGoesAround) {
    TestGrid g(10, 10);
    // Wall blocking straight path
    for (int y = 0; y < 10; y++) g.wall(4, y); // solid wall at x=4
    g.wall(4, 5); // leave gap? no, all walled
    // Remove one tile to create a gap
    g.data[4 * 10 + 5] = '.'; // open tile at (4,5)

    PathResult r = find_path({0,0}, {9,0}, g.walkable_fn(), 10, 10);
    EXPECT_TRUE(r.reachable);
    EXPECT_GE(r.path.size(), 10u);
    // Must go through the gap at x=4
    bool found_gap = false;
    for (auto& p : r.path) {
        if (p.x == 4 && p.y == 5) found_gap = true;
        EXPECT_NE(g.data[p.x * 10 + p.y], '#'); // never on a wall
    }
    EXPECT_TRUE(found_gap);
}

TEST(Pathfinder, UnreachableGoal) {
    TestGrid g(10, 10);
    // Completely enclose the goal
    for (int x = 0; x < 10; x++) g.wall(x, 0);
    for (int y = 0; y < 10; y++) { g.wall(0, y); g.wall(9, y); }
    for (int x = 0; x < 10; x++) g.wall(x, 9);
    // Only start tile is open
    PathResult r = find_path({5,5}, {1,1}, g.walkable_fn(), 10, 10);
    EXPECT_FALSE(r.reachable);
}

TEST(Pathfinder, SameStartGoal) {
    TestGrid g(10, 10);
    PathResult r = find_path({3,3}, {3,3}, g.walkable_fn(), 10, 10);
    EXPECT_TRUE(r.reachable);
    EXPECT_EQ(r.path.size(), 1u);
}

TEST(Pathfinder, Deterministic) {
    TestGrid g(10, 10);
    PathResult a = find_path({0,0}, {9,9}, g.walkable_fn(), 10, 10);
    PathResult b = find_path({0,0}, {9,9}, g.walkable_fn(), 10, 10);
    EXPECT_TRUE(a.reachable && b.reachable);
    EXPECT_EQ(a.path.size(), b.path.size());
    for (size_t i = 0; i < a.path.size(); i++) {
        EXPECT_EQ(a.path[i].x, b.path[i].x);
        EXPECT_EQ(a.path[i].y, b.path[i].y);
    }
}

TEST(Pathfinder, LargeOpenMap) {
    TestGrid g(30, 30);
    PathResult r = find_path({0,0}, {29,29}, g.walkable_fn(), 30, 30, 2000);
    EXPECT_TRUE(r.reachable);
    EXPECT_EQ(r.path.front().x, 0);
    EXPECT_EQ(r.path.back().x, 29);
}

TEST(Pathfinder, NoDiagonalMove) {
    TestGrid g(10, 10);
    PathResult r = find_path({0,0}, {2,2}, g.walkable_fn(), 10, 10);
    EXPECT_TRUE(r.reachable);
    // With no diagonal moves, Manhattan distance sets minimum path length
    // From (0,0) to (2,2) needs at least 4 steps (right, right, down, down in any order) + start tile = 5
    EXPECT_EQ(r.path.front().x, 0);
    EXPECT_EQ(r.path.back().x, 2);
    EXPECT_EQ(r.path.back().y, 2);
    // Verify no diagonal jumps: each step should change either x OR y by exactly 1
    for (size_t i = 1; i < r.path.size(); i++) {
        int dx = std::abs(r.path[i].x - r.path[i-1].x);
        int dy = std::abs(r.path[i].y - r.path[i-1].y);
        EXPECT_TRUE((dx == 1 && dy == 0) || (dx == 0 && dy == 1));
    }
}
