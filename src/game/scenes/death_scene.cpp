#include "death_scene.h"
#include "title_scene.h"
#include "scene_tree.h"
#include "core/logger.h"

extern Font g_font, g_font_small;
extern bool g_font_loaded;

void DeathScene::_render() {
    ClearBackground(BLACK);
    int sw = get_tree()->get_width(), sh = get_tree()->get_height();
    if (g_font_loaded) {
        float w = MeasureTextEx(g_font, "你 死 了", 64, 1).x;
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
        snprintf(buf, sizeof(buf), "第%d层  Lv%d", final_floor, final_level);
        w = MeasureTextEx(g_font_small, buf, 16, 1).x;
        DrawTextEx(g_font_small, buf, {sw/2.0f - w/2, 210}, 16, 1, {180, 180, 180, 255});

        if (meta_soul > 0) {
            snprintf(buf, sizeof(buf), "Meta奖励: Soul +%d  Knowledge +%d",
                     meta_soul, meta_knowledge);
            w = MeasureTextEx(g_font_small, buf, 14, 1).x;
            DrawTextEx(g_font_small, buf, {sw/2.0f - w/2, 240}, 14, 1, {200,220,255,200});
        }

        w = MeasureTextEx(g_font_small, "存档已保留，可从选关界面继续挑战", 16, 1).x;
        DrawTextEx(g_font_small, "存档已保留，可从选关界面继续挑战",
                   {sw/2.0f - w/2, 275}, 16, 1, {220, 180, 100, 255});

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
