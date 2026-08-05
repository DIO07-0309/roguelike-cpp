#include "game_map.h"
#include "config.h"
#include "core/logger.h"
#include "resource_manager.h"                 // M4f: 纹理缓存
#include "game/rendering/sprite_renderer.h"   // M4f: 像素绘制
#include <cmath>
#include <cstdio>

GameMap::GameMap(int w, int h, int ts)
    : width(w), height(h), tile_size(ts),
      pixel_width(w * ts), pixel_height(h * ts) {
    _init_walls();
}

void GameMap::_init_walls() {
    _tiles.resize(height);
    for (int y = 0; y < height; y++) {
        _tiles[y].resize(width);
        for (int x = 0; x < width; x++) {
            _tiles[y][x] = Tile::wall();
        }
    }
}

Vector2 GameMap::tile_to_pixel(int tx, int ty) const {
    return {(float)tx * tile_size, (float)ty * tile_size};
}
std::pair<int,int> GameMap::pixel_to_tile(float px, float py) const {
    return {(int)(px / tile_size), (int)(py / tile_size)};
}

void GameMap::load_from_template(const std::vector<std::string>& tmpl) {
    for (int y = 0; y < std::min((int)tmpl.size(), height); y++) {
        const auto& line = tmpl[y];
        for (int x = 0; x < std::min((int)line.size(), width); x++) {
            if (line[x] == '#') _tiles[y][x] = Tile::wall();
            else if (line[x] == '.') _tiles[y][x] = Tile::floor();
        }
    }
}

void GameMap::set_tile(int x, int y, TileType t) {
    if (_in_bounds(x, y)) {
        _tiles[y][x].type = t;
        _tiles[y][x].is_walkable = (t != TileType::WALL);
    }
}

bool GameMap::is_walkable(int tx, int ty) const {
    if (!_in_bounds(tx, ty)) return false;
    return _tiles[ty][tx].is_walkable;
}

bool GameMap::is_rect_walkable(Rectangle r) const {
    float pts[8][2] = {
        {r.x, r.y}, {r.x + r.width - 1, r.y},
        {r.x, r.y + r.height - 1}, {r.x + r.width - 1, r.y + r.height - 1},
        {r.x + r.width/2, r.y}, {r.x + r.width/2, r.y + r.height - 1},
        {r.x, r.y + r.height/2}, {r.x + r.width - 1, r.y + r.height/2}
    };
    for (auto& [px, py] : pts) {
        auto [tx, ty] = pixel_to_tile(px, py);
        if (!is_walkable(tx, ty)) return false;
    }
    return true;
}

bool GameMap::_in_bounds(int tx, int ty) const {
    return tx >= 0 && tx < width && ty >= 0 && ty < height;
}

SpecialRoom* GameMap::get_special_room_at(int tile_x, int tile_y) {
    for (auto& sr : special_rooms) {
        if (tile_x >= sr.rx && tile_x < sr.rx + sr.rw &&
            tile_y >= sr.ry && tile_y < sr.ry + sr.rh)
            return &sr;
    }
    return nullptr;
}

const SpecialRoom* GameMap::get_special_room_at(int tile_x, int tile_y) const {
    for (auto& sr : special_rooms) {
        if (tile_x >= sr.rx && tile_x < sr.rx + sr.rw &&
            tile_y >= sr.ry && tile_y < sr.ry + sr.rh)
            return &sr;
    }
    return nullptr;
}

void GameMap::set_palette(const TilePalette* palette) {
    _has_palette = (palette != nullptr);
    if (_has_palette) _palette = *palette;
}

void GameMap::draw(float cam_x, float cam_y, int sw, int sh) const {
    int sc = std::max(0, (int)(cam_x / tile_size));
    int sr = std::max(0, (int)(cam_y / tile_size));
    int ec = std::min(width, (int)((cam_x + sw) / tile_size) + 1);
    int er = std::min(height, (int)((cam_y + sh) / tile_size) + 1);

    // M4f.4: 数据驱动素材 (wall/floor) 优先, 未配置回退程序化像素
    Color wall_c  = _has_palette ? _palette.wall_face : Color{60, 60, 80, 255};
    Color floor_c = _has_palette ? _palette.floor_base : Color{25, 25, 35, 255};
    char wall_key[40], floor_key[40];
    snprintf(wall_key, sizeof(wall_key), "wall_%02x%02x%02x",
        wall_c.r, wall_c.g, wall_c.b);
    snprintf(floor_key, sizeof(floor_key), "floor_%02x%02x%02x",
        floor_c.r, floor_c.g, floor_c.b);
    auto& rm = ResourceManager::inst();
    SpriteDef wall_def, floor_def;
    Texture2D wall_data = rm.sprite_by_key("wall", wall_def);
    Texture2D floor_data = rm.sprite_by_key("floor", floor_def);
    Texture2D wall_tex = wall_data.id > 0 ? wall_data
                       : rm.procedural_tile(wall_key, wall_c, true);
    Texture2D floor_tex = floor_data.id > 0 ? floor_data
                        : rm.procedural_tile(floor_key, floor_c, false);
    SpriteDef sd; sd.frame_w = tile_size; sd.frame_h = tile_size;

    for (int y = sr; y < er; y++) {
        for (int x = sc; x < ec; x++) {
            float dx = x * tile_size - cam_x;
            float dy = y * tile_size - cam_y;
            const auto& t = _tiles[y][x];

            if (t.type == TileType::WALL) {
                if (wall_tex.id > 0) {
                    SpriteDef& wd = wall_data.id > 0 ? wall_def : sd;
                    SpriteRenderer::draw_sprite(wall_tex, wd, 0,
                        {dx, dy, (float)tile_size, (float)tile_size});
                } else
                    DrawRectangle(dx, dy, tile_size, tile_size, wall_c);
                DrawRectangleLines(dx, dy, tile_size, tile_size, {40, 40, 55, 255});
            } else if (t.type == TileType::FLOOR) {
                const SpecialRoom* sr = get_special_room_at(x, y);
                if (sr) {
                    // 特殊房间地板颜色区分
                    Color base;
                    switch (sr->type) {
                        case SpecialRoomType::ALTAR:      base = {50, 35, 15, 255}; break;
                        case SpecialRoomType::TREASURE:   base = {25, 35, 60, 255}; break;
                        case SpecialRoomType::FOUNTAIN:   base = {20, 45, 25, 255}; break;
                        case SpecialRoomType::SHOP:       base = {50, 45, 15, 255}; break;  // gold
                        case SpecialRoomType::BLACKSMITH: base = {55, 30, 20, 255}; break;  // orange
                        case SpecialRoomType::LIBRARY:    base = {15, 30, 55, 255}; break;  // blue
                        case SpecialRoomType::GAMBLER:    base = {45, 15, 50, 255}; break;  // purple
                        case SpecialRoomType::SHRINE:     base = {40, 40, 10, 255}; break;  // gold-dark
                        case SpecialRoomType::SECRET:     base = {50, 10, 10, 255}; break;  // deep red
                        default: base = {25, 25, 35, 255}; break;
                    }
                    if (sr->triggered) {
                        base.r = (unsigned char)(base.r * 0.55f);
                        base.g = (unsigned char)(base.g * 0.55f);
                        base.b = (unsigned char)(base.b * 0.55f);
                    }
                    DrawRectangle(dx, dy, tile_size, tile_size, base);
                    DrawRectangleLines(dx, dy, tile_size, tile_size, {35, 35, 45, 255});
                    // 房间中心绘制图标
                    if (x == sr->cx && y == sr->cy) {
                        const char* icon = "?";
                        switch (sr->type) {
                            case SpecialRoomType::ALTAR:      icon = "+"; break;
                            case SpecialRoomType::TREASURE:   icon = "$"; break;
                            case SpecialRoomType::FOUNTAIN:   icon = "~"; break;
                            case SpecialRoomType::SHOP:       icon = "S"; break;
                            case SpecialRoomType::BLACKSMITH: icon = "B"; break;
                            case SpecialRoomType::LIBRARY:    icon = "L"; break;
                            case SpecialRoomType::GAMBLER:    icon = "G"; break;
                            case SpecialRoomType::SHRINE:     icon = "!"; break;
                            case SpecialRoomType::SECRET:     icon = "?"; break;
                        }
                        Color ic = sr->triggered ? Color{100, 100, 100, 255}
                                                 : Color{255, 255, 200, 255};
                        DrawText(icon, (int)dx + 10, (int)dy + 5, 20, ic);
                    }
                } else {
                    if (floor_tex.id > 0) {
                        SpriteDef& fd = floor_data.id > 0 ? floor_def : sd;
                        SpriteRenderer::draw_sprite(floor_tex, fd, 0,
                            {dx, dy, (float)tile_size, (float)tile_size});
                    } else
                        DrawRectangle(dx, dy, tile_size, tile_size, floor_c);
                    // 细微网格线
                    DrawRectangleLines(dx, dy, tile_size, tile_size, {35, 35, 45, 255});
                }
            } else if (t.type == TileType::STAIRS_DOWN) {
                DrawRectangle(dx, dy, tile_size, tile_size, {50, 40, 20, 255});
                DrawRectangleLines(dx+2, dy+2, tile_size-4, tile_size-4, {255, 200, 50, 255});
                DrawText(">", dx + 10, dy + 6, 20, {255, 220, 100, 255});
            }

            // D4 Step1: 事件房间中心绘制标记
            if (event_room_index >= 0 && x == event_tile_x && y == event_tile_y) {
                float pulse = 2 + sinf((float)GetTime() * 5) * 2;
                Color ec = event_triggered ? Color{80, 80, 80, 120} : Color{255, 200, 100, 200};
                DrawRectangleLinesEx({dx - pulse, dy - pulse, tile_size + pulse*2, tile_size + pulse*2}, float(1.5), ec);
                DrawText("?", dx + 10, dy + 5, 18, event_triggered ? Color{100, 100, 100, 200} : Color{255, 220, 100, 255});
            }

            // D2 Step5: Arena 元素绘制
            auto* arena = get_special_room_at(x, y) ? nullptr : get_arena_at(x, y);
            if (arena) {
                switch (arena->type) {
                case ArenaObjectType::EXPLOSIVE_BARREL:
                    DrawRectangle(dx+6, dy+6, tile_size-12, tile_size-12, {180, 100, 30, 255});
                    DrawText("!", dx+12, dy+5, 18, {255, 200, 50, 255}); break;
                case ArenaObjectType::HEALING_TOTEM:
                    DrawRectangle(dx+4, dy+4, tile_size-8, tile_size-8, {30, 140, 60, 255});
                    DrawText("+", dx+11, dy+5, 18, {100, 255, 120, 255}); break;
                case ArenaObjectType::POISON_POOL: {
                    // M4a-fix: 亮绿 + 脉动描边 (原深绿不易察觉, 玩家踩毒不自知)
                    float pulse = 0.6f + 0.4f * sinf((float)GetTime() * 5.0f);
                    DrawRectangle(dx+2, dy+2, tile_size-4, tile_size-4, {40, 150, 55, 210});
                    DrawRectangleLines(dx+2, dy+2, tile_size-4, tile_size-4,
                                       {90, 250, 90, (unsigned char)(170 * pulse)});
                    DrawText("~", dx+10, dy+5, 16, {170, 255, 170, 230});
                } break;
                case ArenaObjectType::ROCK: {
                    DrawRectangle(dx+2, dy+8, tile_size-4, tile_size-10, {100, 95, 100, 255});
                    DrawRectangleLines(dx+2, dy+8, tile_size-4, tile_size-10, {130, 125, 130, 255});
                } break;
                case ArenaObjectType::SPIKE:
                    DrawTriangle({dx+tile_size/2, dy+2}, {dx+2, dy+tile_size-4},
                                {dx+tile_size-2, dy+tile_size-4}, {180, 50, 50, 255});
                    break;
                }
            }
        }
    }
}

ArenaObject* GameMap::get_arena_at(int tile_x, int tile_y) {
    for (auto& ao : arena_objects)
        if (ao.tile_x == tile_x && ao.tile_y == tile_y && ao.active)
            return &ao;
    return nullptr;
}

const ArenaObject* GameMap::get_arena_at(int tile_x, int tile_y) const {
    for (auto& ao : arena_objects)
        if (ao.tile_x == tile_x && ao.tile_y == tile_y && ao.active)
            return &ao;
    return nullptr;
}
