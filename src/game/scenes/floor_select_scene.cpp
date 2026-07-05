#include "floor_select_scene.h"
#include "title_scene.h"
#include "game_scene.h"
#include "save/save_manager.h"
#include "scene_tree.h"
#include "config.h"
#include "boss.h"
#include "core/logger.h"

extern Font g_font, g_font_small;
extern bool g_font_loaded;

void FloorSelectScene::_render() {
    ClearBackground(BLACK);
    int sw = get_tree()->get_width();
    int cols = 5, cell_w = 100, cell_h = 70, gap = 10;
    int sx = sw / 2 - (cols * (cell_w + gap)) / 2, sy = 100;

    if (g_font_loaded) {
        float w = MeasureTextEx(g_font, "选择关卡", 32, 1).x;
        DrawTextEx(g_font, "选择关卡", {sw/2.0f - w/2, 30}, 32, 1, WHITE);
        w = MeasureTextEx(g_font_small, "方向键选择 Enter进入 Esc返回", 16, 1).x;
        DrawTextEx(g_font_small, "方向键选择 Enter进入 Esc返回",
                   {sw/2.0f - w/2, 70}, 16, 1, {128, 128, 128, 255});
    } else {
        DrawText("Select Floor", sw/2 - 50, 30, 24, WHITE);
        DrawText("Arrows Enter Esc", sw/2 - 70, 70, 16, {128, 128, 128, 255});
    }

    for (int i = 0; i < MAX_FLOORS; i++) {
        int cx = sx + (i % cols) * (cell_w + gap);
        int cy = sy + (i / cols) * (cell_h + gap);
        bool unlocked = (i + 1) <= max_unlocked;
        bool selected = i == cursor;

        Color bg = selected ? Color{60, 60, 160, 255}
                 : unlocked ? Color{40, 40, 80, 255} : Color{30, 30, 30, 255};
        Color border = selected ? Color{200, 200, 50, 255}
                     : unlocked ? Color{100, 180, 100, 255} : Color{80, 80, 80, 255};

        DrawRectangle(cx, cy, cell_w, cell_h, bg);
        DrawRectangleLines(cx, cy, cell_w, cell_h, border);

        bool is_boss = (i + 1 == 5 || i + 1 == 10 || i + 1 == 15);
        char num[4]; snprintf(num, sizeof(num), "%d", i + 1);
        DrawText(num, cx + cell_w/2 - 10, cy + 8, 28, is_boss ? RED : WHITE);

        if (is_boss && unlocked) {
            auto info = get_boss_info(i + 1);
            if (g_font_loaded) {
                float w = MeasureTextEx(g_font_small, info.name.c_str(), 12, 1).x;
                DrawTextEx(g_font_small, info.name.c_str(),
                           {(float)(cx + cell_w/2) - w/2, (float)(cy + cell_h - 20)}, 12, 1, RED);
            }
        }
    }
}

void FloorSelectScene::_input(const InputMap& input) {
    if (input.is_action_just_pressed("cancel")) {
        get_tree()->change_scene(std::make_shared<TitleScene>());
        return;
    }
    if (input.is_action_just_pressed("move_left"))  cursor = std::max(0, cursor - 1);
    if (input.is_action_just_pressed("move_right")) cursor = std::min(MAX_FLOORS - 1, cursor + 1);
    if (input.is_action_just_pressed("move_up"))    cursor = std::max(0, cursor - 5);
    if (input.is_action_just_pressed("move_down"))  cursor = std::min(MAX_FLOORS - 1, cursor + 5);
    if (input.is_action_just_pressed("confirm")) {
        int floor = cursor + 1;
        if (floor <= max_unlocked) {
            auto gs = std::make_shared<GameScene>();
            gs->name = "GameScene";
            // 尝试加载存档中的玩家数据
            auto* data = SaveManager::load_save();
            if (data && data->player) {
                gs->load_saved_game(floor, max_unlocked, std::move(data->player));
                delete data;
            } else {
                // 无存档: 创建新玩家
                delete data;
                gs->new_game();
                gs->enter_floor(floor);
            }
            get_tree()->change_scene(gs);
            LOG_INFO("选关进入第%d层", floor);
        }
    }
}
