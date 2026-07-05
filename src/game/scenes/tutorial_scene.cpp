#include "tutorial_scene.h"
#include "title_scene.h"
#include "scene_tree.h"
#include "combat_system.h"
#include "config.h"
#include "audio_server.h"
#include "core/logger.h"
#include <cmath>

extern Font g_font, g_font_small;
extern bool g_font_loaded;

void TutorialScene::_ready() {
    name = "TutorialScene";
    game_time = 0;
    inventory_open = false;
    inv_cursor = 0;
    gave_skill = false;

    game_map = build_tutorial_map();
    player = std::make_unique<Player>(2 * TILE_SIZE, 4 * TILE_SIZE, PLAYER_SPEED,
        50, PLAYER_ATTACK, PLAYER_PDEF, PLAYER_MDEF);  // 教程用50HP方便检测药水使用

    monsters.clear();
    monsters.push_back(create_tutorial_dummy(8, 4));
    ground_items = create_tutorial_items(6, 5);
}

void TutorialScene::_process(double delta) {
    if (!player) return;
    float dt = (float)delta;
    game_time += dt;

    if (guide.stage == TutorialStage::WELCOME) return;

    auto& input = get_tree()->get_input();

    if (!inventory_open) {
        // 移动
        Vector2 move = input.get_movement_axis();
        if (input.is_action_pressed("move_up") && !input.is_action_pressed("move_down"))
            player->direction = Direction::UP;
        if (input.is_action_pressed("move_down") && !input.is_action_pressed("move_up"))
            player->direction = Direction::DOWN;
        if (input.is_action_pressed("move_left") && !input.is_action_pressed("move_right"))
            player->direction = Direction::LEFT;
        if (input.is_action_pressed("move_right") && !input.is_action_pressed("move_left"))
            player->direction = Direction::RIGHT;

        float s = player->speed * dt;
        auto& e = player->entity;
        e.position.x += move.x * s; e.sync_rect();
        if (!game_map->is_rect_walkable(e.rect)) { e.position.x -= move.x * s; e.sync_rect(); }
        e.position.y += move.y * s; e.sync_rect();
        if (!game_map->is_rect_walkable(e.rect)) { e.position.y -= move.y * s; e.sync_rect(); }
    }

    // 授予技能
    if (guide.stage == TutorialStage::SKILL && !gave_skill) {
        give_tutorial_skill(player.get());
        gave_skill = true;
    }

    // 阶段检测
    guide.check_and_advance(player.get(), inventory_open,
        reinterpret_cast<std::vector<Monster*>&>(monsters), ground_items);

    // 摄像机
    cam_x = player->entity.rect.x + player->entity.rect.width/2 - get_tree()->get_width()/2;
    cam_y = player->entity.rect.y + player->entity.rect.height/2 - get_tree()->get_height()/2;
}

void TutorialScene::_render() {
    ClearBackground(BLACK);
    int sw = get_tree()->get_width(), sh = get_tree()->get_height();

    // 地图
    if (game_map) game_map->draw(cam_x, cam_y, sw, sh);

    // 实体
    for (auto& m : monsters) m->draw(cam_x, cam_y);
    if (player) player->draw_no_cam(cam_x, cam_y);

    // 掉落物 (简化绘制)
    for (auto& d : ground_items) {
        float px = d.tile_x * TILE_SIZE - cam_x + 2;
        float py = d.tile_y * TILE_SIZE - cam_y + 2;
        DrawRectangle(px, py, TILE_SIZE - 4, TILE_SIZE - 4, d.item->color);
    }

    // 背包面板
    if (inventory_open && player) {
        DrawRectangle(0, 0, sw, sh, {0, 0, 0, 180});
        auto& inv = player->inventory;
        if (g_font_loaded) {
            DrawTextEx(g_font_small, "背包 (I关闭)", {sw/2.0f - 200, sh/2.0f - 200}, 20, 1, {200, 200, 255, 255});
            for (int i = 0; i < (int)inv.items.size(); i++) {
                std::string mk = (i == inv_cursor) ? ">" : " ";
                std::string txt = mk + " " + inv.items[i]->get_description();
                DrawTextEx(g_font_small, txt.c_str(), {sw/2.0f - 180, sh/2.0f - 160 + (float)i * 26}, 16, 1, inv.items[i]->color);
            }
        }
    }

    // 教程提示框
    auto lines = guide.get_instructions();
    if (!lines.empty()) {
        float bw = 380, bh = (float)lines.size() * 24 + 30;
        float bx = sw/2.0f - bw/2, by = 60;
        DrawRectangle(bx, by, bw, bh, {10, 10, 30, 220});
        DrawRectangleLines(bx, by, bw, bh, {80, 80, 150, 255});
        if (g_font_loaded) {
            for (int i = 0; i < (int)lines.size(); i++) {
                DrawTextEx(g_font_small, lines[i].c_str(),
                    {bx + 12, by + 16 + (float)i * 24}, 18, 1, WHITE);
            }
        }
    }

    // 底部按键提示
    if (g_font_loaded && guide.stage != TutorialStage::WELCOME) {
        DrawTextEx(g_font_small, "WASD移动 | 空格攻击 | E拾取 | I背包 | P跳过本步 | T退出",
            {(float)sw/2 - 220, (float)(sh - 24)}, 14, 1, {140, 140, 140, 255});
    }
}

void TutorialScene::_input(const InputMap& input) {
    if (!player) return;

    if (input.is_action_just_pressed("cancel") || IsKeyPressed(KEY_T)) {
        auto ts = std::make_shared<TitleScene>();
        ts->name = "TitleScene";
        get_tree()->change_scene(ts);
        LOG_INFO("退出教程");
        return;
    }

    // P键跳过当前阶段
    if (IsKeyPressed(KEY_P) && guide.stage != TutorialStage::WELCOME
        && guide.stage != TutorialStage::COMPLETE) {
        guide.advance_stage();
        LOG_INFO("跳过教程阶段");
        return;
    }

    if (guide.stage == TutorialStage::WELCOME) {
        if (input.is_action_just_pressed("confirm")) {
            guide.advance_stage();
        }
        return;
    }

    if (guide.stage == TutorialStage::COMPLETE) {
        if (input.is_action_just_pressed("confirm")) {
            auto ts = std::make_shared<TitleScene>();
            ts->name = "TitleScene";
            get_tree()->change_scene(ts);
        }
        return;
    }

    // 背包模式
    if (inventory_open) {
        if (input.is_action_just_pressed("inventory") || input.is_action_just_pressed("cancel"))
            { inventory_open = false; return; }
        if (input.is_action_just_pressed("move_up"))    inv_cursor = std::max(0, inv_cursor - 1);
        if (input.is_action_just_pressed("move_down"))  inv_cursor = std::min((int)player->inventory.items.size() - 1, inv_cursor + 1);
        if (IsKeyPressed(KEY_X)) { player->inventory.equip(inv_cursor, player.get()); inv_cursor = std::min(inv_cursor, std::max(0, (int)player->inventory.items.size() - 1)); }
        if (IsKeyPressed(KEY_U)) {
            if (guide.stage == TutorialStage::INVENTORY) guide.item_used = true;
            player->inventory.use_item(inv_cursor, player.get());
            inv_cursor = std::min(inv_cursor, std::max(0, (int)player->inventory.items.size() - 1));
        }
        guide.check_and_advance(player.get(), inventory_open,
            reinterpret_cast<std::vector<Monster*>&>(monsters), ground_items);
        return;
    }

    // 游戏操作
    if (input.is_action_just_pressed("attack")) {
        if (!player->can_attack(game_time)) return;
        std::vector<Monster*> ml;
        for (auto& m : monsters) ml.push_back(m.get());
        auto* target = find_attack_target(player->entity.rect, ml, PLAYER_ATTACK_RANGE);
        if (target) {
            int dmg = calculate_damage(player->combat.get_effective_attack(),
                target->combat.get_effective_defense(AttackType::PHYSICAL));
            player->_last_attack_time = game_time;
            target->combat.take_damage(dmg);
            get_tree()->get_audio()->play_sfx("melee");
        }
    }
    if (input.is_action_just_pressed("pickup")) {
        DroppedItem* best = nullptr;
        float bd = PICKUP_RANGE * TILE_SIZE;
        for (auto& d : ground_items) {
            float px = d.tile_x * TILE_SIZE + TILE_SIZE/2, py = d.tile_y * TILE_SIZE + TILE_SIZE/2;
            float dist = std::hypot(player->entity.rect.x + player->entity.rect.width/2 - px,
                                     player->entity.rect.y + player->entity.rect.height/2 - py);
            if (dist < bd) { bd = dist; best = &d; }
        }
        if (best && player->inventory.add(best->item, player.get())) {
            auto it = std::find_if(ground_items.begin(), ground_items.end(),
                [&](auto& x) { return &x == best; });
            if (it != ground_items.end()) ground_items.erase(it);
            get_tree()->get_audio()->play_sfx("pickup");
        }
    }
    if (input.is_action_just_pressed("inventory")) {
        inventory_open = true; inv_cursor = 0;
    }
    if (input.is_action_just_pressed("skill_1") && guide.stage == TutorialStage::SKILL && !guide._skill_used) {
        guide.notify_skill_used();
        get_tree()->get_audio()->play_sfx("slash");
    }

    guide.check_and_advance(player.get(), inventory_open,
        reinterpret_cast<std::vector<Monster*>&>(monsters), ground_items);
}
