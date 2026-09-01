#include "tutorial_scene.h"
#include "title_scene.h"
#include "scene_tree.h"
#include "combat_system.h"
#include "config.h"
#include "audio_server.h"
#include "rendering/sprite_renderer.h"
#include "resources/resource_manager.h"
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

    // G10.8-fix: 教程漆黑回归 — G10.4 后地图渲染依赖 is_explored,
    // 教程从未调用 update_fov → 全部 tile 被可见性剔除 → 黑屏
    auto [px, py] = game_map->pixel_to_tile(
        player->entity.rect.x + player->entity.rect.width/2,
        player->entity.rect.y + player->entity.rect.height/2);
    game_map->update_fov(px, py, 12);   // 大半径: 小沙箱全亮, 教学无视野迷雾
}

void TutorialScene::_process(double delta) {
    if (!player) return;
    float dt = (float)delta;

    // G10.8-B1: HitStop 冻结期间暂停模拟 (表现层停顿)
    if (_tutorial_hitstop > 0.0f) {
        _tutorial_hitstop -= dt;
        return;
    }
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

    // G10.8-fix: 每帧同步 FOV (渲染依赖 is_explored/is_visible)
    if (game_map) {
        auto [px, py] = game_map->pixel_to_tile(
            player->entity.rect.x + player->entity.rect.width/2,
            player->entity.rect.y + player->entity.rect.height/2);
        game_map->update_fov(px, py, 12);
    }
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
            DrawTextEx(g_font_small, "背包 (B关闭)", {sw/2.0f - 200, sh/2.0f - 200}, 20, 1, {200, 200, 255, 255});
            for (int i = 0; i < (int)inv.items.size(); i++) {
                std::string mk = (i == inv_cursor) ? ">" : " ";
                std::string txt = mk + " " + inv.items[i]->get_description();
                // M4f.13: 物品图标 (16px 贴图)
                const char* ikey = item_icon_key(inv.items[i].get());
                if (ikey) {
                    SpriteDef xd;
                    Texture2D itex = ResourceManager::inst().sprite_by_key(ikey, xd);
                    if (itex.id > 0)
                        SpriteRenderer::draw_sprite(itex, xd, 0,
                            {sw/2.0f - 215, sh/2.0f - 160 + (float)i * 26 + 2, 18, 18});
                }
                DrawTextEx(g_font_small, txt.c_str(),
                    {sw/2.0f - 192, sh/2.0f - 160 + (float)i * 26}, 16, 1, inv.items[i]->color);
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
        DrawTextEx(g_font_small, "WASD移动 | 空格攻击 | E交互 | B背包 | P跳过本步 | T退出",
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

    // G10.8-B2: ELEMENT 步骤 — 数字 1/2/3 选择元素（与正式游戏一致的字段）
    if (guide.stage == TutorialStage::ELEMENT) {
        if (input.is_action_just_pressed("skill_1")) {
            player->element.select(ElementType::FIRE);
            get_tree()->get_audio()->play_sfx("ui_confirm");
        }
        if (input.is_action_just_pressed("skill_2")) {
            player->element.select(ElementType::ICE);
            get_tree()->get_audio()->play_sfx("ui_confirm");
        }
        if (input.is_action_just_pressed("skill_3")) {
            player->element.select(ElementType::POISON);
            get_tree()->get_audio()->play_sfx("ui_confirm");
        }
    }

    // G10.8-B2: COOLDOWN 步骤 — 1.5s 后自动通过（玩家观察蓝条变化）
    if (guide.stage == TutorialStage::COOLDOWN) {
        static float cd_timer = 0.0f;
        cd_timer += GetFrameTime();
        if (cd_timer > 1.5f) {
            guide.cooldown_waited = true;
            cd_timer = 0.0f;
        }
    } else {
        // 重置 static 计时器（离开步骤时）
    }

    // WEAPON_INFO 步骤 — Enter 进入 COMPLETE
    if (guide.stage == TutorialStage::WEAPON_INFO) {
        if (input.is_action_just_pressed("confirm")) {
            guide.advance_stage();
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
        // G10.8-B1: 接入 WeaponExecutor — 教程与正式游戏共享同一战斗链
        // (三段连击/HitShape/元素/HitStop 全一致, 消除 legacy fist 体验断层)
        if (player->weapon.can_attack(game_time)) {
            std::vector<Monster*> ml;
            for (auto& m : monsters) ml.push_back(m.get());
            auto results = WeaponExecutor::execute(
                player.get(), ml, game_time,
                get_tree()->get_audio(), nullptr, game_map.get());
            for (auto& r : results) {
                // 命中反馈: 伤害数字 + HitStop (走正式游戏的 PresentationDirector)
                get_tree()->get_audio()->play_sfx("hit");
            }
            if (!results.empty()) {
                // 轻量打击停顿 — 复用正式游戏 CombatFeelSystem 常量
                _tutorial_hitstop = CombatFeelSystem::LIGHT_HIT;
            }
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
