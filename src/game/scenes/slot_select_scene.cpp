// ══ G10.9-C1: 统一三档选择场景 ══
// 红线遵守: 只调 Slot API, 不碰存档格式; 不改 Meta/Mirror/流程核心
#include "slot_select_scene.h"
#include "title_scene.h"
#include "game_scene.h"
#include "floor_select_scene.h"
#include "scene_tree.h"
#include "audio/audio_server.h"
#include "config.h"
#include "core/logger.h"

extern Font g_font, g_font_small;
extern bool g_font_loaded;

static const char* _mode_title(SlotSelectScene::Mode m) {
    switch (m) {
    case SlotSelectScene::Mode::NEW_GAME:     return "新游戏 · 选择存档";
    case SlotSelectScene::Mode::CONTINUE_GAME: return "继续游戏 · 选择存档";
    case SlotSelectScene::Mode::SELECT_FLOOR:  return "选择关卡 · 选择存档";
    }
    return "";
}

static const char* _mode_hint(SlotSelectScene::Mode m, bool any_exists) {
    if (m == SlotSelectScene::Mode::NEW_GAME)
        return any_exists ? "选择空档开始 (满档需先删除)" : "选择存档位置";
    return "选择要继续的存档";
}

static const char* _element_name(int t) {
    switch (t) {
    case 1: return "火";
    case 2: return "冰";
    case 3: return "毒";
    }
    return "";
}

void SlotSelectScene::_ready() {
    name = "SlotSelectScene";
    _refresh_slots();
}

void SlotSelectScene::_refresh_slots() {
    _slots = SaveManager::get_all_slots();
    _cursor = 0;
    // 光标跳过不可选项
    for (int i = 0; i < (int)_slots.size(); i++) {
        if (_slot_clickable(i)) { _cursor = i; break; }
    }
}

Rectangle SlotSelectScene::_card_rect(int i) const {
    return {CARD_X, CARD_Y0 + i * (CARD_H + CARD_GAP), CARD_W, CARD_H};
}

// 各模式下哪些卡可点:
//  NEW_GAME: 全部可点 (空档=创建, 已有档=删除重建流)
//  CONTINUE/SELECT_FLOOR: 只有已有档可点 (空档灰显不可选)
bool SlotSelectScene::_slot_clickable(int i) const {
    if (i < 0 || i >= (int)_slots.size()) return false;
    if (mode == Mode::NEW_GAME) return true;
    return _slots[i].exists;
}

void SlotSelectScene::_process(double delta) { _anim += (float)delta; }

void SlotSelectScene::_input(const InputMap& input) {
    auto* tree = get_tree();
    if (!tree) return;

    // 删除二次确认框: 模态独占 (Enter=确认删除 / Esc=取消)
    if (_delete_confirm_open) {
        if (input.is_action_just_pressed("cancel"))
            { _delete_confirm_open = false; _delete_target = -1; return; }
        if (input.is_action_just_pressed("confirm")) {
            int deleted_slot = _slots[_delete_target].slot_id;
            _confirm_delete(_delete_target);
            _delete_confirm_open = false;
            _delete_target = -1;
            SaveManager::set_active_slot(deleted_slot);
            auto gs = std::make_shared<GameScene>();
            gs->name = "GameScene";
            gs->new_game();
            tree->change_scene(gs);
            LOG_INFO("删除 slot %d → 原地新游戏", deleted_slot);
        }
        return;
    }

    if (input.is_action_just_pressed("cancel")) {
        tree->change_scene(std::make_shared<TitleScene>());
        return;
    }
    if (input.is_action_just_pressed("move_up")) {
        for (int i = _cursor - 1; i >= 0; i--)
            if (_slot_clickable(i)) { _cursor = i; break; }
    }
    if (input.is_action_just_pressed("move_down")) {
        for (int i = _cursor + 1; i < (int)_slots.size(); i++)
            if (_slot_clickable(i)) { _cursor = i; break; }
    }
    if (input.is_action_just_pressed("confirm")) {
        _activate_slot(_cursor);
        return;
    }
    // 鼠标: 悬停选中 + 左键确认 (空槽 NEW_GAME 也可点)
    Vector2 mouse = tree->get_mouse_logical();
    for (int i = 0; i < (int)_slots.size(); i++) {
        if (CheckCollisionPointRec(mouse, _card_rect(i))) {
            bool can = _slot_clickable(i) || (mode == Mode::NEW_GAME && !_slots[i].exists);
            if (can) {
                _cursor = i;
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                    _activate_slot(i);
            }
            break;
        }
    }
}

void SlotSelectScene::_activate_slot(int i) {
    if (!_slot_clickable(i)) return;
    auto& s = _slots[i];
    get_tree()->get_audio()->play_sfx("ui_confirm", 0.5f);

    if (mode == Mode::NEW_GAME) {
        if (!s.exists) { _enter_game(i); return; }        // 空档 → 直接开新
        // 有档 → 二次确认删除后新建 (C3 满档管理同一入口)
        _delete_confirm_open = true;
        _delete_target = i;
        return;
    }
    // CONTINUE / SELECT_FLOOR: 已有档
    if (!s.exists) return;
    _enter_game(i);
}

void SlotSelectScene::_confirm_delete(int i) {
    if (i < 0 || i >= (int)_slots.size()) return;
    SaveManager::delete_save(_slots[i].slot_id);   // 只删 Slot, Meta 不动
    LOG_INFO("[SLOT] 删除 slot %d (Meta 保留)", _slots[i].slot_id);
}

// 进游戏: 三模式统一收口 — 绑活跃槽 + 分发
void SlotSelectScene::_enter_game(int i) {
    if (i < 0 || i >= (int)_slots.size() || !_slots[i].exists && mode != Mode::NEW_GAME)
        return;
    int slot_id = _slots[i].slot_id;
    SaveManager::set_active_slot(slot_id);

    if (mode == Mode::NEW_GAME) {
        auto gs = std::make_shared<GameScene>();
        gs->name = "GameScene";
        gs->new_game();
        get_tree()->change_scene(gs);
        LOG_INFO("新游戏 → slot %d", slot_id);
        return;
    }
    if (mode == Mode::CONTINUE_GAME) {
        // 复用标题场景同款读档路径
        auto gs = std::make_shared<GameScene>();
        gs->name = "GameScene";
        auto* data = SaveManager::load_game(slot_id);
        if (!data) return;   // 空档不可达 (不可点), 保险
        int floor = data->current_floor, maxf = data->max_unlocked_floor;
        if (data->player)
            gs->load_saved_game(floor, maxf, std::move(data->player),
                                data->dungeon_seed, data->special_triggered,
                                data->special_discovered, data->rule_counters,
                                data->quest_states, data->play_time);
        else {
            auto p = std::make_unique<Player>(TILE_SIZE * 2, TILE_SIZE * 2,
                PLAYER_SPEED, PLAYER_MAX_HP, PLAYER_ATTACK, PLAYER_PDEF, PLAYER_MDEF);
            gs->load_saved_game(floor, maxf, std::move(p),
                                data->dungeon_seed, data->special_triggered,
                                data->special_discovered, data->rule_counters,
                                data->quest_states, data->play_time);
        }
        gs->set_mirror_memory(data->mirror_prior_alpha, data->mirror_prior_beta);
        delete data;
        get_tree()->change_scene(gs);
        LOG_INFO("继续 → slot %d 第%d层", slot_id, floor);
        return;
    }
    // SELECT_FLOOR: 带该档 maxf 进选关场景 (只解锁该档范围)
    auto fs = std::make_shared<FloorSelectScene>();
    fs->name = "FloorSelectScene";
    fs->max_unlocked = SaveManager::get_slot_summary(slot_id).max_floor;
    get_tree()->change_scene(fs);
    LOG_INFO("选关 → slot %d (max %d)", slot_id, fs->max_unlocked);
}

void SlotSelectScene::_render() {
    ClearBackground({20, 15, 30, 255});
    int sw = get_tree()->get_width(), sh = get_tree()->get_height();
    (void)sh;

    bool any_exists = false;
    for (auto& s : _slots) if (s.exists) { any_exists = true; break; }

    // 标题 + 提示
    if (g_font_loaded) {
        const char* title = _mode_title(mode);
        float tw = MeasureTextEx(g_font, title, 30, 1).x;
        DrawTextEx(g_font, title, {sw/2.0f - tw/2, 130}, 30, 1, {255, 220, 150, 255});
        const char* hint = _mode_hint(mode, any_exists);
        float hw = MeasureTextEx(g_font_small, hint, 16, 1).x;
        DrawTextEx(g_font_small, hint, {sw/2.0f - hw/2, 172}, 16, 1, {150, 150, 170, 255});
    }

    // 三张卡
    for (int i = 0; i < (int)_slots.size(); i++) {
        auto& s = _slots[i];
        Rectangle r = _card_rect(i);
        bool clickable = _slot_clickable(i);
        bool active = (i == _cursor);

        Color bg = active ? Color{45, 45, 90, 255}
                 : clickable ? Color{28, 28, 52, 255} : Color{22, 22, 38, 255};
        Color border = active ? Color{255, 210, 90, 255}
                     : clickable ? Color{90, 90, 140, 255} : Color{50, 50, 70, 255};
        float roundness = 0.09f;
        DrawRectangleRounded(r, roundness, 8, bg);
        DrawRectangleRoundedLines(r, roundness, 8, active ? 2.5f : 1.5f, border);

        char label[32];
        snprintf(label, sizeof(label), "存档 %d", s.slot_id);
        Color lc = clickable ? Color{230, 230, 240, 255} : Color{110, 110, 125, 255};

        if (g_font_loaded) {
            // 行1: 存档 N (空档: 空存档 · 可开始新游戏)
            DrawTextEx(g_font_small, label, {r.x + 22, r.y + 14}, 20, 1, lc);
            if (s.exists) {
                char line2[64];
                snprintf(line2, sizeof(line2), "F%d · Lv%d · %s元素 · 历史最高 F%d",
                         s.floor, s.level, _element_name(s.element_type), s.max_floor);
                DrawTextEx(g_font_small, line2, {r.x + 22, r.y + 46}, 16, 1,
                           Color{180, 190, 210, 255});
                int total = (int)s.play_time;
                int h = total / 3600, m = (total % 3600) / 60, sec = total % 60;
                char line3[48];
                snprintf(line3, sizeof(line3), "游戏时间 %02d:%02d:%02d", h, m, sec);
                DrawTextEx(g_font_small, line3, {r.x + 22, r.y + 74}, 15, 1,
                           Color{140, 140, 160, 255});
            } else if (mode == Mode::NEW_GAME) {
                DrawTextEx(g_font_small, "空存档 · 从这里开始新冒险",
                    {r.x + 22, r.y + 52}, 16, 1, Color{130, 200, 130, 255});
            } else {
                DrawTextEx(g_font_small, "空存档", {r.x + 22, r.y + 52}, 16, 1,
                    Color{110, 110, 125, 255});
            }
        }
        // NEW_GAME 模式下已有档: 右上角红字提示可删除重建
        if (g_font_loaded && mode == Mode::NEW_GAME && s.exists) {
            const char* del = "[删除后新建]";
            float dw = MeasureTextEx(g_font_small, del, 13, 1).x;
            DrawTextEx(g_font_small, del,
                {r.x + r.width - dw - 14, r.y + 14}, 13, 1, Color{220, 120, 120, 220});
        }
    }

    // 底部操作提示
    if (g_font_loaded) {
        const char* ops = "W/S 或 鼠标 选择 · Enter 或 点击 确认 · Esc 返回";
        float ow = MeasureTextEx(g_font_small, ops, 15, 1).x;
        DrawTextEx(g_font_small, ops, {sw/2.0f - ow/2, 590}, 15, 1, {120, 120, 140, 255});
    }

    if (_delete_confirm_open) _draw_delete_confirm();
}

// 满档删除二次确认 (模态, 红色警示)
void SlotSelectScene::_draw_delete_confirm() {
    if (_delete_target < 0 || _delete_target >= (int)_slots.size()) return;
    auto& s = _slots[_delete_target];
    DrawRectangle(0, 0, get_tree()->get_width(), get_tree()->get_height(),
                  Color{0, 0, 0, 160});

    float bw = 420, bh = 190;
    Rectangle box = {(get_tree()->get_width() - bw) / 2.0f,
                     (get_tree()->get_height() - bh) / 2.0f, bw, bh};
    DrawRectangleRounded(box, 0.08f, 10, {30, 18, 22, 255});
    DrawRectangleRoundedLines(box, 0.08f, 10, 2, {200, 80, 80, 255});

    if (g_font_loaded) {
        const char* t1 = "⚠ 删除该存档？";
        float w1 = MeasureTextEx(g_font, t1, 26, 1).x;
        DrawTextEx(g_font, t1, {box.x + bw/2 - w1/2, box.y + 26}, 26, 1, {255, 120, 120, 255});
        const char* t2 = "此操作将删除本档的游戏进度";
        float w2 = MeasureTextEx(g_font_small, t2, 17, 1).x;
        DrawTextEx(g_font_small, t2, {box.x + bw/2 - w2/2, box.y + 72}, 17, 1,
                   {210, 180, 180, 255});
        const char* t3 = "(结局收集与账号成长不受影响)";
        float w3 = MeasureTextEx(g_font_small, t3, 14, 1).x;
        DrawTextEx(g_font_small, t3, {box.x + bw/2 - w3/2, box.y + 98}, 14, 1,
                   {130, 200, 130, 255});

        // [取消] [删除] 双按钮 + 点击
        auto* tree = get_tree();
        Vector2 mouse = tree->get_mouse_logical();
        Rectangle rb_cancel = {box.x + 60, box.y + 132, 130, 36};
        Rectangle rb_delete = {box.x + bw - 190, box.y + 132, 130, 36};
        bool hov_c = CheckCollisionPointRec(mouse, rb_cancel);
        bool hov_d = CheckCollisionPointRec(mouse, rb_delete);
        DrawRectangleRounded(rb_cancel, 0.2f, 6,
            hov_c ? Color{70, 70, 90, 255} : Color{50, 50, 65, 255});
        DrawRectangleRoundedLines(rb_cancel, 0.2f, 6, 1, {130, 130, 150, 255});
        float wc = MeasureTextEx(g_font_small, "取消", 18, 1).x;
        DrawTextEx(g_font_small, "取消", {rb_cancel.x + 65 - wc/2, rb_cancel.y + 8}, 18, 1, WHITE);
        DrawRectangleRounded(rb_delete, 0.2f, 6,
            hov_d ? Color{140, 40, 40, 255} : Color{100, 30, 30, 255});
        DrawRectangleRoundedLines(rb_delete, 0.2f, 6, 1, {200, 80, 80, 255});
        float wd = MeasureTextEx(g_font_small, "删除", 18, 1).x;
        DrawTextEx(g_font_small, "删除", {rb_delete.x + 65 - wd/2, rb_delete.y + 8}, 18, 1, WHITE);

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (hov_c) { _delete_confirm_open = false; _delete_target = -1; }
            else if (hov_d) {
                int deleted_slot = _slots[_delete_target].slot_id;
                _confirm_delete(_delete_target);
                _delete_confirm_open = false;
                _delete_target = -1;
                // 满档管理流收尾: 删除后原地开新 (该槽已空)
                SaveManager::set_active_slot(deleted_slot);
                auto gs = std::make_shared<GameScene>();
                gs->name = "GameScene";
                gs->new_game();
                tree->change_scene(gs);
                LOG_INFO("删除 slot %d → 原地新游戏", deleted_slot);
            }
        }
    }
    (void)s;
}
