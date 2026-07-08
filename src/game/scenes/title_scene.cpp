#include "title_scene.h"
#include "floor_select_scene.h"
#include "tutorial_scene.h"
#include "game_scene.h"
#include "scene_tree.h"
#include "config.h"
#include "save/save_manager.h"
#include "core/logger.h"
#include <cmath>

extern Font g_font, g_font_small;
extern bool g_font_loaded;

void TitleScene::_enter_tree() {
    // 每次进入标题画面时重新检测存档状态
    has_save = SaveManager::save_exists();
    if (has_save) {
        auto* data = SaveManager::load_save();
        max_floor = data ? data->max_unlocked_floor : 1;
        delete data;
    }
    LOG_INFO("标题画面: has_save=%d max_floor=%d", has_save, max_floor);
}

void TitleScene::_ready() {
    name = "TitleScene";
    items = {
        {"N", "新游戏", "new", {255, 200, 50, 255}},
        {"C", "继续游戏", "continue", {100, 200, 100, 255}},
        {"F", "选关", "select", {100, 180, 255, 255}},
        {"T", "新手教程", "tutorial", {200, 180, 120, 255}},
        {"F11", "全屏切换", "fullscreen", {160, 160, 200, 255}},
        {"Esc", "退出", "quit", {200, 100, 100, 255}},
    };
}

void TitleScene::_process(double delta) {
    anim_time += (float)delta;
}

void TitleScene::_render() {
    ClearBackground(BLACK);
    auto* tree = get_tree();
    if (!tree) return;
    int sw = tree->get_width(), sh = tree->get_height();

    // 背景粒子
    for (int i = 0; i < 20; i++) {
        float x = fmodf((float)(i * 127 + 31) * 1.3f + anim_time * 20 * (i % 3 + 1), (float)sw);
        float y = fmodf((float)(i * 53 + 17), (float)sh);
        DrawCircle(x, y, 1.5f + (i % 3), {60, 60, 100, 100});
    }

    // 标题
    if (g_font_loaded) {
        float pulse = 1.0f + sinf(anim_time * 2) * 0.02f;
        float title_size = 52 * pulse;
        float w = MeasureTextEx(g_font, "Roguelike 肉鸽游戏", title_size, 1).x;
        DrawTextEx(g_font, "Roguelike 肉鸽游戏", {sw/2.0f - w/2, 80}, title_size, 1, {255, 255, 220, 255});
        w = MeasureTextEx(g_font_small, "- 地牢深处 -", 18, 1).x;
        DrawTextEx(g_font_small, "- 地牢深处 -", {sw/2.0f - w/2, 140}, 18, 1, {180, 180, 180, 255});
    } else {
        DrawRectangle(sw/2 - 180, 65, 360, 60, {40, 40, 60, 255});
        DrawRectangleLines(sw/2 - 180, 65, 360, 60, {100, 100, 180, 255});
        DrawText("Roguelike (No CJK Font)", sw/2 - 120, 85, 20, {200, 200, 200, 255});
    }

    // 面板
    float pw = 340, ph = 270;
    Rectangle pr = {sw/2.0f - pw/2, 170, pw, ph};
    DrawRectangleRounded(pr, 0.08f, 8, {20, 20, 40, 230});
    DrawRectangleRoundedLines(pr, 0.08f, 8, 2, {100, 100, 180, 255});

    if (g_font_loaded)
        DrawTextEx(g_font_small, "选 单", {pr.x + 12, pr.y + 8}, 20, 1, {200, 200, 255, 255});
    else
        DrawText("Menu", (int)pr.x + 12, (int)pr.y + 10, 20, {200, 200, 255, 255});

    // 存档状态
    float y = pr.y + 35;
    if (g_font_loaded) {
        char buf[64];
        snprintf(buf, sizeof(buf), has_save ? "存档已存在（已解锁第%d层）" : "暂无存档", max_floor);
        float w = MeasureTextEx(g_font_small, buf, 14, 1).x;
        DrawTextEx(g_font_small, buf, {(float)(pr.x + (pw - w)/2), y}, 14, 1, {160, 160, 160, 255});
    } else {
        DrawText(has_save ? "Save exists" : "No save", (int)pr.x + 60, (int)y, 14, {160, 160, 160, 255});
    }
    y += 25;

    // 菜单项
    for (auto& mi : items) {
        Color c = mi.color;
        if (mi.action == "continue" && !has_save) c = {80, 80, 80, 255};
        if (mi.action == "select" && !has_save) c = {80, 80, 80, 255};

        if (g_font_loaded) {
            std::string txt = "[" + mi.key + "] " + mi.label;
            DrawTextEx(g_font_small, txt.c_str(), {(float)(pr.x + 60), y}, 16, 1, c);
        } else {
            std::string txt = "[" + mi.key + "] " + mi.action;
            DrawText(txt.c_str(), (int)(pr.x + 60), (int)y, 16, c);
        }
        y += 36;
    }

    // 底部版权
    if (g_font_loaded) {
        float w = MeasureTextEx(g_font_small, "重庆大学大数据与软件学院 · 程序设计实训", 12, 1).x;
        DrawTextEx(g_font_small, "重庆大学大数据与软件学院 · 程序设计实训",
                   {sw/2.0f - w/2, (float)(sh - 50)}, 12, 1, {80, 80, 80, 255});
        w = MeasureTextEx(g_font_small, "开发者：ruozhiDIO  C++版", 14, 1).x;
        DrawTextEx(g_font_small, "开发者：ruozhiDIO  C++版",
                   {sw/2.0f - w/2, (float)(sh - 30)}, 14, 1, {140, 140, 160, 255});
    } else {
        DrawText("CQU PT | ruozhiDIO | C++ Edition", sw/2 - 100, sh - 30, 14, {140, 140, 160, 255});
    }
}

void TitleScene::_input(const InputMap& input) {
    auto* tree = get_tree();
    if (!tree) return;

    if (input.is_action_just_pressed("cancel")) {
        tree->quit();  // 标题画面Esc = 退出游戏
        return;
    }
    if (IsKeyPressed(KEY_N)) {
        auto gs = std::make_shared<GameScene>();
        gs->name = "GameScene";
        gs->new_game();
        tree->change_scene(gs);
        LOG_INFO("开始新游戏");
    } else if (IsKeyPressed(KEY_C) && has_save) {
        auto* data = SaveManager::load_save();
        if (data) {
            auto gs = std::make_shared<GameScene>();
            gs->name = "GameScene";
            int floor = data->current_floor;
            int maxf = data->max_unlocked_floor;
            // 传递存档中的 player (如果有的话)
            if (data->player) {
                gs->load_saved_game(floor, maxf, std::move(data->player),
                                    data->dungeon_seed, data->special_triggered,
                                    data->special_discovered);
            } else {
                auto p = std::make_unique<Player>(TILE_SIZE * 2, TILE_SIZE * 2,
                    PLAYER_SPEED, PLAYER_MAX_HP, PLAYER_ATTACK, PLAYER_PDEF, PLAYER_MDEF);
                gs->load_saved_game(floor, maxf, std::move(p),
                                    data->dungeon_seed, data->special_triggered,
                                    data->special_discovered);
            }
            delete data;
            tree->change_scene(gs);
            LOG_INFO("继续游戏: 第%d层", floor);
        }
    } else if (IsKeyPressed(KEY_F)) {
        auto fs = std::make_shared<FloorSelectScene>();
        fs->name = "FloorSelectScene";
        fs->max_unlocked = max_floor;
        tree->change_scene(fs);
    } else if (IsKeyPressed(KEY_T)) {
        auto ts = std::make_shared<TutorialScene>();
        ts->name = "TutorialScene";
        tree->change_scene(ts);
        LOG_INFO("进入教程");
    }
}
