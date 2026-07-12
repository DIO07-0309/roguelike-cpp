#include "game_flow_director.h"
#include "scenes/game_scene.h"
#include "scenes/death_scene.h"
#include "scenes/victory_scene.h"
#include "scenes/credits_scene.h"
#include "scene_tree.h"
#include "core/logger.h"

const char* GameFlowDirector::state_name(GameFlowState s) {
    switch (s) {
        case GameFlowState::TITLE:        return "TITLE";
        case GameFlowState::NEW_GAME:     return "NEW_GAME";
        case GameFlowState::ENTER_FLOOR:  return "ENTER_FLOOR";
        case GameFlowState::PLAYING:      return "PLAYING";
        case GameFlowState::BOSS_INTRO:   return "BOSS_INTRO";
        case GameFlowState::BOSS_FIGHT:   return "BOSS_FIGHT";
        case GameFlowState::FLOOR_CLEAR:  return "FLOOR_CLEAR";
        case GameFlowState::BOSS_DEAD:    return "BOSS_DEAD";
        case GameFlowState::PLAYER_DEAD:  return "PLAYER_DEAD";
        case GameFlowState::GAME_CLEAR:   return "GAME_CLEAR";
        case GameFlowState::ENDING:       return "ENDING";
        case GameFlowState::CREDITS:      return "CREDITS";
        case GameFlowState::RETURN_TITLE: return "RETURN_TITLE";
    }
    return "?";
}

void GameFlowDirector::new_game() {
    if (!_scene) return;
    current_state = GameFlowState::NEW_GAME;

    // 委托给 GameScene 已有的 new_game() 实现
    _scene->new_game();
    current_state = GameFlowState::PLAYING;
}

void GameFlowDirector::load_saved_game(int floor, int max_f, std::unique_ptr<Player> p,
                                        uint32_t seed,
                                        const std::vector<bool>& special_triggered,
                                        const std::vector<bool>& special_discovered) {
    if (!_scene) return;
    current_state = GameFlowState::ENTER_FLOOR;
    _scene->load_saved_game(floor, max_f, std::move(p), seed, special_triggered, special_discovered);
    current_state = GameFlowState::PLAYING;
}

void GameFlowDirector::on_boss_intro_confirm() {
    if (!_scene) return;
    current_state = GameFlowState::BOSS_FIGHT;
}

void GameFlowDirector::on_player_dead() {
    if (!_scene) return;
    current_state = GameFlowState::PLAYER_DEAD;

    // 组装 DeathScene
    auto ds = std::make_shared<DeathScene>();
    ds->name = "DeathScene";
    ds->final_floor = _scene->current_floor;
    ds->final_level = _scene->player->level;
    ds->ending_name  = _scene->_gameplay.ending_dir.ending_name();
    ds->final_line   = _scene->_gameplay.ending_dir.final_line();
    ds->meta_soul    = _scene->_gameplay.ending_dir.meta_reward_soul() / 2;
    ds->meta_knowledge = _scene->_gameplay.ending_dir.meta_reward_knowledge() / 2;

    _scene->get_tree()->change_scene(ds);
    current_state = GameFlowState::ENDING;
    LOG_INFO("死亡→DeathScene (第%d层 Lv%d)", ds->final_floor, ds->final_level);
}

void GameFlowDirector::on_game_clear() {
    if (!_scene) return;
    current_state = GameFlowState::GAME_CLEAR;

    // ── 计算结局 ──
    auto* gs = _scene;
    auto& ending = gs->_gameplay.ending_dir;
    ending.begin(gs->_gameplay.world_state, gs->_boss.battle_report.rank,
                 g_relic_archive.collection_pct(), gs->_gameplay.rels, gs->_gameplay.quest_mgr);

    // ── VictoryScene ──
    auto vs = std::make_shared<VictoryScene>();
    vs->name = "VictoryScene";
    vs->final_level = gs->player->level;
    vs->ending_name   = ending.ending_name();
    vs->final_line    = ending.final_line();
    vs->sky_color     = ending.sky_color();
    vs->meta_soul     = ending.meta_reward_soul();
    vs->meta_knowledge = ending.meta_reward_knowledge();

    // 填充 Credits 数据 (NPC / Timeline / Report / Summary)
    auto& npc_ends = ending.npc_endings();
    vs->npc_count = (int)npc_ends.size();
    for (size_t i = 0; i < npc_ends.size() && i < 8; i++) {
        vs->npc_names[i]   = npc_ends[i].name;
        vs->npc_results[i] = npc_ends[i].result;
        vs->npc_details[i] = npc_ends[i].detail;
    }
    vs->timeline_count = gs->_boss.timeline.count();
    auto& tl = gs->_boss.timeline.events();
    for (int i = 0; i < gs->_boss.timeline.count() && i < 16; i++) {
        vs->timeline_times[i]  = tl[i].time;
        vs->timeline_labels[i] = tl[i].label ? tl[i].label : "";
    }
    vs->boss_rank     = gs->_boss.battle_report.rank_name();
    vs->boss_dmg_done = gs->_boss.battle_report.total_damage;
    vs->boss_dmg_taken= gs->_boss.battle_report.damage_taken;
    vs->boss_time     = gs->_boss.battle_report.battle_time;
    vs->boss_arena    = gs->_boss.battle_report.arena_zones_spawned;
    vs->run_floor     = gs->_gameplay.run_stats.floor_reached;
    vs->run_bosses    = gs->_gameplay.run_stats.bosses_killed;
    vs->run_kills     = gs->_gameplay.run_stats.total_kills;
    vs->run_elites    = gs->_gameplay.run_stats.elite_kills;
    vs->run_relics    = gs->_gameplay.run_stats.relics_collected;
    vs->run_quests    = gs->_gameplay.run_stats.quests_done;
    vs->run_combo     = gs->_gameplay.run_stats.combo_max;
    vs->run_playtime  = gs->_gameplay.run_stats.play_time;

    // 发放 Meta 奖励
    MetaCurrency mc;
    mc.soul_fragments = vs->meta_soul;
    mc.knowledge = vs->meta_knowledge;
    mc.ancient_memory = 2;
    g_meta.add_currency(mc);
    g_meta.save();

    gs->get_tree()->change_scene(vs);
    current_state = GameFlowState::ENDING;
    LOG_INFO("通关→VictoryScene (%s)", ending.ending_name());
}
