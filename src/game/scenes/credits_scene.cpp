#include "credits_scene.h"
#include "title_scene.h"
#include "scene_tree.h"
#include "core/logger.h"
#include "meta_progression.h"
#include <cstdio>
#include <cmath>

extern Font g_font, g_font_small;
extern bool g_font_loaded;

void CreditsScene::_ready() {
    _page = 0; _npc_page = 0; _timeline_page = 0; _scroll_y = (float)get_tree()->get_height();
}

CreditsScene::CreditsPhase CreditsScene::_phase() const {
    // 阶段判定 (纯数据, 无外部依赖)
    int npc_total = npc_count * 2; // each NPC = name+detail 2 screens
    if (_page < npc_total) return CreditsPhase::NPC_EPILOGUE;
    if (_page == npc_total) return CreditsPhase::TIMELINE;
    if (_page == npc_total + 1) return CreditsPhase::REPORT;
    if (_page == npc_total + 2) return CreditsPhase::SUMMARY;
    return CreditsPhase::CREDITS;
}

void CreditsScene::_render() {
    ClearBackground(BLACK);
    int sw = get_tree()->get_width(), sh = get_tree()->get_height();
    if (!g_font_loaded) return;

    CreditsPhase ph = _phase();
    int npc_total = npc_count * 2;

    switch (ph) {
    case CreditsPhase::NPC_EPILOGUE: {
        int npc_idx = _page / 2; // 0=name, 1=detail
        bool is_name = (_page % 2 == 0);
        if (npc_idx < npc_count) {
            if (is_name) {
                float w = MeasureTextEx(g_font, npc_names[npc_idx].c_str(), 32, 1).x;
                DrawTextEx(g_font, npc_names[npc_idx].c_str(), {sw/2.0f-w/2, sh/2.0f-40}, 32, 1,
                           {255,220,100,255});
                w = MeasureTextEx(g_font_small, npc_results[npc_idx].c_str(), 22, 1).x;
                DrawTextEx(g_font_small, npc_results[npc_idx].c_str(), {sw/2.0f-w/2, sh/2.0f+10}, 22, 1,
                           {180,220,255,255});
            } else {
                // detail: 最多3行
                std::string s(npc_details[npc_idx]);
                float y = sh/2.0f - 30;
                size_t pos = 0;
                while (pos < s.size()) {
                    size_t nl = s.find('\n', pos);
                    std::string line = (nl == std::string::npos) ? s.substr(pos) : s.substr(pos, nl-pos);
                    float lw = MeasureTextEx(g_font_small, line.c_str(), 16, 1).x;
                    DrawTextEx(g_font_small, line.c_str(), {sw/2.0f - lw/2, y}, 16, 1, {230,230,240,240});
                    y += 24;
                    if (nl == std::string::npos) break;
                    pos = nl + 1;
                }
            }
        }
        DrawTextEx(g_font_small, "[ENTER]继续", {sw/2.0f-40, (float)(sh-50)}, 14, 1, {140,140,160,200});
        break;
    }
    case CreditsPhase::TIMELINE: {
        DrawTextEx(g_font, "战斗时间线", {sw/2.0f-65, 60}, 26, 1, {255,200,100,255});
        float y = 110;
        int show = 0;
        for (int i = 0; i < timeline_count && show < 10; i++) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%5.1fs  %s", timeline_times[i], timeline_labels[i].c_str());
            DrawTextEx(g_font_small, buf, {sw/2.0f - 100, y}, 16, 1, {200,220,255,255});
            y += 24; show++;
        }
        if (timeline_count > 10) {
            DrawTextEx(g_font_small, "...", {sw/2.0f-10, y}, 16, 1, {120,120,150,200});
        }
        DrawTextEx(g_font_small, "[ENTER]继续", {sw/2.0f-40, (float)(sh-50)}, 14, 1, {140,140,160,200});
        break;
    }
    case CreditsPhase::REPORT: {
        DrawTextEx(g_font, "战斗报告", {sw/2.0f-52, 60}, 26, 1, {255,200,100,255});

        float y = 110;
        auto line = [&](const char* fmt, float val) {
            char buf[32]; snprintf(buf, sizeof(buf), fmt, val);
            float w = MeasureTextEx(g_font_small, buf, 15, 1).x;
            DrawTextEx(g_font_small, buf, {sw/2.0f - w/2, y}, 15, 1, {220,240,220,255});
            y += 26;
        };
        line("BossRank  %s", 0);
        float lw = MeasureTextEx(g_font_small, boss_rank.c_str(), 22, 1).x;
        DrawTextEx(g_font_small, boss_rank.c_str(), {sw/2.0f - lw/2, y-26}, 22, 1, {255,220,60,255});
        line("Damage     %d", (float)boss_dmg_done);
        line("Damage再  %d", (float)boss_dmg_taken);
        line("战时长    %.1fs", boss_time);
        line("Arena区域  %d", (float)boss_arena_zones);

        DrawTextEx(g_font_small, "[ENTER]继续", {sw/2.0f-40, (float)(sh-50)}, 14, 1, {140,140,160,200});
        break;
    }
    case CreditsPhase::SUMMARY: {
        DrawTextEx(g_font, "旅程总结", {sw/2.0f-52, 60}, 26, 1, {255,200,100,255});

        float y = 105;
        auto sline = [&](const char* label, int val) {
            char buf[64]; snprintf(buf, sizeof(buf), "%s  %d", label, val);
            float w = MeasureTextEx(g_font_small, buf, 14, 1).x;
            DrawTextEx(g_font_small, buf, {sw/2.0f - w/2, y}, 14, 1, {200,220,255,255});
            y += 24;
        };
        sline("到达楼层", floor_reached);
        sline("击杀Boss", bosses_killed);
        sline("击杀总数", total_kills);
        sline("击杀精英", elite_kills);
        sline("收集圣物", relics_collected);
        sline("完成任务", quests_done);
        sline("最高连击", combo_max);
        {
            char buf[48];
            int m = (int)play_time / 60, s = (int)play_time % 60;
            snprintf(buf, sizeof(buf), "游戏时间  %d:%02d", m, s);
            float w = MeasureTextEx(g_font_small, buf, 14, 1).x;
            DrawTextEx(g_font_small, buf, {sw/2.0f - w/2, y}, 14, 1, {200,220,255,255});
        }

        DrawTextEx(g_font_small, "[ENTER]进入片尾", {sw/2.0f-55, (float)(sh-50)}, 14, 1, {140,140,160,200});
        break;
    }
    case CreditsPhase::CREDITS: {
        // 滚动字幕
        float cx = sw/2.0f;
        _scroll_y -= 30.0f * GetFrameTime(); // 30px/s
        float sy = _scroll_y;

        Color gold = {255, 220, 80, 255};
        Color white = {220, 220, 240, 255};
        Color dim = {160, 160, 190, 200};

        auto draw_c = [&](const char* text, int sz, Color c) {
            float w = MeasureTextEx(g_font_small, text, (float)sz, 1).x;
            DrawTextEx(g_font_small, text, {cx - w/2, sy}, (float)sz, 1, c);
            sy += sz * 1.6f;
        };

        draw_c(ending_title.c_str(), 28, gold);
        sy += 20;
        draw_c("─ ─ ─ ─ ─ ─ ─ ─ ─ ─", 16, dim);
        sy += 30;

        // NPC epilogue lines
        for (int i = 0; i < npc_count; i++) {
            char buf[128];
            snprintf(buf, sizeof(buf), "%s — %s", npc_names[i].c_str(), npc_results[i].c_str());
            draw_c(buf, 15, white);
        }
        sy += 20;

        draw_c("─ ─ ─ ─ ─ ─ ─ ─ ─ ─", 16, dim);
        sy += 20;

        draw_c(sky_color.c_str(), 20, gold);
        draw_c(final_line.c_str(), 14, white);
        sy += 20;
        draw_c("─ ─ ─ ─ ─ ─ ─ ─ ─ ─", 16, dim);
        sy += 30;

        draw_c("Developed by Zhou Yutong", 17, gold);
        draw_c("C++ Raylib Roguelike", 14, dim);
        sy += 15;
        draw_c("2026", 14, gold);
        sy += 30;
        draw_c("Special thanks to", 13, dim);
        draw_c("all who played and tested.", 13, dim);
        sy += 40;
        draw_c("The End.", 20, gold);

        // 字幕结束后提示
        if (sy < 60) {
            DrawTextEx(g_font_small, "[ENTER] 返回标题", {cx-60, (float)(sh-50)}, 16, 1, {255,200,50,255});
        }
        break;
    }
    }
}

void CreditsScene::_input(const InputMap& input) {
    if (!input.is_action_just_pressed("confirm")) return;

    CreditsPhase ph = _phase();
    if (ph == CreditsPhase::CREDITS) {
        if (_scroll_y > (float)(get_tree()->get_height())) return; // 还在滚动
        auto ts = std::make_shared<TitleScene>();
        ts->name = "TitleScene";
        get_tree()->change_scene(ts);
        LOG_INFO("Credits→返回标题");
        return;
    }

    // 推进页面
    _page++;
}