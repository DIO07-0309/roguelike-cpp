#include "sim_runner.h"
#include "build_score.h"  // G7.4: BuildType enum
#include <cstdio>
#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>

SimRunner& SimRunner::inst() { static SimRunner sr; return sr; }

void SimRunner::begin(const SimulationConfig& cfg) {
    _active = true;
    _current_run = 0;
    _cfg = cfg;
    _report = BalanceReport{};
    _report.runs.reserve(cfg.runs);
    printf("[SIM] Starting %d runs (seed_start=%u)\n", cfg.runs, cfg.seed_start);
}

void SimRunner::record_run(const RunResult& s) {
    _report.runs.push_back(s);
    _report.total_runs++;
    int n = _report.total_runs;

    _report.avg_floor        = (_report.avg_floor   * (n-1) + s.floor_reached) / n;
    _report.avg_turns        = (_report.avg_turns   * (n-1) + s.turns) / n;
    _report.avg_damage_dealt = (_report.avg_damage_dealt * (n-1) + s.damage_dealt) / n;
    _report.avg_damage_taken = (_report.avg_damage_taken * (n-1) + s.damage_taken) / n;
    _report.avg_heal         = (_report.avg_heal * (n-1) + s.heal_total) / n;
    _report.avg_relics       = (_report.avg_relics  * (n-1) + s.relics_collected) / n;
    _report.avg_equipment    = (_report.avg_equipment * (n-1) + s.equipment_count) / n;

    if (s.victory) _report.total_wins++;
    if (s.bosses_killed >= 1) _report.boss_kill_count[0]++;
    if (s.bosses_killed >= 2) _report.boss_kill_count[1]++;
    if (s.bosses_killed >= 3) _report.boss_kill_count[2]++;
    if (s.build_type >= 0 && s.build_type <= 12)
        _report.build_count[s.build_type]++;
    if (s.floor_reached >= 1 && s.floor_reached <= 15)
        _report.death_floor_dist[s.floor_reached - 1]++;

    // Build stats
    if (!s.build_name.empty()) {
        auto& b = _report.builds[s.build_name];
        b.build_id = s.build_name; b.games++;
        if (s.victory) b.wins++;
        b.avg_floor = (b.avg_floor * (b.games-1) + s.floor_reached) / b.games;
    }
    // Relic stats
    for (auto& rid : s.relics_picked) {
        auto& r = _report.relics[rid]; r.relic_id = rid;
        r.picked++; if (s.victory) r.wins++;
    }
    // Enemy stats
    for (auto& eid : s.enemies_fought) {
        auto& e = _report.enemies[eid]; e.enemy_id = eid; e.appearances++;
        // M2: 该敌人是击杀者 → 记 player_deaths (死因归因到 enemy 维度)
        if (s.outcome == RunOutcome::DEATH_MONSTER && eid == s.death_cause)
            e.player_deaths++;
    }
    // M2: 结果分类/死因/武器 汇总
    _report.outcome_dist.counts[(int)s.outcome]++;
    if (!s.death_cause.empty()) {
        auto& c = _report.causes[s.death_cause];
        c.cause = s.death_cause; c.deaths++;
    }
    {
        auto& w = _report.weapons[s.weapon_id_final.empty() ? "none" : s.weapon_id_final];
        w.weapon_id = s.weapon_id_final.empty() ? "none" : s.weapon_id_final;
        w.games++;
        if (s.victory) w.wins++;
        w.avg_floor = (w.avg_floor * (w.games-1) + s.floor_reached) / w.games;
    }
    _report.win_rate = _report.total_runs > 0
        ? (float)_report.total_wins / _report.total_runs : 0;

    _current_run++;
    // G7.4: all-builds rotation — when `_cfg.runs` done for current build, advance
    if (_all_builds && _current_run >= _cfg.runs && _all_builds_next < 12) {
        _all_builds_next++;
        _current_run = 0;
        static const char* bnames[] = {"NONE","Berserker","FireMage","Poison","TimeM","Support",
            "Projectile","IceMage","Lightning","Bleed","Shadow","Juggernaut","Summon"};
        if (_all_builds_next < 12)
            printf("[SIM] Switching to build %d/%d: %s\n",
                   _all_builds_next+1, 12, bnames[_all_builds_next+1]);
    }

    int pct = (_current_run * 100) / _cfg.runs;
    int prev_pct = ((_current_run - 1) * 100) / _cfg.runs;
    if (pct != prev_pct)
        printf("[SIM] %3d%%  %d/%d  (wins:%d, avg_floor:%d)\n",
               pct, _current_run, _cfg.runs, _report.total_wins, _report.avg_floor);
}

bool SimRunner::should_restart() const {
    if (!_active) return false;
    if (_all_builds) {
        // In all-builds mode, restart means move to next build
        return _all_builds_next < 12;
    }
    return _current_run < _cfg.runs;
}

int SimRunner::next_build_type() const {
    if (!_all_builds || _all_builds_next >= 12) return 0; // NONE = random
    return _all_builds_next + 1; // BuildType 1-12
}

uint32_t SimRunner::next_seed() const {
    return _cfg.seed_start + (uint32_t)_current_run * 1234567u;
}

void SimRunner::finalize() {
    _active = false;
    auto& r = _report;
    int N = r.total_runs;
    printf("\n═══ BALANCE REPORT: %d runs ═══\n", N);
    printf("  Win rate:     %.1f%% (%d/%d)\n", r.win_rate*100, r.total_wins, N);
    printf("  Avg floor:    %d\n", r.avg_floor);
    printf("  Avg turns:    %d\n", r.avg_turns);
    printf("  Avg damage:   dealt=%d  taken=%d  heal=%d\n",
        r.avg_damage_dealt, r.avg_damage_taken, r.avg_heal);
    printf("  Avg relics:   %d\n", r.avg_relics);
    printf("  Avg equips:   %d\n", r.avg_equipment);
    printf("  Boss kills:   F5=%.1f%%  F10=%.1f%%  F15=%.1f%%\n",
        r.boss_kill_count[0]*100.0/N, r.boss_kill_count[1]*100.0/N, r.boss_kill_count[2]*100.0/N);

    // Build ranking
    printf("\n  BUILD RANKING:\n");
    std::vector<const BuildStats*> branked;
    for (auto& [id, bs] : r.builds) branked.push_back(&bs);
    std::sort(branked.begin(), branked.end(),
        [](auto* a, auto* b) { return a->win_rate() > b->win_rate(); });
    for (auto* bs : branked) {
        const char* tag = bs->win_rate() > 0.70f ? " [OP]" : bs->win_rate() < 0.25f ? " [UP]" : "";
        printf("    %15s: %3d runs  win=%.1f%%  avg_f=%.1f%s\n",
            bs->build_id.c_str(), bs->games, bs->win_rate()*100, bs->avg_floor, tag);
    }

    // Relic ranking (top 10)
    printf("\n  RELIC TOP 10:\n");
    std::vector<const RelicStats*> rranked;
    for (auto& [id, rs] : r.relics) rranked.push_back(&rs);
    std::sort(rranked.begin(), rranked.end(),
        [](auto* a, auto* b) { return a->picked > b->picked; });
    int s = 0;
    for (auto* rs : rranked) {
        if (s++ >= 10) break;
        float pr = (float)rs->picked / N;
        printf("    %20s: picked=%d  pick=%.1f%%  win=%.1f%%%s\n",
            rs->relic_id.c_str(), rs->picked, pr*100, rs->win_rate()*100,
            pr > 0.80f ? " [HOT]" : pr < 0.05f ? " [COLD]" : "");
    }

    // Enemy threat
    printf("\n  ENEMY THREAT:\n");
    for (auto& [id, es] : r.enemies) {
        float threat = es.appearances > 0 ? (float)es.kills / es.appearances : 0;
        printf("    %20s: appear=%d  kills=%d  threat=%.1f%%%s\n",
            es.enemy_id.c_str(), es.appearances, es.kills, threat*100,
            threat > 0.30f ? " [HIGH]" : "");
    }

    r.save_to_file("reports/balance_report.json");
    printf("\n  Report saved to reports/balance_report.json\n");
}

// ═══════════════════════════════════════════════════════════
//  G7.3: JSON export
// ═══════════════════════════════════════════════════════════

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

std::string BalanceReport::to_json() const {
    using json = nlohmann::json;
    json j;
    j["summary"]["total_runs"]  = total_runs;
    j["summary"]["total_wins"]  = total_wins;
    j["summary"]["win_rate"]    = win_rate;
    j["summary"]["avg_floor"]   = avg_floor;
    j["summary"]["avg_turns"]   = avg_turns;
    j["summary"]["avg_damage_dealt"] = avg_damage_dealt;
    j["summary"]["avg_damage_taken"] = avg_damage_taken;
    j["summary"]["avg_relics"]  = avg_relics;
    j["summary"]["avg_equipment"] = avg_equipment;
    json bk = json::array(); for (int i=0;i<3;i++) bk.push_back(boss_kill_count[i]);
    j["summary"]["boss_kill_counts"] = bk;
    json dd = json::array(); for (int i=0;i<15;i++) dd.push_back(death_floor_dist[i]);
    j["summary"]["death_floor_distribution"] = dd;

    // ── M2 仪表盘: 结果分类 / 死因 Top10 / 武器表现 ──
    {
        json od = json::object();
        const char* names[] = {"VICTORY","DEATH_MONSTER","DEATH_BOSS","DEATH_DOT",
                               "DEATH_ENVIRONMENT","TIMEOUT_GAME","TIMEOUT_WALL","STUCK_RECOVERED"};
        for (int i = 0; i < 8; i++) od[names[i]] = outcome_dist.counts[i];
        j["summary"]["outcome_dist"] = od;
        // cause_top10: 按死亡数排序
        std::vector<std::pair<std::string,int>> cs;
        for (auto& [k, v] : causes) cs.push_back({k, v.deaths});
        std::sort(cs.begin(), cs.end(),
                  [](auto& a, auto& b){ return a.second > b.second; });
        json ct = json::array();
        for (size_t i = 0; i < cs.size() && i < 10; i++) {
            json c; c["cause"]=cs[i].first; c["deaths"]=cs[i].second;
            ct.push_back(c);
        }
        j["summary"]["cause_top10"] = ct;
        // weapon_perf
        json wa = json::array();
        for (auto& [id, w] : weapons) {
            json wj; wj["id"]=w.weapon_id; wj["games"]=w.games; wj["wins"]=w.wins;
            wj["win_rate"]=w.win_rate(); wj["avg_floor"]=w.avg_floor;
            wa.push_back(wj);
        }
        j["weapons"] = wa;
    }

    json barr = json::array();
    for (auto& [id, bs] : builds) {
        json bj; bj["id"]=bs.build_id; bj["games"]=bs.games; bj["wins"]=bs.wins;
        bj["win_rate"]=bs.win_rate(); bj["avg_floor"]=bs.avg_floor;
        barr.push_back(bj);
    }
    j["builds"] = barr;

    json rarr = json::array();
    for (auto& [id, rs] : relics) {
        json rj; rj["id"]=rs.relic_id; rj["picked"]=rs.picked; rj["wins"]=rs.wins;
        rj["pick_rate"] = total_runs>0 ? (float)rs.picked/total_runs : 0;
        rj["win_rate"] = rs.win_rate();
        rarr.push_back(rj);
    }
    j["relics"] = rarr;

    json earr = json::array();
    for (auto& [id, es] : enemies) {
        json ej; ej["id"]=es.enemy_id; ej["appearances"]=es.appearances;
        ej["kills"]=es.kills; ej["player_deaths"]=es.player_deaths;
        ej["threat"] = es.appearances>0 ? (float)es.kills/es.appearances : 0;
        earr.push_back(ej);
    }
    j["enemies"] = earr;

    json runarr = json::array();
    for (const auto& r : runs) {
        json rj; rj["floor"]=r.floor_reached; rj["turns"]=r.turns; rj["victory"]=r.victory;
        rj["damage_dealt"]=r.damage_dealt; rj["damage_taken"]=r.damage_taken;
        rj["heal_total"]=r.heal_total; rj["kills"]=r.enemies_killed;
        rj["bosses"]=r.bosses_killed; rj["equip"]=r.equipment_count;
        rj["relics"]=r.relics_collected; rj["build"]=r.build_name;
        // ── M2 仪表盘明细 ──
        rj["outcome"] = run_outcome_name(r.outcome);
        rj["cause"]   = r.death_cause;
        rj["death_floor"] = r.death_floor;
        rj["weapon"]  = r.weapon_id_final;
        rj["element"] = r.element_type;
        rj["level"]   = r.level_final;
        rj["combat_pct"] = r.turns > 0 ? (float)r.combat_frames / r.turns : 0;
        rj["rooms"]   = r.rooms_discovered;
        rj["picks"]   = r.items_picked;
        rj["gold"]    = r.gold_earned;
        rj["stuck_tp"]= r.stuck_teleports;
        rj["stuck_rot"] = r.stuck_rotations;
        rj["loot_wd"] = r.loot_watchdog_descends;
        runarr.push_back(rj);
    }
    j["runs"] = runarr;

    return j.dump(2);
}

bool BalanceReport::save_to_file(const char* path) const {
    MKDIR("reports");
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << to_json();
    f.close();
    return true;
}
