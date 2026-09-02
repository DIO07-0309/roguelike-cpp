#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

// ============================================================
// G7.3: Simulation & Balance Framework (extends G5.6 SimRunner)
// ============================================================

// ── G7.3: Configuration ────────────────────────────────────
struct SimulationConfig {
    int runs = 10000;
    uint32_t seed_start = 0;
    bool random_build = true;
    bool fixed_build = false;     // --sim-build blood
    std::string fixed_build_id;
};

// ── M2-A: Run 结果分类 — 不再让所有失败混成一个 "victory=false" ──
enum class RunOutcome : uint8_t {
    VICTORY = 0,        // F15 通关
    DEATH_MONSTER,      // 普通怪/精英击杀
    DEATH_BOSS,         // Boss 战内死亡 (floor 5/10/15)
    DEATH_DOT,          // 毒/持续伤害 (buff tick)
    DEATH_ENVIRONMENT,  // 尖刺/岩浆/桶爆
    TIMEOUT_GAME,       // 900s 游戏时上限 (确定性)
    TIMEOUT_WALL,       // 帧数兜底上限 (原 600s 墙钟, M2-E 已确定性化)
    STUCK_RECOVERED,    // 看门狗强制结算
};
inline const char* run_outcome_name(RunOutcome o) {
    switch (o) {
    case RunOutcome::VICTORY:           return "VICTORY";
    case RunOutcome::DEATH_MONSTER:     return "DEATH_MONSTER";
    case RunOutcome::DEATH_BOSS:        return "DEATH_BOSS";
    case RunOutcome::DEATH_DOT:        return "DEATH_DOT";
    case RunOutcome::DEATH_ENVIRONMENT: return "DEATH_ENVIRONMENT";
    case RunOutcome::TIMEOUT_GAME:      return "TIMEOUT_GAME";
    case RunOutcome::TIMEOUT_WALL:      return "TIMEOUT_WALL";
    case RunOutcome::STUCK_RECOVERED:   return "STUCK_RECOVERED";
    }
    return "UNKNOWN";
}

// ── G7.3: Per-run result ──────────────────────────────────
struct RunResult {
    uint32_t seed = 0;
    bool victory = false;            // G7.3: explicit win/loss (M2: 保留旧读者兼容)
    int floor_reached = 0;
    int turns = 0;
    int damage_dealt = 0;
    int damage_taken = 0;
    int heal_total = 0;
    int enemies_killed = 0;
    int bosses_killed = 0;
    int elite_kills = 0;
    int relics_collected = 0;
    int equipment_count = 0;   // Q3.2: 自动装备数量 (已穿上的装备)
    int build_type = 0;
    std::string build_name;
    std::vector<std::string> relics_picked;
    std::vector<std::string> enemies_fought;
    int total_runs = 0;

    // ── M2: 仪表盘字段 (全默认值, 死亡/超时点填充) ──
    RunOutcome outcome = RunOutcome::VICTORY;
    std::string death_cause;        // M2-B: CombatStats::last_damage_source 快照
    int death_floor = 0;            // M2: 死亡时楼层 (=floor_reached, 显式记录避免歧义)
    int weapon_type_final = 0;      // M2-D: 武器类型枚举
    std::string weapon_id_final;    // M2-D: 武器 def id
    int element_type = 0;           // M2-D: 元素 (sim 恒火 — 如实记录暴露覆盖缺口)
    int level_final = 0;            // M2-D
    int relics_held = 0, buffs_held = 0;   // M2-D: 死亡快照
    int combat_frames = 0;          // M2-C: 有敌交战帧数
    int rooms_discovered = 0;       // M2-C
    int items_picked = 0;           // M2-C
    int gold_earned = 0;            // M2-C: 死亡时 gold 快照
    int stuck_teleports = 0;        // M2-C: [PLAYER-FIX] 传送次数
    int stuck_rotations = 0;        // M2-C: 旋转脱困次数
    int loot_watchdog_descends = 0; // M2-C: 搜刮看门狗强制下楼
};

// ── G7.3: Per-build aggregate ─────────────────────────────
struct BuildStats {
    std::string build_id;
    int games = 0;
    int wins = 0;
    float avg_floor = 0;
    float win_rate() const { return games > 0 ? (float)wins / games : 0; }
};

// ── G7.3: Per-relic aggregate ─────────────────────────────
struct RelicStats {
    std::string relic_id;
    int picked = 0;
    int wins = 0;
    float pick_rate = 0;
    float win_rate() const { return picked > 0 ? (float)wins / picked : 0; }
};

// ── G7.3: Per-enemy aggregate ─────────────────────────────
struct EnemyStats {
    std::string enemy_id;
    int appearances = 0;
    int kills = 0;
    int player_deaths = 0;
};

// ── M2: 结果分类/死因/武器 汇总 ──────────────────────────
struct OutcomeDist {
    int counts[8] = {0};          // RunOutcome 枚举序
    const int* data() const { return counts; }
};
struct CauseStats {
    std::string cause;
    int deaths = 0;
};
struct WeaponPerf {
    std::string weapon_id;
    int games = 0;
    int wins = 0;
    float avg_floor = 0;
    float win_rate() const { return games > 0 ? (float)wins / games : 0; }
};

// ── G7.3: Full balance report ─────────────────────────────
struct BalanceReport {
    int total_runs = 0;
    int total_wins = 0;
    float win_rate = 0;
    int avg_floor = 0;
    int avg_turns = 0;
    int avg_damage_dealt = 0;
    int avg_damage_taken = 0;
    int avg_heal = 0;
    int avg_relics = 0;
    int avg_equipment = 0;   // Q3.2: 平均每局自动装备数
    int boss_kill_count[3] = {0};          // F5, F10, F15 counts
    int death_floor_dist[16] = {0};        // floor 1-15
    int build_count[13] = {0};             // 12 BuildType counts (index 0 unused)

    std::vector<RunResult> runs;
    std::unordered_map<std::string, BuildStats> builds;
    std::unordered_map<std::string, RelicStats> relics;
    std::unordered_map<std::string, EnemyStats> enemies;
    // M2 仪表盘汇总
    OutcomeDist outcome_dist;
    std::unordered_map<std::string, CauseStats> causes;     // cause → deaths
    std::unordered_map<std::string, WeaponPerf> weapons;    // weapon_id → perf

    // G7.3: JSON serialization
    std::string to_json() const;
    bool save_to_file(const char* path) const;
};

// ── G7.3: Simulation runner ───────────────────────────────
class SimRunner {
public:
    static SimRunner& inst();

    void begin(const SimulationConfig& cfg);
    void record_run(const RunResult& stats);
    bool is_active() const { return _active; }
    int  current_run() const { return _current_run; }
    int  total_runs() const { return _cfg.runs; }
    bool should_restart() const;
    uint32_t next_seed() const;
    // Q3.5: 当前局种子 (run 1 的 new_game 需要在 enter_floor 前播种 rng)
    uint32_t current_seed() const { return _cfg.seed_start + (uint32_t)_current_run * 1234567u; }
    // G7.4: all-builds rotation
    bool all_builds() const { return _all_builds; }
    void set_all_builds(bool v) { _all_builds = v; }
    int  next_build_type() const;

    const BalanceReport& report() const { return _report; }
    void finalize();

private:
    bool _active = false;
    int _current_run = 0;
    bool _all_builds = false;
    int _all_builds_next = 0;
    SimulationConfig _cfg;
    BalanceReport _report;
};
