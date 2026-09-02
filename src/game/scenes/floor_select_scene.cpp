#include "floor_select_scene.h"
#include "title_scene.h"
#include "game_scene.h"
#include "save/save_manager.h"
#include "scene_tree.h"
#include "config.h"
#include "boss.h"
#include "data/boss_defs.h"   // G1 Step6
#include "core/logger.h"

extern Font g_font, g_font_small;
extern bool g_font_loaded;

// ── M3: 楼层群系着色 (与 biomes.json 顺序一致: 1-4苔原 5熔岩 6-9沼泽 10黄沙 11-14深渊 15终焉) ──
static Color _floor_accent(int floor) {
    if (floor >= 15) return {180, 60, 220, 255};   // 终焉 紫
    if (floor >= 11) return {90, 60, 140, 255};    // 深渊 暗紫蓝
    if (floor >= 10) return {200, 170, 90, 255};   // 黄沙
    if (floor >= 6)  return {80, 160, 110, 255};   // 沼泽
    if (floor >= 5)  return {230, 100, 50, 255};   // 熔岩
    return {90, 190, 170, 255};                    // 微光苔原
}

void FloorSelectScene::_render() {
    // M3: 深空底色 + 顶部渐变 (脱离黑底色块)
    ClearBackground({16, 12, 24, 255});
    int sw = get_tree()->get_width(), sh = get_tree()->get_height();
    for (int i = 0; i < 60; i++) {
        float t = i / 60.0f;
        Color c = {(unsigned char)(16 + 14 * t), (unsigned char)(12 + 8 * t),
                   (unsigned char)(24 + 16 * t), 255};
        DrawRectangle(0, i, sw, 1, c);
    }
    // 标题带底线
    if (g_font_loaded) {
        float tw = MeasureTextEx(g_font, "选择关卡", 32, 1).x;
        DrawTextEx(g_font, "选择关卡", {sw/2.0f - tw/2, 26}, 32, 1, {255, 220, 150, 255});
        DrawLine(sw/2 - tw/2, 66, sw/2 + tw/2, 66, {255, 220, 150, 90});
        float hw = MeasureTextEx(g_font_small, "W/S/A/D 选择 · Enter 进入 · Esc 返回", 15, 1).x;
        DrawTextEx(g_font_small, "W/S/A/D 选择 · Enter 进入 · Esc 返回",
                   {sw/2.0f - hw/2, 72}, 15, 1, {130, 130, 150, 255});
    }

    int cols = 5, cell_w = 100, cell_h = 70, gap = 10;
    int sx = sw / 2 - (cols * (cell_w + gap)) / 2, sy = 100;

    for (int i = 0; i < MAX_FLOORS; i++) {
        int cx = sx + (i % cols) * (cell_w + gap);
        int cy = sy + (i / cols) * (cell_h + gap);
        int floor_num = i + 1;
        bool unlocked = floor_num <= max_unlocked
                     || (floor_num == 15 && max_unlocked >= 14);
        bool selected = i == cursor;
        bool is_boss = (floor_num == 5 || floor_num == 10 || floor_num == 15);
        Color accent = _floor_accent(floor_num);

        // M3: 解锁格按群系着色; 锁定格暗灰 + 内部斜纹
        Color bg   = selected ? Color{50, 46, 100, 255}
                   : unlocked ? Color{28 + accent.r/6, 26 + accent.g/6, 44 + accent.b/6, 255}
                              : Color{22, 20, 30, 255};
        Color border = selected ? Color{255, 210, 90, 255}
                    : unlocked ? Color{accent.r/2 + 60, accent.g/2 + 60, accent.b/2 + 60, 255}
                               : Color{60, 58, 72, 255};

        DrawRectangle(cx, cy, cell_w, cell_h, bg);
        DrawRectangleLinesEx({(float)cx, (float)cy, (float)cell_w, (float)cell_h},
                              selected ? 2.5f : 1.5f, border);
        // Boss 格: accent 角标三角 ( menace 提示, 立绘感)
        if (is_boss && unlocked) {
            DrawTriangle({(float)(cx + cell_w - 14), (float)cy + 4},
                         {(float)(cx + cell_w - 4), (float)cy + 4},
                         {(float)(cx + cell_w - 4), (float)cy + 14},
                         accent);
        }
        // 锁定格: 中央锁孔点
        if (!unlocked) {
            DrawCircle(cx + cell_w/2, cy + cell_h/2 - 8, 3, {70, 68, 84, 255});
            DrawRectangle(cx + cell_w/2 - 4, cy + cell_h/2 - 4, 8, 7, {70, 68, 84, 255});
        }

        char num[4]; snprintf(num, sizeof(num), "%d", floor_num);
        Color num_c = !unlocked ? Color{75, 72, 88, 255}
                    : is_boss ? Color{255, 120, 90, 255}
                              : Color{235, 235, 245, 255};
        if (g_font_loaded) {
            float nw = MeasureTextEx(g_font, num, 26, 1).x;
            DrawTextEx(g_font, num, {cx + cell_w/2 - nw/2, cy + 10}, 26, 1, num_c);
        } else {
            DrawText(num, cx + cell_w/2 - 10, cy + 8, 28, num_c);
        }

        // Boss 名 (解锁格才显示; M3: 主字号 13 + accent 底衬线)
        if (is_boss && unlocked) {
            const BossDef* def = get_boss_def_for_floor(floor_num);
            if (def && g_font_loaded) {
                float w = MeasureTextEx(g_font_small, def->name.c_str(), 13, 1).x;
                DrawTextEx(g_font_small, def->name.c_str(),
                           {(float)(cx + cell_w/2) - w/2, (float)(cy + cell_h - 22)}, 13, 1,
                           Color{(unsigned char)(255 - (255-accent.r)/2),
                                 (unsigned char)(160), (unsigned char)(150), 255});
                DrawLine(cx + cell_w/2 - (int)w/2, cy + cell_h - 8,
                         cx + cell_w/2 + (int)w/2, cy + cell_h - 8,
                         {accent.r, accent.g, accent.b, 120});
            }
        } else if (unlocked && g_font_loaded) {
            // 非Boss解锁格: 小字群系名 (加地理感)
            const char* biome = floor_num >= 15 ? "终焉" : floor_num >= 11 ? "深渊"
                              : floor_num == 10 ? "黄沙" : floor_num >= 6 ? "幽沼"
                              : floor_num == 5 ? "熔岩" : "苔原";
            float w = MeasureTextEx(g_font_small, biome, 11, 1).x;
            DrawTextEx(g_font_small, biome,
                {(float)(cx + cell_w/2) - w/2, (float)(cy + cell_h - 18)}, 11, 1,
                Color{(unsigned char)(accent.r/2+40), (unsigned char)(accent.g/2+40),
                      (unsigned char)(accent.b/2+40), 220});
        }
    }

    // 底部: 本档进度 + 返回提示
    if (g_font_loaded) {
        const char* ops = "已解锁楼层将以群系颜色点亮 · 红字为Boss层";
        float ow = MeasureTextEx(g_font_small, ops, 13, 1).x;
        DrawTextEx(g_font_small, ops, {sw/2.0f - ow/2, (float)(sh - 34)}, 13, 1, {110, 110, 130, 200});
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
        if (floor <= max_unlocked || (floor == 15 && max_unlocked >= 14)) {
            auto gs = std::make_shared<GameScene>();
            gs->name = "GameScene";
            // G10.9-B2: 读活跃槽 (endings 从 meta 恢复, 不再传档内数据)
            auto* data = SaveManager::load_save();
            if (data && data->player) {
                gs->load_saved_game(floor, max_unlocked, std::move(data->player),
                                    0, {}, {}, data->rule_counters, data->quest_states,
                                    data->play_time);
                // M4e: 跨对局镜像记忆
                gs->set_mirror_memory(data->mirror_prior_alpha,
                                      data->mirror_prior_beta);
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
