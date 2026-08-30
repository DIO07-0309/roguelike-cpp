#include "player_controller.h"
#include "scenes/game_scene.h"
#include "player.h"
#include "monster.h"
#include "game_map.h"
#include "combat_system.h"
#include "combat_coordinator.h"
#include "skill.h"
#include "item.h"
#include "vfx_server.h"
#include "challenge_room.h"
#include "combat_feel.h"
#include "weapon_executor.h"   // G9
#include "collision_utils.h"
#include "skill_evolution.h"   // G1
#include "flow_director.h"
#include "input_map.h"
#include "event_system.h"
#include "config.h"
#include "scene_tree.h"
#include "audio_server.h"
#include "ai/player_behavior/player_behavior_recorder.h" // F15.2
#include "reward_manager.h"
#include "relic_progression.h"
#include <cmath>
#include <algorithm>

// Batch 3C: sell helper — separated for unit testing
int PlayerController::sell_selected_item(Inventory& inv, Player* player,
                                          int& cursor,
                                          PresentationSystemDirector& pres) {
    int val = inv.sell_item(cursor, player);
    if (val > 0) {
        pres.show_message(("出售获得 " + std::to_string(val) + " Gold").c_str(), 2.0f);
    }
    int item_count = (int)inv.items.size();
    cursor = std::min(cursor, std::max(0, item_count - 1));
    return val;
}

void PlayerController::tick(float dt) {
    if (!_scene || !_scene->player || !_scene->player->combat.is_alive) return;

    // M1: 每帧刷新行为克隆上下文 (HP 比例/最近敌人距离/技能就绪位)
    {
        auto& c = *_scene;
        float hp = c.player->combat.max_hp > 0
            ? (float)c.player->combat.current_hp / c.player->combat.max_hp : 0;
        float nearest = 999.0f;
        for (auto& m : c.monsters) {
            if (!m->combat.is_alive) continue;
            float d = hypotf(m->entity.rect.x - c.player->entity.rect.x,
                             m->entity.rect.y - c.player->entity.rect.y) / 32.0f;
            if (d < nearest) nearest = d;
        }
        int mask = 0;
        for (size_t i = 0; i < c.player->skills.active_skills.size() && i < 4; i++)
            if (c.player->skills.active_skills[i]->remaining_cooldown(c.game_time) <= 0.0f)
                mask |= 1 << (int)i;
        g_behavior.set_context(hp, nearest > 900.0f ? -1.0f : nearest, mask);
        // M5: 条件维度 — 朝向 (Direction 枚举) + 近1s受击窗口 (供受压反击学习)
        int hp_now = c.player->combat.current_hp;
        if (_last_seen_hp >= 0 && hp_now < _last_seen_hp) _hit_window = 1.0f;
        _last_seen_hp = hp_now;
        if (_hit_window > 0) _hit_window -= dt;
        if (_hit_window < 0.0f) _hit_window = 0.0f;
        int facing = (int)c.player->direction;
        g_behavior.set_battle_context(facing, _hit_window > 0 ? 1 : 0);
    }

    // ── 移动 ──
    auto& gs = *_scene;
    static float _last_mx = 0, _last_my = 0;
    static int _last_move_dir = -1;
    // F15.2: movement state-change detection
    auto _record_move = [&](float mx, float my) {
        float dx = mx - _last_mx, dy = my - _last_my;
        _last_mx = mx; _last_my = my;
        if (fabsf(dx) > 200.0f || fabsf(dy) > 200.0f)
            g_behavior.on_dodge((float)gs.game_time, gs.current_floor,
                gs.player->entity.rect.x + gs.player->entity.rect.width/2,
                gs.player->entity.rect.y + gs.player->entity.rect.height/2);
        if (mx == 0 && my == 0) return; // idle
        int dir = (fabsf(mx) > fabsf(my)) ? (mx > 0 ? 1 : 0) : (my > 0 ? 3 : 2);
        if (dir != _last_move_dir) {
            _last_move_dir = dir;
            g_behavior.on_move_state_change((float)gs.game_time, gs.current_floor,
                gs.player->entity.rect.x + gs.player->entity.rect.width/2,
                gs.player->entity.rect.y + gs.player->entity.rect.height/2, dir);
        }
    };

    // M4.2: 镜像冻结期间玩家禁移动 (仍 tick 怪物AI与玩家buff)
    if (gs.player_frozen_by_mirror()) {
        if (gs.time_stop_remaining <= 0) {
            int hp_before = gs.player->combat.current_hp;
            gs._update_monsters(dt);
            int dmg_taken = hp_before - gs.player->combat.current_hp;
            if (dmg_taken > 0) {
                gs._boss.dmg_taken += dmg_taken;
                // F15.2: record damage taken
                PlayerAction a;
                a.type = PlayerActionType::TAKE_DAMAGE;
                a.timestamp = (float)gs.game_time; a.floor = gs.current_floor;
                a.pos_x = gs.player->entity.rect.x + gs.player->entity.rect.width/2;
                a.pos_y = gs.player->entity.rect.y + gs.player->entity.rect.height/2;
                a.value = dmg_taken;
                g_behavior.record(a);
            } else if (int heal_amt = gs.player->combat.current_hp - hp_before; heal_amt > 0) {
                g_behavior.on_heal((float)gs.game_time, gs.current_floor, heal_amt);
            }
            if (dmg_taken > 0 && gs.player->combat.is_alive) {
                gs._presentation.damage_floats.push_back({
                    gs.player->entity.rect.x + gs.player->entity.rect.width/2,
                    gs.player->entity.rect.y - 12, 0.6f, dmg_taken,
                    dmg_taken >= 30 ? Color{255, 60, 30, 255} : Color{255, 80, 80, 255}
                });
                float shake = dmg_taken >= 30 ? 12.0f : dmg_taken > 15 ? 5.0f : 2.0f;
                gs._presentation.trigger_shake(shake);
                if (dmg_taken >= 20) gs._presentation.trigger_freeze(0.05f);
                gs.get_tree()->get_audio()->play_sfx("hurt", 0.6f);  // Q4.4
                gs._presentation.trigger_hit_flash();  // Q4.7: 受击红屏
            }
        }
        return;
    }

    if (!gs.inventory_open && !gs._is_event_running() && !gs._dialogue.active && !gs._quest_log_open) {
        Vector2 move = gs.player->handle_input(gs.get_tree()->get_input());
        if (gs._sim_mode) {
            // Q3.1: headless 无真实键盘 — SimAI 决定移动方向与朝向
            InputMap& sm = gs.get_tree()->get_input();
            move.x = gs._is_action_just_pressed(sm, "move_left") ? -1.0f
                   : gs._is_action_just_pressed(sm, "move_right") ? 1.0f : 0.0f;
            move.y = gs._is_action_just_pressed(sm, "move_up") ? -1.0f
                   : gs._is_action_just_pressed(sm, "move_down") ? 1.0f : 0.0f;
            if (move.y < 0) gs.player->direction = Direction::UP;
            else if (move.y > 0) gs.player->direction = Direction::DOWN;
            else if (move.x < 0) gs.player->direction = Direction::LEFT;
            else if (move.x > 0) gs.player->direction = Direction::RIGHT;
            gs.player->is_moving = (move.x != 0 || move.y != 0);
        }
        auto& e = gs.player->entity;
        _record_move(move.x, move.y);
        float speed_mul = (gs._tw_speed_boost > 0) ? 1.25f : 1.0f;
        float s = get_effective_speed(gs.player.get()) * speed_mul * dt;
        e.position.x += move.x * s; e.sync_rect();
        if (!gs.game_map->is_rect_walkable(e.rect)) {
            e.position.x -= move.x * s; e.sync_rect();
        }
        e.position.y += move.y * s; e.sync_rect();
        if (!gs.game_map->is_rect_walkable(e.rect)) {
            e.position.y -= move.y * s; e.sync_rect();
        }

        // ── 房间发现 ──
        std::string disc = gs._interact.check_special_discovery(gs.player.get(), gs.game_map.get());
        if (!disc.empty()) { gs._presentation.room_msg = disc; gs._presentation.room_msg_timer = 2.5f; }
        if (gs.game_map && gs.game_map->event_room_index >= 0) {
            auto [tx, ty] = gs.game_map->pixel_to_tile(e.rect.x + e.rect.width/2, e.rect.y + e.rect.height/2);
            if (tx == gs.game_map->event_tile_x && ty == gs.game_map->event_tile_y) {
                if (!gs.game_map->event_triggered) {
                    auto ename = event_type_name(gs.game_map->event_type);
                    gs._presentation.room_msg = "EVENT: " + std::string(ename);
                    gs._presentation.room_msg_timer = 1.8f;
                }
            }
        }

        // ── 怪物AI ──
        if (gs.time_stop_remaining <= 0) {
            int hp_before = gs.player->combat.current_hp;
            gs._update_monsters(dt);
            int dmg_taken = hp_before - gs.player->combat.current_hp;
            if (dmg_taken > 0) {
                gs._boss.dmg_taken += dmg_taken;
                // F15.2: record damage taken
                PlayerAction a;
                a.type = PlayerActionType::TAKE_DAMAGE;
                a.timestamp = (float)gs.game_time; a.floor = gs.current_floor;
                a.pos_x = gs.player->entity.rect.x + gs.player->entity.rect.width/2;
                a.pos_y = gs.player->entity.rect.y + gs.player->entity.rect.height/2;
                a.value = dmg_taken;
                g_behavior.record(a);
            } else if (int heal_amt = gs.player->combat.current_hp - hp_before; heal_amt > 0) {
                g_behavior.on_heal((float)gs.game_time, gs.current_floor, heal_amt);
            }
            if (dmg_taken > 0 && gs.player->combat.is_alive) {
                gs._presentation.damage_floats.push_back({
                    gs.player->entity.rect.x + gs.player->entity.rect.width/2,
                    gs.player->entity.rect.y - 12, 0.6f, dmg_taken,
                    dmg_taken >= 30 ? Color{255, 60, 30, 255} : Color{255, 80, 80, 255}
                });
                float shake = dmg_taken >= 30 ? 12.0f : dmg_taken > 15 ? 5.0f : 2.0f;
                gs._presentation.trigger_shake(shake);
                if (dmg_taken >= 20) gs._presentation.trigger_freeze(0.05f);
                gs.get_tree()->get_audio()->play_sfx("hurt", 0.6f);  // Q4.4
                gs._presentation.trigger_hit_flash();  // Q4.7: 受击红屏
            }
            gs._check_floor_transition();
        }

        // ── 自愈 Lv3 持续回复 ──
        for (auto& sk : gs.player->skills.active_skills) {
            if (auto* h = dynamic_cast<SelfHealSkill*>(sk.get()))
                h->tick_regen(gs.player.get(), dt);
        }
    }
}

void PlayerController::handle_input(const InputMap& input) {
    if (!_scene || !_scene->player) return;
    auto& gs = *_scene;

    if (gs.inventory_open) {
        const int kPage = Inventory::kPageSize;
        int item_count = (int)gs.player->inventory.items.size();
        int max_page = std::max(0, (item_count + kPage - 1) / kPage - 1);
        int page = gs.inventory_cursor / kPage;
        int rel = gs.inventory_cursor % kPage;
        if (gs._is_action_just_pressed(input,"inventory") || gs._is_action_just_pressed(input,"cancel"))
            gs.inventory_open = false;
        else if (IsKeyPressed(KEY_X))
            { gs.player->inventory.equip(gs.inventory_cursor, gs.player.get()); gs.inventory_cursor = std::min(gs.inventory_cursor, std::max(0, (int)gs.player->inventory.items.size() - 1)); }
        else if (IsKeyPressed(KEY_U))
            { gs.player->inventory.use_item(gs.inventory_cursor, gs.player.get()); gs.inventory_cursor = std::min(gs.inventory_cursor, std::max(0, (int)gs.player->inventory.items.size() - 1)); }
        else if (IsKeyPressed(KEY_D))
            { gs.player->inventory.remove(gs.inventory_cursor); gs.inventory_cursor = std::min(gs.inventory_cursor, std::max(0, (int)gs.player->inventory.items.size() - 1)); }
        else if (IsKeyPressed(KEY_T))
            sell_selected_item(gs.player->inventory, gs.player.get(),
                               gs.inventory_cursor, gs._presentation);
        else if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))
            gs.inventory_cursor = std::max(0, gs.inventory_cursor - 1);
        else if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))
            gs.inventory_cursor = std::min(std::max(0, item_count - 1), gs.inventory_cursor + 1);
        else if (IsKeyPressed(KEY_LEFT) && page > 0)
            gs.inventory_cursor = (page - 1) * kPage + rel;
        else if (IsKeyPressed(KEY_RIGHT) && page < max_page)
            gs.inventory_cursor = std::min((page + 1) * kPage + rel, item_count - 1);
        return;
    }

    // Batch 3H: Gamble Room UI — E to spin, B/ESC to close
    if (gs.gamble_open) {
        if (gs._is_action_just_pressed(input,"inventory") || gs._is_action_just_pressed(input,"cancel")) {
            gs.gamble_open = false;
            gs.get_tree()->get_audio()->play_sfx("ui_click", 0.35f);
        } else if (gs._is_action_just_pressed(input,"pickup")) {
            // E key: spin
            constexpr int kGambleCost = 20;
            if (gs.player->gold < kGambleCost) {
                gs.gamble_result_msg = "金币不足！需要 " + std::to_string(kGambleCost) + " 金币。";
                gs.gamble_result_timer = 2.0f;
                gs.get_tree()->get_audio()->play_sfx("ui_click", 0.35f);
            } else {
                gs.player->spend_gold(kGambleCost);
                int roll = rng() % 100;
                if (roll < 65) {
                    // 65% Equipment
                    auto item = generate_random_item();
                    if (item && RewardManager::grant_item(*gs.player, item))
                        gs.gamble_result_msg = "运气不错！获得了 " + item->get_description();
                    else
                        gs.gamble_result_msg = "赌徒摊手：今天没货了。";
                } else if (roll < 85) {
                    // 20% Key
                    RewardManager::grant_key(*gs.player, 1);
                    gs.gamble_result_msg = "获得了一把钥匙！";
                } else if (roll < 95) {
                    // 10% Gold return
                    RewardManager::grant_gold(*gs.player, 10);
                    gs.gamble_result_msg = "获得 10 金币！";
                } else {
                    // 5% RUN Relic
                    auto all_ids = get_all_relic_ids();
                    std::vector<std::string> candidates;
                    for (auto& id : all_ids)
                        if (!player_has_relic(gs.player.get(), id))
                            candidates.push_back(id);
                    if (!candidates.empty()) {
                        std::string chosen = candidates[rng() % candidates.size()];
                        if (RewardManager::grant_relic(*gs.player, chosen, PersistenceScope::RUN)) {
                            const RelicDef* def = get_relic_def(chosen);
                            gs.gamble_result_msg = def ? ("RELIC:" + def->name) : "获得了一件圣物！";
                        } else {
                            RewardManager::grant_key(*gs.player, 1);
                            gs.gamble_result_msg = "圣物库已满，获得钥匙作为替代。";
                        }
                    } else {
                        RewardManager::grant_key(*gs.player, 1);
                        gs.gamble_result_msg = "圣物库已满，获得钥匙作为替代。";
                    }
                }
                gs.gamble_result_timer = 2.5f;
                gs.get_tree()->get_audio()->play_sfx("pickup", 0.55f);
            }
        }
        return;
    }

    // Batch 3I: Challenge choice UI
    if (gs.challenge_choice_active) {
        if (gs._is_action_just_pressed(input, "ui_up")) {
            gs.challenge_choice_cursor = (gs.challenge_choice_cursor + 1) % 2;
            gs.get_tree()->get_audio()->play_sfx("ui_click", 0.35f);
        }
        if (gs._is_action_just_pressed(input, "ui_down")) {
            gs.challenge_choice_cursor = (gs.challenge_choice_cursor + 1) % 2;
            gs.get_tree()->get_audio()->play_sfx("ui_click", 0.35f);
        }
        if (gs._is_action_just_pressed(input, "confirm") || gs._is_action_just_pressed(input, "interact")) {
            if (gs.challenge_choice_cursor == 0) {
                // Start challenge
                if (gs._challenge.consume_key_for_challenge(*gs.player)) {
                    gs.enter_challenge_arena();
                    gs.get_tree()->get_audio()->play_sfx("door_open", 0.6f);
                } else {
                    gs._presentation.room_msg = "需要钥匙才能开启挑战。";
                    gs._presentation.room_msg_timer = 2.0f;
                }
            }
            gs.challenge_choice_active = false;
            gs.get_tree()->get_audio()->play_sfx("ui_click", 0.35f);
        }
        if (gs._is_action_just_pressed(input, "cancel")) {
            gs.challenge_choice_active = false;
            gs.get_tree()->get_audio()->play_sfx("ui_click", 0.35f);
        }
        return;
    }

    // M4.2: 镜像冻结期间禁攻击/技能/拾取/交互
    if (gs.player_frozen_by_mirror()) return;

    if (gs._is_action_just_pressed(input,"attack")) {
        if (gs._sim_mode) {
            // Q3.10: 武器命中形状朝前 (长枪矩形/扇形) — 攻击前须面向最近目标, 否则原地空挥
            // 实锤: sim 自动装备长枪后 F1 被围, 朝旧方向空挥 180s, 怪血量纹丝不动
            float px = gs.player->entity.rect.x + gs.player->entity.rect.width/2;
            float py = gs.player->entity.rect.y + gs.player->entity.rect.height/2;
            float bd = 1e18f;
            Direction fd = gs.player->direction;
            for (auto& m : gs.monsters) {
                if (!m || !m->combat.is_alive) continue;
                float dx = m->entity.rect.x + m->entity.rect.width/2 - px;
                float dy = m->entity.rect.y + m->entity.rect.height/2 - py;
                float dd = dx*dx + dy*dy;
                if (dd < bd) {
                    bd = dd;
                    fd = (fabsf(dx) > fabsf(dy))
                         ? (dx > 0 ? Direction::RIGHT : Direction::LEFT)
                         : (dy > 0 ? Direction::DOWN : Direction::UP);
                }
            }
            gs.player->direction = fd;
        }
        player_attack();
    }
    else if (gs._is_action_just_pressed(input,"pickup")) {
        // E 键门交互 — 只开 CLOSED 门 (LOCKED 不可开)
        {
            auto [ptx, pty] = gs.game_map->pixel_to_tile(
                gs.player->entity.rect.x + gs.player->entity.rect.width/2,
                gs.player->entity.rect.y + gs.player->entity.rect.height/2);
            constexpr int dx4[] = {0, 0, -1, 1};
            constexpr int dy4[] = {-1, 1, 0, 0};
            for (int i = 0; i < 4; i++) {
                int nx = ptx + dx4[i], ny = pty + dy4[i];
                if (gs.game_map->door_state_at(nx, ny) == DoorState::CLOSED) {
                    gs.game_map->set_door_state(nx, ny, DoorState::OPEN);
                    gs.on_door_opened();
                    gs.get_tree()->get_audio()->play_sfx("door_open", 0.5f);
                    return;
                }
            }
        }
        // D4 Step4: NPC交互 + D4 Step1: 事件交互 + B8: 特殊房间
        {
            auto [ptx, pty] = gs.game_map->pixel_to_tile(
                gs.player->entity.rect.x + gs.player->entity.rect.width/2,
                gs.player->entity.rect.y + gs.player->entity.rect.height/2);
            for (int i = 0; i < gs._npc_count; i++) {
                int dtx = abs(ptx - gs._npc_tile_x[i]);
                int dty = abs(pty - gs._npc_tile_y[i]);
                if (dtx <= 1 && dty <= 1 && !gs._npc_state[i].finished) {
                    gs._current_npc_index = i; gs._start_dialogue(i); return;
                }
            }
        }
        if (gs.game_map && gs.game_map->event_room_index >= 0 && !gs.game_map->event_triggered) {
            auto [tx, ty] = gs.game_map->pixel_to_tile(
                gs.player->entity.rect.x + gs.player->entity.rect.width/2,
                gs.player->entity.rect.y + gs.player->entity.rect.height/2);
            if (tx == gs.game_map->event_tile_x && ty == gs.game_map->event_tile_y) {
                gs._start_event_presentation(gs.game_map->event_type);
                return;
            }
        }
        // Batch 3I: Challenge Room — Portal interaction (portal is on wall OUTSIDE room rect)
        {
            auto [ptx, pty] = gs.game_map->pixel_to_tile(
                gs.player->entity.rect.x + gs.player->entity.rect.width/2,
                gs.player->entity.rect.y + gs.player->entity.rect.height/2);
            // Arena return portal (arena map has no special_rooms, check controller directly)
            if (gs._world_mode == WorldMode::CHALLENGE_ARENA &&
                gs._challenge.phase() == ChallengePhase::CLEARED &&
                gs._challenge.return_portal_tx() >= 0) {
                int dx = abs(ptx - gs._challenge.return_portal_tx());
                int dy = abs(pty - gs._challenge.return_portal_ty());
                if (dx + dy <= 1) {
                    gs.exit_challenge_arena();
                    gs.get_tree()->get_audio()->play_sfx("door_open", 0.6f);
                    return;
                }
            }
            for (auto& sr : gs.game_map->special_rooms) {
                if (sr.type != SpecialRoomType::CHALLENGE) continue;
                if (gs._challenge.phase() == ChallengePhase::PORTAL_ACTIVE) {
                    int dx = abs(ptx - sr.portal_tx);
                    int dy = abs(pty - sr.portal_ty);
                    if (dx + dy <= 1) {
                        if (!gs.challenge_choice_active) {
                            gs.challenge_choice_active = true;
                            gs.challenge_choice_cursor = 0;
                            gs.get_tree()->get_audio()->play_sfx("ui_click", 0.35f);
                        }
                        return;
                    }
                }
            }
            SpecialRoom* room = gs.game_map->get_special_room_at(ptx, pty);
            if (room && room->type == SpecialRoomType::GAMBLER) {
                if (!gs.gamble_open) {
                    gs.gamble_open = true;
                    gs.gamble_cursor = 0;
                    gs.get_tree()->get_audio()->play_sfx("ui_click", 0.35f);
                }
                return;
            }
        }
        std::string result = gs._interact.try_interact(gs.player.get(), gs.game_map.get(), gs.ground_items);
        if (!result.empty()) {
            gs._gameplay.flow.mark_reward();
            bool is_relic = (result.find("圣物") != std::string::npos);
            gs._presentation.room_msg = result;
            gs._presentation.room_msg_timer = is_relic ? 3.5f : 2.5f;
            // Q4.3: 拾取音效 + 拾取闪光
            if (result.find("拾取: ") == 0) {
                gs.get_tree()->get_audio()->play_sfx("pickup", 0.55f);
                float px = gs.player->entity.rect.x + gs.player->entity.rect.width/2;
                float py = gs.player->entity.rect.y + gs.player->entity.rect.height/2;
                VFXServer vfx;
                vfx.ring(px, py, 22.0f, is_relic ? Color{255,220,80,220} : Color{255,200,120,200}, 2, 0.35f);
                vfx.spark_burst(px, py, 8, is_relic ? Color{255,240,140,230} : Color{255,220,160,210}, 0.30f);
                for (auto& e : vfx.effects) gs.active_effects.push_back(e);
            }
            // D9: 获得圣物 — shake+freeze 仪式感
            if (is_relic && gs._presentation.combat_juice_on) {
                gs._presentation.trigger_shake(CombatFeelSystem::SHAKE_LIGHT);
                gs._presentation.trigger_freeze(CombatFeelSystem::RELIC_PICKUP);
            }
            if (!gs._interact.shown_relic_hint && !gs.player->relics.empty()) {
                gs._interact.shown_relic_hint = true;
                gs._presentation.room_msg = "按 R 可查看圣物面板";
                gs._presentation.room_msg_timer = 2.5f;
            }
        }
    }
    else if (gs._is_action_just_pressed(input,"inventory")) { gs.inventory_open = true; gs.inventory_cursor = 0;
        gs.get_tree()->get_audio()->play_sfx("ui_click", 0.35f); }  // Q4.5
    else if (gs._is_action_just_pressed(input,"skill_1")) use_skill(0);
    else if (gs._is_action_just_pressed(input,"skill_2")) use_skill(1);
    else if (gs._is_action_just_pressed(input,"skill_3")) use_skill(2);
    else if (gs._is_action_just_pressed(input,"skill_4")) use_skill(3);
}

void PlayerController::player_attack() {
    if (!_scene) return;
    auto& gs = *_scene;
    auto& p = *gs.player;

    if (!p.combat.is_alive) return;

    // 收官: 攻击范围内未点燃的木桶 → 点燃 (近战/武器攻击均可引爆)
    Rectangle attack_zone = p.entity.rect;
    attack_zone.x -= PLAYER_ATTACK_RANGE;       attack_zone.y -= PLAYER_ATTACK_RANGE;
    attack_zone.width  += 2.0f * PLAYER_ATTACK_RANGE;
    attack_zone.height += 2.0f * PLAYER_ATTACK_RANGE;
    gs._try_trigger_barrel_near(attack_zone);

    // ── G9: Weapon-driven attack via WeaponExecutor ──
    if (p.weapon.current_def() && p.weapon.current_def()->type != WeaponType::FIST) {
        _weapon_attack(gs, p);
        return;
    }

    // ── Legacy fallback: fist / no weapon (keeps existing behavior) ──
    if (!p.can_attack(gs.game_time)) return;

    auto* target = find_attack_target(p.entity.rect,
        reinterpret_cast<const std::vector<Monster*>&>(gs.monsters), PLAYER_ATTACK_RANGE);
    if (!target) return;

    gs._gameplay.flow.mark_combat();
    gs._boss.behavior.memory.record_attack();
    gs._boss.replay_mem.melee_hits++;

    p.combo.hit(gs.game_time);
    float combo_mul = p.combo.multiplier();
    bool is_heavy = p.combo.is_heavy();

    int base_dmg = calculate_damage(get_effective_attack(gs.player.get()),
        target->combat.get_effective_defense(p.attack_type));
    int dmg = (int)(base_dmg * combo_mul);

    bool is_crit = false;
    if (gs._presentation.combat_juice_on) {
        if ((rng() % 1000) / 1000.0f < CombatFeelSystem::crit_chance(p.combo.count)) {
            dmg *= CombatFeelSystem::crit_multiplier();
            is_crit = true;
        }
    }
    const char* cb_msg = CombatFeelSystem::combo_message(p.combo.count);
    if (cb_msg && p.combo.count > gs._presentation.last_combo_announced) {
        gs._presentation.room_msg = cb_msg;
        gs._presentation.room_msg_timer = 1.0f;
        gs._presentation.last_combo_announced = p.combo.count;
        gs._presentation.trigger_shake(CombatFeelSystem::SHAKE_COMBO);
        gs._presentation.trigger_freeze(CombatFeelSystem::LIGHT_HIT);
        if (p.combo.count > gs._gameplay.run_stats.combo_max)
            gs._gameplay.run_stats.combo_max = p.combo.count;
    }

    p._last_attack_time = gs.game_time;
    gs.get_tree()->get_audio()->play_sfx("melee");
    gs._boss.dmg_done += dmg;

    if (gs.time_stop_remaining > 0) {
        gs.pending_damage.emplace_back(target, dmg);
    } else {
        CombatCoordinator::apply_attack_damage(target, dmg,
            gs.active_effects, gs.get_tree()->get_audio());
        Color dc = is_crit ? Color{255, 220, 30, 255}
                 : is_heavy ? Color{255, 220, 30, 255}
                 : dmg_color_for(dmg, false, false);
        gs._presentation.damage_floats.push_back({
            target->entity.rect.x + target->entity.rect.width/2,
            target->entity.rect.y,
            (is_crit || is_heavy) ? 0.85f : 0.6f, dmg, dc
        });
        _apply_attack_feedback(gs, p, target, is_crit, is_heavy);
        if (!target->combat.is_alive) _kill_target(gs, target);
    }

    VFXServer vfx;
    float range = is_heavy ? PLAYER_ATTACK_RANGE * 1.5f : PLAYER_ATTACK_RANGE;
    vfx.player_attack(p.entity.rect.x + p.entity.rect.width/2,
                      p.entity.rect.y + p.entity.rect.height/2, range * TILE_SIZE,
                      p.attack_evo);
    for (auto& e : vfx.effects) gs.active_effects.push_back(e);
}

// ── G9: process one attack result (damage float + feedback + kill) ──
void PlayerController::_process_weapon_result(GameScene& gs, Player& p,
    const WeaponAttackResult& r)
{
    if (gs.time_stop_remaining > 0) {
        gs.pending_damage.emplace_back(r.target, r.damage);
        return;
    }
    // P0-B: _resolve_one 已施加 take_damage，此处不再重复
    // 仅保留 VFX (damage float) + kill 处理
    Color dc = r.is_crit ? Color{255, 220, 30, 255}
             : dmg_color_for(r.damage, false, false);
    gs._presentation.damage_floats.push_back({
        r.hit_point.x, r.hit_point.y,
        r.is_crit ? 0.85f : 0.6f, r.damage, dc
    });
    (void)p;
    if (r.is_killing_blow) _kill_target(gs, r.target);
}

// ── G9: Weapon-driven attack (new path) ──
void PlayerController::_weapon_attack(GameScene& gs, Player& p) {
    // Read stage BEFORE execute advances the combo
    int stage = p.weapon.combo_index();

    std::vector<Monster*> mlist;
    for (auto& m : gs.monsters) mlist.push_back(m.get());
    auto results = WeaponExecutor::execute(
        &p, mlist, gs.game_time, gs.get_tree()->get_audio(),
        &gs.projectiles, gs.game_map.get());

    // Feedback always, even on whiff
    p.combo.hit(gs.game_time);
    p._last_attack_time = gs.game_time;

    // Stage-specific shake: stage0=light, stage1=medium, stage2=heavy
    static const float stage_shake[] = {3.0f, 6.0f, 14.0f};
    float s = (stage >= 0 && stage < 3) ? stage_shake[stage] : 3.0f;
    gs._presentation.trigger_shake(s);

    // ══════════════════════════════════════════════════════
    // G9: Per-weapon per-stage VFX (comprehensive)
    // ══════════════════════════════════════════════════════
    VFXServer vfx;
    float px = p.entity.rect.x + p.entity.rect.width/2;
    float py = p.entity.rect.y + p.entity.rect.height/2;
    const WeaponDef* def = p.weapon.current_def();
    WeaponType wt = def ? def->type : WeaponType::FIST;

    switch (wt) {
    case WeaponType::FIST: {
        vfx.ring(px, py, 28.0f, {200,200,200,180}, 2, 0.25f);
        vfx.spark_burst(px, py, 5, {220,220,200,180}, 0.22f);
        break;
    }
    case WeaponType::DAGGER: {
        Color dc = {220,180,80,230};
        vfx.slash_arc(px, py, p.direction, 50.0f, dc, 0.30f);
        vfx.spark_burst(px, py, 4, dc, 0.25f);
        if (stage >= 1) {
            vfx.slash_arc(px, py, p.direction, 55.0f, {240,200,60,220}, 0.32f);
            vfx.spark_burst(px + 20, py - 10, 5, {240,200,80,200}, 0.28f);
        }
        if (stage >= 2) {
            float tx = px + (p.direction == Direction::RIGHT ? 60 : p.direction == Direction::LEFT ? -60 : 0);
            float ty = py + (p.direction == Direction::DOWN ? 60 : p.direction == Direction::UP ? -60 : 0);
            vfx.beam(px, py, tx, ty, {255,150,40,230}, 0.30f);
            for (auto& r : results) {
                vfx.ring(r.hit_point.x, r.hit_point.y, 22.0f, {255,140,30,220}, 2, 0.35f);
                vfx.explosion(r.hit_point.x, r.hit_point.y, 24.0f, {255,160,40,200}, 8, 0.32f);
            }
        }
        break;
    }
    case WeaponType::SWORD: {
        if (stage == 0) {
            float fx = px + (p.direction == Direction::RIGHT ? 32 : p.direction == Direction::LEFT ? -32 : 0);
            float fy = py + (p.direction == Direction::DOWN ? 32 : p.direction == Direction::UP ? -32 : 0);
            vfx.shockwave(px, py, 30.0f, {180,180,140,180}, 2, 0.30f);
            vfx.slash_arc(fx, fy, p.direction, 60.0f, {200,200,100,230}, 0.35f);
            vfx.smoke_puff(px, py, 16.0f, {140,130,100,120}, 4, 0.40f);
            // G10.3-B2: 命中点像素反馈 — 攻击→命中→像素爆点
            for (auto& r : results) {
                vfx.hit_flash(r.hit_point.x, r.hit_point.y, 14.0f);
                vfx.spark_burst(r.hit_point.x, r.hit_point.y, 4, {230,230,200,220}, 0.22f);
            }
        } else if (stage == 1) {
            vfx.slash_arc(px, py, p.direction, 70.0f, {220,220,120,220}, 0.38f);
            vfx.slash_arc(px + 15, py - 10, p.direction, 55.0f, {200,200,80,200}, 0.32f);
            vfx.spark_burst(px, py, 10, {200,200,100,220}, 0.35f);
            for (auto& r : results) {
                vfx.hit_flash(r.hit_point.x, r.hit_point.y, 18.0f);
                vfx.spark_burst(r.hit_point.x, r.hit_point.y, 6, {240,220,120,220}, 0.25f);
            }
        } else {
            vfx.shockwave(px, py, 80.0f, {220,200,80,200}, 4, 0.55f);
            vfx.explosion(px, py, 36.0f, {240,220,100,220}, 14, 0.45f);
            vfx.smoke_puff(px, py, 24.0f, {160,140,100,140}, 6, 0.50f);
            vfx.flash(px, py, 20.0f, {255,240,200,200}, 0.12f);
            for (auto& r : results) {
                vfx.ring(r.hit_point.x, r.hit_point.y, 28.0f, {240,200,60,200}, 3, 0.40f);
                vfx.explosion(r.hit_point.x, r.hit_point.y, 20.0f, {255,220,120,230}, 8, 0.30f);
            }
        }
        break;
    }
    case WeaponType::NUNCHAKU: {
        Color nc = {220,160,80,220};
        if (stage == 0) {
            vfx.slash_arc(px, py, p.direction, 90.0f, nc, 0.30f);
            vfx.spark_burst(px, py, 6, nc, 0.28f);
        } else if (stage == 1) {
            vfx.slash_arc(px, py, p.direction, 95.0f, {240,180,60,220}, 0.33f);
            vfx.slash_arc(px - 20, py + 10, (Direction)(((int)p.direction + 2) % 4),
                70.0f, {200,140,50,180}, 0.28f);
            vfx.ring(px, py, 40.0f, nc, 1, 0.30f);
        } else {
            vfx.play_recipe("skill_chain_lightning", px, py, p.direction, 0, 0, 3);
            for (auto& r : results) {
                vfx.ring(r.hit_point.x, r.hit_point.y, 20.0f, {240,160,40,200}, 2, 0.30f);
            }
        }
        break;
    }
    case WeaponType::SPEAR: {
        // Stage1: thick blue beam + ring at hit
        // Stage2: beam + explosion + double ring
        // Stage3: shockwave at origin (rapid hits handled by tick_specials)
        vfx.play_recipe("skill_slash", px, py, p.direction, 0, 0, stage + 1);
        Color sc = {80,170,255,255};
        for (auto& r : results) {
            // Double beam for thickness visibility
            vfx.beam(px, py, r.hit_point.x, r.hit_point.y, sc, 0.40f);
            vfx.beam(px - 2, py - 2, r.hit_point.x - 2, r.hit_point.y - 2,
                {60,150,240,200}, 0.38f);
            vfx.ring(r.hit_point.x, r.hit_point.y, 20.0f, sc, 2, 0.32f);
            vfx.spark_burst(r.hit_point.x, r.hit_point.y, 4, {80,170,255,230}, 0.28f);
            if (stage >= 1) {
                vfx.explosion(r.hit_point.x, r.hit_point.y, 22.0f, {80,150,255,200}, 8, 0.32f);
                vfx.lightning(px, py, r.hit_point.x, r.hit_point.y, 3,
                    {80,160,255,220}, 0.25f);
            }
            if (stage >= 2) {
                // Stage-3 initiation: shockwave + flash at origin
                vfx.shockwave(px, py, 50.0f, {80,160,255,180}, 3, 0.40f);
                vfx.flash(px, py, 16.0f, {100,180,255,180}, 0.10f);
            }
        }
        break;
    }
    case WeaponType::CROSSBOW: {
        if (stage == 2) {
            vfx.play_recipe("boss_cone_attack", px, py, p.direction, 0, 0, 3);
            vfx.ring(px, py, 24.0f, {255,180,50,200}, 2, 0.35f);
            vfx.shockwave(px, py, 60.0f, {255,140,30,150}, 2, 0.40f);
        } else {
            vfx.play_recipe("skill_chain_lightning", px, py, p.direction, 0, 0, stage + 1);
        }
        break;
    }
    default: break;
    }
    for (auto& e : vfx.effects) gs.active_effects.push_back(e);
    vfx.effects.clear();

    if (results.empty()) return; // whiff: VFX + shake played

    gs._gameplay.flow.mark_combat();
    gs._boss.behavior.memory.record_attack();
    gs._boss.replay_mem.melee_hits++;

    for (auto& r : results) {
        gs._boss.dmg_done += r.damage;
        _process_weapon_result(gs, p, r);
    }
}

// ── Helper: apply hit feedback (shake, freeze, knockback, kill flash) ──
void PlayerController::_apply_attack_feedback(GameScene& gs, Player& p,
    Monster* target, bool is_crit, bool is_heavy)
{
    if (is_heavy && target->combat.is_alive) {
        float dx = target->entity.rect.x - p.entity.rect.x;
        float dy = target->entity.rect.y - p.entity.rect.y;
        float len = sqrtf(dx*dx + dy*dy);
        if (len > 0) {
            float knock = is_crit ? 36.0f : 24.0f;
            clamp_displacement(target->entity, dx / len * knock, dy / len * knock, gs.game_map.get());
        }
        gs._presentation.trigger_shake(is_crit ? 16.0f : CombatFeelSystem::SHAKE_HEAVY);
        gs._presentation.trigger_freeze(is_crit ? CombatFeelSystem::CRITICAL_HIT
                                                : CombatFeelSystem::HEAVY_HIT);
    } else if (is_crit && gs._presentation.combat_juice_on) {
        gs._presentation.trigger_shake(CombatFeelSystem::SHAKE_MEDIUM);
        gs._presentation.trigger_freeze(CombatFeelSystem::LIGHT_HIT);
    }
    // Kill flash
    if (!target->combat.is_alive && gs._presentation.combat_juice_on) {
        Effect flash;
        flash.kind = "flash";
        flash.world_x = target->entity.rect.x;
        flash.world_y = target->entity.rect.y;
        flash.radius = target->entity.rect.width * 1.5f;
        flash.duration = CombatFeelSystem::KILL_SLOWMO;
        flash.elapsed = 0;
        flash.color = {255, 255, 255, 180};
        gs.active_effects.push_back(flash);
        gs._presentation.trigger_freeze(CombatFeelSystem::KILL_SLOWMO);
    }
}

// ── Helper: kill a target and remove from monster list ──
void PlayerController::_kill_target(GameScene& gs, Monster* target) {
    gs._on_monster_killed(target);
    auto it = std::find_if(gs.monsters.begin(), gs.monsters.end(),
        [target](auto& m) { return m.get() == target; });
    if (it != gs.monsters.end()) gs.monsters.erase(it);
}

void PlayerController::use_skill(int index) {
    if (!_scene) return;
    auto& gs = *_scene;

    gs._boss.behavior.memory.record_skill();
    bool was_heavy = gs.player->consume_heavy_combo();

    if (index >= 0 && index < (int)gs.player->skills.active_skills.size()
        && dynamic_cast<TheWorldSkill*>(gs.player->skills.active_skills[index].get()))
        gs._tw_evo_level = gs.player->skills.active_skills[index]->evolution_level;

    CombatCoordinator::use_skill(index, gs.player.get(), gs.monsters, gs.game_map.get(),
        gs.active_effects, gs.get_tree()->get_audio(), gs.game_time,
        gs.time_stop_remaining, gs.pending_damage, was_heavy);

    if (was_heavy) {
        gs._presentation.trigger_shake(10.0f);
        gs._presentation.trigger_freeze(0.06f);
        gs._presentation.room_msg = "SKILL CHAIN!";
        gs._presentation.room_msg_timer = 0.8f;
    }
    if (!was_heavy) gs.player->combo.timer = ComboState::WINDOW;

    // G1: 每次技能使用后检查进化条件
    SkillEvolutionManager::check_unlock(gs.player.get());

    auto it = gs.monsters.begin();
    while (it != gs.monsters.end()) {
        if (!(*it)->combat.is_alive) {
            gs._on_monster_killed((*it).get());
            it = gs.monsters.erase(it);
        } else ++it;
    }
}
