#include "death_scene.h"
#include "title_scene.h"
#include "scene_tree.h"
#include "core/logger.h"
#include "meta_progression.h"     // v1.6-B2: 死因谱 (g_meta.top_death_causes)

extern Font g_font, g_font_small;
extern bool g_font_loaded;

void DeathScene::_render() {
    // M3: 血色渐晕底 (脱离纯黑) — 上深下更暗, 中央微红
    ClearBackground({12, 6, 8, 255});
    int sw = get_tree()->get_width(), sh = get_tree()->get_height();
    for (int i = 0; i < 80; i++) {
        float t = i / 80.0f;
        DrawRectangle(0, i, sw, 1,
            {(unsigned char)(12 + 26 * t), (unsigned char)(6 + 8 * t),
             (unsigned char)(8 + 10 * t), 255});
    }
    if (g_font_loaded) {
        // M3: 大字投影 + 呼吸明度 (静字变尸碑)
        float w = MeasureTextEx(g_font, "你 死 了", 64, 1).x;
        DrawTextEx(g_font, "你 死 了", {sw/2.0f - w/2 + 3, 63}, 64, 1, {0, 0, 0, 160});
        DrawTextEx(g_font, "你 死 了", {sw/2.0f - w/2, 60}, 64, 1, {220, 40, 40, 255});

        // D6: 结局信息
        if (!ending_name.empty()) {
            Color ec = (ending_name == "BAD END") ? Color{200,80,60,255}
                     : (ending_name == "TRUE END") ? Color{255,220,80,255}
                     : Color{200,200,200,255};
            w = MeasureTextEx(g_font, ending_name.c_str(), 36, 1).x;
            DrawTextEx(g_font, ending_name.c_str(), {sw/2.0f - w/2, 130}, 36, 1, ec);
        }
        if (!final_line.empty()) {
            // 取第一行 (避免多行长文本)
            std::string firstline = final_line.substr(0, final_line.find('\n'));
            w = MeasureTextEx(g_font_small, firstline.c_str(), 16, 1).x;
            DrawTextEx(g_font_small, firstline.c_str(), {sw/2.0f - w/2, 175},
                       16, 1, {220, 200, 180, 240});
        }
        char buf[128];
        // v1.6-B2: 本局死因 — 死亡界面第一要务 (红字醒目)
        if (!death_cause.empty()) {
            std::string cause_line = "死于: " + death_cause;
            w = MeasureTextEx(g_font_small, cause_line.c_str(), 17, 1).x;
            DrawTextEx(g_font_small, cause_line.c_str(), {sw/2.0f - w/2, 196}, 17, 1,
                       {255, 90, 70, 245});
        }
        snprintf(buf, sizeof(buf), "第%d层  Lv%d", final_floor, final_level);
        w = MeasureTextEx(g_font_small, buf, 16, 1).x;
        DrawTextEx(g_font_small, buf, {sw/2.0f - w/2, 222}, 16, 1, {180, 180, 180, 255});

        if (meta_soul > 0) {
            snprintf(buf, sizeof(buf), "Meta奖励: Soul +%d  Knowledge +%d",
                     meta_soul, meta_knowledge);
            w = MeasureTextEx(g_font_small, buf, 14, 1).x;
            DrawTextEx(g_font_small, buf, {sw/2.0f - w/2, 240}, 14, 1, {200,220,255,200});
        }

        // v1.6-B1: 镜像复盘 (仅 F15 死亡有内容 — 死在"自己"手里的特别演出)
        if (!mirror_verdict.empty()) {
            float vy = 300.0f;
            w = MeasureTextEx(g_font_small, "— 镜像复盘 —", 16, 1).x;
            DrawTextEx(g_font_small, "— 镜像复盘 —", {sw/2.0f - w/2, vy}, 16, 1,
                       {255, 120, 100, 240});
            vy += 24;
            w = MeasureTextEx(g_font_small, mirror_verdict.c_str(), 15, 1).x;
            DrawTextEx(g_font_small, mirror_verdict.c_str(), {sw/2.0f - w/2, vy},
                       15, 1, {220, 160, 150, 235});
            vy += 26;
            // 逐行画习惯 (手动折行: 每行一个 '\n'; 上限避开死因谱区)
            size_t pos = 0;
            while (pos < mirror_habits.size() && vy < sh - 155) {
                size_t nl = mirror_habits.find('\n', pos);
                if (nl == std::string::npos) nl = mirror_habits.size();
                std::string line = mirror_habits.substr(pos, nl - pos);
                if (!line.empty()) {
                    w = MeasureTextEx(g_font_small, line.c_str(), 13, 1).x;
                    DrawTextEx(g_font_small, line.c_str(), {sw/2.0f - w/2, vy},
                               13, 1, {170, 140, 135, 225});
                    vy += 19;
                }
                pos = nl + 1;
            }
        }

        w = MeasureTextEx(g_font_small, "存档已保留，可从选关界面继续挑战", 16, 1).x;
        DrawTextEx(g_font_small, "存档已保留，可从选关界面继续挑战",
                   {sw/2.0f - w/2, 275}, 16, 1, {220, 180, 100, 255});

        // v1.6-B2: 死因谱 — 跨局 Top3 (死得多了才显示, 首死只看本局)
        auto top_causes = g_meta.top_death_causes(3);
        if (top_causes.size() >= 2) {
            float py = (float)(sh - 160);
            w = MeasureTextEx(g_font_small, "— 死因谱 —", 14, 1).x;
            DrawTextEx(g_font_small, "— 死因谱 —", {sw/2.0f - w/2, py}, 14, 1,
                       {200, 140, 120, 220});
            py += 18;
            for (auto& [cause, cnt] : top_causes) {
                char tb[96];
                snprintf(tb, sizeof(tb), "%s ×%d", cause.c_str(), cnt);
                w = MeasureTextEx(g_font_small, tb, 13, 1).x;
                DrawTextEx(g_font_small, tb, {sw/2.0f - w/2, py}, 13, 1,
                           {170, 130, 120, 215});
                py += 17;
            }
        }

        w = MeasureTextEx(g_font, "按 Enter 返回标题", 22, 1).x;
        DrawTextEx(g_font, "按 Enter 返回标题",
                   {sw/2.0f - w/2, (float)(sh - 70)}, 22, 1, {200, 200, 200, 255});
    } else {
        DrawText("YOU DIED", sw/2 - 60, 130, 48, {220, 40, 40, 255});
        DrawText("Press Enter to return", sw/2 - 80, 200, 18, {200, 200, 200, 255});
    }
}

void DeathScene::_input(const InputMap& input) {
    if (input.is_action_just_pressed("confirm")) {
        auto ts = std::make_shared<TitleScene>();
        ts->name = "TitleScene";
        get_tree()->change_scene(ts);
        LOG_INFO("死亡→返回标题");
    }
}
