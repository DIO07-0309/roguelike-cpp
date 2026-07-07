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
#include <cmath>
#include <algorithm>
#include <cstdio>

// 字体指针 (在 main.cpp 中初始化)
extern Font g_font;
extern Font g_font_small;
extern bool g_font_loaded;

// ---- 内部辅助：绘制工具 ----
static void draw_panel(Rectangle r, const char* title, Color bg = {20, 20, 40, 230}) {
    DrawRectangleRounded(r, 0.08f, 8, bg);
    DrawRectangleRoundedLines(r, 0.08f, 8, 2, {100, 100, 180, 255});
    if (title && g_font_loaded)
        DrawTextEx(g_font_small, title, {r.x + 12, r.y + 8}, 18, 1, {200, 200, 255, 255});
}

static void draw_glow_text(const char* text, float x, float y, int size, Color c, bool center = true) {
    if (!g_font_loaded) return;
    float w = MeasureTextEx(g_font, text, (float)size, 1).x;
    if (center) x -= w / 2;
    DrawTextEx(g_font, text, {x + 1, y + 1}, size, 1, {0, 0, 0, 100});
    DrawTextEx(g_font, text, {x, y}, size, 1, c);
}

static void draw_progress_bar(Rectangle r, float ratio, Color fill, Color bg = {30, 30, 60, 255}) {
    DrawRectangleRec(r, bg);
    DrawRectangleRec({r.x, r.y, r.width * ratio, r.height}, fill);
    DrawRectangleLinesEx(r, 1, {60, 60, 90, 255});
}

// ============================================================
// GameScene 实现
// ============================================================
void GameScene::_ready() {
    // 由外部调用 new_game() / enter_floor() 初始化
}

void GameScene::new_game() {
    player = std::make_unique<Player>(TILE_SIZE * 2, TILE_SIZE * 2,
        PLAYER_SPEED, PLAYER_MAX_HP, PLAYER_ATTACK, PLAYER_PDEF, PLAYER_MDEF);
    auto sk = random_active_skill();
    player->skills.learn(std::move(sk));
    player->skills.apply_all_passives(player.get());
    current_floor = 1;
    max_unlocked_floor = 1;
    enter_floor(1);
}

void GameScene::load_saved_game(int floor, int max_f, std::unique_ptr<Player> p) {
    player = std::move(p);
    current_floor = floor;
    max_unlocked_floor = max_f;
    enter_floor(floor);
}

void GameScene::enter_floor(int floor) {
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

    // 生成地牢
    DungeonGenerator gen(MAP_WIDTH, MAP_HEIGHT, TILE_SIZE);
    game_map = gen.generate();
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
    } else {
        _spawn_floor_monsters(rooms);
        state = GameState::PLAYING;
    }

    stairs_pos = rooms.back();
    player->combat.current_hp = player->combat.max_hp;
    player->reset_attack_timers();

    // BGM: Boss层→boss, 普通层→dungeon (延迟到 _process 中播放, 避免 get_tree() 为空)
    _pending_bgm = is_boss_floor(floor) ? "boss" : "dungeon";

    if (is_boss_floor(floor)) {
        LOG_INFO("进入第%d层 (Boss: %s) - HP:%d ATK:%d DEF:%d",
            floor, get_boss_info(floor).name.c_str(),
            get_boss_info(floor).max_hp, get_boss_info(floor).attack,
            get_boss_info(floor).pdef);
    } else {
        int monster_count = (int)monsters.size();
        LOG_INFO("进入第%d层 - %d只怪物, 难度倍率: HPx%.2f ATKx%.2f",
            floor, monster_count,
            FLOOR_MONSTER_HP_MULT[std::min(floor - 1, 14)],
            FLOOR_MONSTER_ATK_MULT[std::min(floor - 1, 14)]);
    }
}

void GameScene::_spawn_floor_monsters(const std::vector<std::pair<int,int>>& rooms) {
    int idx = std::min(current_floor - 1, 14);
    int count = FLOOR_MONSTER_COUNT[idx];
    float hp_m = FLOOR_MONSTER_HP_MULT[idx];
    float atk_m = FLOOR_MONSTER_ATK_MULT[idx];

    int ri = 1;
    while ((int)monsters.size() < count && ri < 500) {
        auto [tx, ty] = rooms[ri % rooms.size()];
        int off_x = (int)(rng() % 5) - 2;
        int off_y = (int)(rng() % 5) - 2;
        int stx = tx + off_x, sty = ty + off_y;
        if (game_map->is_walkable(stx, sty)) {
            auto [px, py] = game_map->tile_to_pixel(stx, sty);
            const char* type = (rng() % 3 == 0) ? "orc" : "slime";
            auto* m = spawn_monster(px, py, type);
            m->combat.max_hp = (int)(m->combat.max_hp * hp_m);
            m->combat.current_hp = m->combat.max_hp;
            m->combat.attack = (int)(m->combat.attack * atk_m);
            m->entity.sync_rect();
            monsters.emplace_back(m);
        }
        ri++;
    }
}

// ============================================================
// 主循环
// ============================================================
void GameScene::_process(double delta) {
    if (!player) return;
    float dt = (float)delta;

    if (state == GameState::BOSS_CINEMATIC) {
        boss_cinematic_timer -= dt;
        if (boss_cinematic_timer <= 0) {
            boss_cinematic_timer = 0;
            state = GameState::PLAYING;
        }
    }

    if (state != GameState::PLAYING) return;
    game_time += dt;

    // ── Buff 逐帧结算 ──
    std::vector<BuffEvent> buf_events;
    tick_buffs(player.get(), dt, &buf_events);
    for (auto& m : monsters) tick_buffs(m.get(), dt, &buf_events);

    // Buff 事件日志
    for (auto& ev : buf_events) {
        if (ev.type == BuffEventType::APPLIED)
            LOG_INFO("[BUF] %s applied to %s (stacks=%d)", ev.buff_id.c_str(), ev.target.c_str(), ev.stacks);
        else if (ev.type == BuffEventType::TICK_DAMAGE)
            LOG_INFO("[BUF] %s tick on %s: %d dmg (stacks=%d)", ev.buff_id.c_str(), ev.target.c_str(), ev.value, ev.stacks);
        else if (ev.type == BuffEventType::EXPIRED)
            LOG_INFO("[BUF] %s expired from %s", ev.buff_id.c_str(), ev.target.c_str());
    }

    // 清理被毒死的怪物
    _cleanup_dead_monsters();

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
        }
    }

    if (!player->combat.is_alive) {
        LOG_INFO("玩家死亡! 第%d层 Lv%d - 存档已保留", current_floor, player->level);
        auto ds = std::make_shared<DeathScene>();
        ds->name = "DeathScene";
        ds->final_floor = current_floor;
        ds->final_level = player->level;
        get_tree()->change_scene(ds);
        return;
    }

    if (!inventory_open) {
        // 玩家移动
        Vector2 move = player->handle_input(get_tree()->get_input());
        auto& e = player->entity;
        float s = get_effective_speed(player.get()) * dt;
        e.position.x += move.x * s; e.sync_rect();
        if (!game_map->is_rect_walkable(e.rect)) { e.position.x -= move.x * s; e.sync_rect(); }
        e.position.y += move.y * s; e.sync_rect();
        if (!game_map->is_rect_walkable(e.rect)) { e.position.y -= move.y * s; e.sync_rect(); }

        // 怪物 AI（时停期间冻结）
        if (time_stop_remaining <= 0) {
            _update_monsters(dt);
            _check_floor_transition();
        }

        // 自愈 Lv3 持续回复
        for (auto& sk : player->skills.active_skills) {
            if (auto* h = dynamic_cast<SelfHealSkill*>(sk.get()))
                h->tick_regen(player.get(), dt);
        }
    }

    // VFX 更新
    for (auto& fx : active_effects) fx.elapsed += dt;
    active_effects.erase(std::remove_if(active_effects.begin(), active_effects.end(),
        [](auto& fx) { return fx.elapsed >= fx.duration; }), active_effects.end());
}

// ============================================================
// 输入处理
// ============================================================
void GameScene::_input(const InputMap& input) {
    if (!player) return;
    auto* tree = get_tree();
    if (!tree) return;

    if (input.is_action_just_pressed("cancel")) {
        if (state == GameState::PLAYING && player->combat.is_alive) {
            max_unlocked_floor = std::max(max_unlocked_floor, current_floor);
            SaveManager::save_game(player.get(), current_floor, max_unlocked_floor);
            LOG_INFO("中途退出→存档 (第%d层)", current_floor);
        }
        // 切换到标题场景
        auto ts = std::make_shared<TitleScene>();
        ts->name = "TitleScene";
        ts->has_save = SaveManager::save_exists();
        tree->change_scene(ts);
        return;
    }
    if (state == GameState::BOSS_INTRO) {
        if (input.is_action_just_pressed("confirm")) {
            auto pos = game_map->tile_to_pixel(stairs_pos.first, stairs_pos.second);
            auto* boss = spawn_boss(stairs_pos.first, stairs_pos.second, boss_floor);
            monsters.emplace_back(boss);
            boss_cinematic_timer = 1.0f;
            state = GameState::BOSS_CINEMATIC;
        }
        return;
    }

    if (state != GameState::PLAYING) return;

#ifdef _DEBUG
    _handle_debug_buff_test_input();   // F1-F6 Buff 调试
#endif

    if (inventory_open) {
        if (input.is_action_just_pressed("inventory") || input.is_action_just_pressed("cancel"))
            inventory_open = false;
        else if (input.is_action_just_pressed("move_up"))
            inventory_cursor = std::max(0, inventory_cursor - 1);
        else if (input.is_action_just_pressed("move_down"))
            inventory_cursor = std::min(std::max(0, (int)player->inventory.items.size() - 1), inventory_cursor + 1);
        else if (IsKeyPressed(KEY_X))
            { player->inventory.equip(inventory_cursor, player.get()); inventory_cursor = std::min(inventory_cursor, std::max(0, (int)player->inventory.items.size() - 1)); }
        else if (IsKeyPressed(KEY_U))
            { player->inventory.use_item(inventory_cursor, player.get()); inventory_cursor = std::min(inventory_cursor, std::max(0, (int)player->inventory.items.size() - 1)); }
        else if (IsKeyPressed(KEY_D))
            { player->inventory.remove(inventory_cursor); inventory_cursor = std::min(inventory_cursor, std::max(0, (int)player->inventory.items.size() - 1)); }
        return;
    }

    if (input.is_action_just_pressed("attack")) _player_attack();
    else if (input.is_action_just_pressed("pickup")) _pickup();
    else if (input.is_action_just_pressed("inventory")) { inventory_open = true; inventory_cursor = 0; }
    else if (input.is_action_just_pressed("skill_1")) _use_skill(0);
    else if (input.is_action_just_pressed("skill_2")) _use_skill(1);
    else if (input.is_action_just_pressed("skill_3")) _use_skill(2);
    else if (input.is_action_just_pressed("skill_4")) _use_skill(3);
}

// ============================================================
// Debug Buff 测试入口 (仅 _DEBUG 构建)
// ============================================================
void GameScene::_handle_debug_buff_test_input() {
    if (!player) return;
    if (IsKeyPressed(KEY_F1))
        apply_buff(player.get(), "attack_up", 1);
    if (IsKeyPressed(KEY_F2))
        apply_buff(player.get(), "slow", 1);
    if (IsKeyPressed(KEY_F3) && !monsters.empty())
        apply_buff(monsters[0].get(), "poison", 2);
    if (IsKeyPressed(KEY_F4)) {
        auto dump = [](const std::vector<BuffInstance>& buffs, const char* who) {
            if (buffs.empty()) { LOG_INFO("[F4] %s buffs: (none)", who); return; }
            for (auto& b : buffs)
                LOG_INFO("[F4] %s buff: %s stacks=%d rem=%.2f tick=%.2f",
                    who, b.id.c_str(), b.stacks, b.remaining, b.tick_timer);
        };
        dump(player->active_buffs, "Player");
        if (!monsters.empty()) dump(monsters[0]->active_buffs, "Monster[0]");
    }
    if (IsKeyPressed(KEY_F5) && !monsters.empty())
        apply_buff(monsters[0].get(), "slow", 2);
    if (IsKeyPressed(KEY_F6) && !monsters.empty())
        apply_buff(monsters[0].get(), "attack_up", 2);
}

// ============================================================
// 战斗
// ============================================================
void GameScene::_player_attack() {
    if (!player->combat.is_alive) return;
    if (!player->can_attack(game_time)) return;

    auto* target = find_attack_target(player->entity.rect,
        reinterpret_cast<const std::vector<Monster*>&>(monsters), PLAYER_ATTACK_RANGE);
    if (!target) return;

    int dmg = calculate_damage(get_effective_attack(player.get()),
        target->combat.get_effective_defense(player->attack_type));
    player->_last_attack_time = game_time;

    get_tree()->get_audio()->play_sfx("melee");

    if (time_stop_remaining > 0) {
        pending_damage.emplace_back(target, dmg);
    } else {
        target->combat.take_damage(dmg);
        get_tree()->get_audio()->play_sfx("hit");
        VFXServer v;
        v.hit_flash(target->entity.position.x, target->entity.position.y, target->entity.size.x);
        for (auto& e : v.effects) active_effects.push_back(e);
        if (!target->combat.is_alive) {
            _on_monster_killed(target);
            auto it = std::find_if(monsters.begin(), monsters.end(),
                [&](auto& m) { return m.get() == target; });
            if (it != monsters.end()) monsters.erase(it);
        }
    }

    // 玩家普攻 VFX
    VFXServer vfx;
    vfx.player_attack(player->entity.rect.x + player->entity.rect.width/2,
                      player->entity.rect.y + player->entity.rect.height/2,
                      PLAYER_ATTACK_RANGE * TILE_SIZE);
    for (auto& e : vfx.effects) active_effects.push_back(e);
}

void GameScene::_use_skill(int index) {
    if (index >= (int)player->skills.active_skills.size()) return;
    auto& sk = player->skills.active_skills[index];
    if (!sk->can_use(game_time)) return;

    // The World 时停
    if (dynamic_cast<TheWorldSkill*>(sk.get())) {
        std::vector<Monster*> mlist;
        for (auto& m : monsters) mlist.push_back(m.get());
        sk->execute(player.get(), mlist, game_map.get());
        sk->mark_used(game_time);
        time_stop_remaining = static_cast<TheWorldSkill*>(sk.get())->get_stop_duration();
        pending_damage.clear();
        get_tree()->get_audio()->play_sfx("timestop", 1.0f);
        VFXServer vfx;
        vfx.time_stop(player->entity.rect.x + player->entity.rect.width/2,
                      player->entity.rect.y + player->entity.rect.height/2);
        for (auto& e : vfx.effects) active_effects.push_back(e);
        return;
    }

    // 时停期间的技能：伤害暂存
    if (time_stop_remaining > 0) {
        std::unordered_map<Monster*, int> pre_hp;
        for (auto& m : monsters) pre_hp[m.get()] = m->combat.current_hp;

        std::vector<Monster*> mlist;
        for (auto& m : monsters) mlist.push_back(m.get());
        sk->execute(player.get(), mlist, game_map.get());
        sk->mark_used(game_time);

        for (auto& m : monsters) {
            int delta = pre_hp[m.get()] - m->combat.current_hp;
            if (delta > 0) {
                pending_damage.emplace_back(m.get(), delta);
                m->combat.current_hp = pre_hp[m.get()];
                m->combat.is_alive = true;
            }
        }
        return;
    }

    // 普通技能
    std::vector<Monster*> mlist;
    for (auto& m : monsters) mlist.push_back(m.get());
    sk->execute(player.get(), mlist, game_map.get());
    sk->mark_used(game_time);

    // 技能 SFX (匹配Python版: slash/bolt/heal)
    if (dynamic_cast<SlashSkill*>(sk.get()))
        get_tree()->get_audio()->play_sfx("slash");
    else if (dynamic_cast<FireballSkill*>(sk.get()))
        get_tree()->get_audio()->play_sfx("bolt");
    else if (dynamic_cast<SelfHealSkill*>(sk.get()))
        get_tree()->get_audio()->play_sfx("heal");

    // 技能 VFX (匹配Python版 fx_engine.py)
    VFXServer vfx;
    float cx = player->entity.rect.x + player->entity.rect.width/2;
    float cy = player->entity.rect.y + player->entity.rect.height/2;
    if (dynamic_cast<SlashSkill*>(sk.get())) {
        vfx.slash_skill(cx, cy, player->direction, sk->level);
    } else if (dynamic_cast<FireballSkill*>(sk.get())) {
        auto t = find_attack_target(player->entity.rect,
            reinterpret_cast<const std::vector<Monster*>&>(monsters), 10.0f);
        float tx = t ? t->entity.rect.x + t->entity.rect.width/2 : cx + 100;
        float ty = t ? t->entity.rect.y + t->entity.rect.height/2 : cy;
        vfx.fireball(cx, cy, tx, ty, sk->level);
    } else if (dynamic_cast<SelfHealSkill*>(sk.get())) {
        vfx.heal(cx, cy, sk->level);
    }
    for (auto& e : vfx.effects) active_effects.push_back(e);

    // 清理死亡怪物
    auto it = monsters.begin();
    while (it != monsters.end()) {
        if (!(*it)->combat.is_alive) {
            _on_monster_killed((*it).get());
            it = monsters.erase(it);
        } else ++it;
    }
}

void GameScene::_update_monsters(float dt) {
    int hp_before = player->combat.current_hp;
    std::vector<Monster*> mlist;
    for (auto& m : monsters) mlist.push_back(m.get());
    for (auto& m : monsters)
        m->update_ai(player.get(), game_map.get(), dt, game_time, &mlist, &active_effects);
    // 检测玩家死亡
    // if (player->combat.current_hp < hp_before) { /* SFX */ }
}

void GameScene::_on_monster_killed(Monster* m) {
    // XP
    int xp = m->is_boss ? XP_PER_KILL_BOSS
                        : XP_PER_KILL_BASE + (int)(m->combat.max_hp * 0.5f);
    // Level up logic
    player->xp += xp;
    while (player->xp >= player->xp_to_next) {
        player->level++;
        player->xp -= player->xp_to_next;
        player->xp_to_next = Player::calc_xp_for_level(player->level + 1);
        player->combat.attack += 2;
        player->combat.physical_defense += 1;
        player->combat.magical_defense += 1;
        player->combat.max_hp += 10;
        player->combat.current_hp = player->combat.max_hp;
        get_tree()->get_audio()->play_sfx("levelup");

        LOG_INFO("玩家升级! Lv%d HP:%d ATK:%d PD:%d MD:%d XP:%d/%d",
            player->level, player->combat.max_hp,
            player->combat.get_effective_attack(),
            player->combat.get_effective_defense(AttackType::PHYSICAL),
            player->combat.get_effective_defense(AttackType::MAGICAL),
            player->xp, player->xp_to_next);

        if (player->skills.can_learn()) {
            auto names = get_learned_names(player->skills);
            auto sk = random_active_skill(names);
            std::string skill_name = sk->name;
            player->skills.learn(std::move(sk));
            player->skills.apply_all_passives(player.get());
            LOG_INFO("觉醒新技能: %s", skill_name.c_str());
        }
        on_player_leveled.emit(player->level);
    }

    // Boss 奖励
    if (m->is_boss) {
        LOG_INFO("Boss击杀! %s - 第%d层", m->name.c_str(), current_floor);
        get_tree()->get_audio()->play_sfx("victory");
        _drop_boss_reward(m);
    }

    // 普通掉落
    else if ((float)(rng() % 1000) / 1000.0f < LOOT_DROP_CHANCE) {
        auto [tx, ty] = game_map->pixel_to_tile(m->entity.position.x, m->entity.position.y);
        ground_items.push_back({generate_random_item(), tx, ty});
    }

    _check_floor_clear();
}

void GameScene::_cleanup_dead_monsters() {
    auto it = monsters.begin();
    while (it != monsters.end()) {
        if (!(*it)->combat.is_alive) {
            _on_monster_killed(it->get());
            it = monsters.erase(it);
        } else ++it;
    }
    _check_floor_clear();
}

void GameScene::_check_floor_clear() {
    bool all_dead = true;
    for (auto& m : monsters) if (m->combat.is_alive) { all_dead = false; break; }
    if (all_dead && !stairs_active) _activate_stairs();
}

void GameScene::_activate_stairs() {
    if (stairs_active) return;
    auto boss = _get_boss();
    if (boss && boss->combat.is_alive) return;
    game_map->set_tile(stairs_pos.first, stairs_pos.second, TileType::STAIRS_DOWN);
    stairs_active = true;
    max_unlocked_floor = std::max(max_unlocked_floor, current_floor);
    LOG_INFO("第%d层清空! 楼梯已激活, 自动存档", current_floor);
    SaveManager::save_game(player.get(), current_floor, max_unlocked_floor);
    on_floor_cleared.emit();
}

void GameScene::_check_floor_transition() {
    if (!stairs_active || !player) return;
    auto [tx, ty] = game_map->pixel_to_tile(
        player->entity.rect.x + player->entity.rect.width/2,
        player->entity.rect.y + player->entity.rect.height/2);
    if (std::make_pair(tx, ty) != stairs_pos) return;

    auto& input = get_tree()->get_input();
    if (!input.is_action_just_pressed("descend")) return;

    if (current_floor >= MAX_FLOORS) {
        LOG_INFO("通关! 最终第%d层 Lv%d", current_floor, player->level);
        auto vs = std::make_shared<VictoryScene>();
        vs->name = "VictoryScene";
        vs->final_level = player->level;
        get_tree()->change_scene(vs);
        return;
    } else {
        enter_floor(current_floor + 1);
    }
}

void GameScene::_apply_pending_damage() {
    for (auto& [m, dmg] : pending_damage) {
        if (m && m->combat.is_alive) m->combat.take_damage(dmg);
    }
    auto it = monsters.begin();
    while (it != monsters.end()) {
        if (!(*it)->combat.is_alive) {
            _on_monster_killed(it->get());
            it = monsters.erase(it);
        } else ++it;
    }
    pending_damage.clear();
}

// ============================================================
// 拾取
// ============================================================
void GameScene::_pickup() {
    DroppedItem* best = nullptr;
    float best_dist = PICKUP_RANGE * TILE_SIZE;
    Rectangle pr = player->entity.rect;
    for (auto& d : ground_items) {
        float px = d.tile_x * TILE_SIZE + TILE_SIZE / 2;
        float py = d.tile_y * TILE_SIZE + TILE_SIZE / 2;
        float dist = sqrtf(powf(pr.x + pr.width/2 - px, 2) + powf(pr.y + pr.height/2 - py, 2));
        if (dist < best_dist) { best_dist = dist; best = &d; }
    }
    if (!best) return;
    if (player->inventory.add(best->item, player.get())) {
        LOG_DEBUG("拾取: %s", best->item->get_description().c_str());
        auto it = std::find_if(ground_items.begin(), ground_items.end(),
            [&](auto& d) { return &d == best; });
        if (it != ground_items.end()) ground_items.erase(it);
    }
}

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
    if (state == GameState::BOSS_INTRO) { _draw_boss_intro(); return; }

    ClearBackground(BLACK);
    _update_camera();
    _draw_map();
    _draw_ground_items();
    _draw_entities();
    _draw_effects();  // 特效在最上层
    _draw_hud();

    if (inventory_open) _draw_inventory_panel();
    if (time_stop_remaining > 0) _draw_time_stop_overlay();
    if (state == GameState::BOSS_CINEMATIC) _draw_boss_cinematic_overlay();
}

void GameScene::_update_camera() {
    if (!player) return;
    int sw = get_tree()->get_width(), sh = get_tree()->get_height();
    _cam_x = player->entity.rect.x + player->entity.rect.width/2 - sw/2;
    _cam_y = player->entity.rect.y + player->entity.rect.height/2 - sh/2;
    if (game_map) {
        _cam_x = std::max(0.0f, std::min(_cam_x, (float)game_map->pixel_width - sw));
        _cam_y = std::max(0.0f, std::min(_cam_y, (float)game_map->pixel_height - sh));
    }
}

void GameScene::_draw_map() {
    if (game_map) game_map->draw(_cam_x, _cam_y, get_tree()->get_width(), get_tree()->get_height());
}

void GameScene::_draw_entities() {
    for (auto& m : monsters) {
        m->draw(_cam_x, _cam_y);
        _draw_monster_buffs(*m,
            m->entity.position.x - _cam_x,
            m->entity.position.y - _cam_y);
    }
    if (player) player->draw_no_cam(_cam_x, _cam_y);
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

void GameScene::_draw_hud() {
    if (!player) return;
    int sw = get_tree()->get_width();
    auto& c = player->combat;

    // 玩家血条
    float hp_r = (float)c.current_hp / c.max_hp;
    Color hp_c = hp_r > 0.5f ? Color{50, 200, 50, 255}
               : hp_r > 0.25f ? Color{200, 200, 50, 255} : Color{200, 50, 50, 255};
    draw_progress_bar({10, 10, 200, 16}, hp_r, hp_c, {40, 20, 20, 255});

    if (g_font_loaded) {
        char buf[128];
        snprintf(buf, sizeof(buf), "HP:%d/%d ATK:%d PD:%d MD:%d",
            c.current_hp, c.max_hp, c.get_effective_attack(),
            c.get_effective_defense(AttackType::PHYSICAL),
            c.get_effective_defense(AttackType::MAGICAL));
        DrawTextEx(g_font_small, buf, {215, 10}, 16, 1, {220, 220, 220, 255});
    }

    // XP 条
    float xp_r = (float)player->xp / player->xp_to_next;
    draw_progress_bar({10, 30, 200, 10}, xp_r, {80, 120, 255, 255});

    if (g_font_loaded) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Lv%d XP:%d/%d", player->level, player->xp, player->xp_to_next);
        DrawTextEx(g_font_small, buf, {12, 42}, 13, 1, {180, 200, 255, 255});
    }

    // 楼层
    if (g_font_loaded) {
        char buf[32];
        snprintf(buf, sizeof(buf), "第%d/%d层", current_floor, MAX_FLOORS);
        DrawTextEx(g_font_small, buf, {220, 42}, 16, 1, {200, 200, 50, 255});
    }

    // Boss 血条
    auto* boss = _get_boss();
    if (boss) {
        float bw = 400, bh = 20;
        float bx = sw / 2 - bw / 2, by = 4;
        draw_progress_bar({bx, by, bw, bh},
            (float)boss->combat.current_hp / boss->combat.max_hp,
            {220, 100, 30, 255}, {30, 5, 5, 255});
        if (g_font_loaded) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%s HP:%d/%d",
                boss->name.c_str(), boss->combat.current_hp, boss->combat.max_hp);
            float tw = MeasureTextEx(g_font_small, buf, 18, 1).x;
            DrawTextEx(g_font_small, buf, {bx + (bw - tw) / 2, by + bh + 3}, 18, 1, {255, 220, 100, 255});
        }
    }

    // 技能栏
    _draw_skill_bar();
    // 玩家 Buff (技能栏下方)
    _draw_player_buffs();
}

void GameScene::_draw_skill_bar() {
    auto& active = player->skills.active_skills;
    if (active.empty() || !g_font_loaded) return;
    float x = 10, y = 56;
    for (int i = 0; i < (int)active.size(); i++) {
        float ry = y + i * 28;
        bool ready = active[i]->can_use(game_time);
        Color bg = ready ? Color{50, 160, 50, 255} : Color{60, 60, 60, 255};
        DrawRectangleRounded({x, ry, 22, 18}, 0.2f, 3, bg);
        DrawTextEx(g_font_small, std::to_string(i + 1).c_str(), {x + 7, ry + 1}, 14, 1, WHITE);

        std::string label = active[i]->name + " " + active[i]->get_level_text();
        DrawTextEx(g_font_small, label.c_str(), {x + 26, ry + 2}, 14, 1,
                   ready ? Color{180, 220, 255, 255} : Color{100, 100, 100, 255});

        float cd_r = 1.0f - active[i]->remaining_cooldown(game_time) / active[i]->cooldown;
        draw_progress_bar({x + 26, ry + 16, 90, 8}, cd_r,
                          ready ? Color{60, 180, 255, 255} : Color{70, 70, 70, 255});
    }
}

// ============================================================
// 玩家 Buff HUD — 技能栏下方，全名 + 层数 + 剩余时间
// ============================================================
void GameScene::_draw_player_buffs() {
    if (!player || player->active_buffs.empty() || !g_font_loaded) return;
    auto& buffs = player->active_buffs;
    float x = 10;
    float y = 56.0f + player->skills.active_skills.size() * 28.0f + 4.0f;
    for (auto& b : buffs) {
        std::string line = get_buff_display_name(b.id)
            + " x" + std::to_string(b.stacks)
            + "  " + format_buff_time(b.remaining);
        DrawTextEx(g_font_small, line.c_str(), {x, y}, 14, 1, get_buff_hud_color(b.id));
        y += 18;
    }
}

// ============================================================
// 怪物 Buff 标签 — 怪物头顶简写
// ============================================================
void GameScene::_draw_monster_buffs(const Monster& m, float draw_x, float draw_y) {
    if (m.active_buffs.empty() || !g_font_loaded) return;
    std::string label;
    for (auto& b : m.active_buffs) {
        if (!label.empty()) label += " ";
        label += get_buff_short_name(b.id) + "x" + std::to_string(b.stacks);
    }
    float tw = MeasureTextEx(g_font_small, label.c_str(), 12, 1).x;
    float px = draw_x + (m.entity.size.x - tw) / 2;
    float py = draw_y - 16;
    Color c = get_buff_hud_color(m.active_buffs[0].id);
    DrawTextEx(g_font_small, label.c_str(), {px + 1, py + 1}, 12, 1, {0, 0, 0, 180});
    DrawTextEx(g_font_small, label.c_str(), {px, py}, 12, 1, c);
}

void GameScene::_draw_inventory_panel() {
    int sw = get_tree()->get_width(), sh = get_tree()->get_height();
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, 180});
    float pw = 440, ph = 480;
    Rectangle pr = {sw / 2.0f - pw / 2, sh / 2.0f - ph / 2, pw, ph};
    draw_panel(pr, "背包 I关闭");

    auto& inv = player->inventory;
    float y0 = pr.y + 40;

    // 装备槽
    std::string eq = "装备: weapon:";
    eq += inv.equipped["weapon"] ? inv.equipped["weapon"]->get_description() : "空";
    eq += " armor:";
    eq += inv.equipped["armor"] ? inv.equipped["armor"]->get_description() : "空";
    if (g_font_loaded)
        DrawTextEx(g_font_small, eq.c_str(), {pr.x + 30, y0}, 18, 1, {255, 200, 50, 255});
    DrawLine(pr.x + 30, y0 + 26, pr.x + pw - 30, y0 + 26, {60, 60, 90, 255});

    // 物品列表
    for (int i = 0; i < (int)inv.items.size(); i++) {
        float ry = y0 + 38 + i * 30;
        std::string mk = (i == inventory_cursor) ? ">" : " ";
        char idx[4]; snprintf(idx, sizeof(idx), "%2d", i + 1);
        std::string txt = mk + " [" + idx + "] " + inv.items[i]->get_description();
        if (g_font_loaded)
            DrawTextEx(g_font_small, txt.c_str(), {pr.x + 30, ry}, 18, 1, inv.items[i]->color);
    }
    if (g_font_loaded) {
        DrawTextEx(g_font_small, "^v择 X装 U用 D丢 I关",
                   {pr.x + (pw - 220) / 2, pr.y + ph - 30}, 16, 1, {140, 140, 140, 255});
    }
}

void GameScene::_draw_effects() {
    for (auto& e : active_effects) {
        float sx = e.world_x - _cam_x, sy = e.world_y - _cam_y;
        float alpha = 1.0f - e.elapsed / e.duration;
        if (alpha <= 0) continue;
        Color c = e.color; c.a = (unsigned char)(c.a * alpha);

        if (e.kind == "pulse") {
            float r = e.radius * (0.5f + 0.5f * e.elapsed / e.duration);
            DrawRing({sx, sy}, r * 0.6f, r, 0, 360, 24, c);
        } else if (e.kind == "spark") {
            DrawCircle(sx, sy, e.radius, c);
        } else if (e.kind == "bolt") {
            DrawLineEx({sx, sy}, {e.target_x - _cam_x, e.target_y - _cam_y}, 2, c);
        } else if (e.kind == "flash") {
            DrawCircle(sx, sy, e.radius, Fade(c, 0.3f));
        } else if (e.kind == "slash_arc") {
            // 朝向锥形弧线
            float arc_r = e.radius * (0.6f + 0.4f * e.elapsed / e.duration);
            float startAngle = 0, endAngle = 180;
            switch (e.direction) {
                case Direction::DOWN:  startAngle = 0;   break;
                case Direction::UP:    startAngle = 180; break;
                case Direction::RIGHT: startAngle = 270; break;
                case Direction::LEFT:  startAngle = 90;  break;
            }
            endAngle = startAngle + 120;
            DrawRing({sx, sy}, arc_r * 0.4f, arc_r, startAngle, endAngle, 12, c);
            DrawLineEx({sx, sy}, {sx + cosf((startAngle+60)*DEG2RAD)*arc_r, sy + sinf((startAngle+60)*DEG2RAD)*arc_r}, 3, c);
            // 散布火花
            for (int i = 0; i < 5; i++) {
                float a = (startAngle + (float)(rand()%120)) * DEG2RAD;
                float dist = arc_r * (0.5f + (float)(rand()%50)/100.0f);
                DrawCircle(sx + cosf(a)*dist, sy + sinf(a)*dist, 2.5f, Fade(c, 0.5f));
            }
        } else if (e.kind == "cone") {
            // 矩形锥形 (Boss技能等)
            DrawRectangleLines(sx - e.radius/2, sy - e.radius/4, e.radius, e.radius/2, c);
        }
    }
}

void GameScene::_draw_time_stop_overlay() {
    int sw = get_tree()->get_width(), sh = get_tree()->get_height();
    DrawRectangle(0, 0, sw, sh, {90, 90, 100, 130});
    int remain = (int)time_stop_remaining;
    char buf[4]; snprintf(buf, sizeof(buf), "%d", remain);
    draw_glow_text(buf, sw / 2.0f, sh / 2.0f, 80, WHITE, true);
    draw_glow_text("The World · 时停", sw / 2.0f, 60, 24, WHITE, true);
}

void GameScene::_draw_boss_cinematic_overlay() {
    int sw = get_tree()->get_width(), sh = get_tree()->get_height();
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, 160});
    float pulse = 1.0f + sinf((float)GetTime() * 8) * 0.08f;
    draw_glow_text("BOSS 来了！", sw / 2.0f, sh / 2.0f, 48, {230, 50, 50, 255}, true);
}

void GameScene::_draw_boss_intro() {
    ClearBackground(BLACK);
    int sw = get_tree()->get_width(), sh = get_tree()->get_height();
    float pw = 500, ph = 380;
    Rectangle pr = {sw / 2.0f - pw / 2, sh / 2.0f - ph / 2, pw, ph};
    draw_panel(pr, "! Boss 遭遇 !");

    draw_glow_text(boss_intro_title.c_str(), sw / 2.0f, pr.y + 45, 30, boss_intro_color, true);

    if (g_font_loaded) {
        auto info = get_boss_info(boss_floor);
        DrawTextEx(g_font_small, boss_intro_skills.c_str(), {pr.x + 80, pr.y + 160}, 16, 1, {180, 180, 180, 255});
        DrawTextEx(g_font, boss_intro_lore.c_str(), {pr.x + 40, pr.y + 210}, 18, 1, {160, 160, 180, 255});
    }
    draw_glow_text("按 Enter 进入战斗...", sw / 2.0f, (float)(sh - 60), 20, {140, 20, 20, 255}, true);
}
