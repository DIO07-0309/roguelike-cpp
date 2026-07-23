// G7.3: Simulation framework tests
#include <gtest/gtest.h>
#include "core/sim/sim_runner.h"

TEST(SimConfig, Defaults) {
    SimulationConfig cfg;
    EXPECT_EQ(cfg.runs, 10000);
    EXPECT_EQ(cfg.seed_start, 0u);
    EXPECT_TRUE(cfg.random_build);
    EXPECT_FALSE(cfg.fixed_build);
}

TEST(SimConfig, FixedBuild) {
    SimulationConfig cfg;
    cfg.runs = 500;
    cfg.fixed_build = true;
    cfg.fixed_build_id = "blood";
    EXPECT_EQ(cfg.runs, 500);
    EXPECT_EQ(cfg.fixed_build_id, "blood");
}

TEST(SimRunner, BeginEndCycle) {
    SimulationConfig cfg;
    cfg.runs = 3;
    auto& sim = SimRunner::inst();
    sim.begin(cfg);
    EXPECT_TRUE(sim.is_active());
    EXPECT_EQ(sim.total_runs(), 3);
    EXPECT_EQ(sim.current_run(), 0);

    // Record 3 runs
    RunResult r;
    r.victory = true;
    r.floor_reached = 15;
    r.build_name = "test";
    for (int i = 0; i < 3; i++) sim.record_run(r);

    EXPECT_FALSE(sim.should_restart());
    // Don't call finalize() — it writes files
}

TEST(RunResult, DefaultConstructed) {
    RunResult r;
    EXPECT_EQ(r.seed, 0u);
    EXPECT_FALSE(r.victory);
    EXPECT_EQ(r.floor_reached, 0);
    EXPECT_EQ(r.build_type, 0);
}

TEST(BalanceReport, EmptyReport) {
    BalanceReport r;
    EXPECT_EQ(r.total_runs, 0);
    EXPECT_EQ(r.total_wins, 0);
    EXPECT_EQ(r.win_rate, 0);
}

TEST(BalanceReport, JSONExport) {
    BalanceReport r;
    r.total_runs = 100;
    r.total_wins = 42;
    r.win_rate = 0.42f;
    r.avg_floor = 11;
    std::string json = r.to_json();
    EXPECT_TRUE(json.find("\"total_runs\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"win_rate\"") != std::string::npos);
    EXPECT_TRUE(json.find("0.42") != std::string::npos);
}

TEST(BuildStats, WinRate) {
    BuildStats bs;
    bs.games = 10;
    bs.wins = 6;
    EXPECT_NEAR(bs.win_rate(), 0.6f, 0.01f);
    BuildStats empty;
    EXPECT_EQ(empty.win_rate(), 0);
}

TEST(RelicStats, WinRate) {
    RelicStats rs;
    rs.picked = 5;
    rs.wins = 3;
    EXPECT_NEAR(rs.win_rate(), 0.6f, 0.01f);
}

TEST(EnemyStats, ThreatCalc) {
    EnemyStats es;
    es.appearances = 20;
    es.kills = 6;
    float threat = es.appearances > 0 ? (float)es.kills / es.appearances : 0;
    EXPECT_NEAR(threat, 0.3f, 0.01f);
}
