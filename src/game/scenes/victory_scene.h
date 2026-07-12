#pragma once
#include "node.h"
#include "input_map.h"
#include <string>
#include <vector>

// ============================================================
// D6 Step2: VictoryScene — 通关画面 (点击后转Credits)
// ============================================================
class VictoryScene : public Node {
public:
    int final_level = 1;
    std::string ending_name;
    std::string final_line;
    std::string sky_color;
    int meta_soul = 0, meta_knowledge = 0;

    // D6 Step2: 传给CreditsScene的数据 (纯数据组合, 无GameScene依赖)
    std::string npc_names[8], npc_results[8], npc_details[8];
    int  npc_count = 0;
    float timeline_times[16];
    std::string timeline_labels[16];
    int  timeline_count = 0;
    std::string boss_rank;
    int  boss_dmg_done = 0, boss_dmg_taken = 0;
    float boss_time = 0.0f;
    int  boss_arena = 0;
    int  run_floor = 0, run_bosses = 0, run_kills = 0, run_elites = 0;
    int  run_relics = 0, run_quests = 0, run_combo = 0;
    float run_playtime = 0.0f;

    void _render() override;
    void _input(const InputMap& input) override;
};
