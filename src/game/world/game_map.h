#pragma once
#include <vector>
#include <string>
#include "entity.h"
#include "raylib.h"
#include "special_room.h"

// 前向声明 (避免循环依赖)
enum class EventType : int;

// ============================================================
// D2 Step5: ArenaObject — 战场环境元素
// ============================================================
enum class ArenaObjectType { EXPLOSIVE_BARREL, HEALING_TOTEM, POISON_POOL, ROCK, SPIKE };

struct ArenaObject {
    ArenaObjectType type;
    int tile_x, tile_y;
    bool active = true;
    float timer = 0.0f;
};

// ============================================================
// Tile / GameMap — 地图数据结构
// ============================================================
enum class TileType { FLOOR, WALL, STAIRS_DOWN };

struct Tile {
    TileType type = TileType::WALL;
    bool is_walkable = false;

    static Tile floor()  { return {TileType::FLOOR, true}; }
    static Tile wall()   { return {TileType::WALL, false}; }
    static Tile stairs() { return {TileType::STAIRS_DOWN, true}; }
};

class GameMap {
public:
    int width, height, tile_size;
    int pixel_width, pixel_height;

    GameMap(int w, int h, int ts);

    void load_from_template(const std::vector<std::string>& tmpl);
    void set_tile(int x, int y, TileType type);

    bool is_walkable(int tx, int ty) const;
    bool is_rect_walkable(Rectangle rect) const;

    // 特殊房间 (B8)
    std::vector<SpecialRoom> special_rooms;
    SpecialRoom* get_special_room_at(int tile_x, int tile_y);
    const SpecialRoom* get_special_room_at(int tile_x, int tile_y) const;

    // D2 Step5: 战场元素
    std::vector<ArenaObject> arena_objects;
    ArenaObject*       get_arena_at(int tile_x, int tile_y);
    const ArenaObject* get_arena_at(int tile_x, int tile_y) const;

    // D4 Step1: 动态事件
    int   event_room_index = -1;
    int   event_tile_x = 0, event_tile_y = 0;
    bool  event_triggered = false;
    EventType event_type = (EventType)0;  // EventType::NONE

    Vector2 tile_to_pixel(int tx, int ty) const;
    std::pair<int,int> pixel_to_tile(float px, float py) const;

    void draw(float cam_x, float cam_y, int screen_w, int screen_h) const;

private:
    std::vector<std::vector<Tile>> _tiles;
    bool _in_bounds(int tx, int ty) const;
    void _init_walls();
};
