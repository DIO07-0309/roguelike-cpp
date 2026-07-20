#pragma once
#include <string>
#include <vector>
#include <cstdint>

// ============================================================
// G5.6: SimRunner — automated balance simulation
// Hooks into _is_action_just_pressed gate (same as ReplayPlayer)
// ============================================================

struct RunStats {
    uint32_t seed = 0;
    int floor_reached = 0;
    int bosses_killed = 0;
    int total_kills = 0;
    int elite_kills = 0;
    int relics_collected = 0;
    int build_type = 0;          // BuildType int
    const char* build_name = "";
    int total_runs = 0;
};

struct BalanceReport {
    int total_runs = 0;
    int avg_floor = 0;
    int boss_kill_rate[3] = {0}; // F5, F10, F15 counts
    int build_distribution[12] = {0}; // 12 BuildType counts
    int death_floor_distribution[16] = {0}; // floor 1-15

    std::vector<RunStats> runs;
};

class SimRunner {
public:
    static SimRunner& inst();

    void begin(int total_runs);
    void record_run(const RunStats& stats);
    bool is_active() const { return _active; }
    int  current_run() const { return _current_run; }
    int  total_runs() const { return _total_runs; }
    bool should_restart() const;

    const BalanceReport& report() const { return _report; }
    void finalize();

private:
    bool _active = false;
    int _current_run = 0;
    int _total_runs = 0;
    BalanceReport _report;
};
