#include "sim_ai.h"
#include "player.h"
#include "monster.h"
#include "boss.h"           // Q3.2: BossAI windup 状态读取 (躲招判定)
#include "game_map.h"
#include "combat_system.h"  // rng
#include "build_score.h"    // BuildType, calculate_build
#include "core/logger.h"
#include "ai/mcts/mcts_search.h"
#include "ai/mcts/action.h"
#include "sim_ai_teleport.h"      // P0-M2: room-domain teleport contract
#include "world/room_manager.h"   // P0-M2: room_at for teleport logging
#include <cmath>
#include <cstring>
#include <algorithm>
#include <queue>

bool DecisionAgent::g_use_mcts = false;
int  DecisionAgent::g_mcts_iters = 100;

// M2-C: sim 卡墙恢复诊断计数 (GameScene::_collect_sim_stats 快照; 单线程 sim)
int sim_stuck_teleports = 0;   // [PLAYER-FIX] 口袋传送次数
int sim_stuck_rotations = 0;   // 旋转脱困进入次数
int sim_stuck_loot_wd = 0;     // 搜刮看门狗强制下楼次数

// G8.3: Build SimulationState snapshot from live game state
mcts::SimulationState DecisionAgent::build_sim_state(
    const Player* player, const std::vector<Monster*>& monsters, double game_time) {
    mcts::SimulationState s;
    if (!player) return s;
    auto& p = s.player;
    p.hp = (float)player->combat.current_hp;
    p.max_hp = (float)player->combat.max_hp;
    p.x = player->entity.rect.x / 32.0f;
    p.y = player->entity.rect.y / 32.0f;
    p.attack = player->combat.attack;
    p.pdef = player->combat.physical_defense;
    p.mdef = player->combat.magical_defense;
    p.alive = player->combat.is_alive;
    // Cooldowns: real remaining time (Q3.15 A6 fix — was faked to constant
    // 0.5s/0s, which permanently disabled ATTACK at the MCTS root since
    // get_possible_actions requires attack_cooldown <= 0)
    p.attack_cooldown = std::max(0.0f,
        player->_last_attack_time + Player::ATTACK_COOLDOWN - (float)game_time);
    for (int i = 0; i < 4; i++) {
        if (i < (int)player->skills.active_skills.size() && player->skills.active_skills[i])
            p.skill_cooldowns[i] = std::max(0.0f,
                (float)player->skills.active_skills[i]->remaining_cooldown(game_time));
        else
            p.skill_cooldowns[i] = 99.0f;   // 未拥有的技能 = 永不可用
    }
    // Buffs
    for (auto& b : player->active_buffs)
        if (b.stacks > 0)
            p.buffs.push_back({b.id, b.stacks, b.remaining});

    for (auto* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        mcts::MonsterSnapshot ms;
        ms.type = m->name;
        ms.hp = (float)m->combat.current_hp;
        ms.max_hp = (float)m->combat.max_hp;
        ms.x = m->entity.rect.x / 32.0f;
        ms.y = m->entity.rect.y / 32.0f;
        ms.attack = m->combat.attack;
        ms.pdef = m->combat.physical_defense;
        ms.mdef = m->combat.magical_defense;
        ms.alive = true;
        ms.is_boss = m->is_boss;
        s.monsters.push_back(ms);
    }
    s.rng.seed = (uint32_t)(int)(player->entity.rect.x * 1000 + player->entity.rect.y);
    return s;
}

// ═══════════════════════════════════════════════════════════
//  G7.4: Build-aware behavioral profiles
// ═══════════════════════════════════════════════════════════

DecisionAgent::DecisionAgent() {}

void DecisionAgent::start(const Player* player) {
    _frame = 0; _dir_timer = 0; _current_dir = -1;
    _resolve_profile(player);
}

void DecisionAgent::_resolve_profile(const Player* player) {
    if (!player) return;
    _build_type = calculate_build(player).identify();

    // Default: balanced
    _prefer_range = 0.3f; _prefer_aoe = 0.2f;
    _prefer_skill = 0.4f; _aggro_bias = 0.5f;
    _prefer_heal = 0.35f;
    _skill_priority[0]=0; _skill_priority[1]=1;
    _skill_priority[2]=2; _skill_priority[3]=3;

    switch (_build_type) {
    case BuildType::ICE_MAGE:
        _prefer_range = 0.9f; _prefer_aoe = 0.8f;
        _prefer_skill = 0.7f; _aggro_bias = 0.2f; // kite
        break;
    case BuildType::FIRE_MAGE:
        _prefer_range = 0.7f; _prefer_aoe = 0.7f;
        _prefer_skill = 0.8f; _aggro_bias = 0.3f;
        break;
    case BuildType::LIGHTNING_MAGE:
        _prefer_range = 0.6f; _prefer_aoe = 0.6f;
        _prefer_skill = 0.7f; _aggro_bias = 0.4f;
        break;
    case BuildType::BERSERKER:
        _prefer_range = 0.0f; _prefer_aoe = 0.3f;
        _prefer_skill = 0.3f; _aggro_bias = 0.9f; // rush in
        break;
    case BuildType::BLEED_BLADE:
        _prefer_range = 0.1f; _prefer_aoe = 0.3f;
        _prefer_skill = 0.5f; _aggro_bias = 0.7f;
        break;
    case BuildType::SHADOW_STRIKER:
        _prefer_range = 0.0f; _prefer_aoe = 0.0f;
        _prefer_skill = 0.6f; _aggro_bias = 0.6f; // single target burst
        break;
    case BuildType::SUPPORT:
        _prefer_range = 0.4f; _aggro_bias = 0.3f;
        _prefer_heal = 0.50f; // heal early
        break;
    case BuildType::JUGGERNAUT:
        _prefer_range = 0.0f; _prefer_aoe = 0.4f;
        _prefer_skill = 0.2f; _aggro_bias = 0.8f; // tank
        break;
    case BuildType::SUMMON_LORD:
        _prefer_range = 0.6f; _prefer_skill = 0.8f;
        _aggro_bias = 0.2f; // stay back, let summons fight
        break;
    case BuildType::POISON_MASTER:
        _prefer_range = 0.5f; _prefer_aoe = 0.4f;
        _prefer_skill = 0.5f; _aggro_bias = 0.4f;
        break;
    case BuildType::TIME_MASTER:
        _prefer_range = 0.5f; _prefer_skill = 0.7f;
        _aggro_bias = 0.3f;
        break;
    default: break; // keep defaults
    }
}

void DecisionAgent::tick() {
    _frame++;
    _cached_frame = -1;  // Q3.1: 强制下一查询重算 (世界已变)
}

// ═══════════════════════════════════════════════════════════
//  G7.4: Action evaluators
// ═══════════════════════════════════════════════════════════

float DecisionAgent::_evaluate_attack(const Player* p,
    const std::vector<Monster*>& monsters) const {
    auto* t = _find_nearest(p, monsters);
    if (!t) return 0;
    float d = hypotf(t->entity.rect.x + t->entity.rect.width/2 - (p->entity.rect.x + p->entity.rect.width/2),
                     t->entity.rect.y + t->entity.rect.height/2 - (p->entity.rect.y + p->entity.rect.height/2));
    if (d > 1.5f * 32.0f) return 0; // out of range — no score
    // Melee builds score higher for attacking
    float base = 1.0f - _prefer_range; // range=0 → score 1.0
    return base * (1.0f - d / (3.0f * 32.0f)); // closer = better
}

float DecisionAgent::_evaluate_skill(int slot, const Player* p,
    const std::vector<Monster*>& monsters) const {
    if (slot < 0 || slot >= 4) return 0;
    // Q3.2: 槽位越界/技能空/冷却中 → 不得给分 (否则站桩按CD技能挨打)
    if (slot >= (int)p->skills.active_skills.size()) return 0;
    auto& sk = p->skills.active_skills[slot];
    if (!sk || !sk->can_use(_game_time)) return 0;
    int n = _count_in_range(p, monsters, 5.0f * 32.0f);
    if (n <= 0) return 0;
    float aoe_bonus = _prefer_aoe * (n > 1 ? 1.0f : 0.3f);
    float score = _prefer_skill * (0.5f + aoe_bonus);
    // M4.4: 单体Boss战 — 伤害技能冷却好就放 (补足普攻DPS缺口, 对冲Boss自愈)
    auto* t = _find_nearest(p, monsters);
    if (t && t->is_boss && !dynamic_cast<SelfHealSkill*>(sk.get())) score += 0.9f;
    return score;
}

static const int kBfsDx[4] = {0, 0, -1, 1};  // up, down, left, right
static const int kBfsDy[4] = {-1, 1, 0, 0};

// Q3.10: 口袋兜底 — 卡死≥8s(四向逃脱均失败)时传送玩家至最近存活怪相邻可行走格
// 破口袋/48-51px 隔墙死局: 传送后玩家必能普攻到该怪, 战斗恢复, 楼层推进
static bool _teleport_player_to_nearest(Player* p,
    const std::vector<Monster*>& monsters, const GameMap* map,
    const RoomManager* rooms) {
    if (!p || !map) return false;
    const Monster* t = nullptr;
    float bd = 1e9f;
    for (const Monster* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        float d = hypotf(m->entity.rect.x - p->entity.rect.x,
                         m->entity.rect.y - p->entity.rect.y);
        if (d < bd) { bd = d; t = m; }
    }
    if (!t) return false;

    // P0-M2: room-domain deterministic target selection
    // (contract: see core/sim/sim_ai_teleport.h + tests/sim/p0_teleport_test.cpp)
    TeleportQuery q;
    q.player_rect = p->entity.rect;
    q.target = const_cast<Monster*>(t);
    q.map = map;
    q.rooms = rooms;
    for (const Monster* m : monsters)
        if (m && m->combat.is_alive && m != t)
            q.extra_monsters.push_back(const_cast<Monster*>(m));
    TeleportResult r = sim_ai_teleport_target(q);
    if (!r.found) return false;

    p->entity.position.x = (float)(r.tile_x * 32);
    p->entity.position.y = (float)(r.tile_y * 32);
    p->entity.sync_rect();
    LOG_INFO("[PLAYER-FIX] 口袋传送 → tile(%d,%d) room=%d",
             r.tile_x, r.tile_y, rooms ? rooms->room_at(r.tile_x, r.tile_y) : -1);
    return true;
}

// Q3.2: Boss 蓄力判定 — 任一技能处于 windup 阶段即视为"即将出招"
static bool _boss_winding_up(const Monster* m) {
    const auto* bai = dynamic_cast<const BossAI*>(m->ai);
    if (!bai) return false;
    if (bai->_charge    && bai->_charge->windup_left    > 0.0f) return true;
    if (bai->_shockwave && bai->_shockwave->windup_left > 0.0f) return true;
    if (bai->_whirlwind && bai->_whirlwind->windup_left > 0.0f) return true;
    if (bai->_laser     && bai->_laser->windup_left     > 0.0f) return true;
    if (bai->_cone      && bai->_cone->windup_left      > 0.0f) return true;
    if (bai->_blink     && bai->_blink->windup_left     > 0.0f) return true;
    if (bai->_barrage   && bai->_barrage->windup_left   > 0.0f) return true;
    return false;
}

// Q3.2: tile 级 rect 碰撞判定 — BFS 与真实移动(rect)对齐, 防 tile可行走但玩家进不去导致的卡墙
static bool _tile_rect_walkable(const GameMap* map, int tx, int ty) {
    if (!map) return false;
    DoorState ds = map->door_state_at(tx, ty);
    if (ds == DoorState::CLOSED) return true;   // Sim 自动开 CLOSED
    if (ds != DoorState::NONE) return false;     // LOCKED/SEALED 不可走
    Rectangle r = { (float)(tx * 32), (float)(ty * 32), 32.0f, 32.0f };
    return map->is_rect_walkable(r);
}

// Q3.2: 危险视野 — 活性毒池/尖刺圈/木桶 (伤害圈 1.2 格 + 缓冲 = 1.5 格)
bool DecisionAgent::_is_hazard_near(float px, float py, const GameMap* map) const {
    if (!map) return false;
    // M4b: 熔岩地砖 (脚下 + 邻格缓冲)
    auto [tx, ty] = map->pixel_to_tile(px, py);
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (map->tile_at(tx + dx, ty + dy) == TileType::LAVA) return true;
    for (auto& ao : map->arena_objects) {
        if (!ao.active) continue;
        if (ao.type != ArenaObjectType::POISON_POOL &&
            ao.type != ArenaObjectType::SPIKE &&
            ao.type != ArenaObjectType::EXPLOSIVE_BARREL) continue;  // 收官: 木桶可爆炸
        float ax = ao.tile_x * 32.0f + 16.0f;
        float ay = ao.tile_y * 32.0f + 16.0f;
        if (hypotf(px - ax, py - ay) <= 1.5f * 32.0f) return true;
    }
    return false;
}

// Q3.2: 残血且无可用自愈 → 需要找泉水/祭坛回血
bool DecisionAgent::_needs_recovery(const Player* p) const {
    if (!p) return false;
    if (_hp_ratio(p) >= 0.50f) return false;
    for (auto& s : p->skills.active_skills)
        if (dynamic_cast<SelfHealSkill*>(s.get()) && s->can_use(_game_time))
            return false;
    return true;
}

// Q3.2: BFS 至最近未触发的特殊房 — 战斗间隙搜刮资源 (圣物/装备/泉水)
int DecisionAgent::_bfs_toward_room(const Player* p, const GameMap* map) const {
    if (!map || !p) return -1;
    int w = map->width, h = map->height;
    auto [sx, sy] = map->pixel_to_tile(
        p->entity.rect.x + p->entity.rect.width/2,
        p->entity.rect.y + p->entity.rect.height/2);
    // Q3.13: 钳制玩家瓦片 — 否则 first[] 越界写堆损坏
    if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
    if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
    const int N = w * h;
    std::vector<char> is_target((size_t)N, 0);
    size_t pending = 0;
    for (auto& sr : map->special_rooms) {
        if (sr.triggered) continue;
        // Q3.13: 房间坐标越界保护 (数据驱动异常时不得写堆)
        if (sr.cx < 0 || sr.cx >= w || sr.cy < 0 || sr.cy >= h) continue;
        is_target[sr.cy * w + sr.cx] = 1;
        pending++;
    }
    if (pending == 0) return -1;
    std::vector<int> first((size_t)N, -2);
    std::queue<int> q;
    first[sy * w + sx] = -1;
    q.push(sy * w + sx);
    while (!q.empty()) {
        int cur = q.front(); q.pop();
        int cx = cur % w, cy = cur / w;
        if (is_target[cur]) return (first[cur] >= 0) ? first[cur] : -1;
        for (int d = 0; d < 4; d++) {
            int nx = cx + kBfsDx[d], ny = cy + kBfsDy[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            int ni = ny * w + nx;
            if (first[ni] != -2 || !_tile_rect_walkable(map, nx, ny)) continue;
            first[ni] = (cur == sy * w + sx) ? d : first[cur];
            q.push(ni);
        }
    }
    return -1;
}

// Q3.2: BFS 寻路 — 从玩家所在格出发, 找最近可达的存活怪物, 返回第一步方向 (0-3, -1=不可达)
int DecisionAgent::_bfs_toward(const Player* p,
    const std::vector<Monster*>& monsters, const GameMap* map, bool avoid_hazard) const {
    if (!map || !p) return -1;
    int w = map->width, h = map->height;
    auto [sx, sy] = map->pixel_to_tile(
        p->entity.rect.x + p->entity.rect.width/2,
        p->entity.rect.y + p->entity.rect.height/2);
    // Q3.13: 钳制玩家瓦片 — 位置可能出图(边缘传送), 否则 first[] 越界写堆损坏
    if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
    if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
    const int N = w * h;
    std::vector<char> is_target((size_t)N, 0);
    for (auto* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        auto [tx, ty] = map->pixel_to_tile(
            m->entity.rect.x + m->entity.rect.width/2,
            m->entity.rect.y + m->entity.rect.height/2);
        // Q3.13: 越界怪跳过 — 击退/传送可使位置出图, 否则 is_target 越界写堆损坏
        if (tx < 0 || tx >= w || ty < 0 || ty >= h) continue;
        is_target[ty * w + tx] = 1;
    }
    std::vector<int> first((size_t)N, -2);  // 从起点出发的第一步方向, -1=起点, -2=未访问
    std::queue<int> q;
    first[sy * w + sx] = -1;
    q.push(sy * w + sx);
    while (!q.empty()) {
        int cur = q.front(); q.pop();
        int cx = cur % w, cy = cur / w;
        if (is_target[cur]) return (first[cur] >= 0) ? first[cur] : -1;
        for (int d = 0; d < 4; d++) {
            int nx = cx + kBfsDx[d], ny = cy + kBfsDy[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            int ni = ny * w + nx;
            if (first[ni] != -2 || !_tile_rect_walkable(map, nx, ny)) continue;
            // Q3.2: 避开危险瓦片中心圈 (毒池/尖刺)
            if (avoid_hazard && _is_hazard_near(nx * 32.0f + 16.0f, ny * 32.0f + 16.0f, map)) continue;
            first[ni] = (cur == sy * w + sx) ? d : first[cur];
            q.push(ni);
        }
    }
    return -1;
}

// Q3.2: BFS 远离 — 从目标怪所在格 BFS 整图, 返回玩家 4 邻居中距怪最远的方向 (0-3, -1=全堵)
int DecisionAgent::_bfs_away(const Player* p, const Monster* t,
    const GameMap* map, bool avoid_hazard) const {
    if (!map || !p || !t) return -1;
    int w = map->width, h = map->height;
    auto [mx, my] = map->pixel_to_tile(
        t->entity.rect.x + t->entity.rect.width/2,
        t->entity.rect.y + t->entity.rect.height/2);
    auto [sx, sy] = map->pixel_to_tile(
        p->entity.rect.x + p->entity.rect.width/2,
        p->entity.rect.y + p->entity.rect.height/2);
    // Q3.13: 钳制怪物/玩家瓦片 — 否则 dist[] 越界写堆损坏
    if (mx < 0) mx = 0; else if (mx >= w) mx = w - 1;
    if (my < 0) my = 0; else if (my >= h) my = h - 1;
    if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
    if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
    const int N = w * h;
    std::vector<int> dist((size_t)N, -1);
    std::queue<int> q;
    dist[my * w + mx] = 0;
    q.push(my * w + mx);
    while (!q.empty()) {
        int cur = q.front(); q.pop();
        int cx = cur % w, cy = cur / w;
        for (int d = 0; d < 4; d++) {
            int nx = cx + kBfsDx[d], ny = cy + kBfsDy[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            int ni = ny * w + nx;
            if (dist[ni] >= 0 || !_tile_rect_walkable(map, nx, ny)) continue;
            if (avoid_hazard && _is_hazard_near(nx * 32.0f + 16.0f, ny * 32.0f + 16.0f, map)) continue;
            dist[ni] = dist[cur] + 1;
            q.push(ni);
        }
    }
    int best = -1, best_dist = -1;
    for (int d = 0; d < 4; d++) {
        int nx = sx + kBfsDx[d], ny = sy + kBfsDy[d];
        if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
        if (!_tile_rect_walkable(map, nx, ny)) continue;
        if (avoid_hazard && _is_hazard_near(nx * 32.0f + 16.0f, ny * 32.0f + 16.0f, map)) continue;
        if (dist[ny * w + nx] > best_dist) { best_dist = dist[ny * w + nx]; best = d; }
    }
    return best;
}

// Q3.2: 轴贪心兜底 — BFS 无路时按主轴直行, 用真实rect校验, 每步避开毒池
int DecisionAgent::_greedy_step(const Player* p, const Monster* t,
                                const GameMap* map) const {
    if (!p || !t || !map) return -1;
    float px = p->entity.rect.x + p->entity.rect.width/2;
    float py = p->entity.rect.y + p->entity.rect.height/2;
    float tx = t->entity.rect.x + t->entity.rect.width/2;
    float ty = t->entity.rect.y + t->entity.rect.height/2;
    int dx = (tx > px) ? 3 : 2;
    int dy = (ty > py) ? 1 : 0;
    int cand[4] = {dx, dy, (dx == 2) ? 3 : 2, (dy == 0) ? 1 : 0};
    for (int i = 0; i < 4; i++) {
        float mdx = (cand[i] == 2) ? -32.0f : (cand[i] == 3) ? 32.0f : 0.0f;
        float mdy = (cand[i] == 0) ? -32.0f : (cand[i] == 1) ? 32.0f : 0.0f;
        Rectangle r = p->entity.rect;
        r.x += mdx; r.y += mdy;
        if (!map->is_rect_walkable(r)) continue;
        if (_is_hazard_near(r.x + r.width/2, r.y + r.height/2, map)) continue;
        return cand[i];
    }
    return -1;
}

float DecisionAgent::_evaluate_move(int dir, const Player* p,
    const std::vector<Monster*>& monsters, const GameMap* map) const {
    if (!p) return -999;
    float dx = (dir == 2) ? -1.0f : (dir == 3) ? 1.0f : 0.0f;
    float dy = (dir == 0) ? -1.0f : (dir == 1) ? 1.0f : 0.0f;
    // Check walkable (Q3.1: 用 rect 判定对齐真实移动, 避免 tile 级误判撞墙)
    Rectangle target_rect = p->entity.rect;
    target_rect.x += dx * 32.0f;
    target_rect.y += dy * 32.0f;
    if (map && !map->is_rect_walkable(target_rect)) return -999; // blocked

    float px = p->entity.rect.x + p->entity.rect.width/2;
    float py = p->entity.rect.y + p->entity.rect.height/2;
    // Q3.2: 落脚点进入毒池/尖刺圈 → 重罚 (Q3.10: -999→-1.0, 全图无安全路径时允许踩毒渡河)
    if (map && _is_hazard_near(target_rect.x + target_rect.width/2,
                               target_rect.y + target_rect.height/2, map))
        return -1.0f;

    auto* t = _find_nearest(p, monsters);
    if (!t) return 0.1f; // 全图无存活怪 → 中性

    float ex = t->entity.rect.x + t->entity.rect.width/2;
    float ey = t->entity.rect.y + t->entity.rect.height/2;
    float d = hypotf(ex - px, ey - py);

    // Q3.2: Boss 蓄力闪避 — 起手瞬间脱离 (1.4 > 攻击 1.0, 躲招优先于换血)
    if (t->is_boss && _boss_winding_up(t) && d < 220.0f) {
        int away = _bfs_away(p, t, map, true);
        if (away < 0) return 1.4f;  // 无安全路径 → 任意方向裸躲
        return (dir == away) ? 1.4f : 0.0f;
    }

    // Q3.2: 站在毒池里 → 任何安全方向优先逃离 (1.2 > 攻击上限 1.0)
    if (map && _is_hazard_near(px, py, map)) return 1.2f;

    // Q3.2: 战斗间隙搜刮 — 最近怪 >5 格(160px)时走向最近未触发特殊房 (圣物/装备/泉水)
    // 交战圈内(≤ideal)先打; rect级BFS保证路径真实可达, 不会卡墙
    if (d > 160.0f && map) {
        int room_step = _bfs_toward_room(p, map);
        if (room_step >= 0) return (dir == room_step) ? 0.6f : 0.0f;
    }

    // Q3.1: 理想距离按玩家真实武器判定 — 近战FIST不可风筝(火系profile会抖动挨打)
    float atk_range = (p->weapon.weapon_type() != WeaponType::FIST) ? 2.5f : 1.5f;
    float ideal_dist = atk_range + _prefer_range * 2.0f; // 近战=1.5, 远程=2.5~4.5

    // Q3.2: 已到攻击圈内 → 站桩攻击/放技能, 不移动
    // Q3.15: 此处存在已知理论缺陷 — d ∈ (48px, ideal_dist] 区间 attack=0/move=0,
    // 远程 build 死区宽达 ~90px。曾尝试激活"拉开距离"分支消除死区, 实测 200 局
    // 胜率 10.0%→3.5%(风筝震荡破坏 Q3.12 数值平衡), 故回退保留站桩行为。
    // 后续若重调此段必须同步重跑 500 局平衡回归。
    if (d <= ideal_dist * 32.0f) return 0;

    // Q3.2: 太远 → BFS 寻路接近 (绕墙+绕毒, 无路时轴贪心兜底)
    int step = _bfs_toward(p, monsters, map, true);
    if (step < 0) step = _bfs_toward(p, monsters, map, false);
    if (step < 0) step = _greedy_step(p, t, map);
    if (step < 0) return 0.0f;

    // Q3.2: 路径记忆 — 同一目标沿用上次实际走的步, 消除 BFS 等权震荡
    if (t && _mem_target == t->instance_id && _mem_step >= 0) {
        float mdx = (_mem_step == 2) ? -32.0f : (_mem_step == 3) ? 32.0f : 0.0f;
        float mdy = (_mem_step == 0) ? -32.0f : (_mem_step == 1) ? 32.0f : 0.0f;
        Rectangle mr = p->entity.rect;
        mr.x += mdx; mr.y += mdy;
        float nd = hypotf(ex - (px + mdx), ey - (py + mdy));
        if (map->is_rect_walkable(mr) && nd < d)
            return (dir == _mem_step) ? 0.8f : 0.0f;
    }
    return (dir == step) ? 0.6f : 0.0f;
}

float DecisionAgent::_evaluate_pickup(const Player* p, const GameMap* map,
    const std::vector<Monster*>& monsters) const {
    if (!map) return 0;
    float threat = 0.0f;
    for (auto* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        float md = hypotf(m->entity.rect.x + m->entity.rect.width/2 - (p->entity.rect.x + p->entity.rect.width/2),
                          m->entity.rect.y + m->entity.rect.height/2 - (p->entity.rect.y + p->entity.rect.height/2));
        threat = std::max(threat, 1.0f - md / (6.0f * 32.0f));
    }
    for (auto& sr : map->special_rooms) {
        if (sr.triggered) continue; // Q3.2: 已拾取房间不再给分 — 否则 loot 完站桩等死
        float dx = p->entity.rect.x + p->entity.rect.width/2 - (sr.cx * 32 + 16);
        float dy = p->entity.rect.y + p->entity.rect.height/2 - (sr.cy * 32 + 16);
        // Q3.1: 只有站在房间上(≤1格)才给分 — 且随最近威胁衰减, 被围殴时不得锁死拾取
        // Q3.2: 残血时给 2.0 保底分 (泉水满血优先), 威胁权重减半
        if (sqrtf(dx*dx+dy*dy) < 1.0f * 32.0f) {
            // Q3.1: 只有站在房间上(≤1格)才给分 — 且随最近威胁衰减, 被围殴时不得锁死拾取
            float threat_w = threat;
            float base_w  = 1.5f;
            return base_w * (1.0f - threat_w);
        }
    }
    return 0;
}

// ═══════════════════════════════════════════════════════════
//  G7.4: Best action selection (evaluate → pick max)
// ═══════════════════════════════════════════════════════════

std::string DecisionAgent::best_action(const Player* player,
    const std::vector<Monster*>& monsters,
    const GameMap* map, bool stairs_active, bool boss_intro_active) {
    if (!player) return "";

    if (boss_intro_active) return "confirm";
    if (stairs_active) {
        // Q3.2: 清层后先搜刮未触发特殊房 (原逻辑直接下楼 → 整层资源全丢)
        bool can_move = false;
        std::string move_act;
        if (map) {
            for (auto& sr : map->special_rooms) {
                if (sr.triggered) continue;
                float ddx = player->entity.rect.x + player->entity.rect.width/2 - (sr.cx * 32 + 16);
                float ddy = player->entity.rect.y + player->entity.rect.height/2 - (sr.cy * 32 + 16);
                if (sqrtf(ddx*ddx + ddy*ddy) < 1.0f * 32.0f) return "pickup";
            }
            int rs = _bfs_toward_room(player, map);
            if (rs >= 0) {
                const char* dl[] = {"move_up","move_down","move_left","move_right"};
                float mdxs[4] = {0,0,-32,32}, mdys[4] = {-32,32,0,0};
                for (int try_d = 0; try_d < 4; try_d++) {
                    int d2 = (try_d == 0) ? rs : (rs + try_d) % 4;
                    Rectangle er = player->entity.rect;
                    er.x += mdxs[d2]; er.y += mdys[d2];
                    if (map->is_rect_walkable(er)) { can_move = true; move_act = dl[d2]; break; }
                }
            }
        }
        if (can_move) {
            // Q3.2-fix: 搜刮被半格偏移卡死 → 原地 ≥2s 放弃搜刮直接下楼
            int ct0 = (int)(player->entity.rect.x + player->entity.rect.width / 2) / 32;
            int ct1 = (int)(player->entity.rect.y + player->entity.rect.height / 2) / 32;
            if (abs(ct0 - _loot_last_tx) + abs(ct1 - _loot_last_ty) >= 2) {
                _loot_last_tx = ct0; _loot_last_ty = ct1; _loot_stuck_since = -1;
            } else if (_loot_stuck_since < 0) {
                _loot_stuck_since = (float)_game_time;
            } else if ((float)_game_time - _loot_stuck_since > 2.0f) {
                sim_stuck_loot_wd++;   // M2-C: 搜刮看门狗强制下楼计数
                return "descend";
            }
            return move_act;
        }
        return "descend";
    }

    // ── G8.3: MCTS path (combat-only, enemies present) ──
    if (g_use_mcts && !monsters.empty()) {
        auto sim = build_sim_state(player, monsters, _game_time);
        if (sim.alive_monsters() > 0) {
            mcts::MCTS mcts(g_mcts_iters);
            mcts::CombatAction ca = mcts.search(sim);
            return mcts::action_name(ca);
        }
    }

    // Q3.2: 卡死逃脱 — 原地 ≥2s 且无近距怪 → 直线脱困 (先于一切评分)
    // Q3.10: 仅怪物血量总和变动视为战斗 (毒/环境只影响自身HP, 不得掩盖卡死)
    // P0-M2: 锚点半径卡死判定 — 原同 tile 判定被"两 tile 来回震荡"永久重置
    // (BFS 路径记忆在墙前 L/R 横跳 896s 不触发传送 → 900s 死局)
    if (player && map) {
        int pt0 = (int)(player->entity.rect.x + player->entity.rect.width / 2) / 32;
        int pt1 = (int)(player->entity.rect.y + player->entity.rect.height / 2) / 32;
        float drift = hypotf((float)(pt0 - (int)_last_px), (float)(pt1 - (int)_last_py));
        bool local_wander = (drift <= 2.0f);   // 2-tile 徘徊半径
        if (!local_wander) { _stuck_since = -1; _escape_dir = -1;
                             _last_px = (float)pt0; _last_py = (float)pt1; }
        else {
            if (pt0 != (int)_last_px || pt1 != (int)_last_py) { _escape_dir = -1; }
            float mon = 0;
            for (const Monster* m : monsters) if (m->combat.is_alive) mon += m->combat.current_hp;
            if (mon != _last_mon_sum) { _stuck_since = -1; _escape_dir = -1; }
            _last_mon_sum = mon;
            if (_stuck_since < 0) {
                _stuck_since = (float)_game_time;
                _last_px = (float)pt0; _last_py = (float)pt1;  // 锚定徘徊中心
            }
            else if ((float)_game_time - _stuck_since > 8.0f) {
                // Q3.10: ≥8s 原地徘徊 → 兜底传送 (口袋/毒池/隔墙死局, 不依赖怪距)
                if (_teleport_player_to_nearest(const_cast<Player*>(player), monsters, map, _rooms)) {
                    sim_stuck_teleports++;   // M2-C: [PLAYER-FIX] 传送计数
                    _stuck_since = -1; _escape_dir = -1;
                    _last_px = -999; _last_py = -999;   // 传送后重新锚定
                }
                return "none";
            }
            else if ((float)_game_time - _stuck_since > 2.0f &&
                     _count_in_range(player, monsters, 1.5f * 32.0f) == 0) {
                // M2-C: 旋转脱困计数 — 仅首次进入脱困状态计 1 次
                // (4 方向轮换每换向都触发本分支, 按事件计数而非按换向计数)
                if (_escape_dir < 0) { sim_stuck_rotations++; _escape_dir = 0; }
                float mdx = (_escape_dir == 2) ? -1.0f : (_escape_dir == 3) ? 1.0f : 0.0f;
                float mdy = (_escape_dir == 0) ? -1.0f : (_escape_dir == 1) ? 1.0f : 0.0f;
                Rectangle er = player->entity.rect;
                er.x += mdx * 32; er.y += mdy * 32;
                if (!map->is_rect_walkable(er) ||
                    _is_hazard_near(er.x + er.width/2, er.y + er.height/2, map))
                    _escape_dir = (_escape_dir + 1) % 4;
                const char* esc[] = {"move_up","move_down","move_left","move_right"};
                return esc[_escape_dir];
            }
        }
    }

    float best_score = 0;
    std::string best = "";

    // Attack
    float atk_score = _evaluate_attack(player, monsters);
    if (atk_score > best_score) { best_score = atk_score; best = "attack"; }

    // Skills (priority-ordered)
    for (int si = 0; si < 4; si++) {
        int slot = _skill_priority[si];
        float sk_score = _evaluate_skill(slot, player, monsters);
        if (sk_score > best_score) {
            best_score = sk_score;
            best = "skill_" + std::to_string(slot + 1);
        }
    }

    // Heal decision (G7.4: HP below threshold → prioritize heal)
    if (_hp_ratio(player) < _prefer_heal) {
        for (int si = 0; si < 4; si++) {
            int slot = _skill_priority[si];
            // Q3.1: 只有真·治疗槽才触发 — 火系玩家初始无自愈时不得锁死 skill_3
            // Q3.2: 冷却中不得锁死 — 否则站桩等CD被环境伤害磨死
            if (slot < (int)player->skills.active_skills.size() &&
                dynamic_cast<SelfHealSkill*>(player->skills.active_skills[slot].get()) &&
                player->skills.active_skills[slot]->can_use(_game_time)) {
                best_score = 3.0f; // override other actions
                best = "skill_" + std::to_string(slot + 1);
                break; // Q3.15 (P1-2 fix): _skill_priority 已按优先序排列, 首个可用即最优 —
                       // 原 continue 遍历使最低优先级槽反向覆盖
            }
        }
    }

    // Q3.3: 药水 — 残血且本帧无可发自愈技能 → 喝治疗药水 (1s CD 防连灌)
    // M4.4: Boss战阈值 0.35→0.55 (冻结 1.5s 后血量会被秒杀, 必须提前喝)
    // M4.4: Boss蓄力瞬间不喝 — 优先 _evaluate_move 的躲招 (1.4 分 > 药水收益)
    float potion_line = _prefer_heal;
    auto* boss = _find_nearest(player, monsters);
    if (boss && boss->is_boss) potion_line = 0.55f;
    if ((best.empty() || best[0] != 's') && _hp_ratio(player) < potion_line &&
        _game_time - _last_potion_time > 1.0f &&
        !(boss && boss->is_boss && _boss_winding_up(boss))) {
        for (const auto& it : player->inventory.items) {
            const auto* c = dynamic_cast<const ConsumableItem*>(it.get());
            if (c && c->effect_type == "heal") {
                _last_potion_time = _game_time;
                return "use_potion";
            }
        }
    }

    // Pickup
    float pu_score = _evaluate_pickup(player, map, monsters);
    if (pu_score > best_score) { best_score = pu_score; best = "pickup"; }

    // Movement (pick best direction)
    float move_scores[4];
    for (int d = 0; d < 4; d++)
        move_scores[d] = _evaluate_move(d, player, monsters, map);
    int best_dir = 0;
    for (int d = 1; d < 4; d++)
        if (move_scores[d] > move_scores[best_dir]) best_dir = d;
    // Q3.2: 方向迟滞 — 当前方向仍接近最优(±0.03)时保持, 避免对角逼近时逐帧翻转抖动
    if (_current_dir >= 0 && move_scores[_current_dir] >= move_scores[best_dir] - 0.03f)
        best_dir = _current_dir;
    if (move_scores[best_dir] > best_score) {
        const char* dirs[] = {"move_up","move_down","move_left","move_right"};
        best = dirs[best_dir];
        _current_dir = best_dir;
        _mem_step = best_dir;
        auto* mem_t = _find_nearest(player, monsters);
        _mem_target = mem_t ? mem_t->instance_id : 0;
        // Sim: 目标 tile 是 CLOSED 门 → 先开门再走
        if (map && best != "pickup") {
            float mdx[] = {0, 0, -1, 1}, mdy[] = {-1, 1, 0, 0};
            auto [cx, cy] = map->pixel_to_tile(
                player->entity.rect.x + player->entity.rect.width/2,
                player->entity.rect.y + player->entity.rect.height/2);
            int ntx = cx + (int)mdx[best_dir], nty = cy + (int)mdy[best_dir];
            if (map->door_state_at(ntx, nty) == DoorState::CLOSED)
                best = "pickup";
        }
    }

    // If nothing better — move randomly
    if (best.empty()) {
        const char* rand_dirs[] = {"move_up","move_down","move_left","move_right"};
        best = rand_dirs[rng() % 4];
    }
    // P0-M1 诊断: 每 2s 采样一次最终决策 + 上下文 (仅 sim)
    {
        static float s_next = 0.0f;
        if (_game_time >= s_next) {
            s_next = _game_time + 2.0f;
            auto* nt = _find_nearest(player, monsters);
            float nd = nt ? hypotf(nt->entity.rect.x + 14 - (player->entity.rect.x + player->entity.rect.width/2),
                                    nt->entity.rect.y + 14 - (player->entity.rect.y + player->entity.rect.height/2)) : -1;
            printf("[P0DIAG2] t=%.0f best='%s' atk=%.2f hp=%.2f near=%.0f pt=(%.0f,%.0f) nt=(%.0f,%.0f) m0..3=(%.2f,%.2f,%.2f,%.2f) stairs=%d\n",
                _game_time, best.c_str(), atk_score, _hp_ratio(player), nd,
                player->entity.rect.x, player->entity.rect.y,
                nt ? nt->entity.rect.x : -1, nt ? nt->entity.rect.y : -1,
                move_scores[0], move_scores[1], move_scores[2], move_scores[3],
                (int)stairs_active);
        }
    }
    return best;
}

// ═══════════════════════════════════════════════════════════
//  G7.4: Event decision
// ═══════════════════════════════════════════════════════════

bool DecisionAgent::accept_event(float risk_pct, const std::string& effect_desc,
                                  const Player* player) const {
    if (!player) return false;
    float hp = _hp_ratio(player);

    // Never suicide
    if (risk_pct > 0.40f && hp < 0.50f) return false;
    if (risk_pct > 0.25f && hp < 0.30f) return false;
    if (risk_pct > 0.10f && hp < 0.15f) return false;

    // High-value effects worth risking for
    bool high_value = (effect_desc.find("relic") != std::string::npos) ||
                      (effect_desc.find("skill_level") != std::string::npos) ||
                      (effect_desc.find("legendary") != std::string::npos);

    if (high_value && hp > 0.60f) return true;
    if (risk_pct == 0) return true;  // no risk → always accept

    // Moderate risk: accept if HP > risk*2 + buffer
    return hp > risk_pct * 2.0f + 0.25f;
}

// ═══════════════════════════════════════════════════════════
//  Backward compat: is_action_just_pressed gate
// ═══════════════════════════════════════════════════════════

bool DecisionAgent::is_action_just_pressed(const char* action_name,
    const Player* player, const std::vector<Monster*>& monsters,
    const GameMap* map, bool stairs_active, bool boss_intro_active) {
    if (!player) return false;

    // Q3.1: 同帧内所有动作名查询共享同一次 best_action 结果
    if (_cached_frame != _frame) {
        _cached_best = best_action(player, monsters, map, stairs_active, boss_intro_active);
        _cached_frame = _frame;
    }
    return !_cached_best.empty() && _cached_best == action_name;
}

// ═══════════════════════════════════════════════════════════
//  Helpers (unchanged from G5.6)
// ═══════════════════════════════════════════════════════════

Monster* DecisionAgent::_find_nearest(const Player* player,
    const std::vector<Monster*>& monsters) const {
    Monster* best = nullptr; float bd = 99999;
    float px = player->entity.rect.x + player->entity.rect.width/2;
    float py = player->entity.rect.y + player->entity.rect.height/2;
    for (auto* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        float d = hypotf(m->entity.rect.x + m->entity.rect.width/2 - px,
                         m->entity.rect.y + m->entity.rect.height/2 - py);
        if (d < bd) { bd = d; best = m; }
    }
    return best;
}

int DecisionAgent::_count_in_range(const Player* player,
    const std::vector<Monster*>& monsters, float range_px) const {
    int n = 0;
    float px = player->entity.rect.x + player->entity.rect.width/2;
    float py = player->entity.rect.y + player->entity.rect.height/2;
    for (auto* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        float d = hypotf(m->entity.rect.x + m->entity.rect.width/2 - px,
                         m->entity.rect.y + m->entity.rect.height/2 - py);
        if (d < range_px) n++;
    }
    return n;
}

float DecisionAgent::_hp_ratio(const Player* p) const {
    if (!p || p->combat.max_hp <= 0) return 0;
    return (float)p->combat.current_hp / (float)p->combat.max_hp;
}

void DecisionAgent::_pick_direction(const Player* player,
    const std::vector<Monster*>& monsters) {
    Monster* t = _find_nearest(player, monsters);
    if (t) {
        float dx = t->entity.rect.x + t->entity.rect.width/2 -
                   (player->entity.rect.x + player->entity.rect.width/2);
        float dy = t->entity.rect.y + t->entity.rect.height/2 -
                   (player->entity.rect.y + player->entity.rect.height/2);
        if (fabsf(dx) > fabsf(dy))
            _current_dir = (dx > 0) ? 3 : 2;
        else
            _current_dir = (dy > 0) ? 1 : 0;
    } else {
        _current_dir = rng() % 4;
    }
}
