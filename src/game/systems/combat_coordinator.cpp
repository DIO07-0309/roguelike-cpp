#include "combat_coordinator.h"
#include "player.h"
#include "monster.h"
#include "game_map.h"
#include "skill.h"
#include "item.h"
#include "combat_system.h"
#include "vfx_server.h"
#include "audio_server.h"
#include "config.h"
#include "core/logger.h"
#include <cmath>

void CombatCoordinator::player_attack(Player* player,
                                       std::vector<std::unique_ptr<Monster>>& monsters,
                                       std::vector<Effect>& effects, AudioServer* audio) {
    if (!player || !player->can_attack(GetTime())) return;
    player->_last_attack_time = (float)GetTime();

    Monster* target = find_attack_target(player->entity.rect,
        reinterpret_cast<const std::vector<Monster*>&>(monsters),
        PLAYER_ATTACK_RANGE);
    if (!target) return;

    int dmg = calculate_damage(player->combat.get_effective_attack(),
        target->combat.get_effective_defense(AttackType::PHYSICAL));
    target->combat.take_damage(dmg);

    if (audio) audio->play_sfx("melee");

    // 命中 VFX
    VFXServer vfx;
    vfx.hit_flash(target->entity.rect.x + target->entity.rect.width / 2,
                  target->entity.rect.y + target->entity.rect.height / 2,
                  target->entity.rect.width);
    vfx.player_attack(player->entity.rect.x + player->entity.rect.width / 2,
                      player->entity.rect.y + player->entity.rect.height / 2,
                      PLAYER_ATTACK_RANGE * TILE_SIZE);
    for (auto& e : vfx.effects) effects.push_back(e);

    // 清理死亡怪物 (不触发击杀逻辑，仅移除)
    auto it = monsters.begin();
    while (it != monsters.end()) {
        if (!(*it)->combat.is_alive) {
            it = monsters.erase(it);
        } else ++it;
    }
}

void CombatCoordinator::apply_attack_damage(Monster* target, int dmg,
                                            std::vector<Effect>& effects, AudioServer* audio) {
    target->combat.take_damage(dmg);
    if (audio) audio->play_sfx("hit");
    VFXServer v;
    v.hit_flash(target->entity.position.x, target->entity.position.y, target->entity.size.x);
    for (auto& e : v.effects) effects.push_back(e);
}

void CombatCoordinator::skill_heavy_vfx(Player* player, const std::string&,
                                         std::vector<Effect>& effects) {
    VFXServer vfx;
    float cx = player->entity.rect.x + player->entity.rect.width / 2;
    float cy = player->entity.rect.y + player->entity.rect.height / 2;
    vfx.boss_circle(cx, cy);  // 复用 Boss AOE 特效作为 Heavy 技能反馈
    for (auto& e : vfx.effects) effects.push_back(e);
}

std::string CombatCoordinator::use_skill(int index, Player* player,
                                          std::vector<std::unique_ptr<Monster>>& monsters,
                                          GameMap* map, std::vector<Effect>& effects,
                                          AudioServer* audio, double game_time,
                                          float& time_stop_remaining,
                                          std::vector<std::pair<Monster*, int>>& pending_damage,
                                          bool is_heavy) {
    if (!player || index >= (int)player->skills.active_skills.size()) return "";
    auto& sk = player->skills.active_skills[index];
    if (!sk->can_use(game_time)) return "";

    std::vector<Monster*> mlist;
    for (auto& m : monsters) mlist.push_back(m.get());

    // The World 时停 special-case (D2: Heavy 时停加长; D3 Step3: Evo)
    if (dynamic_cast<TheWorldSkill*>(sk.get())) {
        auto* tw = static_cast<TheWorldSkill*>(sk.get());
        sk->execute(player, mlist, map, is_heavy);
        sk->mark_used(game_time);
        float dur = tw->get_stop_duration();
        if (is_heavy) dur *= 1.3f;
        // D3 Step3 E1: 时停+20%
        if (sk->evolution_level >= 1) dur *= 1.2f;
        time_stop_remaining = dur;
        pending_damage.clear();
        if (audio) audio->play_sfx("timestop", 1.0f);
        VFXServer vfx;
        vfx.time_stop(player->entity.rect.x + player->entity.rect.width / 2,
                      player->entity.rect.y + player->entity.rect.height / 2);
        for (auto& e : vfx.effects) effects.push_back(e);
        return is_heavy ? "重·The World: 时间停止了!" : "The World: 时间停止了!";
    }

    // 时停期间技能：暂存伤害
    if (time_stop_remaining > 0) {
        std::unordered_map<Monster*, int> pre_hp;
        for (auto& m : monsters) pre_hp[m.get()] = m->combat.current_hp;
        sk->execute(player, mlist, map, is_heavy);
        sk->mark_used(game_time);
        for (auto& m : monsters) {
            int delta = pre_hp[m.get()] - m->combat.current_hp;
            if (delta > 0) {
                pending_damage.emplace_back(m.get(), delta);
                m->combat.current_hp = pre_hp[m.get()];
                m->combat.is_alive = true;
            }
        }
        return "";
    }

    // 普通/Heavy 技能
    std::string result = sk->execute(player, mlist, map, is_heavy);
    sk->mark_used(game_time);

    // 技能 SFX (G3.2: _skill_id 替代 dynamic_cast)
    if (audio) {
        const std::string& sid = sk->_skill_id;
        if (sid == "slash" || sid == "shadow_strike" || sid == "blood_frenzy")
            audio->play_sfx("slash");
        else if (sid == "fireball" || sid == "ice_nova" || sid == "chain_lightning")
            audio->play_sfx("bolt");
        else if (sid == "self_heal") audio->play_sfx("heal");
        else if (sid == "summon_spirit") audio->play_sfx("bolt");
        if (is_heavy) audio->play_sfx("melee");
    }

    // 技能 VFX (G3.2: _skill_id 替代 dynamic_cast)
    VFXServer vfx;
    float cx = player->entity.rect.x + player->entity.rect.width / 2;
    float cy = player->entity.rect.y + player->entity.rect.height / 2;
    const std::string& sid_v = sk->_skill_id;
    if (sid_v == "slash" || sid_v == "shadow_strike" || sid_v == "blood_frenzy") {
        int lvl = is_heavy ? sk->level + 1 : sk->level;
        vfx.slash_skill(cx, cy, player->direction, lvl);
    } else if (sid_v == "fireball" || sid_v == "ice_nova" || sid_v == "chain_lightning") {
        auto t = find_attack_target(player->entity.rect,
            reinterpret_cast<const std::vector<Monster*>&>(monsters), 10.0f);
        float tx = t ? t->entity.rect.x + t->entity.rect.width / 2 : cx + 100;
        float ty = t ? t->entity.rect.y + t->entity.rect.height / 2 : cy;
        vfx.fireball(cx, cy, tx, ty, is_heavy ? sk->level + 1 : sk->level);
    } else if (sid_v == "self_heal") {
        vfx.heal(cx, cy, sk->level);
    } else if (sid_v == "summon_spirit") {
        vfx.heal(cx, cy, 1);  // summon VFX — reuse heal ring
    }
    // D2: Heavy 额外特效环
    if (is_heavy) skill_heavy_vfx(player, sk->name, effects);
    for (auto& e : vfx.effects) effects.push_back(e);

    return result;
}

void CombatCoordinator::on_monster_killed(Monster* m, Player* player,
                                           std::vector<std::unique_ptr<Monster>>&,
                                           std::vector<DroppedItem>& ground_items,
                                           AudioServer* audio) {
    if (!m || !player) return;

    // XP (B14: Elite +50%)
    int xp = m->is_boss ? XP_PER_KILL_BOSS
                        : XP_PER_KILL_BASE + (int)(m->combat.max_hp * 0.5f);
    if (m->is_elite) xp = xp * 3 / 2;  // Elite +50% XP
    player->xp += xp;
    while (player->xp >= player->xp_to_next) {
        player->level++;
        player->xp -= player->xp_to_next;
        player->xp_to_next = Player::calc_xp_for_level(player->level + 1);
        player->combat.attack += 2;
        player->combat.physical_defense += 1;
        player->combat.magical_defense += 1;
        player->combat.max_hp += 10;
        player->combat.current_hp = get_effective_max_hp(player);
        if (audio) audio->play_sfx("levelup");

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
            player->skills.apply_all_passives(player);
            LOG_INFO("觉醒新技能: %s", skill_name.c_str());
        }
        // Signal: on_player_leveled fires (handled by GameScene)
    }

    // B11: leech_blade — 击杀时 20% 回 5 HP
    if (player_has_relic(player, "leech_blade")) {
        const RelicDef* def = get_relic_def("leech_blade");
        float chance = def ? def->param : 0.20f;
        int heal = def ? def->param2 : 5;
        if ((rng() % 1000) < (int)(chance * 1000.0f)) {
            heal_player(player, heal);
            LOG_INFO("[RELIC] 吸血之刃触发：回复 %d HP", heal);
        }
    }
    // B12: battle_totem — 击杀 15% attack_up
    if (player_has_relic(player, "battle_totem")) {
        const RelicDef* def = get_relic_def("battle_totem");
        float chance = def ? def->param : 0.15f;
        if ((rng() % 1000) < (int)(chance * 1000.0f)) {
            apply_buff(player, "attack_up", 1);
            LOG_INFO("[RELIC] 战斗图腾触发：获得 attack_up");
        }
    }
}

void CombatCoordinator::cleanup_dead_monsters(
    std::vector<std::unique_ptr<Monster>>& monsters,
    Player* player, std::vector<DroppedItem>& ground_items, AudioServer* audio) {
    auto it = monsters.begin();
    while (it != monsters.end()) {
        if (!(*it)->combat.is_alive) {
            on_monster_killed(it->get(), player, monsters, ground_items, audio);
            it = monsters.erase(it);
        } else ++it;
    }
}

void CombatCoordinator::apply_pending_damage(
    std::vector<std::pair<Monster*, int>>& pending,
    std::vector<std::unique_ptr<Monster>>& monsters,
    Player* player, std::vector<DroppedItem>& ground_items, AudioServer* audio) {
    for (auto& [m, dmg] : pending) {
        if (m && m->combat.is_alive) m->combat.take_damage(dmg);
    }
    auto it = monsters.begin();
    while (it != monsters.end()) {
        if (!(*it)->combat.is_alive) {
            on_monster_killed(it->get(), player, monsters, ground_items, audio);
            it = monsters.erase(it);
        } else ++it;
    }
    pending.clear();
}
