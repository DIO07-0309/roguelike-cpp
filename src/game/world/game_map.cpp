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
            else if (line[x] == 'D') _tiles[y][x] = Tile::door();  // Phase 2
        }
    }
}

void GameMap::set_tile(int x, int y, TileType t) {
    if (_in_bounds(x, y)) {
        _tiles[y][x].type = t;
        _tiles[y][x].is_walkable = (t != TileType::WALL);
        // Batch 1: 门态归一化 — 非 DOOR tile 门态必须为 NONE, DOOR tile 置默认 OPEN
        _tiles[y][x].door_state = (t == TileType::DOOR) ? DoorState::OPEN : DoorState::NONE;
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

// ── Phase 1: FOV ──────────────────────────────────────────

bool GameMap::isVisible(int x, int y) const {
    return _in_bounds(x, y) && _tiles[y][x].is_visible;
}

bool GameMap::isExplored(int x, int y) const {
    return _in_bounds(x, y) && _tiles[y][x].is_explored;
}

bool GameMap::blocks_sight(int x, int y) const {
    if (!_in_bounds(x, y)) return true;
    // Batch 1: WALL 恒阻挡; DOOR 按状态 (OPEN 透射 / CLOSED 阻挡); 其余类型透射
    // 未来可扩展为独立字段 (支持半透明/可破坏墙壁)
    const Tile& t = _tiles[y][x];
    if (t.type == TileType::WALL) return true;
    if (t.type == TileType::DOOR) return t.door_state == DoorState::CLOSED;
    return false;
}

// ── Batch 1: Door 状态 API ─────────────────────────────────

DoorState GameMap::door_state_at(int tx, int ty) const {
    if (!_in_bounds(tx, ty)) return DoorState::NONE;
    const Tile& t = _tiles[ty][tx];
    return (t.type == TileType::DOOR) ? t.door_state : DoorState::NONE;
}

bool GameMap::set_door_state(int tx, int ty, DoorState s) {
    if (!_in_bounds(tx, ty)) return false;
    Tile& t = _tiles[ty][tx];
    if (t.type != TileType::DOOR) return false;   // 仅 DOOR tile 可设门态
    if (t.door_state == s) return true;
    t.door_state = s;
    // 语义落点: OPEN=可走, CLOSED=不可走 (is_walkable 物化字段单点更新)
    t.is_walkable = (s == DoorState::OPEN);
    return true;
}

bool GameMap::is_door(int tx, int ty) const {
    return _in_bounds(tx, ty) && _tiles[ty][tx].type == TileType::DOOR;
}

void GameMap::reset_visibility() {
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            _tiles[y][x].is_visible = false;
            _tiles[y][x].is_explored = false;
        }
}

void GameMap::update_fov(int cx, int cy, int radius) {
    // 1. 清除所有 is_visible
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            _tiles[y][x].is_visible = false;

    // 2. 射线投射: 360 条, 每度一条
    for (int deg = 0; deg < 360; deg++) {
        float rad = deg * DEG2RAD;
        float dx = cosf(rad);
        float dy = sinf(rad);

        for (float dist = 0; dist <= radius; dist += 0.5f) {
            int tx = cx + static_cast<int>(roundf(dx * dist));
            int ty = cy + static_cast<int>(roundf(dy * dist));
            if (!_in_bounds(tx, ty)) break;

            _tiles[ty][tx].is_visible = true;
            _tiles[ty][tx].is_explored = true;

            if (blocks_sight(tx, ty)) break;
        }
    }
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

// Phase 1: 颜色变暗 (已探索但不在视野内时使用)
static Color _dim(Color c, float brightness) {
    return {
        (unsigned char)(c.r * brightness),
        (unsigned char)(c.g * brightness),
        (unsigned char)(c.b * brightness),
        c.a
    };
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

            // Phase 1: 未探索 → 不渲染
            if (!t.is_explored) continue;

            // Phase 1: 可见性亮度 (1.0=正常, 0.6=昏暗)
            float bright = t.is_visible ? 1.0f : 0.6f;

            if (t.type == TileType::WALL) {
                if (wall_tex.id > 0) {
                    SpriteDef& wd = wall_data.id > 0 ? wall_def : sd;
                    SpriteRenderer::draw_sprite(wall_tex, wd, 0,
                        {dx, dy, (float)tile_size, (float)tile_size},
                        _dim({255,255,255,255}, bright));
                } else
                    DrawRectangle(dx, dy, tile_size, tile_size, _dim(wall_c, bright));
                DrawRectangleLines(dx, dy, tile_size, tile_size, _dim({40, 40, 55, 255}, bright));
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
                    DrawRectangle(dx, dy, tile_size, tile_size, _dim(base, bright));
                    DrawRectangleLines(dx, dy, tile_size, tile_size, _dim({35, 35, 45, 255}, bright));
                    // 房间中心绘制图标 (M4f.5: 素材装饰优先, 缺配回退字符)
                    if (x == sr->cx && y == sr->cy) {
                        const char* pkey = nullptr;
                        switch (sr->type) {
                            case SpecialRoomType::ALTAR:      pkey = "room_altar"; break;
                            case SpecialRoomType::TREASURE:   pkey = "room_chest"; break;
                            case SpecialRoomType::FOUNTAIN:   pkey = "room_spring"; break;
                            case SpecialRoomType::SHOP:       pkey = "room_shop"; break;
                            case SpecialRoomType::BLACKSMITH: pkey = "room_blacksmith"; break;
                            case SpecialRoomType::LIBRARY:    pkey = "room_library"; break;
                            case SpecialRoomType::GAMBLER:    pkey = "room_gambler"; break;
                            case SpecialRoomType::SHRINE:     pkey = "room_shrine"; break;
                            case SpecialRoomType::SECRET:     pkey = "room_secret"; break;
                            default: break;
                        }
                        SpriteDef pdef;
                        Texture2D ptex = pkey
                            ? rm.sprite_by_key(pkey, pdef) : Texture2D{0};
                        if (ptex.id > 0 && !sr->triggered) {
                            float isz = tile_size * 0.75f;
                            SpriteRenderer::draw_sprite(ptex, pdef, 0,
                                {dx + (tile_size - isz) / 2, dy + (tile_size - isz) / 2,
                                 isz, isz});
                        } else {
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
                            DrawText(icon, (int)dx + 10, (int)dy + 5, 20, _dim(ic, bright));
                        }
                    }
                } else {
                    if (floor_tex.id > 0) {
                        SpriteDef& fd = floor_data.id > 0 ? floor_def : sd;
                        SpriteRenderer::draw_sprite(floor_tex, fd, 0,
                            {dx, dy, (float)tile_size, (float)tile_size},
                            _dim({255,255,255,255}, bright));
                    } else
                        DrawRectangle(dx, dy, tile_size, tile_size, _dim(floor_c, bright));
                    // 细微网格线
                    DrawRectangleLines(dx, dy, tile_size, tile_size, _dim({35, 35, 45, 255}, bright));
                }
            } else if (t.type == TileType::STAIRS_DOWN) {
                DrawRectangle(dx, dy, tile_size, tile_size, _dim({50, 40, 20, 255}, bright));
                DrawRectangleLines(dx+2, dy+2, tile_size-4, tile_size-4, _dim({255, 200, 50, 255}, bright));
                DrawText(">", dx + 10, dy + 6, 20, _dim({255, 220, 100, 255}, bright));
            } else if (t.type == TileType::LAVA) {
                // M4b: 熔岩地砖 — 橙红脉动 (Boss 房机制地形)
                float pulse = 0.7f + 0.3f * sinf((float)GetTime() * 4.0f);
                Color lc = _dim({ (unsigned char)(200 + 40 * pulse), 60, 15, 255 }, bright);
                DrawRectangle(dx, dy, tile_size, tile_size, lc);
                DrawRectangleLines(dx, dy, tile_size, tile_size,
                                   _dim({255, 120, 40, (unsigned char)(160 * pulse)}, bright));
                DrawCircle(dx + tile_size/2, dy + tile_size/2, 5.0f * pulse + 2.0f,
                           _dim({255, 160, 60, (unsigned char)(120 + 80 * pulse)}, bright));
            } else if (t.type == TileType::DOOR) {
                // Phase 2: 门 — 棕色木质标记
                DrawRectangle(dx, dy, tile_size, tile_size, _dim({140, 100, 50, 255}, bright));
                DrawRectangleLines(dx, dy, tile_size, tile_size, _dim({100, 70, 30, 255}, bright));
                DrawCircle(dx + tile_size/2, dy + tile_size/2, 3.0f,
                           _dim({200, 180, 100, 255}, bright));
            }

            // D4 Step1: 事件房间中心绘制标记
            if (event_room_index >= 0 && x == event_tile_x && y == event_tile_y) {
                float pulse = 2 + sinf((float)GetTime() * 5) * 2;
                Color ec = event_triggered ? Color{80, 80, 80, 120} : Color{255, 200, 100, 200};
                DrawRectangleLinesEx({dx - pulse, dy - pulse, tile_size + pulse*2, tile_size + pulse*2}, float(1.5), _dim(ec, bright));
                DrawText("?", dx + 10, dy + 5, 18, _dim(event_triggered ? Color{100, 100, 100, 200} : Color{255, 220, 100, 255}, bright));
            }

            // D2 Step5: Arena 元素绘制
            auto* arena = get_special_room_at(x, y) ? nullptr : get_arena_at(x, y);
            if (arena) {
                switch (arena->type) {
                case ArenaObjectType::EXPLOSIVE_BARREL:
                    // 收官: 点燃中 (timer>0) 红色脉动警告
                    if (arena->timer > 0.0f) {
                        float fuse = 0.5f + 0.5f * sinf((float)GetTime() * 14.0f);
                        DrawRectangle(dx+4, dy+4, tile_size-8, tile_size-8, _dim({220, 70, 30, 255}, bright));
                        DrawRectangleLines(dx+2, dy+2, tile_size-4, tile_size-4,
                                           _dim({255, 90, 40, (unsigned char)(200 * fuse)}, bright));
                    } else {
                        DrawRectangle(dx+6, dy+6, tile_size-12, tile_size-12, _dim({180, 100, 30, 255}, bright));
                    }
                    DrawText("!", dx+12, dy+5, 18, _dim({255, 200, 50, 255}, bright)); break;
                case ArenaObjectType::HEALING_TOTEM:
                    DrawRectangle(dx+4, dy+4, tile_size-8, tile_size-8, _dim({30, 140, 60, 255}, bright));
                    DrawText("+", dx+11, dy+5, 18, _dim({100, 255, 120, 255}, bright)); break;
                case ArenaObjectType::POISON_POOL: {
                    // M4a-fix: 亮绿 + 脉动描边 (原深绿不易察觉, 玩家踩毒不自知)
                    float pulse = 0.6f + 0.4f * sinf((float)GetTime() * 5.0f);
                    DrawRectangle(dx+2, dy+2, tile_size-4, tile_size-4, _dim({40, 150, 55, 210}, bright));
                    DrawRectangleLines(dx+2, dy+2, tile_size-4, tile_size-4,
                                       _dim({90, 250, 90, (unsigned char)(170 * pulse)}, bright));
                    DrawText("~", dx+10, dy+5, 16, _dim({170, 255, 170, 230}, bright));
                } break;
                case ArenaObjectType::ROCK: {
                    DrawRectangle(dx+2, dy+8, tile_size-4, tile_size-10, _dim({100, 95, 100, 255}, bright));
                    DrawRectangleLines(dx+2, dy+8, tile_size-4, tile_size-10, _dim({130, 125, 130, 255}, bright));
                } break;
                case ArenaObjectType::SPIKE:
                    DrawTriangle({dx+tile_size/2, dy+2}, {dx+2, dy+tile_size-4},
                                {dx+tile_size-2, dy+tile_size-4}, _dim({180, 50, 50, 255}, bright));
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
