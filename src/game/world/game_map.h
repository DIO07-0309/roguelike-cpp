#pragma once
#include <vector>
#include <string>
#include "entity.h"
#include "raylib.h"

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

    Vector2 tile_to_pixel(int tx, int ty) const {
        return {(float)tx * tile_size, (float)ty * tile_size};
    }
    std::pair<int,int> pixel_to_tile(float px, float py) const {
        return {(int)(px / tile_size), (int)(py / tile_size)};
    }

    void draw(float cam_x, float cam_y, int screen_w, int screen_h) const;

private:
    std::vector<std::vector<Tile>> _tiles;
    bool _in_bounds(int tx, int ty) const;
    void _init_walls();
};
