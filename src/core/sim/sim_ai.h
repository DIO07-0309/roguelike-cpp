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
    DecisionAgent();

    void start(const Player* player);
    void tick();
    void set_time(double t) { _game_time = t; }

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
    mutable int _mem_step = -1;
    mutable const void* _mem_target = nullptr;
    // Q3.2: 卡死逃脱 — 原地 ≥2s 且无近距怪 → 直线脱困 (口袋/贴墙钉子户)
    mutable float _stuck_since = -1.0f;
    mutable float _last_px = -1.0f, _last_py = -1.0f;
    mutable float _last_hp_sum = -1.0f, _last_mon_sum = -1.0f;  // 换血检测
    mutable int _escape_dir = -1;
    // Q3.2: 危险视野 — 活性毒池/尖刺圈内判定 (半径 1.5 格)
    bool _is_hazard_near(float px, float py, const GameMap* map) const;
    // Q3.2: 残血且无可用自愈 → 需去找泉水/祭坛回血
    bool _needs_recovery(const Player* p) const;
    // Q3.2: BFS 至最近未触发特殊房 (回血/增益资源), -1=不可达
    int _bfs_toward_room(const Player* p, const GameMap* map) const;

public:
    // ── G8.3: MCTS integration ──
    static bool g_use_mcts;       // --sim-ai mcts flag
    static int  g_mcts_iters;    // iterations per search (default 100)

    // ── G8.3: Build SimulationState from game state ──
    static mcts::SimulationState build_sim_state(
        const Player* player, const std::vector<Monster*>& monsters);
};

// ── Backward compat alias ──
using SimAI = DecisionAgent;
