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
                                                  int arena_density,
                                                  const std::string& biome_id) {
    _seed = seed;
    if (seed != 0) _local_rng.seed(seed);

    _rooms.clear(); _corridors.clear(); _connections.clear(); _special_rooms.clear();
    delete _root;
    _root = new BSPNode(0, 0, _w, _h);

    _partition(_root);
    _create_rooms(_root);
    _connect_rooms(_root);

    auto gm = std::make_shared<GameMap>(_w, _h, _ts);
    auto tmpl = _build_template();
    gm->load_from_template(tmpl);

    _assign_special_rooms(special_room_count, biome_id);
    gm->special_rooms = _special_rooms;

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

// B8/G6.2: 从 _rooms 中挑选 N 个作为特殊房间 (50% landmarks, 30% SECRET, rest normal)
void DungeonGenerator::_assign_special_rooms(int count, const std::string& biome_id) {
    if (_rooms.size() < 4) return;

    std::vector<int> candidates;
    // G10: prioritize rooms near player spawn (room 0) for first few relics
    // Put room 1 first to guarantee at least one relic room is close
    if ((int)_rooms.size() >= 3) candidates.push_back(1);
    if ((int)_rooms.size() >= 4) candidates.push_back(2);
    for (int i = 3; i < (int)_rooms.size() - 1; i++)
        candidates.push_back(i);

    for (int i = (int)candidates.size() - 1; i > 0; i--) {
        int j = _rand_int(i + 1);
        // Don't shuffle room 1 away from first position
        if (candidates[i] == 1 || candidates[j] == 1) continue;
        std::swap(candidates[i], candidates[j]);
    }

    // G6.2: load landmarks for this biome
    std::vector<const LandmarkDef*> landmarks;
    if (!biome_id.empty())
        landmarks = get_landmarks_for_biome(biome_id);

    int scount = std::min(count, (int)candidates.size());
    int type_idx = 0;
    int placed_lm = 0;
    for (int i = 0; i < scount; i++) {
        auto [rx, ry, rw, rh] = _rooms[candidates[i]];
        SpecialRoom sr;
        sr.cx = rx + rw / 2; sr.cy = ry + rh / 2;
        sr.rx = rx; sr.ry = ry; sr.rw = rw; sr.rh = rh;

        // G6.2: ~50% chance → biome landmark
        if (!landmarks.empty() && _rand_int(2) == 0 && placed_lm < 3) {
            const LandmarkDef* lm = landmarks[_rand_int((int)landmarks.size())];
            sr.type = SpecialRoomType::LANDMARK;
            sr.landmark_id = lm->id;
            sr.biome_id = biome_id;
            placed_lm++;
        } else {
            sr.type = special_room_from_index(type_idx++);
        }
        sr.triggered = false;
        _special_rooms.push_back(sr);
    }

    // G6.6: 30% chance to convert one existing room → SECRET
    if (!biome_id.empty() && !_special_rooms.empty() && _rand_int(100) < 30) {
        auto* enc = pick_encounter_by_trigger(biome_id, "wall_interact");
        if (enc) {
            int idx = _rand_int((int)_special_rooms.size());
            if (_special_rooms[idx].type != SpecialRoomType::LANDMARK)
                _special_rooms[idx].type = SpecialRoomType::SECRET;
        }
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

    // Phase 2: 获取两个子树的房间信息
    auto [arx, ary, arw, arh] = _get_room_rect(node->left);
    auto [brx, bry, brw, brh] = _get_room_rect(node->right);
    if (arw <= 0 || brw <= 0) return;

    // 获取两个房间的中心（用于选择最近边缘）
    int acx = arx + arw/2, acy = ary + arh/2;
    int bcx = brx + brw/2, bcy = bry + brh/2;

    // 选择边缘连接点
    auto edge_a = _pick_room_edge(node->left, bcx, bcy);
    auto edge_b = _pick_room_edge(node->right, acx, acy);
    if (edge_a.first < 0 || edge_b.first < 0) return;

    // 计算 Door 位置
    auto door_a = _compute_door_pos(edge_a.first, edge_a.second, arx, ary, arw, arh);
    auto door_b = _compute_door_pos(edge_b.first, edge_b.second, brx, bry, brw, brh);
    if (door_a.first < 0 || door_b.first < 0) return;

    // 存储连接信息
    _connections.push_back({edge_a, door_a, door_b, edge_b});

    // 保留旧 _corridors 用于兼容（走廊从 Door 到 Door）
    _corridors.emplace_back(door_a.first, door_a.second, door_b.first, door_b.second);
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

// Phase 2: 返回 Room 内部紧邻墙壁的地板 tile（边缘中点）
std::pair<int,int> DungeonGenerator::_pick_room_edge(BSPNode* node, int target_x, int target_y) {
    auto [rx, ry, rw, rh] = _get_room_rect(node);
    if (rw <= 0 || rh <= 0) return {-1, -1};

    // 4 个边缘中点（房间内部地板 tile）
    struct Edge { int x, y; int door_x, door_y; };
    Edge edges[4] = {
        {rx + rw/2, ry,         rx + rw/2, ry - 1},      // 上 → Door 向上
        {rx + rw/2, ry+rh-1,    rx + rw/2, ry + rh},     // 下 → Door 向下
        {rx,        ry + rh/2,  rx - 1,    ry + rh/2},   // 左 → Door 向左
        {rx+rw-1,   ry + rh/2,  rx + rw,   ry + rh/2},   // 右 → Door 向右
    };

    // 过滤 Door 越界的边缘
    int best_idx = -1;
    double best_dist = 1e9;
    for (int i = 0; i < 4; i++) {
        if (edges[i].door_x < 0 || edges[i].door_x >= _w) continue;
        if (edges[i].door_y < 0 || edges[i].door_y >= _h) continue;
        double dist = (double)(edges[i].x - target_x) * (edges[i].x - target_x)
                    + (double)(edges[i].y - target_y) * (edges[i].y - target_y);
        if (dist < best_dist) { best_dist = dist; best_idx = i; }
    }
    if (best_idx < 0) return {-1, -1};
    return {edges[best_idx].x, edges[best_idx].y};
}

// Phase 2: 从 edge point 计算 Door 位置
std::pair<int,int> DungeonGenerator::_compute_door_pos(int edge_x, int edge_y,
                                                        int room_rx, int room_ry,
                                                        int room_rw, int room_rh) {
    // edge 是房间内部地板 tile，Door 是其向外 1 格的墙壁 tile
    if (edge_y == room_ry && edge_x >= room_rx && edge_x < room_rx + room_rw)
        return {edge_x, room_ry - 1};           // 上边缘 → Door 向上
    if (edge_y == room_ry + room_rh - 1 && edge_x >= room_rx && edge_x < room_rx + room_rw)
        return {edge_x, room_ry + room_rh};     // 下边缘 → Door 向下
    if (edge_x == room_rx && edge_y >= room_ry && edge_y < room_ry + room_rh)
        return {room_rx - 1, edge_y};           // 左边缘 → Door 向左
    if (edge_x == room_rx + room_rw - 1 && edge_y >= room_ry && edge_y < room_ry + room_rh)
        return {room_rx + room_rw, edge_y};     // 右边缘 → Door 向右
    return {-1, -1};  // 不应该到这里
}

// Phase 2: 获取 BSP 叶子节点的 Room 矩形
std::tuple<int,int,int,int> DungeonGenerator::_get_room_rect(BSPNode* node) {
    if (node->is_leaf() && node->has_room)
        return {node->rx, node->ry, node->rw, node->rh};
    // 非叶子：递归找任意一个子房间
    if (node->left) {
        auto r = _get_room_rect(node->left);
        if (std::get<2>(r) > 0) return r;
    }
    if (node->right) {
        auto r = _get_room_rect(node->right);
        if (std::get<2>(r) > 0) return r;
    }
    return {0, 0, 0, 0};
}

std::vector<std::string> DungeonGenerator::_build_template() {
    std::vector<std::string> grid(_h, std::string(_w, '#'));
    for (auto& [rx, ry, rw, rh] : _rooms)
        _carve_rect(grid, rx, ry, rw, rh);
    for (auto& [x1, y1, x2, y2] : _corridors)
        _carve_corridor(grid, x1, y1, x2, y2);
    // Phase 2: 放置 Door tiles
    for (auto& conn : _connections) {
        auto [dx, dy] = conn.door_a;
        if (dx >= 0 && dx < _w && dy >= 0 && dy < _h)
            grid[dy][dx] = 'D';
        auto [dx2, dy2] = conn.door_b;
        if (dx2 >= 0 && dx2 < _w && dy2 >= 0 && dy2 < _h)
            grid[dy2][dx2] = 'D';
    }
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
                if (g[ty][tx] == '#')  // Phase 2: 只雕刻墙壁，保护 Room Interior
                    g[ty][tx] = '.';
        }
}
