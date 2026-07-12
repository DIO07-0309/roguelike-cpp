#include "dungeon_generator.h"
#include "item.h"  // for rng
#include <algorithm>

DungeonGenerator::DungeonGenerator(int w, int h, int ts, int mp, int mr, int mg)
    : _w(w), _h(h), _ts(ts), _min_part(mp), _min_room(mr), _margin(mg) {}

// ---- BSPNode 实现 ----
BSPNode::BSPNode(int x_, int y_, int w_, int h_) : x(x_), y(y_), w(w_), h(h_) {}
BSPNode::~BSPNode() { delete left; delete right; }
bool BSPNode::is_leaf() const { return !left && !right; }
std::pair<int,int> BSPNode::center() const { return {x + w/2, y + h/2}; }
std::pair<int,int> BSPNode::room_center() const { return {rx + rw/2, ry + rh/2}; }

std::shared_ptr<GameMap> DungeonGenerator::generate(uint32_t seed, int special_room_count,
                                                  int arena_density) {
    _seed = seed;
    if (seed != 0) _local_rng.seed(seed);

    _rooms.clear(); _corridors.clear(); _special_rooms.clear();
    delete _root;
    _root = new BSPNode(0, 0, _w, _h);

    _partition(_root);
    _create_rooms(_root);
    _connect_rooms(_root);

    auto gm = std::make_shared<GameMap>(_w, _h, _ts);
    auto tmpl = _build_template();
    gm->load_from_template(tmpl);

    _assign_special_rooms(special_room_count);
    gm->special_rooms = _special_rooms;

    // D2 Step5: 战场元素
    if (arena_density > 0) _assign_arena_objects(gm.get(), arena_density);

    return gm;
}

// D2 Step5: 在每个非特殊房间放置战场元素
void DungeonGenerator::_assign_arena_objects(GameMap* gm, int density) {
    static const ArenaObjectType POOL[] = {
        ArenaObjectType::EXPLOSIVE_BARREL, ArenaObjectType::HEALING_TOTEM,
        ArenaObjectType::POISON_POOL, ArenaObjectType::ROCK, ArenaObjectType::SPIKE
    };
    for (auto& [rx, ry, rw, rh] : _rooms) {
        if (_special_rooms.size() > 0) {
            // 跳过已有特殊房间
            bool is_special = false;
            for (auto& sr : _special_rooms)
                if (rx == sr.rx && ry == sr.ry) { is_special = true; break; }
            if (is_special) continue;
        }
        for (int i = 0; i < density; i++) {
            int tx = rx + 1 + _rand_int(std::max(1, rw - 2));
            int ty = ry + 1 + _rand_int(std::max(1, rh - 2));
            if (!gm->is_walkable(tx, ty)) continue;
            // 检查不在已有 Arena 上
            bool dup = false;
            for (auto& ao : gm->arena_objects)
                if (ao.tile_x == tx && ao.tile_y == ty) { dup = true; break; }
            if (dup) continue;
            ArenaObject obj;
            obj.type = POOL[_rand_int(5)];
            obj.tile_x = tx; obj.tile_y = ty; obj.active = true;
            gm->arena_objects.push_back(obj);
        }
    }
}

std::vector<std::pair<int,int>> DungeonGenerator::get_room_centers() const {
    std::vector<std::pair<int,int>> centers;
    for (auto& [rx, ry, rw, rh] : _rooms)
        centers.emplace_back(rx + rw/2, ry + rh/2);
    return centers;
}

// B8: 统一的随机入口 — seed≠0 时走本地 rng，否则走全局 rng
int DungeonGenerator::_rand_int(int max_exclusive) {
    if (max_exclusive <= 0) return 0;
    if (_seed != 0) return (int)(_local_rng() % (uint32_t)max_exclusive);
    return (int)(rng() % max_exclusive);
}

// B8: 从 _rooms 中挑选 N 个作为特殊房间 (D1: N 从 FloorConfig 读取)
void DungeonGenerator::_assign_special_rooms(int count) {
    // 需要至少 4 个房间: 玩家 + 楼梯 + 2 特殊
    if (_rooms.size() < 4) return;

    // 排除 rooms[0] (玩家) 和 rooms.back() (楼梯)
    std::vector<int> candidates;
    for (int i = 1; i < (int)_rooms.size() - 1; i++)
        candidates.push_back(i);

    // Fisher-Yates shuffle (用本地随机保证同 seed 同结果)
    for (int i = (int)candidates.size() - 1; i > 0; i--) {
        int j = _rand_int(i + 1);
        std::swap(candidates[i], candidates[j]);
    }

    int scount = std::min(count, (int)candidates.size());
    for (int i = 0; i < scount; i++) {
        auto [rx, ry, rw, rh] = _rooms[candidates[i]];
        SpecialRoom sr;
        sr.cx = rx + rw / 2;
        sr.cy = ry + rh / 2;
        sr.rx = rx; sr.ry = ry; sr.rw = rw; sr.rh = rh;
        sr.type = special_room_from_index(i); // 0→ALTAR, 1→TREASURE, 2→FOUNTAIN
        sr.triggered = false;
        _special_rooms.push_back(sr);
    }
}

void DungeonGenerator::_partition(BSPNode* node) {
    bool vertical = (node->w > node->h) ? true
                  : (node->h > node->w) ? false
                  : (_rand_int(2) == 0);

    int region = vertical ? node->w : node->h;
    if (region < _min_part * 2) return;

    int split = _min_part + _rand_int(region - _min_part * 2 + 1);
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
    int rw = _min_room + _rand_int(std::max(1, node->w - 2 * _margin - _min_room + 1));
    int rh = _min_room + _rand_int(std::max(1, node->h - 2 * _margin - _min_room + 1));
    int rx = node->x + _margin + _rand_int(std::max(1, node->w - rw - 2 * _margin + 1));
    int ry = node->y + _margin + _rand_int(std::max(1, node->h - rh - 2 * _margin + 1));

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
    return _pick_room(valid[_rand_int((int)valid.size())]);
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
    int width = DUNGEON_CORRIDOR_MIN + _rand_int(DUNGEON_CORRIDOR_MAX - DUNGEON_CORRIDOR_MIN + 1);
    if (_rand_int(2) == 0) {
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
