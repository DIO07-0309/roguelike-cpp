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
#include "combat_feel.h"
#include "flow_director.h"
#include "input_map.h"
#include "event_system.h"
#include "config.h"
#include "scene_tree.h"
#include "audio_server.h"
#include <cmath>
#include <algorithm>

void PlayerController::tick(float dt) {
    if (!_scene || !_scene->player || !_scene->player->combat.is_alive) return;

    // ── 移动 ──
    auto& gs = *_scene;
    if (!gs.inventory_open && !gs._is_event_running() && !gs._dialogue.active && !gs._quest_log_open) {
        Vector2 move = gs.player->handle_input(gs.get_tree()->get_input());
        auto& e = gs.player->entity;
        float speed_mul = (gs._tw_speed_boost > 0) ? 1.25f : 1.0f;
        float s = get_effective_speed(gs.player.get()) * speed_mul * dt;
        e.position.x += move.x * s; e.sync_rect();
        if (!gs.game_map->is_rect_walkable(e.rect)) { e.position.x -= move.x * s; e.sync_rect(); }
        e.position.y += move.y * s; e.sync_rect();
        if (!gs.game_map->is_rect_walkable(e.rect)) { e.position.y -= move.y * s; e.sync_rect(); }

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
            if (dmg_taken > 0) gs._boss.dmg_taken += dmg_taken;
            if (dmg_taken > 0 && gs.player->combat.is_alive) {
                gs._presentation.damage_floats.push_back({
                    gs.player->entity.rect.x + gs.player->entity.rect.width/2,
                    gs.player->entity.rect.y - 12, 0.6f, dmg_taken,
                    dmg_taken >= 30 ? Color{255, 60, 30, 255} : Color{255, 80, 80, 255}
                });
                float shake = dmg_taken >= 30 ? 12.0f : dmg_taken > 15 ? 5.0f : 2.0f;
                gs._presentation.trigger_shake(shake);
                if (dmg_taken >= 20) gs._presentation.trigger_freeze(0.05f);
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
        if (input.is_action_just_pressed("inventory") || input.is_action_just_pressed("cancel"))
            gs.inventory_open = false;
        else if (input.is_action_just_pressed("move_up"))
            gs.inventory_cursor = std::max(0, gs.inventory_cursor - 1);
        else if (input.is_action_just_pressed("move_down"))
            gs.inventory_cursor = std::min(std::max(0, (int)gs.player->inventory.items.size() - 1), gs.inventory_cursor + 1);
        else if (IsKeyPressed(KEY_X))
            { gs.player->inventory.equip(gs.inventory_cursor, gs.player.get()); gs.inventory_cursor = std::min(gs.inventory_cursor, std::max(0, (int)gs.player->inventory.items.size() - 1)); }
        else if (IsKeyPressed(KEY_U))
            { gs.player->inventory.use_item(gs.inventory_cursor, gs.player.get()); gs.inventory_cursor = std::min(gs.inventory_cursor, std::max(0, (int)gs.player->inventory.items.size() - 1)); }
        else if (IsKeyPressed(KEY_D))
            { gs.player->inventory.remove(gs.inventory_cursor); gs.inventory_cursor = std::min(gs.inventory_cursor, std::max(0, (int)gs.player->inventory.items.size() - 1)); }
        return;
    }

    if (input.is_action_just_pressed("attack")) player_attack();
    else if (input.is_action_just_pressed("pickup")) {
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
        std::string result = gs._interact.try_interact(gs.player.get(), gs.game_map.get(), gs.ground_items);
        if (!result.empty()) {
            gs._gameplay.flow.mark_reward();
            bool is_relic = (result.find("圣物") != std::string::npos);
            gs._presentation.room_msg = result;
            gs._presentation.room_msg_timer = is_relic ? 3.5f : 2.5f;
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
    else if (input.is_action_just_pressed("inventory")) { gs.inventory_open = true; gs.inventory_cursor = 0; }
    else if (input.is_action_just_pressed("skill_1")) use_skill(0);
    else if (input.is_action_just_pressed("skill_2")) use_skill(1);
    else if (input.is_action_just_pressed("skill_3")) use_skill(2);
    else if (input.is_action_just_pressed("skill_4")) use_skill(3);
}

void PlayerController::player_attack() {
    if (!_scene) return;
    auto& gs = *_scene;
    auto& p = *gs.player;

    if (!p.combat.is_alive) return;
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
        float crit_roll = (float)(rng() % 1000) / 1000.0f;
        if (crit_roll < CombatFeelSystem::crit_chance(p.combo.count)) {
            dmg *= CombatFeelSystem::crit_multiplier();
            is_crit = true;
        }
    }
    const char* cb_msg = CombatFeelSystem::combo_message(p.combo.count);
    if (cb_msg && p.combo.count > gs._presentation.last_combo_announced) {
        gs._presentation.room_msg = cb_msg;
        gs._presentation.room_msg_timer = 1.0f;
        gs._presentation.last_combo_announced = p.combo.count;
        if (p.combo.count > gs._gameplay.run_stats.combo_max)
            gs._gameplay.run_stats.combo_max = p.combo.count;
    }

    p._last_attack_time = gs.game_time;
    gs.get_tree()->get_audio()->play_sfx("melee");
    gs._boss.dmg_done += dmg;

    if (gs.time_stop_remaining > 0) {
        gs.pending_damage.emplace_back(target, dmg);
    } else {
        CombatCoordinator::apply_attack_damage(target, dmg, gs.active_effects, gs.get_tree()->get_audio());
        Color dc = is_crit ? Color{255, 220, 30, 255}
                 : is_heavy ? Color{255, 220, 30, 255}
                 : dmg_color_for(dmg, false, false);
        gs._presentation.damage_floats.push_back({
            target->entity.rect.x + target->entity.rect.width/2,
            target->entity.rect.y,
            (is_crit || is_heavy) ? 0.85f : 0.6f, dmg, dc
        });
        if (is_heavy && target->combat.is_alive) {
            float dx = target->entity.rect.x - p.entity.rect.x;
            float dy = target->entity.rect.y - p.entity.rect.y;
            float len = sqrtf(dx*dx + dy*dy);
            if (len > 0) {
                float knock = is_crit ? 36.0f : 24.0f;
                target->entity.position.x += dx / len * knock;
                target->entity.position.y += dy / len * knock;
                target->entity.sync_rect();
            }
            gs._presentation.trigger_shake(is_crit ? 16.0f : CombatFeelSystem::SHAKE_HEAVY);
            gs._presentation.trigger_freeze(is_crit ? CombatFeelSystem::CRITICAL_HIT : CombatFeelSystem::HEAVY_HIT);
        } else if (is_crit && gs._presentation.combat_juice_on) {
            gs._presentation.trigger_shake(CombatFeelSystem::SHAKE_MEDIUM);
            gs._presentation.trigger_freeze(CombatFeelSystem::LIGHT_HIT);
        }
        if (!target->combat.is_alive && gs._presentation.combat_juice_on) {
            Effect flash;
            flash.kind = "flash"; flash.world_x = target->entity.rect.x; flash.world_y = target->entity.rect.y;
            flash.radius = target->entity.rect.width * 1.5f; flash.duration = CombatFeelSystem::KILL_SLOWMO;
            flash.elapsed = 0; flash.color = {255, 255, 255, 180};
            gs.active_effects.push_back(flash);
            gs._presentation.trigger_freeze(CombatFeelSystem::KILL_SLOWMO);
        }
        if (!target->combat.is_alive) {
            gs._on_monster_killed(target);
            auto it = std::find_if(gs.monsters.begin(), gs.monsters.end(),
                [&](auto& m) { return m.get() == target; });
            if (it != gs.monsters.end()) gs.monsters.erase(it);
        }
    }

    VFXServer vfx;
    float range = is_heavy ? PLAYER_ATTACK_RANGE * 1.5f : PLAYER_ATTACK_RANGE;
    vfx.player_attack(p.entity.rect.x + p.entity.rect.width/2,
                      p.entity.rect.y + p.entity.rect.height/2, range * TILE_SIZE);
    for (auto& e : vfx.effects) gs.active_effects.push_back(e);
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

    auto it = gs.monsters.begin();
    while (it != gs.monsters.end()) {
        if (!(*it)->combat.is_alive) {
            gs._on_monster_killed((*it).get());
            it = gs.monsters.erase(it);
        } else ++it;
    }
}
