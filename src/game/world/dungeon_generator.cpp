#include "dungeon_generator.h"
#include "item.h"  // for rng
#include <algorithm>

DungeonGenerator::DungeonGenerator(int w, int h, int ts, int mp, int mr, int mg)
    : _w(w), _h(h), _ts(ts), _min_part(mp), _min_room(mr), _margin(mg) {}

std::shared_ptr<GameMap> DungeonGenerator::generate() {
    _rooms.clear(); _corridors.clear();
    delete _root;
    _root = new BSPNode(0, 0, _w, _h);

    _partition(_root);
    _create_rooms(_root);
    _connect_rooms(_root);

    auto gm = std::make_shared<GameMap>(_w, _h, _ts);
    auto tmpl = _build_template();
    gm->load_from_template(tmpl);
    return gm;
}

std::vector<std::pair<int,int>> DungeonGenerator::get_room_centers() const {
    std::vector<std::pair<int,int>> centers;
    for (auto& [rx, ry, rw, rh] : _rooms)
        centers.emplace_back(rx + rw/2, ry + rh/2);
    return centers;
}

void DungeonGenerator::_partition(BSPNode* node) {
    bool vertical = (node->w > node->h) ? true
                  : (node->h > node->w) ? false
                  : ((rng() % 2) == 0);

    int region = vertical ? node->w : node->h;
    if (region < _min_part * 2) return;

    int split = _min_part + (int)(rng() % (region - _min_part * 2 + 1));
    _create_child_nodes(node, vertical, split);
    _partition(node->left);
    _partition(node->right);
}

void DungeonGenerator::_create_child_nodes(BSPNode* node, bool vert, int split) {
    if (vert) {
        node->left  = new BSPNode(node->x, node->y, split, node->h);
        node->right = new BSPNode(node->x + split, node->y, node->w - split, node->h);
    } else {
        node->left  = new BSPNode(node->x, node->y, node->w, split);
        node->right = new BSPNode(node->x, node->y + split, node->w, node->h - split);
    }
}

void DungeonGenerator::_create_rooms(BSPNode* node) {
    if (!node->is_leaf()) {
        if (node->left)  _create_rooms(node->left);
        if (node->right) _create_rooms(node->right);
        return;
    }
    int rw = _min_room + (int)(rng() % std::max(1, node->w - 2 * _margin - _min_room + 1));
    int rh = _min_room + (int)(rng() % std::max(1, node->h - 2 * _margin - _min_room + 1));
    int rx = node->x + _margin + (int)(rng() % std::max(1, node->w - rw - 2 * _margin + 1));
    int ry = node->y + _margin + (int)(rng() % std::max(1, node->h - rh - 2 * _margin + 1));

    node->rx = rx; node->ry = ry; node->rw = rw; node->rh = rh;
    node->has_room = true;
    _rooms.emplace_back(rx, ry, rw, rh);
}

void DungeonGenerator::_connect_rooms(BSPNode* node) {
    if (node->is_leaf()) return;
    if (node->left)  _connect_rooms(node->left);
    if (node->right) _connect_rooms(node->right);
    if (!node->left || !node->right) return;

    auto a = _pick_room(node->left);
    auto b = _pick_room(node->right);
    if (a.first >= 0 && b.first >= 0)
        _corridors.emplace_back(a.first, a.second, b.first, b.second);
}

std::pair<int,int> DungeonGenerator::_pick_room(BSPNode* node) {
    if (node->is_leaf()) {
        if (node->has_room) return node->room_center();
        return {-1, -1};
    }
    auto children = {node->left, node->right};
    std::vector<BSPNode*> valid;
    for (auto* c : children) if (c) valid.push_back(c);
    if (valid.empty()) return {-1, -1};
    return _pick_room(valid[rng() % valid.size()]);
}

std::vector<std::string> DungeonGenerator::_build_template() {
    std::vector<std::string> grid(_h, std::string(_w, '#'));
    for (auto& [rx, ry, rw, rh] : _rooms)
        _carve_rect(grid, rx, ry, rw, rh);
    for (auto& [x1, y1, x2, y2] : _corridors)
        _carve_corridor(grid, x1, y1, x2, y2);
    return grid;
}

void DungeonGenerator::_carve_rect(std::vector<std::string>& g, int x, int y, int w, int h) {
    for (int row = y; row < std::min(y + h, _h); row++)
        for (int col = x; col < std::min(x + w, _w); col++)
            g[row][col] = '.';
}

void DungeonGenerator::_carve_corridor(std::vector<std::string>& g, int x1, int y1, int x2, int y2) {
    int width = DUNGEON_CORRIDOR_MIN + (int)(rng() % (DUNGEON_CORRIDOR_MAX - DUNGEON_CORRIDOR_MIN + 1));
    if ((rng() % 2) == 0) {
        _carve_line(g, x1, y1, x2, y1, width);
        _carve_line(g, x2, y1, x2, y2, width);
    } else {
        _carve_line(g, x1, y1, x1, y2, width);
        _carve_line(g, x1, y2, x2, y2, width);
    }
}

void DungeonGenerator::_carve_line(std::vector<std::string>& g, int x1, int y1, int x2, int y2, int w) {
    if (x1 == x2) {
        int step = (y2 >= y1) ? 1 : -1;
        for (int y = y1; y != y2 + step; y += step)
            _carve_diamond(g, x1, y, w);
    } else {
        int step = (x2 >= x1) ? 1 : -1;
        for (int x = x1; x != x2 + step; x += step)
            _carve_diamond(g, x, y1, w);
    }
}

void DungeonGenerator::_carve_diamond(std::vector<std::string>& g, int cx, int cy, int r) {
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            if (abs(dx) + abs(dy) > r) continue;
            int tx = cx + dx, ty = cy + dy;
            if (tx >= 0 && tx < _w && ty >= 0 && ty < _h)
                g[ty][tx] = '.';
        }
}
