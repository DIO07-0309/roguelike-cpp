#include "victory_scene.h"
#include "credits_scene.h"
#include "title_scene.h"
#include "scene_tree.h"
#include "core/logger.h"
#include "meta_progression.h"

extern Font g_font, g_font_small;
extern bool g_font_loaded;

void VictoryScene::_render() {
    ClearBackground(BLACK);
    int sw = get_tree()->get_width(), sh = get_tree()->get_height();

    if (g_font_loaded) {
        Color ec;
        if (ending_name == "TRUE END" || ending_name == "ABSOLUTE END")
            ec = {255, 220, 80, 255};
        else if (ending_name == "GOOD END")
            ec = {100, 255, 120, 255};
        else if (ending_name == "BAD END")
            ec = {220, 80, 60, 255};
        else
            ec = {200, 200, 200, 255};

        float w = MeasureTextEx(g_font, ending_name.c_str(), 48, 1).x;
        DrawTextEx(g_font, ending_name.c_str(), {sw/2.0f - w/2, 60}, 48, 1, ec);

        // 最终台词 (多行)
        auto draw_lines = [&](const char* text, float x, float y, int sz, Color c) {
            std::string s(text);
            size_t pos = 0; float ly = y;
            while (pos < s.size()) {
                size_t nl = s.find('\n', pos);
                std::string line = (nl == std::string::npos) ? s.substr(pos) : s.substr(pos, nl-pos);
                float lw = MeasureTextEx(g_font_small, line.c_str(), sz, 1).x;
                DrawTextEx(g_font_small, line.c_str(), {x - lw/2, ly}, sz, 1, c);
                ly += sz * 1.4f;
                if (nl == std::string::npos) break;
                pos = nl + 1;
            }
        };
        if (!final_line.empty())
            draw_lines(final_line.c_str(), sw/2.0f, 130, 18, {230, 230, 240, 255});

        char buf[128];
        snprintf(buf, sizeof(buf), "天空颜色: %s  |  Lv%d", sky_color.c_str(), final_level);
        w = MeasureTextEx(g_font_small, buf, 14, 1).x;
        DrawTextEx(g_font_small, buf, {sw/2.0f - w/2, 260}, 14, 1, {180, 200, 180, 255});

        snprintf(buf, sizeof(buf), "Meta奖励: Soul +%d  Knowledge +%d", meta_soul, meta_knowledge);
        w = MeasureTextEx(g_font_small, buf, 15, 1).x;
        DrawTextEx(g_font_small, buf, {sw/2.0f - w/2, 285}, 15, 1, {200, 220, 255, 255});

        int total_runs = g_meta.total_runs();
        snprintf(buf, sizeof(buf), "累计 %d 局  |  Soul: %d  Knowledge: %d",
                 total_runs, g_meta.currency().soul_fragments, g_meta.currency().knowledge);
        w = MeasureTextEx(g_font_small, buf, 13, 1).x;
        DrawTextEx(g_font_small, buf, {sw/2.0f - w/2, 315}, 13, 1, {160, 170, 200, 220});

        w = MeasureTextEx(g_font, "按 Enter 观看片尾 →", 22, 1).x;
        DrawTextEx(g_font, "按 Enter 观看片尾 →",
                   {sw/2.0f - w/2, (float)(sh - 60)}, 22, 1, {255, 200, 50, 255});
    } else {
        DrawText("VICTORY!", sw/2 - 60, 110, 48, {50, 255, 100, 255});
        DrawText("Press Enter", sw/2 - 50, 180, 18, {200, 240, 200, 255});
    }
}

void VictoryScene::_input(const InputMap& input) {
    if (input.is_action_just_pressed("confirm")) {
        // D6 Step2: 转入CreditsScene (片尾演出)
        auto cs = std::make_shared<CreditsScene>();
        cs->name = "CreditsScene";
        cs->ending_name   = ending_name;
        cs->ending_title  = ending_name;
        cs->sky_color     = sky_color;
        cs->final_line    = final_line;
        cs->meta_soul     = meta_soul;
        cs->meta_knowledge = meta_knowledge;

        // NPC epilogues
        cs->npc_count = npc_count;
        for (int i = 0; i < npc_count; i++) {
            cs->npc_names[i]   = npc_names[i];
            cs->npc_results[i] = npc_results[i];
            cs->npc_details[i] = npc_details[i];
        }
        // Timeline
        cs->timeline_count = timeline_count;
        for (int i = 0; i < timeline_count; i++) {
            cs->timeline_times[i]  = timeline_times[i];
            cs->timeline_labels[i] = timeline_labels[i];
        }
        // Battle report
        cs->boss_rank    = boss_rank;
        cs->boss_dmg_done = boss_dmg_done;
        cs->boss_dmg_taken = boss_dmg_taken;
        cs->boss_time    = boss_time;
        cs->boss_arena_zones = boss_arena;
        // Run summary
        cs->floor_reached   = run_floor;
        cs->bosses_killed   = run_bosses;
        cs->total_kills     = run_kills;
        cs->elite_kills     = run_elites;
        cs->relics_collected = run_relics;
        cs->quests_done     = run_quests;
        cs->combo_max       = run_combo;
        cs->play_time       = run_playtime;
        // Meta
        cs->total_runs = g_meta.total_runs();
        cs->curr_soul  = g_meta.currency().soul_fragments;
        cs->curr_know  = g_meta.currency().knowledge;

        get_tree()->change_scene(cs);
        LOG_INFO("通关→Credits");
    }
}
