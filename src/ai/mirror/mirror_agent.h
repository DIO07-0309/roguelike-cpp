#pragma once
#include "ai/player_behavior/player_habit_profile.h"
#include "ai/player_behavior/player_action.h"
#include "ai/mirror/online_adaptive_policy.h"
#include "ai/mirror/behavior_clone_table.h"
#include "ai/mirror/rolling_accuracy.h"
#include <vector>
#include <memory>
#include <map>
#include <string>

class Monster;
class Player;

// ============================================================
// F15.3: MirrorAgent — analysis layer, NOT a control layer
// Reads player habits → adjusts BossAI strategy parameters.
// Does NOT call attack/move — BossAI still makes final decisions.
// ============================================================

// Simple battle state snapshot (avoid depending on full BattleState)
struct MirrorBattleState {
    float boss_hp_pct = 1.0f;
    float player_hp_pct = 1.0f;
    float dist_tiles = 5.0f;
    bool  player_attacking = false;
    bool  player_using_skill = false;
    bool  boss_can_attack = false;
    bool  boss_in_domain = false;       // F10 domain phase
    int   player_skills_ready = 0;      // M1: cooldown-ready skill count
};

class MirrorAgent {
public:
    MirrorAgent();

    // ── Initialize from player behavior data (called on F15 enter) ──
    void init(const PlayerHabitProfile& profile);

    // ── Phase 1-2-3 behavior selection ──
    int  current_phase() const { return _phase; }   // 1=observe, 2=mirror, 3=evolve
    void set_phase(int p) { _phase = p; }

    // ── M2: 在线观测与动态阶段触发 ──
    // 预测后立即上报 (附上下文用于同桶命中统计); 玩家实际动作出现时上报比对
    void on_prediction(PlayerActionType predicted, float dist_tiles,
                       float hp_pct, int skills_ready);
    void observe_actual(PlayerActionType actual);
    float prediction_accuracy() const { return _rolling_accuracy.accuracy(); }
    int   observed_actions() const { return _observed_actions; }
    // 当前战斗攻击/技能频率 vs 画像频率的归一化偏差 [0,1] (数据不足返回0)
    float profile_drift() const;
    // 动态阶段判定: P1→P2 (准确率/观察数/时间), P2→P3 (核心模式/濒危) — 替代纯计时
    void  tick_phase(float dt, const MirrorBattleState& st);

    // ── M1: behavior-clone layer (built from F1-F14 action stream) ──
    void set_clone_table(std::unique_ptr<BehaviorCloneTable> t);
    const BehaviorCloneTable* clone_table() const { return _clone.get(); }

    // ── During combat: recommend BossAI adjustments ──
    float recommend_distance() const;
    bool should_interrupt_skill(const MirrorBattleState& st) const;
    bool should_pressure_close(const MirrorBattleState& st) const;
    PlayerActionType predict_next_action(const MirrorBattleState& st) const;

    // ── M4e: 在线自适应 — Thompson 决策与反馈 ──
    // Phase<2 返回 -1 (观察期用规则); 否则采样决策并记录待反馈状态
    int recommend_action(const MirrorBattleState& st);
    // 战斗中上报最近一次决策的结果: 命中/落空 → 更新 Beta 后验
    void report_outcome(bool hit, float damage);

    // 跨对局记忆: 导出 Beta (alpha, beta) 供存档; 导入叠加回先验
    void export_memory(std::vector<float>& alpha, std::vector<float>& beta) const;
    void import_memory(const std::vector<float>& alpha,
                       const std::vector<float>& beta_vals);

    // ── M4e: HUD 学习可视化查询 (只读) ──
    int  last_action() const { return _last_action; }
    int  last_bucket() const { return _last_bucket; }
    float arm_win_rate(int bucket, int action) const;

    // ── F15.4: Mirror reward for RL self-play ──
    // Returns a bonus reward when the Boss successfully counters the player's
    // predicted action (interrupting a skill, punishing a heal, baiting an attack).
    // Caller adds this to the base damage-healing reward.
    static double mirror_reward(const PlayerHabitProfile& profile,
        int boss_action, double damage_dealt, double damage_taken,
        bool player_dodged, bool player_healed);

    // ── F15.4: Convert PlayerStyle to an int for SimulationState ──
    static int style_to_int(PlayerStyle s);

    // ── Debug ──
    const char* phase_name() const;
    const char* strategy_summary() const { return _profile.counter_strategy_text(); }

private:
    PlayerHabitProfile _profile;
    int  _phase = 1;
    float _preferred_distance = 250.0f;
    // M4e: 在线学习
    std::unique_ptr<OnlineAdaptivePolicy> _policy;
    int _last_bucket = -1;    // 最近一次决策的上下文桶
    int _last_action = -1;    // 最近一次决策的动作臂
    std::unique_ptr<BehaviorCloneTable> _clone;   // M1: 行为克隆层
    static float _rand01();   // [0,1) 均匀采样 (技能窗口探索用)

    // M2: 在线观察
    RollingAccuracy _rolling_accuracy;
    PlayerActionType _last_prediction = PlayerActionType::NONE;
    std::string _last_obs_key;                       // 最近预测的 ObservationKey
    std::map<std::string, int> _bucket_hits;         // 同桶命中计数
    int _observed_actions = 0;
    int _obs_attack = 0, _obs_skill = 0;
    int _obs_dodge = 0, _obs_heal = 0;
    float _battle_time = 0.0f;
    static constexpr int kPhase1Observations = 40;   // 观察数兜底阈值
    int _max_bucket_hits() const;
};
