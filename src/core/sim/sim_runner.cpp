#include "sim_runner.h"
#include <cstdio>
#include <algorithm>

SimRunner& SimRunner::inst() { static SimRunner sr; return sr; }

void SimRunner::begin(int total_runs) {
    _active = true;
    _current_run = 0;
    _total_runs = total_runs;
    _report = BalanceReport{};
    _report.runs.reserve(total_runs);
    printf("[SIM] Starting %d runs\n", total_runs);
}

void SimRunner::record_run(const RunStats& stats) {
    _report.runs.push_back(stats);
    _report.total_runs++;

    // aggregate
    _report.avg_floor = (_report.avg_floor * (_report.total_runs - 1) + stats.floor_reached) / _report.total_runs;
    if (stats.bosses_killed >= 1) _report.boss_kill_rate[0]++;
    if (stats.bosses_killed >= 2) _report.boss_kill_rate[1]++;
    if (stats.bosses_killed >= 3) _report.boss_kill_rate[2]++;
    if (stats.build_type >= 0 && stats.build_type < 12)
        _report.build_distribution[stats.build_type]++;
    if (stats.floor_reached >= 1 && stats.floor_reached <= 15)
        _report.death_floor_distribution[stats.floor_reached - 1]++;

    _current_run++;
    if (_current_run < _total_runs) {
        if (_current_run % 10 == 0)
            printf("[SIM] %d/%d complete\n", _current_run, _total_runs);
    }
}

bool SimRunner::should_restart() const {
    return _active && _current_run < _total_runs;
}

void SimRunner::finalize() {
    _active = false;
    auto& r = _report;
    printf("\n═══ BALANCE REPORT: %d runs ═══\n", r.total_runs);
    printf("  Average floor: %d\n", r.avg_floor);
    printf("  Boss kill rates: F5=%.1f%% F10=%.1f%% F15=%.1f%%\n",
        r.boss_kill_rate[0]*100.0/r.total_runs,
        r.boss_kill_rate[1]*100.0/r.total_runs,
        r.boss_kill_rate[2]*100.0/r.total_runs);
    printf("  Build distribution:\n");
    const char* bnames[] = {"NONE","Berserker","FireMage","Poison","TimeM","Support","Projectile","IceMage","Lightning","Bleed","Shadow","Juggernaut","Summon"};
    for (int i = 1; i <= 12; i++) if (r.build_distribution[i] > 0)
        printf("    %s: %.1f%%\n", bnames[i], r.build_distribution[i]*100.0/r.total_runs);
    printf("  Death floors:\n");
    for (int f = 0; f < 15; f++) if (r.death_floor_distribution[f] > 0)
        printf("    F%d: %d\n", f+1, r.death_floor_distribution[f]);
    printf("\n");
}
