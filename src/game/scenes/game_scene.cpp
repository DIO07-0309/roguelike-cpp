#include "game_scene.h"
#include "title_scene.h"
#include "death_scene.h"
#include "victory_scene.h"
#include "config.h"
#include "boss.h"
#include "skill.h"
#include "combat_system.h"
#include "dungeon_generator.h"
#include "scene_tree.h"
#include "input_map.h"
#include "core/logger.h"
#include "save/save_manager.h"
#include "audio_server.h"
#include "floor_config.h"
#include "event_system.h"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstring>

// 字体指针 (在 main.cpp 中初始化)
extern Font g_font;
extern Font g_font_small;
extern bool g_font_loaded;

// ============================================================
// C1: 体验打磨 — 伤害数字/震动/冻结 辅助函数
// ============================================================
// _dmg_color_for moved to PresentationSystemDirector (dmg_color_for)
static Color _olddmg_color_for(int dmg, bool is_magic, bool is_poison) {
    if (is_poison) return {40, 220, 80, 255};
    if (is_magic)  return {160, 120, 255, 255};
    if (dmg >= 50) return {255, 220, 50, 255};  // 大数字偏黄
    return {255, 255, 255, 255};
}

// _trigger_shake / _trigger_freeze moved to PresentationSystemDirector

// ============================================================
// D4 Step2: Event Presentation impl
// ============================================================
static int _event_option_count(EventType et) {
    switch (et) {
        case EventType::BLOOD_RITUAL: return 2; // YES/NO
        case EventType::PRISONER:     return 2;
        case EventType::CURSED_ROOM:  return 2;
        case EventType::ALTAR_CHOICE: return 3;
        case EventType::MERCHANT:     return 3;
        default: return 0; // confirm-only
    }
}

static Color _event_anim_color(EventType et) {
    switch (et) {
        case EventType::AMBUSH:         return {255, 40, 30, 255};
        case EventType::CURSED_ROOM:    return {140, 40, 200, 255};
        case EventType::BLOOD_RITUAL:   return {200, 30, 30, 255};
        case EventType::LOST_CAMP:      return {255, 200, 60, 255};
        case EventType::TREASURE_GUARD: return {255, 200, 30, 255};
        case EventType::MERCHANT:       return {255, 220, 100, 255};
        case EventType::ALTAR_CHOICE:   return {200, 200, 255, 255};
        case EventType::STATUE:         return {180, 160, 220, 255};
        default: return {255, 200, 50, 255};
    }
}

void GameScene::_start_event_presentation(EventType et) {
    _event_ui.active = true;
    _event_ui.phase = EventPhase::ENTER;
    _event_ui.current = et;
    _event_ui.timer = 1.2f;
    _event_ui.fade = 0.0f;
    _event_ui.selected = 0;
    _event_ui.option_count = _event_option_count(et);
    _event_ui.desc_text = "";  // filled in DESC phase
    _event_ui.reward_text = "";
    _event_ui.complete_text = "";
    _event_ui.anim_color = _event_anim_color(et);
}

void GameScene::_tick_event_ui(float dt) {
    auto& ui = _event_ui;
    if (!ui.active) return;

    // fade in/out
    if (ui.fade < 1.0f) ui.fade = std::min(1.0f, ui.fade + dt * 3.0f);
    ui.timer -= dt;

    switch (ui.phase) {
    case EventPhase::ENTER:
        if (ui.timer <= 0) {
            ui.phase = EventPhase::DESC;
            ui.timer = 0;
            // Pick desc text from pool
            auto pool = event_text_pool((int)ui.current - 1);
            if (!pool.empty()) ui.desc_text = pool[rng() % pool.size()];
        }
        break;
    case EventPhase::ANIM:
        if (ui.timer <= 0) {
            ui.phase = EventPhase::REWARD;
            ui.timer = 1.5f;
            // Execute event here
            DungeonEvent ev; ev.type = ui.current; ev.triggered = false;
            ui.reward_text = execute_event(ev, player.get(), current_floor);
            // D4 Step5: 事件WorldFlag
            if (ev.type == EventType::CURSED_ROOM && ui.selected == 0) {
                _gameplay.world_state.set(WorldFlag::Accepted_Curse);
                // D4 Step5.3: 诅咒→祭司信任-1, 所有NPC恐惧+1
                _gameplay.rels.add_trust(70, -1); _gameplay.rels.add_fear(70, 1);
            }
            else if (ev.type == EventType::BLOOD_RITUAL && ui.selected == 0) {
                _gameplay.world_state.set(WorldFlag::Blood_Ritual);
                // D4 Step5.3: 血祭→祭司恐惧+2腐化+1
                _gameplay.rels.apply_reward(RR_BLOOD_RITUAL);
            }
            if (ui.reward_text.size() > 6 && ui.reward_text.substr(0, 6) == "RELIC:") {
                ui.complete_text = "获得圣物:" + ui.reward_text.substr(6);
            } else if (ui.reward_text.size() > 4 && ui.reward_text.substr(0, 4) == "MSG:") {
                ui.complete_text = ui.reward_text.substr(4);
            } else {
                ui.complete_text = ui.reward_text;
            }
        }
        break;
    case EventPhase::REWARD:
        if (ui.timer <= 0) {
            ui.phase = EventPhase::COMPLETE;
            ui.timer = 2.0f;
            ui.fade = 0.0f; // restart fade for complete
        }
        break;
    case EventPhase::COMPLETE:
        if (ui.timer <= 0) {
            ui.active = false;
            ui.phase = EventPhase::INACTIVE;
            game_map->event_triggered = true;
        }
        break;
    default: break;
    }
}

void GameScene::_draw_event_ui(int sw, int sh) {
    auto& ui = _event_ui;
    if (!ui.active) return;

    float alpha = ui.fade;
    auto ename = event_type_name(ui.current);
    Color gold = {255, 210, 50, (unsigned char)(255 * alpha)};
    Color white = {240, 240, 240, (unsigned char)(230 * alpha)};
    Color bg = {0, 0, 0, (unsigned char)(140 * alpha)};

    switch (ui.phase) {
    case EventPhase::ENTER: {
        // 中央大标题
        DrawRectangle(0, 0, sw, sh, bg);
        GameRenderer::draw_glow_text("══════════════", sw/2.0f, sh/2.0f - 40, 22, gold, true);
        char title_buf[64]; snprintf(title_buf, sizeof(title_buf), "EVENT");
        GameRenderer::draw_glow_text(title_buf, sw/2.0f, sh/2.0f - 12, 36, gold, true);
        GameRenderer::draw_glow_text(ename, sw/2.0f, sh/2.0f + 28, 22, white, true);
        GameRenderer::draw_glow_text("══════════════", sw/2.0f, sh/2.0f + 52, 22, gold, true);
        break;
    }
    case EventPhase::DESC:
    case EventPhase::CHOICE: {
        // 事件窗口
        float pw = 500, ph = 260;
        Rectangle pr = {sw/2.0f - pw/2, sh/2.0f - ph/2, pw, ph};
        GameRenderer::draw_panel(pr, ename, {15,15,35,(unsigned char)(220*alpha)});

        // 描述文字
        if (g_font_loaded && !ui.desc_text.empty())
            DrawTextEx(g_font_small, ui.desc_text.c_str(), {pr.x+30, pr.y+50}, 16, 1, white);

        // 选项 (CHOICE phase)
        if (ui.phase == EventPhase::CHOICE) {
            float oy = pr.y + 130;
            for (int i = 0; i < ui.option_count; i++) {
                const char* choice_text = "?";
                if (ui.current == EventType::BLOOD_RITUAL || ui.current == EventType::PRISONER
                    || ui.current == EventType::CURSED_ROOM) {
                    choice_text = (i == 0) ? "▶ YES" : "  NO";
                } else if (ui.current == EventType::ALTAR_CHOICE) {
                    const char* opts[] = {"▶ 攻击强化", "  生命恢复", "  技能进化"};
                    choice_text = (i < 3) ? opts[i] : "?";
                } else if (ui.current == EventType::MERCHANT) {
                    const char* opts[] = {"▶ 购买药水", "  攻击祝福", "  随机圣物"};
                    choice_text = (i < 3) ? opts[i] : "?";
                }
                Color cc = (i == ui.selected) ? gold : Color{180,180,180,(unsigned char)(200*alpha)};
                DrawTextEx(g_font_small, choice_text, {pr.x+40, oy + i*28}, 16, 1, cc);
            }
        }
        // 提示
        const char* hint = (ui.phase == EventPhase::DESC)
            ? "[ENTER]继续  [ESC]离开"
            : "[←→]选择  [ENTER]确认  [ESC]取消";
        DrawTextEx(g_font_small, hint,
                   {pr.x + (pw - MeasureTextEx(g_font_small,hint,14,1).x)/2, pr.y+ph-28},
                   14, 1, Color{140,140,160,(unsigned char)(180*alpha)});
        break;
    }
    case EventPhase::ANIM: {
        // 简单动画: 彩色脉冲环
        DrawRectangle(0, 0, sw, sh, bg);
        float pulse = sinf(ui.timer * 8) * 30;
        float r = 80 + pulse;
        DrawRing({sw/2.0f, sh/2.0f}, r*0.7f, r, 0, 360, 24,
                 Color{ui.anim_color.r, ui.anim_color.g, ui.anim_color.b, (unsigned char)(180*alpha)});
        DrawRing({sw/2.0f, sh/2.0f}, r*0.4f, r*0.5f, 0, 360, 16,
                 Color{ui.anim_color.r, ui.anim_color.g, ui.anim_color.b, (unsigned char)(120*alpha)});
        break;
    }
    case EventPhase::REWARD: {
        DrawRectangle(0, 0, sw, sh, bg);
        GameRenderer::draw_glow_text("EVENT COMPLETE", sw/2.0f, sh/2.0f - 30, 28, gold, true);
        if (!ui.complete_text.empty())
            GameRenderer::draw_glow_text(ui.complete_text.c_str(), sw/2.0f, sh/2.0f + 20, 20, white, true);
        break;
    }
    case EventPhase::COMPLETE: {
        DrawRectangle(0, 0, sw, sh, {0,0,0,(unsigned char)(120*alpha)});
        GameRenderer::draw_glow_text("EVENT COMPLETE", sw/2.0f, sh/2.0f - 20, 24,
                                      {255,210,50,(unsigned char)(200*alpha)}, true);
        if (!ui.complete_text.empty())
            GameRenderer::draw_glow_text(ui.complete_text.c_str(), sw/2.0f, sh/2.0f + 18, 18,
                                          {240,240,240,(unsigned char)(180*alpha)}, true);
        break;
    }
    default: break;
    }
}

// ============================================================
// GameScene 实现
// ============================================================
void GameScene::_ready() {
    // D6: 场景入树时绑定Director (new_game/load_saved_game都会走这里)
    _flow.bind(this);
    _player_ctrl.bind(this);
}

void GameScene::new_game() {
    player = std::make_unique<Player>(TILE_SIZE * 2, TILE_SIZE * 2,
        PLAYER_SPEED, PLAYER_MAX_HP, PLAYER_ATTACK, PLAYER_PDEF, PLAYER_MDEF);
    auto sk = random_active_skill();
    player->skills.learn(std::move(sk));

    // D4.6 Step5: 加载Meta存档 + 重置本局统计
    g_meta.load();
    _gameplay.run_stats = RunSummary{};
    player->skills.apply_all_passives(player.get());
    current_floor = 1;
    max_unlocked_floor = 1;
    enter_floor(1);
}

void GameScene::load_saved_game(int floor, int max_f, std::unique_ptr<Player> p,
                                 uint32_t seed,
                                 const std::vector<bool>& special_triggered,
                                 const std::vector<bool>& special_discovered) {
    player = std::move(p);
    current_floor = floor;
    max_unlocked_floor = max_f;
    enter_floor(floor, seed);

    // B8: 地图生成完成后恢复特殊房间触发状态
    if (game_map && !special_triggered.empty()) {
        auto& rooms = game_map->special_rooms;
        size_t n = std::min(rooms.size(), special_triggered.size());
        for (size_t i = 0; i < n; i++)
            rooms[i].triggered = special_triggered[i];
        LOG_INFO("[ROOM] 恢复触发状态: %zu/%zu", n, special_triggered.size());
    }
    // B10: 恢复发现状态
    if (game_map && !special_discovered.empty()) {
        auto& rooms = game_map->special_rooms;
        size_t n = std::min(rooms.size(), special_discovered.size());
        for (size_t i = 0; i < n; i++)
            rooms[i].discovered = special_discovered[i];
        LOG_INFO("[ROOM] 恢复发现状态: %zu/%zu", n, special_discovered.size());
    }
    // B13: Relic 不再跨层 — 读档不恢复圣物 (每层重新Build)
}

void GameScene::enter_floor(int floor, uint32_t seed) {
    current_floor = floor;
    game_time = 0;
    ground_items.clear();
    inventory_open = false;
    inventory_cursor = 0;
    stairs_active = false;
    active_effects.clear();
    time_stop_remaining = 0;
    pending_damage.clear();
    monsters.clear();
    _presentation.room_msg.clear();
    _presentation.room_msg_timer = 0.0f;

    // B13: Relic 每层重建 — 新楼层清空旧圣物
    player->relics.clear();

    // D4 Step1: 重置事件状态
    if (game_map) {
        game_map->event_room_index = -1;
        game_map->event_triggered = false;
    }
    _presentation.boss_intro_text.clear();
    _presentation.boss_modifier_text.clear();
    _boss.arena.clear();  // D5 Step4: 新楼层清除Arena

    // B8: seed=0 → 新楼层随机生成; seed!=0 → 读档恢复
    if (seed != 0) {
        _dungeon_seed = seed;
    } else {
        _dungeon_seed = static_cast<uint32_t>(rng());
    }

    // D1: FloorConfig — 统一难度/敌人池/特殊房间/BGM/剧情
    const FloorConfig* fcfg = get_floor_config(floor);

    // 生成地牢 (B8: seed 驱动; D1: special_room_count 从配置读)
    DungeonGenerator gen(MAP_WIDTH, MAP_HEIGHT, TILE_SIZE);
    game_map = gen.generate(_dungeon_seed, fcfg->special_room_count, fcfg->arena_density);
    auto rooms = gen.get_room_centers();

    // 放置玩家
    if (!rooms.empty()) {
        auto [tx, ty] = rooms[0];
        auto [px, py] = game_map->tile_to_pixel(tx, ty);
        player->entity.position = {px, py};
        player->entity.sync_rect();
    }

    // 放置怪物
    if (is_boss_floor(floor)) {
        auto pos = rooms.back();
        boss_floor = floor;
        auto info = get_boss_info(floor);
        boss_intro_title = info.title;
        boss_intro_lore = info.lore;
        boss_intro_skills = info.skills_text;
        boss_intro_color = info.color;
        state = GameState::BOSS_INTRO;

        // D4 Step5.5: BossNarrative覆盖intro对话
        BuildType bt = calculate_build(player.get()).identify();
        const BossDialogue* bd = _boss.narrative.find_intro(
            floor, _gameplay.world_state, bt, _gameplay.rels, _gameplay.story);
        if (bd && bd->intro) {
            _presentation.boss_intro_text = bd->intro;
        }
    } else {
        FloorManager::spawn_floor_monsters(floor, game_map.get(), monsters, rooms);
        state = GameState::PLAYING;
    }

    stairs_pos = rooms.back();

    // D4 Step1: 非Boss层生成动态事件 (25%概率, 休息层50%)
    if (!fcfg->is_boss && rooms.size() > 3) {
        float ev_chance = fcfg->is_rest_floor ? 0.50f : 0.25f;
        ChapterConfig ch = *get_chapter_config(fcfg->chapter);
        if ((float)(rng() % 1000) / 1000.0f < ev_chance) {
            DungeonEvent ev = generate_event(floor, ch, rng);
            int ev_room = 1 + (int)(rng() % ((uint32_t)rooms.size() - 2));
            auto [etx, ety] = rooms[ev_room];
            game_map->event_room_index = ev_room;
            game_map->event_tile_x = etx;
            game_map->event_tile_y = ety;
            game_map->event_triggered = false;
            game_map->event_type = ev.type;  // 存储事件类型
        }
    }

    // D4 Step4: NPC 生成 (配置中有NPC的楼层)
    if (!fcfg->is_boss) _spawn_floor_npcs(floor, rooms);

    // D4 Step5.1: StoryDirector 楼层推进
    _gameplay.story.enter_floor(floor);
    // D4 Step5.2: QuestManager 楼层推进
    _gameplay.quest_mgr.set_relationship_system(&_gameplay.rels);
    _gameplay.quest_mgr.update(_gameplay.world_state, _gameplay.story);

    // B11: blood_charm — 进入新楼层时使用有效最大生命
    player->combat.current_hp = get_effective_max_hp(player.get());
    player->reset_attack_timers();

    // D4 Step3: 楼层入场演出 (非Boss层, 首次进入, new_game)
    if (!fcfg->is_boss && !_gameplay.narr_state.floor_intro_played[floor - 1]) {
        _gameplay.narr_state.floor_intro_played[floor - 1] = true;
        _presentation.floor_intro_active = true;
        _presentation.floor_intro_timer = 2.0f;
        _presentation.floor_intro_fade = 0.0f;
        _presentation.floor_intro_floor = floor;
        _gameplay.narr_state.narration_timer = 25.0f + (float)(rng() % 15);
    }
    // D4 Step3: 章节入场 (每新章节开始)
    if (!fcfg->is_boss && fcfg->chapter != _presentation.chapter_intro_ch) {
        _presentation.chapter_intro_active = true;
        _presentation.chapter_intro_timer = 3.0f;
        _presentation.chapter_intro_ch = fcfg->chapter;
        _presentation.floor_intro_active = false;  // chapter intro overrides floor intro
    }

    // D1: BGM + 剧情从 FloorConfig 读取
    _pending_bgm = fcfg->bgm;

    if (fcfg->story_msg && !_presentation.floor_intro_active && !_presentation.chapter_intro_active) {
        _presentation.room_msg = fcfg->story_msg;
        _presentation.room_msg_timer = 3.0f;
    }

    if (fcfg->is_boss) {
        LOG_INFO("进入第%d层 (Boss: %s) - HP:%d ATK:%d DEF:%d",
            floor, get_boss_info(floor).name.c_str(),
            get_boss_info(floor).max_hp, get_boss_info(floor).attack,
            get_boss_info(floor).pdef);
    } else {
        LOG_INFO("进入第%d层 [%s] - %d只怪物, HPx%.2f ATKx%.2f",
            floor, fcfg->chapter_label, (int)monsters.size(),
            fcfg->hp_mult, fcfg->atk_mult);
    }
}

// ============================================================
// 主循环
// ============================================================
void GameScene::_process(double delta) {
    if (!player) return;
    float dt = (float)delta;

    if (state == GameState::BOSS_CINEMATIC) {
        // B15: Boss登场 — 玩家冻结, Boss暂停, 2秒后启动
        if (_boss_entrance_timer > 0) {
            _boss_entrance_timer -= dt;
            if (_boss_entrance_timer <= 0) {
                _boss_entered = true;
                state = GameState::PLAYING;
            }
        } else {
            boss_cinematic_timer -= dt;
            if (boss_cinematic_timer <= 0) {
                boss_cinematic_timer = 0;
                _boss_entered = true;
                state = GameState::PLAYING;
            }
        }
    }

    if (state != GameState::PLAYING) return;
    game_time += dt;

    // B15: Boss Phase2 提示
    if (!_boss_phase2_shown) {
        auto* boss = _get_boss();
        if (boss) {
            auto* bai = dynamic_cast<BossAI*>(boss->ai);
            if (bai && bai->phase2) {
                // D4 Step5.5: Boss Phase2对话
                BuildType bt = calculate_build(player.get()).identify();
                const BossDialogue* pd = _boss.narrative.find_phase2(
                    current_floor, _gameplay.world_state, bt);
                _presentation.room_msg = pd && pd->phase2 ? pd->phase2 : "BOSS 狂暴！";
                _presentation.room_msg_timer = 2.5f;
                _boss_phase2_shown = true;
                _presentation.trigger_shake(10.0f);
                _boss.cinematic.trigger_phase2();   // D5 Step6
                _boss.timeline.record(_boss.encounter.total_time(), "PHASE2");
            }
        }
    }

    // D5 Step2: LastStand 检测  D5 Step3: BossBehavior 评估
    if (!_boss.evolution.last_stand_triggered) {
        auto* boss = _get_boss();
        if (boss && boss->is_boss && (float)boss->combat.current_hp / boss->combat.max_hp < 0.15f) {
            _boss.evolution.last_stand_triggered = true;
            // Boss 最终阶段: 所有技能CD减半 + 超大范围
            if (auto* bai = dynamic_cast<BossAI*>(boss->ai)) {
                bai->_charge->cooldown *= 0.5f;
                bai->_shockwave->cooldown *= 0.5f;
                bai->_summon->cooldown *= 0.5f;
                bai->_shockwave->fx_radius *= 1.5f;
                bai->_charge->fx_radius *= 1.3f;
            }
            _boss.cinematic.trigger_last_stand();  // D5 Step6
            _boss.timeline.record(_boss.encounter.total_time(), "LAST_STAND");
            _presentation.room_msg = _boss.evolution.evolution_name
                ? std::string(_boss.evolution.evolution_name) + std::string("!")
                : std::string("LAST STAND!");
            _presentation.room_msg_timer = 2.5f;
            _presentation.trigger_shake(14.0f);
        }
    }

    // D5 Step3: Boss Memory tick + Behavior 评估 (仅Boss存在时)
    {
        auto* boss = _get_boss();
        if (boss && boss->is_boss) {
            _boss.behavior.memory.tick(dt, false);
            BossContext ctx;
            ctx.hp_pct = (float)boss->combat.current_hp / boss->combat.max_hp;
            ctx.dist_tiles = hypotf(
                boss->entity.rect.x - player->entity.rect.x,
                boss->entity.rect.y - player->entity.rect.y) / TILE_SIZE;
            ctx.player_low_hp = player->combat.current_hp < player->combat.max_hp * 0.3f;
            ctx.player_combo_high = player->combo.count >= 4;
            ctx.player_far = ctx.dist_tiles > 7;
            ctx.player_near = ctx.dist_tiles < 3;
            ctx.last_stand = _boss.evolution.last_stand_triggered;
            ctx.build = calculate_build(player.get()).identify();
            ctx.stage = _gameplay.story.stage();
            _boss.behavior.personality = boss_personality_for_floor(current_floor);
            evaluate_boss_decision(current_floor, ctx, _gameplay.world_state, _gameplay.rels,
                                    _boss.behavior, dt);
            // D5 Step5: BossEncounter tick
            _boss.encounter.tick(dt, ctx.hp_pct);
            _boss.cinematic.tick(dt);  // D5 Step6

            // D5 Step4: Behavior→Command (决策→执行)
            _boss.current_cmd = boss_decision_to_command(
                (int)_boss.behavior.current);
            // Boss战 arena tick
            _boss.arena.tick(dt, player.get(), monsters);
            // Phase2: 生成 arena (每8秒随机放置)
            static float _arena_spawn_timer = 0;
            _arena_spawn_timer += dt;
            if (_arena_spawn_timer > 8.0f) {
                _arena_spawn_timer = 0;
                float bx = boss->entity.rect.x + boss->entity.rect.width/2
                         + (float)((int)rng()%200 - 100);
                float by = boss->entity.rect.y + boss->entity.rect.height/2
                         + (float)((int)rng()%200 - 100);
                if (current_floor == 5)
                    _boss.arena.spawn_shadow_wall(bx, by, 3.0f);
                else if (current_floor == 10)
                    _boss.arena.spawn_lava(bx, by, 2.5f);
                else if (current_floor == 15)
                    _boss.arena.spawn_void_crack(bx, by, 3.5f);
            }
        }
    }

    // D3 Step4: Build Fusion 检测 — 构筑成型/切换
    {
        BuildScore bs = calculate_build(player.get());
        BuildType bt = bs.identify();
        if (bt != BuildType::NONE && bt != _gameplay.last_notified_build) {
            std::string msg;
            if (_gameplay.last_notified_build == BuildType::NONE)
                msg = std::string("BUILD COMPLETE! ") + bs.build_name();
            else
                msg = std::string("BUILD CHANGED: ") + bs.build_name();
            _presentation.room_msg = msg;
            _presentation.room_msg_timer = 2.0f;
            _gameplay.last_notified_build = bt;
        }
    }

    // ── Buff 逐帧结算 ──
    std::vector<BuffEvent> buf_events;
    tick_buffs(player.get(), dt, &buf_events);
    for (auto& m : monsters) tick_buffs(m.get(), dt, &buf_events, player.get()); // B11: venom_fang

    // Buff 事件日志 + C1: poison tick 伤害数字
    for (auto& ev : buf_events) {
        if (ev.type == BuffEventType::APPLIED)
            LOG_INFO("[BUF] %s applied to %s (stacks=%d)", ev.buff_id.c_str(), ev.target.c_str(), ev.stacks);
        else if (ev.type == BuffEventType::TICK_DAMAGE) {
            LOG_INFO("[BUF] %s tick on %s: %d dmg (stacks=%d)", ev.buff_id.c_str(), ev.target.c_str(), ev.value, ev.stacks);
            // C1: Poison 浮动数字 (在怪物头顶上方)
            for (auto& m : monsters) {
                if (m->name == ev.target) {
                    _presentation.damage_floats.push_back({
                        m->entity.rect.x + m->entity.rect.width/2,
                        m->entity.rect.y - 8,
                        0.6f, ev.value,
                        dmg_color_for(ev.value, false, true)
                    });
                    break;
                }
            }
        }
        else if (ev.type == BuffEventType::EXPIRED)
            LOG_INFO("[BUF] %s expired from %s", ev.buff_id.c_str(), ev.target.c_str());
    }

    // 清理被毒死的怪物
    _cleanup_dead_monsters();

    // D2 Step5: Arena 环境 tick (D4.6: arena_scale)
    if (game_map && !game_map->arena_objects.empty()) {
        float px = player->entity.rect.x + player->entity.rect.width/2;
        float py = player->entity.rect.y + player->entity.rect.height/2;
        float ascale = g_growth.arena_scale(current_floor);
        for (auto& ao : game_map->arena_objects) {
            if (!ao.active) continue;
            float ax = ao.tile_x * TILE_SIZE + TILE_SIZE/2;
            float ay = ao.tile_y * TILE_SIZE + TILE_SIZE/2;
            float dist = hypotf(px - ax, py - ay);
            switch (ao.type) {
            case ArenaObjectType::EXPLOSIVE_BARREL:
                ao.timer += dt;
                break;
            case ArenaObjectType::HEALING_TOTEM:
                ao.timer += dt;
                if (ao.timer >= 2.0f) { ao.timer = 0;
                    if (dist < 2.5f * TILE_SIZE) heal_player(player.get(), (int)(5 * ascale));
                    for (auto& m : monsters)
                        if (m->combat.is_alive && hypotf(m->entity.rect.x + m->entity.rect.width/2 - ax,
                            m->entity.rect.y + m->entity.rect.height/2 - ay) < 2.5f * TILE_SIZE)
                            m->combat.heal((int)(m->combat.max_hp * 0.08f * ascale));
                }
                break;
            case ArenaObjectType::POISON_POOL:
                if (dist < 1.2f * TILE_SIZE) apply_buff(player.get(), "poison", 1);
                for (auto& m : monsters)
                    if (m->combat.is_alive && hypotf(m->entity.rect.x + m->entity.rect.width/2 - ax,
                        m->entity.rect.y + m->entity.rect.height/2 - ay) < 1.2f * TILE_SIZE)
                        apply_buff(m.get(), "poison", 1);
                break;
            case ArenaObjectType::SPIKE: {
                int sd = (int)(3 * ascale), md = (int)(4 * ascale);
                if (dist < 0.8f * TILE_SIZE) {
                    player->combat.take_damage(sd);
                    _presentation.damage_floats.push_back({px, py-8, 0.4f, sd, {255, 60, 40, 255}});
                }
                for (auto& m : monsters)
                    if (m->combat.is_alive && hypotf(m->entity.rect.x + m->entity.rect.width/2 - ax,
                        m->entity.rect.y + m->entity.rect.height/2 - ay) < 0.8f * TILE_SIZE)
                        m->combat.take_damage(md);
                break;
            }
            default: break;
            }
        }
    }

    // 延迟播放 BGM（此时 get_tree() 已可用）
    if (!_pending_bgm.empty() && get_tree()) {
        LOG_DEBUG("BGM延迟播放: %s", _pending_bgm.c_str());
        get_tree()->get_audio()->play_bgm(_pending_bgm, 0.38f);
        _pending_bgm.clear();
    }

    // 时停倒计时
    if (time_stop_remaining > 0) {
        time_stop_remaining -= dt;
        if (time_stop_remaining <= 0) {
            time_stop_remaining = 0;
            _apply_pending_damage();
            // D3 Step3: The World E2 — 结束时释放 Shockwave
            if (_tw_evo_level >= 2) {
                float cx = player->entity.rect.x + player->entity.rect.width/2;
                float cy = player->entity.rect.y + player->entity.rect.height/2;
                for (auto& m : monsters) {
                    if (!m->combat.is_alive) continue;
                    float d = hypotf(m->entity.rect.x + m->entity.rect.width/2 - cx,
                                     m->entity.rect.y + m->entity.rect.height/2 - cy);
                    if (d < 120) {
                        int sd = calculate_damage(get_effective_attack(player.get()),
                            m->combat.get_effective_defense(AttackType::PHYSICAL));
                        m->combat.take_damage(sd);
                        // 击退
                        float dx = m->entity.rect.x - cx, dy = m->entity.rect.y - cy;
                        float len = sqrtf(dx*dx + dy*dy);
                        if (len > 0) { m->entity.position.x += dx / len * 30; m->entity.position.y += dy / len * 30; m->entity.sync_rect(); }
                    }
                }
                _presentation.trigger_shake(8.0f);
                _presentation.room_msg = "时停冲击!";
                _presentation.room_msg_timer = 1.0f;
            }
            // D3 Step3: The World E3 — 速度提升 5s
            if (_tw_evo_level >= 3) {
                _tw_speed_boost = 5.0f;
            }
            _tw_evo_level = 0;
        }
    }

    // D3 Step3: TW E3 speed boost tick
    if (_tw_speed_boost > 0) _tw_speed_boost -= dt;

    if (!player->combat.is_alive) {
        LOG_INFO("玩家死亡! 第%d层 Lv%d - 存档已保留", current_floor, player->level);
        // D4.6 Step5: 完成本局统计并结算Meta货币
        // D6 Step6: 死亡 — 委托给 Director 链
        _gameplay.on_player_dead(current_floor, player->level, player.get());
        g_meta.end_run(_gameplay.run_stats);
        _flow.on_player_dead();
        return;
    }

    // D6 Step7: 玩家移动/交互/怪物AI — 委托给 PlayerController
    _player_ctrl.tick(dt);

    // VFX 更新
    for (auto& fx : active_effects) fx.elapsed += dt;
    active_effects.erase(std::remove_if(active_effects.begin(), active_effects.end(),
        [](auto& fx) { return fx.elapsed >= fx.duration; }), active_effects.end());

    // D4.6 Step3: FlowDirector tick
    _gameplay.flow.tick(dt);

    // B10: 房间消息计时器
    if (_presentation.room_msg_timer > 0) _presentation.room_msg_timer -= dt;

    // D4 Step2: 事件演出 tick
    _tick_event_ui(dt);
    // D4 Step4: 对话计时器
    _update_dialogue(dt);
    // D4 Step5.2: QuestManager 自动推进
    _gameplay.quest_mgr.update(_gameplay.world_state, _gameplay.story);

    // D4 Step3: 楼层入场计时器
    if (_presentation.floor_intro_active) {
        _presentation.floor_intro_timer -= dt;
        _presentation.floor_intro_fade = std::min(1.0f, _presentation.floor_intro_fade + dt * 2.5f);
        if (_presentation.floor_intro_timer <= 0) _presentation.floor_intro_active = false;
    }
    // D4 Step3: 章节入场计时器
    if (_presentation.chapter_intro_active) {
        _presentation.chapter_intro_timer -= dt;
        if (_presentation.chapter_intro_timer <= 0) _presentation.chapter_intro_active = false;
    }
    // D4 Step3: 随机旁白 (25-40秒间隔, 不在事件中)
    if (!_presentation.floor_intro_active && !_presentation.chapter_intro_active && !_is_event_running()
        && !inventory_open && state == GameState::PLAYING && !is_boss_floor(current_floor)) {
        _gameplay.narr_state.narration_timer -= dt;
        if (_gameplay.narr_state.narration_timer <= 0) {
            // D4 Step5.4: WorldReaction overlay 优先覆盖随机旁白
            const char* nar = StoryDirector::world_flag_narration(_gameplay.world_state);
            if (!nar) nar = pick_random_narration(current_floor, _gameplay.narr_state);
            if (nar) { _presentation.room_msg = nar; _presentation.room_msg_timer = 3.0f; }
            _gameplay.narr_state.narration_timer = 25.0f + (float)(rng() % 15);
        }
        // D4 Step5.1: StoryDirector update + 世界事件 (~30秒)
        _gameplay.story.update(dt);
        if (_gameplay.story.should_trigger_story()) {
            // D4 Step5.4: WorldReaction 世界事件优先
            const char* we = g_reactions.current_world_event(_gameplay.world_state, _gameplay.story.stage());
            if (!we) we = _gameplay.story.tick_world_event(_gameplay.world_state);
            if (we) { _presentation.room_msg = we; _presentation.room_msg_timer = 3.0f; }
        }

        // D4.6 Step3: FlowDirector自动补救 (超过阈值触发动态内容)
        const char* suggest = _gameplay.flow.auto_spawn_suggestion();
        if (suggest && game_map && !monsters.empty()) {
            if (strcmp(suggest, "AUTO_STORY") == 0) {
                const char* we = _gameplay.story.tick_world_event(_gameplay.world_state);
                if (we) { _presentation.room_msg = we; _presentation.room_msg_timer = 3.0f; _gameplay.flow.mark_story(); }
            } else if (strcmp(suggest, "AUTO_REWARD") == 0) {
                // 在玩家附近生成药水
                auto [tx, ty] = game_map->pixel_to_tile(
                    player->entity.rect.x + player->entity.rect.width/2,
                    player->entity.rect.y + player->entity.rect.height/2);
                auto p = std::make_shared<ConsumableItem>("探索发现", Rarity::RARE, "heal", 25);
                ground_items.push_back({p, tx + 2, ty + 1});
                _presentation.room_msg = "你在角落发现了一件物品。";
                _presentation.room_msg_timer = 2.0f;
                _gameplay.flow.mark_reward();
            } else if (strcmp(suggest, "AUTO_PATROL") == 0) {
                // 在玩家附近生成巡逻怪
                auto [tx, ty] = game_map->pixel_to_tile(
                    player->entity.rect.x + (rng()%10-5) * TILE_SIZE,
                    player->entity.rect.y + (rng()%10-5) * TILE_SIZE);
                auto [px, py] = game_map->tile_to_pixel(tx, ty);
                if (game_map->is_walkable(tx, ty)) {
                    auto* m = spawn_monster(px, py, (rng()%3==0)?"orc":"slime");
                    m->combat.max_hp = (int)(m->combat.max_hp * g_growth.hp_scale(current_floor));
                    m->combat.current_hp = m->combat.max_hp;
                    monsters.emplace_back(m);
                    _gameplay.flow.mark_combat();
                }
            } else if (strcmp(suggest, "AUTO_ELITE") == 0) {
                auto [tx, ty] = game_map->pixel_to_tile(
                    player->entity.rect.x + (rng()%8-4) * TILE_SIZE,
                    player->entity.rect.y + (rng()%8-4) * TILE_SIZE);
                if (game_map->is_walkable(tx, ty)) {
                    auto [px, py] = game_map->tile_to_pixel(tx, ty);
                    auto* m = spawn_monster(px, py, "elite");
                    m->combat.max_hp = (int)(m->combat.max_hp * g_growth.elite_scale(current_floor));
                    m->combat.current_hp = m->combat.max_hp;
                    monsters.emplace_back(m);
                    _presentation.room_msg = "一支精英巡逻队出现了！";
                    _presentation.room_msg_timer = 2.0f;
                    _gameplay.flow.mark_combat();
                }
            }
        }
    }

    // D2: Combo 窗口衰减 — 超过 WINDOW 未命中则重置
    player->combo.tick(dt);

    // D6 Step5: Presentation tick (shake/freeze/damage/message/intro)
    _presentation.tick(dt);
}

// ============================================================
// 输入处理
// ============================================================
// _input / debug / event / dialogue — delegated to GameSceneInput
void GameScene::_input(const InputMap& input) {
    _input_handler.handle_input(input);
}

// ============================================================
// 战斗
// ============================================================
// _player_attack / _use_skill — delegated to PlayerController (D6 Step7)
void GameScene::_player_attack() { _player_ctrl.player_attack(); }
void GameScene::_use_skill(int index) { _player_ctrl.use_skill(index); }

void GameScene::_update_monsters(float dt) {
    int hp_before = player->combat.current_hp;
    std::vector<Monster*> mlist;
    for (auto& m : monsters) mlist.push_back(m.get());
    for (auto& m : monsters)
        m->update_ai(player.get(), game_map.get(), dt, game_time, &mlist, &active_effects);
    // 检测玩家死亡
    // if (player->combat.current_hp < hp_before) { /* SFX */ }
}

void GameScene::_on_monster_killed(Monster* m)  { _combat.on_monster_killed(m); }
void GameScene::_cleanup_dead_monsters()         { _combat.cleanup_dead_monsters(); }
void GameScene::_check_floor_clear() {
    if (FloorManager::is_floor_cleared(monsters) && !stairs_active) _activate_stairs();
}

void GameScene::_activate_stairs() {
    if (stairs_active) return;
    auto boss = _get_boss();
    if (boss && boss->combat.is_alive) return;
    game_map->set_tile(stairs_pos.first, stairs_pos.second, TileType::STAIRS_DOWN);
    stairs_active = true;
    max_unlocked_floor = std::max(max_unlocked_floor, current_floor);
    LOG_INFO("第%d层清空! 楼梯已激活, 自动存档", current_floor);
    std::vector<bool> spr, spd;
    if (game_map) for (auto& sr : game_map->special_rooms) {
        spr.push_back(sr.triggered);
        spd.push_back(sr.discovered);
    }
    // B13: Relic 不再跨层 (无需保存圣物)
    SaveManager::save_game(player.get(), current_floor, max_unlocked_floor,
                           _dungeon_seed, spr, spd);
    on_floor_cleared.emit();
}

void GameScene::_check_floor_transition() {
    if (!stairs_active) return;
    int next = FloorManager::check_floor_transition(get_tree()->get_input(),
        current_floor, game_map.get(), player.get(), stairs_pos);
    if (next < 0) return;  // 不下楼

    if (next == MAX_FLOORS) {
        LOG_INFO("通关! 最终第%d层 Lv%d", current_floor, player->level);
        // D6 Step6: 通关 — 委托给 FlowDirector (ending → VictoryScene → Credits)
        _gameplay.on_game_clear(current_floor, player->level, player.get(),
                                 _boss.battle_report, g_relic_archive.collection_pct());
        _flow.on_game_clear();
    } else {
        enter_floor(next);
    }
}

void GameScene::_apply_pending_damage() { _combat.apply_pending_damage(); }

// _pickup, _interact_special, _check_special_room_discovery, _show_room_message
// 已迁移到 InteractionHandler

Monster* GameScene::_get_boss() const {
    for (auto& m : monsters)
        if (m->is_boss && m->combat.is_alive) return m.get();
    return nullptr;
}

void GameScene::_drop_boss_reward(Monster* boss) {
    auto [tx, ty] = game_map->pixel_to_tile(boss->entity.position.x, boss->entity.position.y);
    auto weapon = std::make_shared<EquipmentItem>("魔渊之刃", Rarity::LEGENDARY, "weapon", 18, 3, 0);
    ground_items.push_back({weapon, tx, ty});
    auto potion = std::make_shared<ConsumableItem>("神谕药剂", Rarity::LEGENDARY, "heal", 80);
    ground_items.push_back({potion, tx + 2, ty});
}

// ============================================================
// 渲染
// ============================================================
void GameScene::_render() {
    int sw = get_tree()->get_width(), sh = get_tree()->get_height();
    if (state == GameState::BOSS_INTRO) {
        _renderer.draw_boss_intro(sw, sh, boss_intro_title, boss_intro_lore,
                                   boss_intro_skills, boss_intro_color, boss_floor);
        // D4 Step5.5: BossNarrative覆盖对话 (显示在面板下方)
        if (!_presentation.boss_intro_text.empty() && g_font_loaded) {
            float tw = MeasureTextEx(g_font_small, _presentation.boss_intro_text.c_str(), 17, 1).x;
            DrawTextEx(g_font_small, _presentation.boss_intro_text.c_str(),
                       {sw/2.0f - tw/2, (float)(sh - 100)}, 17, 1, {255, 220, 100, 240});
        }
        // D5 Step1: BossModifier文字 (金色Warning风格)
        if (!_presentation.boss_modifier_text.empty() && g_font_loaded) {
            float mw = MeasureTextEx(g_font_small, _presentation.boss_modifier_text.c_str(), 15, 1).x;
            DrawRectangle(sw/2.0f - mw/2 - 12, (float)(sh - 72), mw + 24, 24,
                          {30, 15, 15, 200});
            DrawTextEx(g_font_small, _presentation.boss_modifier_text.c_str(),
                       {sw/2.0f - mw/2, (float)(sh - 68)}, 15, 1, {255, 80, 40, 240});
        }
        return;
    }

    ClearBackground(BLACK);
    _renderer.update_camera(_cam_x, _cam_y, player.get(), game_map.get(), sw, sh);

    // D4 Step5.4: WorldReaction tint (覆盖 CLEAR_BACKGROUND 之上)
    {
        unsigned char tr = 0, tg = 0, tb = 0;
        g_reactions.current_tint(_gameplay.world_state, _gameplay.story.stage(), tr, tg, tb);
        if (tr > 0 || tg > 0 || tb > 0) {
            DrawRectangle(0, 0, sw, sh, {tr, tg, tb, 18});  // 18 alpha = 7% overlay
        }
    }

    // C1: 屏幕震动 (相机偏移)
    float shake_ox = 0, shake_oy = 0;
    if (_presentation.shake_timer > 0) {
        float s = _presentation.shake_intensity * (_presentation.shake_timer / 0.12f);
        shake_ox = ((float)(rng() % 100) / 100.0f - 0.5f) * s * 2;
        shake_oy = ((float)(rng() % 100) / 100.0f - 0.5f) * s * 2;
    }
    float saved_cx = _cam_x, saved_cy = _cam_y;
    _cam_x += shake_ox; _cam_y += shake_oy;

    _draw_map();
    _draw_ground_items();
    _draw_entities();
    _renderer.draw_effects(active_effects, _cam_x, _cam_y);
    // D5 Step4: Boss战场绘制
    _boss.arena.draw(_cam_x, _cam_y);

    // C1: 伤害数字 (世界坐标→屏幕)
    for (auto& df : _presentation.damage_floats) {
        float sx = df.x - _cam_x, sy = df.y - _cam_y - (0.6f - df.lifetime) * 30;
        unsigned char a = (unsigned char)(df.color.a * (df.lifetime / 0.6f));
        Color c = df.color; c.a = a;
        char buf[16]; snprintf(buf, sizeof(buf), "%d", df.value);
        GameRenderer::draw_glow_text(buf, sx, sy, 16 + df.value / 10, c, true);
    }

    _cam_x = saved_cx; _cam_y = saved_cy;  // 恢复

    // HUD (委托给 GameRenderer)
    _renderer.draw_hud(player.get(), current_floor, game_time,
                        _get_boss(), _show_relic_panel,
                        inventory_open, inventory_cursor,
                        _presentation.room_msg, _presentation.room_msg_timer, sw, sh);

    if (inventory_open) _renderer.draw_inventory_panel(player.get(), inventory_cursor, sw, sh);

    // D4.6 Step1: F8 Growth Curve debug
    if (_presentation.show_growth_debug && g_font_loaded) {
        const GrowthCurve& gc = g_growth.curve(current_floor);
        float pw = 240, ph = 260;
        Rectangle pr = {sw - pw - 10.0f, 10.0f, pw, ph};
        GameRenderer::draw_panel(pr, "Growth Curve", {15,15,30,220});
        float y = pr.y + 36;
        auto line = [&](const char* fmt, float val) {
            char buf[32]; snprintf(buf, sizeof(buf), fmt, val);
            DrawTextEx(g_font_small, buf, {pr.x+14, y}, 14, 1, {200,220,255,255}); y += 20;
        };
        line("Floor %d", (float)current_floor);
        line("Monster HP  x%.2f",  gc.monster_hp);
        line("Monster ATK x%.2f",  gc.monster_atk);
        line("Boss HP     x%.2f",  gc.boss_hp);
        line("Boss ATK    x%.2f",  gc.boss_atk);
        line("Elite Scale x%.2f",  gc.elite_scale);
        line("EXP Scale   x%.2f",  gc.exp_scale);
        line("Gold Scale  x%.2f",  gc.gold_scale);
        line("Relic Scale x%.2f",  gc.relic_scale);
        line("Arena Scale x%.2f",  gc.arena_scale);
    }

    // D4.6 Step3: F10 FlowDirector debug
    if (_presentation.show_flow_debug && g_font_loaded) {
        float pw = 220, ph = 210;
        Rectangle pr = {sw - pw - 10.0f, 280.0f, pw, ph};
        GameRenderer::draw_panel(pr, "Flow Director", {15,30,15,220});
        float y = pr.y + 36;
        auto line = [&](const char* fmt, float val) {
            char buf[32]; snprintf(buf, sizeof(buf), fmt, val);
            DrawTextEx(g_font_small, buf, {pr.x+14, y}, 13, 1, {200,255,200,255}); y += 19;
        };
        const FlowTimer& ft = _gameplay.flow.timer();
        line("Combat  %.1fs", ft.combat);
        line("Reward  %.1fs", ft.reward);
        line("Story   %.1fs", ft.story);
        line("Event   %.1fs", ft.event);
        line("Explorer %d pts", (int)ft.explorer_score);
        line("Rooms   %d",     ft.rooms_explored);
        line("MaxGap  %.1fs",  _gameplay.flow.worst_gap());
    }

    // D5 Step3: F11 BossBehavior debug
    if (_presentation.show_boss_behavior && g_font_loaded) {
        auto* boss = _get_boss();
        float pw = 240, ph = boss ? 260.0f : 120.0f;
        Rectangle pr = {sw - pw - 10.0f, 500.0f, pw, ph};
        GameRenderer::draw_panel(pr, "Boss Behavior", {25,15,25,220});
        float y = pr.y + 36;
        auto line = [&](const char* fmt, float val) {
            char buf[32]; snprintf(buf, sizeof(buf), fmt, val);
            DrawTextEx(g_font_small, buf, {pr.x+14, y}, 13, 1, {255,200,100,255}); y += 18;
        };
        auto& st = _boss.behavior;
        line("Decision %s",     0); // hack: print string via label
        DrawTextEx(g_font_small, st.decision_name ? st.decision_name : "IDLE",
                   {pr.x+100, y-18}, 13, 1, {255,255,100,255});
        auto* bl = _get_boss();
        if (bl) {
            line("Pers   %s", 0);
            DrawTextEx(g_font_small, st.personality_name ? st.personality_name : "?",
                       {pr.x+100, y-18}, 13, 1, {150,255,150,255});
            line("Mem Atk %d",  (float)st.memory.attacks);
            line("Mem Skl %d",  (float)st.memory.skills);
            line("Mem Cmb %d",  (float)st.memory.combos_max);
            line("Mem Dod %d",  (float)st.memory.dodges);
            for (int i = 0; i < 8; i++) {
                if (st.weights[i] == 0) continue;
                const char* dn[] = {"CHS","RET","CRG","SUM","DEF","RNG","MEL","SPC"};
                char wb[32]; snprintf(wb, sizeof(wb), "%s:%d", dn[i], st.weights[i]);
                DrawTextEx(g_font_small, wb, {pr.x+14 + (i%4)*56.0f, y + (i/4)*16.0f},
                           11, 1, Color{(unsigned char)(150+i*12),(unsigned char)(180+i*8),255,255});
            }
        }
    }

    // D5 Step4: F12 BossCombat debug
    if (_presentation.show_boss_cmd && g_font_loaded) {
        float pw = 220, ph = 200;
        Rectangle pr = {sw - pw - 10.0f, 510.0f, pw, ph};
        GameRenderer::draw_panel(pr, "Boss Cmd+Arena", {25,20,20,220});
        float y = pr.y + 36;
        auto line = [&](const char* fmt, float val) {
            char buf[32]; snprintf(buf, sizeof(buf), fmt, val);
            DrawTextEx(g_font_small, buf, {pr.x+14, y}, 13, 1, {200,255,200,255}); y += 18;
        };
        DrawTextEx(g_font_small, boss_command_name(_boss.current_cmd),
                   {pr.x+14, y}, 14, 1, {255,255,100,255}); y += 20;
        line("Arena Zones %d", (float)_boss.arena.zones().size());
        line("SkillQ active %d", _boss.skill_queue.active ? 1.0f : 0.0f);
        line("SkillQ size  %d",  (float)_boss.skill_queue.queue.size());
        y += 10;
        for (auto& z : _boss.arena.zones()) {
            char zbuf[64];
            const char* tn = z.type == DangerType::LAVA ? "LAVA" :
                             z.type == DangerType::SHADOW_WALL ? "WALL" :
                             z.type == DangerType::VOID_CRACK ? "VOID" : "?";
            snprintf(zbuf, sizeof(zbuf), "%s %.1fs r=%.0f",
                     tn, z.remaining, z.radius);
            DrawTextEx(g_font_small, zbuf, {pr.x+14, y}, 11, 1,
                       z.is_warning() ? Color{255,200,100,180} : Color{255,60,40,200});
            y += 14; if (y > pr.y+ph-20) break;
        }
    }

    // D5 Step5: F13 BossReport debug
    if (_presentation.show_boss_report && g_font_loaded) {
        auto& r = _boss.battle_report;
        float pw = 230, ph = 290;
        Rectangle pr = {sw - pw - 10.0f, 480.0f, pw, ph};
        GameRenderer::draw_panel(pr, "Boss Report", {20,25,20,220});
        float y = pr.y + 36;
        auto line = [&](const char* fmt, float val) {
            char buf[32]; snprintf(buf, sizeof(buf), fmt, val);
            DrawTextEx(g_font_small, buf, {pr.x+14, y}, 13, 1, {200,255,200,255}); y += 18;
        };
        line("%s %s", 0);
        DrawTextEx(g_font_small, _boss.encounter.phase_name(), {pr.x+14, y-18}, 13, 1, {255,200,100,255});
        DrawTextEx(g_font_small, r.rank_name(), {pr.x+120, y-18}, 16, 1, {255,255,60,255});
        line("Time     %.1fs", r.battle_time);
        line("Damage   %d", (float)r.total_damage);
        line("Taken    %d", (float)r.damage_taken);
        line("ComboMax %d",  (float)r.replay.combo_max);
        line("Strategy %s", 0);
        DrawTextEx(g_font_small, r.replay.strategy_name(), {pr.x+100, y-18}, 13, 1, {255,150,100,255});
        line("Rank    %s", 0);
        DrawTextEx(g_font_small, r.rank_name(), {pr.x+100, y-18}, 14, 1, {255,200,60,255});
        line("ArenaZns %d", (float)r.arena_zones_spawned);
        line("BloodRit %s", (float)r.replay.blood_ritual);
        line("Curse    %s", (float)r.replay.curse);
    }

    // D4 Step3: 章节入场 (全屏大标题, 3秒)
    if (_presentation.chapter_intro_active) {
        int ch = _presentation.chapter_intro_ch;
        float a = std::min(1.0f, _presentation.chapter_intro_timer);
        Color gold = {255, 200, 50, (unsigned char)(220 * a)};
        Color white = {230, 230, 240, (unsigned char)(200 * a)};
        DrawRectangle(0, 0, sw, sh, {0, 0, 0, (unsigned char)(150 * a)});
        GameRenderer::draw_glow_text(get_chapter_subtitle(ch), sw/2.0f, sh/2.0f - 35,
                                      36, gold, true);
        GameRenderer::draw_glow_text(get_chapter_title(ch), sw/2.0f, sh/2.0f + 15,
                                      28, white, true);
        auto* fn = get_floor_narrative(_presentation.floor_intro_floor > 0 ? _presentation.floor_intro_floor : current_floor);
        if (fn) GameRenderer::draw_glow_text(fn->subtitle, sw/2.0f, sh/2.0f + 50,
                                              18, Color{180,180,200,(unsigned char)(160*a)}, true);
    }

    // D4 Step3: 楼层入场演出 (非章节覆盖时)
    if (_presentation.floor_intro_active && !_presentation.chapter_intro_active) {
        auto* fn = get_floor_narrative(_presentation.floor_intro_floor);
        if (fn) {
            float a = _presentation.floor_intro_fade;
            Color gold = {255, 200, 50, (unsigned char)(200 * a)};
            Color white = {230, 230, 240, (unsigned char)(180 * a)};
            DrawRectangle(0, 0, sw, sh, {0, 0, 0, (unsigned char)(120 * a)});
            char buf[64];
            snprintf(buf, sizeof(buf), "Floor %d", _presentation.floor_intro_floor);
            GameRenderer::draw_glow_text(buf, sw/2.0f, sh/2.0f - 40, 20, gold, true);
            GameRenderer::draw_glow_text("══════════════", sw/2.0f, sh/2.0f - 20, 18, gold, true);
            GameRenderer::draw_glow_text(fn->title, sw/2.0f, sh/2.0f + 10, 28, white, true);
            GameRenderer::draw_glow_text(fn->subtitle, sw/2.0f, sh/2.0f + 42, 16,
                                          Color{180,180,200,(unsigned char)(140*a)}, true);
            GameRenderer::draw_glow_text("══════════════", sw/2.0f, sh/2.0f + 60, 18, gold, true);
        }
    }

    // D4 Step4: 对话UI (在HUD之上, 事件之下)
    _draw_dialogue(sw, sh);
    // D4 Step4: 任务日志
    if (_quest_log_open) _draw_quest_log(sw, sh);

    // D4 Step2: 事件演出 UI (在所有 HUD 之上)
    _draw_event_ui(sw, sh);

    if (time_stop_remaining > 0) _renderer.draw_time_stop_overlay(sw, sh, time_stop_remaining);
    if (state == GameState::BOSS_CINEMATIC && _boss_entrance_timer > 0) {
        // B15: Boss登场文字
        DrawRectangle(0, 0, sw, sh, {0, 0, 0, 160});
        GameRenderer::draw_glow_text("!!! BOSS 登场 !!!", sw / 2.0f, sh / 2.0f - 20,
                                      40, {255, 60, 40, 255}, true);
        auto* boss = _get_boss();
        if (boss) {
            GameRenderer::draw_glow_text(boss->name.c_str(), sw / 2.0f, sh / 2.0f + 30,
                                          28, boss->color, true);
        }
    } else if (state == GameState::BOSS_CINEMATIC) {
        _renderer.draw_boss_cinematic_overlay(sw, sh);
    }

    // D5 Step6: BossCinematic overlay (Phase2/LastStand vignette)
    if (_boss.cinematic.is_running()) {
        float a = std::min(1.0f, _boss.cinematic.timer());
        Color edge = {0,0,0, (unsigned char)(80 * a)};
        // simple 3-bar vignette at screen edges (top/bottom)
        DrawRectangle(0, 0, sw, 40, edge);
        DrawRectangle(0, sh-40, sw, 40, edge);
        if (g_font_loaded) {
            const char* pname = _boss.cinematic.phase_name();
            if (pname && strcmp(pname, "NONE") != 0) {
                float tw = MeasureTextEx(g_font_small, pname, 22, 1).x;
                DrawTextEx(g_font_small, pname, {sw/2.0f - tw/2, sh/2.0f - 12},
                           22, 1, {255,200,50,(unsigned char)(220*a)});
            }
        }
    }
}

void GameScene::_draw_map() {
    if (game_map) game_map->draw(_cam_x, _cam_y, get_tree()->get_width(), get_tree()->get_height());
}

void GameScene::_draw_entities() {
    for (auto& m : monsters) {
        m->draw(_cam_x, _cam_y);
        _renderer.draw_monster_buffs(*m,
            m->entity.position.x - _cam_x,
            m->entity.position.y - _cam_y);
        // D2 Step4: Tank守护连线 (淡蓝色)
        if (m->ai && m->team_role == TeamRole::FRONTLINE && m->ai->_protect_target) {
            float x1 = m->entity.rect.x + m->entity.rect.width/2 - _cam_x;
            float y1 = m->entity.rect.y + m->entity.rect.height/2 - _cam_y;
            auto* t = m->ai->_protect_target;
            float x2 = t->entity.rect.x + t->entity.rect.width/2 - _cam_x;
            float y2 = t->entity.rect.y + t->entity.rect.height/2 - _cam_y;
            DrawLineEx({x1, y1}, {x2, y2}, 1.5f, {60, 140, 255, 100});
        }
    }
    if (player) player->draw_no_cam(_cam_x, _cam_y);

    // D4 Step4: NPC 标记
    for (int i = 0; i < _npc_count; i++) {
        if (_npc_state[i].finished) continue;
        float nx = _npc_tile_x[i] * TILE_SIZE + TILE_SIZE/2 - _cam_x;
        float ny = _npc_tile_y[i] * TILE_SIZE + TILE_SIZE/2 - _cam_y;
        // 绿色圆点标记
        float pulse = 4 + sinf((float)GetTime() * 4) * 2;
        DrawCircle(nx, ny - 10, pulse, {100, 220, 140, 180});
        DrawCircle(nx, ny - 10, 3, {60, 180, 80, 255});
    }
}

void GameScene::_draw_ground_items() {
    for (auto& d : ground_items) {
        float px = d.tile_x * TILE_SIZE - _cam_x;
        float py = d.tile_y * TILE_SIZE - _cam_y;
        float size = TILE_SIZE - 4;
        float cx = px + TILE_SIZE / 2, cy = py + TILE_SIZE / 2;
        float pulse = 6 + sinf((float)GetTime() * 5 + px * 0.1f) * 3;
        DrawRectangleLinesEx({cx - pulse, cy - pulse, pulse * 2, pulse * 2}, 1,
                             Color{(unsigned char)(d.item->color.r / 3),
                                   (unsigned char)(d.item->color.g / 3),
                                   (unsigned char)(d.item->color.b / 3), 200});
        DrawRectangleRounded({px + 2, py + 2, size, size}, 0.1f, 4, d.item->color);
        DrawRectangleRoundedLines({px + 2, py + 2, size, size}, 0.1f, 4, 1, BLACK);
    }
}

// _draw_hud 已迁移到 GameRenderer
// _draw_* methods migrated to GameRenderer

// ============================================================
// D4 Step4: NPC / Dialogue / Quest 实现
// ============================================================
NPCState* GameScene::_find_or_create_npc_state(int npc_id) {
    for (int i = 0; i < _npc_count; i++)
        if (_npc_state[i].id == npc_id) return &_npc_state[i];
    if (_npc_count < 10) {
        _npc_state[_npc_count].id = npc_id;
        return &_npc_state[_npc_count++];
    }
    return nullptr;
}

void GameScene::_spawn_floor_npcs(int floor, const std::vector<std::pair<int,int>>& rooms) {
    for (int slot = 0; slot < 2; slot++) {
        const NPCData* cfg = get_npc_config(floor, slot);
        if (!cfg) continue;
        int npc_id = floor * 10 + slot;
        NPCState* st = _find_or_create_npc_state(npc_id);
        if (!st || st->finished) continue; // 已完成对话的不再生
        // 放在非玩家/非楼梯的房间
        int r_idx = 2 + slot; if (r_idx >= (int)rooms.size() - 1) r_idx = (int)rooms.size()/2;
        auto [rx, ry] = rooms[r_idx];
        _npc_tile_x[_npc_count - 1] = rx;
        _npc_tile_y[_npc_count - 1] = ry;
    }
}

void GameScene::_start_dialogue(int npc_index) {
    if (npc_index < 0 || npc_index >= _npc_count) return;
    NPCState* st = &_npc_state[npc_index];
    const NPCData* cfg = get_npc_config(st->id / 10, st->id % 10);
    if (!cfg) return;

    _dialogue.active = true;
    _dialogue.page = 0;
    _dialogue.timer = 0.0f;
    _dialogue.pages.clear();
    _dialogue.target_npc = st;

    // 首次对话 → first_dialogue, 否则 → repeat
    const char* const* pool = st->met ? cfg->repeat_dialogue : cfg->first_dialogue;
    for (int i = 0; i < 5; i++) {
        if (!pool[i]) break;
        DialoguePage dp;
        dp.speaker = cfg->name;
        dp.text = pool[i];
        _dialogue.pages.push_back(dp);
    }
    if (!st->met) st->met = true;

    // D4 Step5.2: NPC对话自动接取关联任务
    Quest* npc_quest = _gameplay.quest_mgr.get_npc_quest(st->id / 10);
    if (npc_quest && npc_quest->state == QuestState::AVAILABLE)
        _gameplay.quest_mgr.accept(npc_quest->id, _gameplay.world_state);

    // D4 Step5: NPC自适应对话 (根据WorldFlag插入额外页面)
    int npc_floor = st->id / 10;
    const char* adaptive = StoryDirector::npc_adaptive_text(npc_floor, _gameplay.world_state);
    if (adaptive && !st->finished) {
        DialoguePage ap;
        ap.speaker = cfg->name;
        ap.text = adaptive;
        _dialogue.pages.push_back(ap);
    }
}

void GameScene::_update_dialogue(float dt) {
    if (!_dialogue.active) return;
    _dialogue.timer += dt;
}

void GameScene::_draw_dialogue(int sw, int sh) {
    auto& d = _dialogue;
    if (!d.active || d.page >= (int)d.pages.size()) return;

    float pw = 600, ph = 260;
    Rectangle pr = {sw/2.0f - pw/2, sh/2.0f - ph/2, pw, ph};
    GameRenderer::draw_panel(pr, d.pages[d.page].speaker.c_str(), {20,20,45,240});

    if (g_font_loaded) {
        DrawTextEx(g_font_small, d.pages[d.page].text.c_str(), {pr.x+30, pr.y+50}, 16, 1, {230,230,240,255});
    }
    const char* hint = (d.page + 1 < (int)d.pages.size())
        ? "[ENTER]下一页  [ESC]跳过" : "[ENTER]告别  [ESC]离开";
    DrawTextEx(g_font_small, hint,
               {pr.x + (pw - MeasureTextEx(g_font_small,hint,14,1).x)/2, pr.y+ph-28},
               14, 1, {140,140,160,200});
    // 页数指示器
    char pg[16]; snprintf(pg, sizeof(pg), "%d/%d", d.page+1, (int)d.pages.size());
    DrawTextEx(g_font_small, pg, {pr.x+pw-40, pr.y+12}, 13, 1, {180,180,200,200});
}

void GameScene::_draw_quest_log(int sw, int sh) {
    float pw = 380, ph = 420;
    Rectangle pr = {sw/2.0f - pw/2, sh/2.0f - ph/2, pw, ph};
    auto& quests = _gameplay.quest_mgr.all_quests();
    int done = _gameplay.quest_mgr.completed_count(), total = (int)quests.size();
    GameRenderer::draw_panel(pr, "任务日志  Q关闭", {20,20,40,230});

    if (quests.empty()) {
        DrawTextEx(g_font_small, "暂无任务。", {pr.x+30, pr.y+60}, 16, 1, {160,160,180,200});
    } else {
        char progress[32];
        snprintf(progress, sizeof(progress), "%d/%d 完成", done, total);
        DrawTextEx(g_font_small, progress, {pr.x+pw-100, pr.y+14}, 12, 1, {200,200,100,200});

        float y = pr.y+45;
        for (auto& q : quests) {
            if (q.hidden) continue;
            const char* status;
            Color c;
            switch (q.state) {
                case QuestState::COMPLETED: status = "[✓完成]"; c = Color{100,200,100,255}; break;
                case QuestState::ACCEPTED:  status = "[▶进行中]"; c = Color{255,200,50,255}; break;
                case QuestState::AVAILABLE: status = "[○可接]"; c = Color{180,180,220,255}; break;
                case QuestState::FAILED:    status = "[✗失败]"; c = Color{200,100,100,255}; break;
                default: status = "[锁]"; c = Color{100,100,100,200}; break;
            }
            char buf[128];
            snprintf(buf, sizeof(buf), "%s %s", status, q.title.c_str());
            DrawTextEx(g_font_small, buf, {pr.x+25, y}, 16, 1, c);
            DrawTextEx(g_font_small, q.desc.c_str(), {pr.x+40, y+22}, 13, 1, {180,180,200,200});
            y += 44;
            if (y > pr.y + ph - 100) break; // 防止溢出
        }

        // D4 Step5.3: NPC关系摘要
        auto& rels = _gameplay.rels.all();
        if (!rels.empty()) {
            DrawLine(pr.x+15, pr.y+ph-85, pr.x+pw-15, pr.y+ph-85, {60,60,90,180});
            float ry = pr.y+ph-78;
            DrawTextEx(g_font_small, "NPC关系:", {pr.x+20, ry}, 12, 1, {200,180,140,220});
            ry += 16;
            for (auto& r : rels) {
                if (r.total() == 0) continue;
                // NPC名字简写
                const char* npc_name = "???";
                if (r.npc_id == 20) npc_name = "埃德加";
                else if (r.npc_id == 70) npc_name = "泰伦斯";
                else if (r.npc_id == 90) npc_name = "索拉斯";
                else if (r.npc_id == 110) npc_name = "灵魂";
                else if (r.npc_id == 140) npc_name = "守望者";
                char rbuf[80];
                snprintf(rbuf, sizeof(rbuf), "%s  Tr:%d Rs:%d Fr:%d",
                    npc_name, r.trust, r.respect, r.fear);
                DrawTextEx(g_font_small, rbuf, {pr.x+25, ry}, 12, 1, {180,180,200,200});
                ry += 14;
                if (ry > pr.y + ph - 10) break;
            }
        }
    }
}

// _draw_player_buffs, _draw_player_relics, _draw_relic_panel,
// _draw_monster_buffs, _draw_inventory_panel 已迁移到 GameRenderer
// _draw_effects, _draw_time_stop_overlay, _draw_boss_cinematic_overlay, _draw_boss_intro
// 已迁移到 GameRenderer
