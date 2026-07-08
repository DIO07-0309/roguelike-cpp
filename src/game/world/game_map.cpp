#include "game_map.h"
#include "config.h"

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

void GameMap::draw(float cam_x, float cam_y, int sw, int sh) const {
    int sc = std::max(0, (int)(cam_x / tile_size));
    int sr = std::max(0, (int)(cam_y / tile_size));
    int ec = std::min(width, (int)((cam_x + sw) / tile_size) + 1);
    int er = std::min(height, (int)((cam_y + sh) / tile_size) + 1);

    for (int y = sr; y < er; y++) {
        for (int x = sc; x < ec; x++) {
            float dx = x * tile_size - cam_x;
            float dy = y * tile_size - cam_y;
            const auto& t = _tiles[y][x];

            if (t.type == TileType::WALL) {
                DrawRectangle(dx, dy, tile_size, tile_size, {60, 60, 80, 255});
                DrawRectangleLines(dx, dy, tile_size, tile_size, {40, 40, 55, 255});
            } else if (t.type == TileType::FLOOR) {
                const SpecialRoom* sr = get_special_room_at(x, y);
                if (sr) {
                    // 特殊房间地板颜色区分
                    Color base;
                    switch (sr->type) {
                        case SpecialRoomType::ALTAR:    base = {50, 35, 15, 255}; break;
                        case SpecialRoomType::TREASURE: base = {25, 35, 60, 255}; break;
                        case SpecialRoomType::FOUNTAIN: base = {20, 45, 25, 255}; break;
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
                            case SpecialRoomType::ALTAR:    icon = "+"; break;
                            case SpecialRoomType::TREASURE: icon = "$"; break;
                            case SpecialRoomType::FOUNTAIN: icon = "~"; break;
                        }
                        Color ic = sr->triggered ? Color{100, 100, 100, 255}
                                                 : Color{255, 255, 200, 255};
                        DrawText(icon, (int)dx + 10, (int)dy + 5, 20, ic);
                    }
                } else {
                    DrawRectangle(dx, dy, tile_size, tile_size, {25, 25, 35, 255});
                    // 细微网格线
                    DrawRectangleLines(dx, dy, tile_size, tile_size, {35, 35, 45, 255});
                }
            } else if (t.type == TileType::STAIRS_DOWN) {
                DrawRectangle(dx, dy, tile_size, tile_size, {50, 40, 20, 255});
                DrawRectangleLines(dx+2, dy+2, tile_size-4, tile_size-4, {255, 200, 50, 255});
                DrawText(">", dx + 10, dy + 6, 20, {255, 220, 100, 255});
            }
        }
    }
}
