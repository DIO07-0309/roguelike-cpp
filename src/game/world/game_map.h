#pragma once
#include <vector>
#include <string>
#include "entity.h"
#include "raylib.h"
#include "special_room.h"
#include "biome.h"   // M4f: TilePalette

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
enum class TileType { FLOOR, WALL, STAIRS_DOWN, LAVA, DOOR };  // Phase 2: DOOR — 静态开启门

struct Tile {
    TileType type = TileType::WALL;
    bool is_walkable = false;
    bool is_visible = false;    // 当前帧是否在 FOV 内
    bool is_explored = false;   // 是否曾被玩家探索过

    static Tile floor()  { return {TileType::FLOOR, true, false, false}; }
    static Tile wall()   { return {TileType::WALL, false, false, false}; }
    static Tile stairs() { return {TileType::STAIRS_DOWN, true, false, false}; }
    static Tile lava()   { return {TileType::LAVA, true, false, false}; }
    static Tile door()   { return {TileType::DOOR, true, false, false}; }  // Phase 2
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
    TileType tile_at(int tx, int ty) const {  // M4b: tile 类型查询 (lava 感知)
        return (_in_bounds(tx, ty)) ? _tiles[ty][tx].type : TileType::WALL;
    }

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

    // M4f: biome palette (绘制程序化像素纹理的基色)
    void set_palette(const TilePalette* palette);
    const TilePalette& palette() const { return _palette; }
    bool has_palette() const { return _has_palette; }

    // Phase 1: FOV 可见性
    bool isVisible(int x, int y) const;
    bool isExplored(int x, int y) const;
    bool blocks_sight(int x, int y) const;
    void update_fov(int center_x, int center_y, int radius);
    void reset_visibility();

    void draw(float cam_x, float cam_y, int screen_w, int screen_h) const;

private:
    std::vector<std::vector<Tile>> _tiles;
    bool _in_bounds(int tx, int ty) const;
    void _init_walls();
    TilePalette _palette;      // M4f: 当前 biome 调色板
    bool _has_palette = false;
};
