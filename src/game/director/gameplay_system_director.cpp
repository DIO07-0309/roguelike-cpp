#include "gameplay_system_director.h"
#include "event_bus.h"
#include "player.h"
#include "core/logger.h"
#include "item.h"  // rng
#include "boss_replay.h"

// ============================================================
// D6 Step4: GameplaySystemDirector — 综合tick + 生命周期
// ============================================================

void GameplaySystemDirector::tick(float dt, bool is_playing, bool is_boss_floor) {
    flow.tick(dt);
    quest_mgr.set_relationship_system(&rels);
    quest_mgr.update(world_state, story);
    story.update(dt);

    // Build Fusion 检测
    (void)is_playing; (void)is_boss_floor;
}

void GameplaySystemDirector::on_enter_floor(int floor, bool is_boss, bool is_rest) {
    story.enter_floor(floor);
    quest_mgr.set_relationship_system(&rels);
    quest_mgr.update(world_state, story);
    (void)is_boss; (void)is_rest;
}

void GameplaySystemDirector::on_new_game() {
    g_meta.load();
    run_stats = RunSummary{};
    quest_mgr.set_relationship_system(&rels);
}

void GameplaySystemDirector::on_player_dead(int floor, int level, const Player* player) {
    run_stats.floor_reached = floor;
    if (player) {
        run_stats.relics_collected = (int)player->relics.size();
        auto qs = quest_mgr.all_quests();
        for (auto& q : qs) if (q.state == QuestState::COMPLETED) run_stats.quests_done++;
        run_stats.build_type = (int)calculate_build(player).identify();
        if (player->combo.count > run_stats.combo_max)
            run_stats.combo_max = player->combo.count;
    }

    // 简易结局判定 (BAD END if didn't kill Boss3)
    if (!world_state.has(WorldFlag::Boss3_Defeated)) {
        ending_dir.begin(world_state, BossRank::C,
                         g_relic_archive.collection_pct(), rels, quest_mgr);
    }

    MetaCurrency earned = g_meta.end_run(run_stats);
    LOG_INFO("[META] 奖励: 魂片+%d 知识+%d 远古记忆+%d",
        earned.soul_fragments, earned.knowledge, earned.ancient_memory);
}

void GameplaySystemDirector::on_game_clear(int floor, int level, const Player* player,
    const BossBattleReport& boss_report, float collection_pct) {
    run_stats.floor_reached = floor;
    if (player) {
        run_stats.relics_collected = (int)player->relics.size();
        auto qs = quest_mgr.all_quests();
        for (auto& q : qs) if (q.state == QuestState::COMPLETED) run_stats.quests_done++;
        run_stats.build_type = (int)calculate_build(player).identify();
        if (player->combo.count > run_stats.combo_max)
            run_stats.combo_max = player->combo.count;
    }

    ending_dir.begin(world_state, boss_report.rank, collection_pct, rels, quest_mgr);

    MetaCurrency mc;
    mc.soul_fragments = ending_dir.meta_reward_soul();
    mc.knowledge = ending_dir.meta_reward_knowledge();
    mc.ancient_memory = 2;
    g_meta.add_currency(mc);
    g_meta.save();
}

void GameplaySystemDirector::init_events() {
    EventBus::inst().subscribe(GameEventType::RELIC_GAIN,
        [this](const GameEvent& ev) {
            if (ev.str_val) run_stats.relics_collected++;
            (void)ev.int_val;
        }, "Gameplay");
    EventBus::inst().subscribe(GameEventType::FLOOR_ENTER,
        [this](const GameEvent& ev) {
            (void)ev;  // reserved: world state updates
        }, "Gameplay");
}
