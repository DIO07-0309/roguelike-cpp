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

// ── G7.3: Per-run result ──────────────────────────────────
struct RunResult {
    uint32_t seed = 0;
    bool victory = false;            // G7.3: explicit win/loss
    int floor_reached = 0;
    int turns = 0;
    int damage_dealt = 0;
    int damage_taken = 0;
    int enemies_killed = 0;
    int bosses_killed = 0;
    int elite_kills = 0;
    int relics_collected = 0;
    int build_type = 0;
    std::string build_name;
    std::vector<std::string> relics_picked;
    std::vector<std::string> enemies_fought;
    int total_runs = 0;
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

// ── G7.3: Full balance report ─────────────────────────────
struct BalanceReport {
    int total_runs = 0;
    int total_wins = 0;
    float win_rate = 0;
    int avg_floor = 0;
    int avg_turns = 0;
    int avg_damage_dealt = 0;
    int avg_damage_taken = 0;
    int avg_relics = 0;
    int boss_kill_count[3] = {0};          // F5, F10, F15 counts
    int death_floor_dist[16] = {0};        // floor 1-15
    int build_count[13] = {0};             // 12 BuildType counts (index 0 unused)

    std::vector<RunResult> runs;
    std::unordered_map<std::string, BuildStats> builds;
    std::unordered_map<std::string, RelicStats> relics;
    std::unordered_map<std::string, EnemyStats> enemies;

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
