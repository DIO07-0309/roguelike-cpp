#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "ai/mcts/simulation_state.h"

class Player;
class Monster;
class GameMap;
enum class BuildType;

// ============================================================
// G7.4: Decision Agent — build-aware AI with action evaluation
// Replaces G5.6 SimAI with behavioral profiles per BuildType.
// ============================================================

struct ActionScore {
    float value = 0;
    const char* action = "";
};

class DecisionAgent {
public:
    // ── P1-A2: 地面物品感知 (GameScene 每帧只读注入, 归属不变) ──
    // SimAI 决策输入瓶颈: ground_items 原不在 AI 视野 → picks=0 → 无药/无武器
    struct GroundSpot {
        int tile_x, tile_y;
        bool is_potion;               // 药水类 (heal 效果) — 残血时权重加倍
    };
    void set_ground_items(std::vector<GroundSpot> spots) { _ground = std::move(spots); }

    // ── P1-C3: 楼梯感知 (GameScene 每帧只读注入) ──
    // 病理: stairs_active 后 AI 返回 "descend" 但人不走向楼梯格 →
    // _check_floor_transition 只认"站在楼梯上按 E" → 站原地按 E 600s 兜底
    // (P1-C3 探针: 9/20 局 stairs=1 monsters=0 卡死于此)
    // 楼层切换检测: 楼梯坐标变化 → 重置搜刮状态 (新层重新允许搜刮)
    void set_stairs_pos(int tx, int ty) {
        if (tx != _stairs_tx || ty != _stairs_ty) {
            _loot_abandoned = false;
            _stairs_since = -1.0;    // P1-C3-fix: 新层搜刮预算重开
        }
        _stairs_tx = tx; _stairs_ty = ty;
    }

    DecisionAgent();

    void start(const Player* player);
    void tick();
    void set_time(double t) { _game_time = t; }
    // P0-M2: room domain context (set once per scene by GameScene)
    void set_room_manager(const class RoomManager* rm) { _rooms = rm; }

    // ── Main entry: returns the best action for this frame ──
    std::string best_action(const Player* player,
        const std::vector<Monster*>& monsters,
        const GameMap* map, bool stairs_active, bool boss_intro_active);

    // ── Event decision (G7.4) ──
    bool accept_event(float risk_pct, const std::string& effect_desc,
                      const Player* player) const;

    // ── Fake input gate (compat with existing _is_action_just_pressed) ──
    bool is_action_just_pressed(const char* action_name,
        const Player* player,
        const std::vector<Monster*>& monsters,
        const GameMap* map,
        bool stairs_active,
        bool boss_intro_active);

private:
    int  _frame = 0;
    float _dir_timer = 0;
    int  _current_dir = -1;
    double _game_time = 0;
    const class RoomManager* _rooms = nullptr;   // P0-M2

    // Q3.1: 帧级 best_action 缓存 — 同帧多次查询(每动作名一次)结果必须一致
    int _cached_frame = -1;
    std::string _cached_best;

    // G7.4: Build-aware profile
    BuildType _build_type = (BuildType)0;
    float _prefer_range = 0;      // 0=melee aggro, 1=kite & keep distance
    float _prefer_aoe = 0;        // 0=single target, 1=fight groups
    float _prefer_skill = 0;      // 0=basic attacks, 1=skills first
    float _aggro_bias = 0.5f;     // how aggressively to approach enemies
    float _prefer_heal = 0;       // heal threshold (HP% below which heal used)
    int _skill_priority[4] = {0,1,2,3}; // skill index priority order

    // G7.4: Action evaluation
    float _evaluate_attack(const Player* p, const std::vector<Monster*>& monsters) const;
    float _evaluate_skill(int slot, const Player* p,
                          const std::vector<Monster*>& monsters) const;
    float _evaluate_move(int dir, const Player* p,
                         const std::vector<Monster*>& monsters,
                         const GameMap* map) const;
    float _evaluate_pickup(const Player* p, const GameMap* map,
                           const std::vector<Monster*>& monsters) const;

    // Helpers
    Monster* _find_nearest(const Player* player,
                           const std::vector<Monster*>& monsters) const;
    int _count_in_range(const Player* player,
                        const std::vector<Monster*>& monsters, float range_px) const;
    float _hp_ratio(const Player* p) const;
    void _pick_direction(const Player* player,
                         const std::vector<Monster*>& monsters);
    void _resolve_profile(const Player* player);
    // Q3.2: BFS 寻路辅助 — 返回第一步方向 (0-3, -1=不可达)
    int _bfs_toward(const Player* p, const std::vector<Monster*>& monsters,
                    const GameMap* map, bool avoid_hazard) const;
    int _bfs_away(const Player* p, const Monster* t, const GameMap* map,
                  bool avoid_hazard) const;
    // Q3.2: 轴贪心兜底 — BFS 无路时直行逼近 (精确rect校验+避毒)
    int _greedy_step(const Player* p, const Monster* t, const GameMap* map) const;
    // Q3.2: 路径记忆 — 同一目标持续沿上一步走, 消除 BFS 等权震荡
    // (instance_id 键: 原指针键跨进程堆地址不同 + 地址复用 → 旧记忆污染新怪 → 决策分叉)
    mutable int _mem_step = -1;
    mutable uint64_t _mem_target = 0;
    // Q3.2: 卡死逃脱 — 原地 ≥2s 且无近距怪 → 直线脱困 (口袋/贴墙钉子户)
    mutable float _stuck_since = -1.0f;
    mutable float _last_px = -1.0f, _last_py = -1.0f;
    mutable float _last_hp_sum = -1.0f, _last_mon_sum = -1.0f;  // 换血检测
    mutable int _loot_last_tx = -999, _loot_last_ty = -999;     // 搜刮卡死看门狗
    mutable float _loot_stuck_since = -1.0f;
    // P1-C3: 本层搜刮放弃标记 — 看门狗触发后置位, 直奔楼梯 (原直接 "descend"
    // 但人不在楼梯格按 E 无效 → 搜刮→卡2s→descend→搜刮 循环 600s)
    mutable bool _loot_abandoned = false;
    // P1-C3-fix: 层级搜刮预算 — stairs 激活起计时, 15s 内没完成搜刮就下楼.
    // 病理: 楼梯修复后节奏 x7 快, 但"每层全搜刮"让 deep 局资源不足被围殴
    // (P1-C3 vs P1-C2 500局: TWall 29→4 但 deep 22→5, s3) — 搜刮要限时限层
    mutable float _stairs_since = -1.0f;
    mutable int _escape_dir = -1;
    // Q3.3: 药水决策冷却 — 防止残血时逐帧连喝清空背包
    mutable double _last_potion_time = -999.0;
    // P1-A2: 地面物品快照 (每帧 set_ground_items 注入)
    std::vector<GroundSpot> _ground;
    // P1-C3: 楼梯目标 tile (-1=未注入) — set_stairs_pos 每帧注入
    int _stairs_tx = -1, _stairs_ty = -1;
    // P1-C3: BFS 至楼梯, 返回第一步方向 (0-3, -1=不可达/已在格)
    int _bfs_to_stairs(const Player* p, const GameMap* map) const;
    // Q3.2: 危险视野 — 活性毒池/尖刺圈内判定 (半径 1.5 格)
    bool _is_hazard_near(float px, float py, const GameMap* map) const;
    // Q3.2: 残血且无可用自愈 → 需去找泉水/祭坛回血
    bool _needs_recovery(const Player* p) const;
    // Q3.2: BFS 至最近未触发特殊房 (回血/增益资源), -1=不可达
    // P1-A3: heal_only — 危急模式只找回血房 (FOUNTAIN/ALTAR/SHRINE)
    int _bfs_toward_room(const Player* p, const GameMap* map,
                          bool heal_only = false) const;
    // P1-A2: BFS 至最近地面物品 (注入的 _ground), -1=不可达
    int _bfs_toward_loot(const Player* p, const GameMap* map) const;
    // P1-A2: 站位 1 格内最近地面物品距离 (px), -1=无
    float _near_loot_dist(const Player* p) const;

public:
    // ── G8.3: MCTS integration ──
    static bool g_use_mcts;       // --sim-ai mcts flag
    static int  g_mcts_iters;    // iterations per search (default 100)

    // ── Q3.3: 本帧决策结果 (game_scene 消费 use_potion 用) ──
    std::string last_best_action() const { return _cached_best; }

    // ── G8.3: Build SimulationState from game state ──
    // Q3.15 (A6 fix): 需要 game_time 计算真实剩余冷却 (原伪造常量导致根节点永久禁用普攻)
    static mcts::SimulationState build_sim_state(
        const Player* player, const std::vector<Monster*>& monsters, double game_time);
};

// ── Backward compat alias ──
using SimAI = DecisionAgent;

// M2-C: 卡墙恢复诊断计数 (定义在 sim_ai.cpp; GameScene::_collect_sim_stats 读取)
// 只加不改 — 测量 M2 红线
extern int sim_stuck_teleports;   // [PLAYER-FIX] 口袋传送次数
extern int sim_stuck_rotations;   // 旋转脱困进入次数
extern int sim_stuck_loot_wd;     // 搜刮看门狗强制下楼次数

