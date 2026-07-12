#pragma once
#include <vector>
#include <string>
#include <random>
#include <memory>
#include "game_map.h"
#include "config.h"
#include "special_room.h"

// ============================================================
// BSPNode — 二分空间划分节点
// ============================================================
struct BSPNode {
    int x, y, w, h;
    BSPNode* left = nullptr;
    BSPNode* right = nullptr;
    // room: (rx, ry, rw, rh)
    int rx = 0, ry = 0, rw = 0, rh = 0;
    bool has_room = false;

    BSPNode(int x_, int y_, int w_, int h_);
    ~BSPNode();
    bool is_leaf() const;
    std::pair<int,int> center() const;
    std::pair<int,int> room_center() const;
};

// ============================================================
// DungeonGenerator — BSP 地牢生成器
// ============================================================
class DungeonGenerator {
public:
    DungeonGenerator(int w, int h, int ts,
                     int min_part = 8, int min_room = 5, int margin = 1);

    std::shared_ptr<GameMap> generate(uint32_t seed = 0, int special_room_count = 3,
                                     int arena_density = 0);
    std::vector<std::pair<int,int>> get_room_centers() const;
    std::vector<SpecialRoom> get_special_rooms() const { return _special_rooms; }

private:
    int _w, _h, _ts, _min_part, _min_room, _margin;
    BSPNode* _root = nullptr;
    std::vector<std::tuple<int,int,int,int>> _rooms;
    std::vector<std::tuple<int,int,int,int>> _corridors;
    std::vector<SpecialRoom> _special_rooms;

    // Seed 驱动 (B8)
    uint32_t _seed = 0;
    std::mt19937 _local_rng;

    int _rand_int(int max_exclusive);
    void _assign_special_rooms(int count);
    void _assign_arena_objects(GameMap* gm, int density);

    void _partition(BSPNode* node);
    void _create_child_nodes(BSPNode* node, bool vertical, int split);
    void _create_rooms(BSPNode* node);
    void _connect_rooms(BSPNode* node);
    std::pair<int,int> _pick_room(BSPNode* node);
    std::vector<std::string> _build_template();
    void _carve_rect(std::vector<std::string>& grid, int x, int y, int w, int h);
    void _carve_corridor(std::vector<std::string>& grid, int x1, int y1, int x2, int y2);
    void _carve_line(std::vector<std::string>& grid, int x1, int y1, int x2, int y2, int width);
    void _carve_diamond(std::vector<std::string>& grid, int cx, int cy, int r);
};
