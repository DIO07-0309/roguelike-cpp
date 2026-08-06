#include "mirror_combat_director.h"
#include "monster.h"
#include "player.h"
#include "boss.h"
#include "combat_system.h"
#include "ai/mirror/mirror_agent.h"
#include "vfx_server.h"
#include "core/logger.h"
#include <cmath>

MirrorCombatDirector::MirrorCombatDirector() = default;

void MirrorCombatDirector::init(const Player* player, Monster* boss,
                                 BossAI* bai, MirrorAgent* agent) {
    if (!player || !boss || !bai) return;
    _boss = boss; _bai = bai;
    _agent = agent;   // M4e: 在线自适应反馈通道
    _combo_stage = 0; _combo_timer = 0; _last_attack = -99.0f;
    _skill_idx = 0; _skill_cd_timer = 0;
    _behavior_state = 0; _decision_timer = 0;
    _preferred_dist = agent ? agent->recommend_distance() : 200.0f;

    // 复制武器3段数据
    const WeaponDef* wdef = player->weapon.current_def();
    _total_stages = wdef ? wdef->stage_count : 1;
    for (int i = 0; i < _total_stages && i < 3; i++) {
        if (wdef) {
            _stage_mults[i]  = wdef->stages[i].damage_multiplier;
            _stage_ranges[i] = wdef->stages[i].range * 32.0f;
        } else {
            _stage_mults[i]  = 1.0f;
            _stage_ranges[i] = 48.0f;  // 拳头范围
        }
    }

    // 复制技能数据
    _skills.clear();
    for (auto& sk : player->skills.active_skills) {
        MirrorSkillDef ms;
        ms.name = sk->name;
        ms.cooldown = sk->cooldown;
        ms.last_used = -99.0f;
        ms.dmg_mult = 1.0f;
        ms.range = 100.0f;
        // 技能类型识别
        const auto& n = sk->name;
        if (n.find("愈") != std::string::npos ||
            n.find("Heal") != std::string::npos ||
            n.find("回") != std::string::npos) {
            ms.skill_type = 2; ms.range = 0;      // self_heal
        } else if (n.find("时停") != std::string::npos ||
                   n.find("World") != std::string::npos) {
            ms.skill_type = 4; ms.range = 999.0f; ms.dmg_mult = 0; // time_stop
        } else if (n.find("斩") != std::string::npos ||
                   n.find("Slash") != std::string::npos) {
            ms.skill_type = 0; ms.range = 96.0f; ms.dmg_mult = 1.5f; // melee
        } else if (n.find("罚") != std::string::npos ||
                   n.find("Wrath") != std::string::npos) {
            ms.skill_type = 3; ms.range = 140.0f; ms.dmg_mult = 0.8f; // aoe
        } else if (n.find("冰") != std::string::npos ||
                   n.find("Ice") != std::string::npos) {
            ms.skill_type = 1; ms.range = 200.0f; ms.dmg_mult = 1.2f; // projectile
        } else if (n.find("影") != std::string::npos ||
                   n.find("Shadow") != std::string::npos) {
            ms.skill_type = 0; ms.range = 80.0f; ms.dmg_mult = 2.0f;  // melee burst
        } else if (n.find("血") != std::string::npos ||
                   n.find("Blood") != std::string::npos) {
            ms.skill_type = 2; ms.dmg_mult = 1.3f; ms.range = 0;      // self_buff+heal
        } else if (n.find("召唤") != std::string::npos ||
                   n.find("Summon") != std::string::npos) {
            ms.skill_type = 3; ms.range = 160.0f; ms.dmg_mult = 0.6f; // aoe summon
        } else {
            ms.skill_type = 1; ms.range = 120.0f; ms.dmg_mult = 1.0f; // projectile default
        }
        _skills.push_back(ms);
    }
    _attack_cd = 1.2f;
    LOG_INFO("[MIRROR] Combat init: stages=%d skills=%d", _total_stages, (int)_skills.size());
}

// ============================================================
// 主循环 — 每帧由 BossSystemDirector::tick 调用
// ============================================================
bool MirrorCombatDirector::tick(float dt, Monster* boss, Player* player,
                                 double game_time, MirrorAgent* agent,
                                 std::vector<Effect>* effects) {
    if (!boss || !player) return false;

    float dist = hypotf(
        player->entity.rect.x - boss->entity.rect.x,
        player->entity.rect.y - boss->entity.rect.y);

    // M4e: 玩家闪避检测 (单帧位移>200px) → 最近决策视为落空
    float pdx = player->entity.rect.x - _last_player_x;
    float pdy = player->entity.rect.y - _last_player_y;
    if (_agent && (pdx * pdx + pdy * pdy) > 200.0f * 200.0f)
        _agent->report_outcome(false, 0.0f);   // 已反馈过则内部自动跳过
    _last_player_x = player->entity.rect.x;
    _last_player_y = player->entity.rect.y;

    // M2: 玩家实际动作识别 → 在线准确率反馈
    if (_agent) {
        PlayerActionType actual = _detect_player_action(player, game_time,
                                                        pdx, pdy, &_last_player_hp);
        if (actual != PlayerActionType::NONE) _agent->observe_actual(actual);
    }

    // 每0.5s做一次AI决策
    _decision_timer += dt;
    if (_decision_timer >= 0.5f && agent) {
        _decision_timer = 0.0f;
        _ai_decide(boss, player, game_time, agent);
    }

    // 技能冷却计时
    if (_skill_cd_timer > 0) _skill_cd_timer -= dt;

    // 行为状态机
    switch (_behavior_state) {
    case 0: // approach — 追击到合适距离
        if (dist > _preferred_dist * 1.2f) _chase_player(boss, player, dt);
        else _behavior_state = 1;  // 进入攻击
        break;
    case 1: // attack — 武器连招
        _weapon_attack(boss, player, game_time, effects);
        // 每2次攻击切换状态
        if (_combo_stage == 0) _behavior_state = (_skill_cd_timer <= 0) ? 2 : 0;
        break;
    case 2: // skill — 使用镜像技能
        if (_skill_cd_timer <= 0 && !_skills.empty()) {
            _skill_idx = (_skill_idx + 1) % (int)_skills.size();
            _mirror_skill(boss, player, game_time, _skill_idx, effects);
            _skill_cd_timer = _skills[_skill_idx].cooldown;
            _behavior_state = 0;
        } else {
            _behavior_state = 0;
        }
        break;
    case 3: // retreat — 后撤拉开距离
        if (dist < _preferred_dist * 0.6f) {
            float dx = boss->entity.rect.x - player->entity.rect.x;
            float dy = boss->entity.rect.y - player->entity.rect.y;
            float len = hypotf(dx, dy);
            if (len > 1) boss->entity.position.x += dx / len * 100.0f * dt;
            if (len > 1) boss->entity.position.y += dy / len * 100.0f * dt;
            boss->entity.sync_rect();
        } else _behavior_state = 0;
        break;
    }
    return true;
}

// ============================================================
// 武器连招 — 模拟玩家3段攻击
// ============================================================
void MirrorCombatDirector::_weapon_attack(Monster* boss, Player* player,
                                           double gt, std::vector<Effect>* effects) {
    if (gt - _last_attack < _attack_cd) return;
    float dist = hypotf(
        player->entity.rect.x - boss->entity.rect.x,
        player->entity.rect.y - boss->entity.rect.y);
    int stage = _combo_stage;
    float range_px = _stage_ranges[stage];

    if (dist > range_px) { _behavior_state = 0; return; }

    float mult = _stage_mults[stage];
    int raw = (int)(boss->combat.attack * mult * _aggression_bonus);
    int dmg = calculate_damage(raw,
        player->combat.get_effective_defense(AttackType::PHYSICAL));
    player->combat.take_damage(dmg);
    if (dmg > 0) {
        player->combat.mark_damage_logged();
        LOG_INFO("[DMG] Echo镜像[%d段] atk=%d mult=%.2f raw=%d → %d 伤害",
            stage + 1, boss->combat.attack, mult * _aggression_bonus, raw, dmg);
    }
    // M4e: 决策结果反馈 (命中/被防)
    if (_agent) _agent->report_outcome(dmg > 0, (float)dmg);
    _last_attack = (float)gt;
    _combo_stage++;
    bool combo_end = (_combo_stage >= _total_stages);
    if (combo_end) {
        _combo_stage = 0;
        _last_attack += 0.4f;  // 收招硬直
    }

    // VFX
    if (effects) {
        Effect ef;
        ef.kind = "pulse"; ef.duration = 0.25f; ef.elapsed = 0;
        ef.world_x = player->entity.rect.x + player->entity.rect.width / 2;
        ef.world_y = player->entity.rect.y + player->entity.rect.height / 2;
        ef.radius = combo_end ? 40.0f : 24.0f;
        ef.color = combo_end ? Color{200, 50, 40, 180} : Color{160, 80, 60, 140};
        effects->push_back(ef);
    }
}

// ============================================================
// 镜像技能 — 实际效果的Boss版本
// ============================================================
void MirrorCombatDirector::_mirror_skill(Monster* boss, Player* player,
                                          double gt, int idx,
                                          std::vector<Effect>* effects) {
    if (idx < 0 || idx >= (int)_skills.size()) return;
    auto& ms = _skills[idx];
    ms.last_used = (float)gt;
    float dist = hypotf(
        player->entity.rect.x - boss->entity.rect.x,
        player->entity.rect.y - boss->entity.rect.y);
    int atk = boss->combat.attack;

    switch (ms.skill_type) {
    case 0: { // melee — 扇形/矩形近战
        if (dist > ms.range) { if (_agent) _agent->report_outcome(false, 0.0f); break; }
        int raw = (int)(atk * ms.dmg_mult * _aggression_bonus);
        int dmg = calculate_damage(raw,
            player->combat.get_effective_defense(AttackType::PHYSICAL));
        player->combat.take_damage(dmg);
        LOG_INFO("[MIRROR] 镜像近战 [%s]: raw=%d → %d dmg", ms.name.c_str(), raw, dmg);
        if (dmg > 0) player->combat.mark_damage_logged();
        if (_agent) _agent->report_outcome(dmg > 0, (float)dmg);
        break;
    }
    case 1: { // projectile — 弹幕 (需在射程内, 可拉开距离闪避)
        if (dist > ms.range) { if (_agent) _agent->report_outcome(false, 0.0f); break; }
        int total_dmg = 0;
        for (int i = 0; i < 3; i++) {
            int raw = (int)(atk * ms.dmg_mult * 0.6f);
            int dmg = calculate_damage(raw,
                player->combat.get_effective_defense(AttackType::MAGICAL),
                AttackType::MAGICAL);
            player->combat.take_damage(dmg);
            total_dmg += dmg;
            if (dmg > 0) player->combat.mark_damage_logged();
        }
        LOG_INFO("[MIRROR] 镜像弹幕 [%s]: 3发", ms.name.c_str());
        if (_agent) _agent->report_outcome(total_dmg > 0, (float)total_dmg);
        break;
    }
    case 2: { // self_heal — 真正回血
        int heal_amt = boss->combat.max_hp / 5;  // 20% max HP
        boss->combat.heal(heal_amt);
        LOG_INFO("[MIRROR] 镜像自愈 [%s]: +%d HP", ms.name.c_str(), heal_amt);
        break;
    }
    case 3: { // aoe — 范围伤害 (需在范围半径内)
        if (dist > ms.range) { if (_agent) _agent->report_outcome(false, 0.0f); break; }
        int raw = (int)(atk * ms.dmg_mult * _aggression_bonus);
        int dmg = calculate_damage(raw,
            player->combat.get_effective_defense(AttackType::MAGICAL),
            AttackType::MAGICAL);
        player->combat.take_damage(dmg);
        LOG_INFO("[MIRROR] 镜像AOE [%s]: raw=%d → %d dmg", ms.name.c_str(), raw, dmg);
        if (dmg > 0) player->combat.mark_damage_logged();
        if (_agent) _agent->report_outcome(dmg > 0, (float)dmg);
        break;
    }
    case 4: { // time_stop — 玩家减速
        apply_buff(player, "slow", 4);
        LOG_INFO("[MIRROR] 镜像时停 [%s]: 玩家减速×4层", ms.name.c_str());
        break;
    }
    }

    // 技能VFX
    if (effects) {
        Effect ef;
        ef.kind = "shockwave";
        ef.world_x = boss->entity.rect.x + boss->entity.rect.width / 2;
        ef.world_y = boss->entity.rect.y + boss->entity.rect.height / 2;
        ef.radius = ms.range; ef.duration = 0.4f; ef.elapsed = 0;
        ef.color = (ms.skill_type == 2) ? Color{80, 200, 100, 160}
                 : (ms.skill_type == 4) ? Color{100, 60, 200, 180}
                 : Color{200, 100, 60, 160};
        effects->push_back(ef);
    }
}

// ============================================================
// AI决策 — MirrorAgent驱动
// ============================================================
void MirrorCombatDirector::_ai_decide(Monster* boss, Player* player,
                                       double gt, MirrorAgent* agent) {
    if (!agent) return;
    MirrorBattleState st;
    st.boss_hp_pct = boss->combat.max_hp > 0
        ? (float)boss->combat.current_hp / boss->combat.max_hp : 1.0f;
    st.player_hp_pct = player->combat.max_hp > 0
        ? (float)player->combat.current_hp / player->combat.max_hp : 1.0f;
    float dx = player->entity.rect.x - boss->entity.rect.x;
    float dy = player->entity.rect.y - boss->entity.rect.y;
    st.dist_tiles = hypotf(dx, dy) / 32.0f;
    st.player_attacking = (gt - player->_last_attack_time < 1.0);
    st.player_using_skill = (gt - player->_last_skill_time < 1.0);

    // M4e: 在线自适应决策 — Phase>=2 时 Thompson 完全接管行为选择
    if (_apply_online_action(agent->recommend_action(st))) return;

    // 预测玩家下一步
    PlayerActionType pred = agent->predict_next_action(st);
    agent->on_prediction(pred, st.dist_tiles, st.player_hp_pct,
                         st.player_skills_ready);   // M2: 上报预测上下文

    // 压力追击
    if (agent->should_pressure_close(st)) {
        _preferred_dist = 80.0f;
        _aggression_bonus = 1.3f;
    } else {
        _preferred_dist = agent->recommend_distance();
        _aggression_bonus = 1.0f;
    }

    // 打断技能 (M4e: 观察期也响应玩家技能窗口)
    if (agent->should_interrupt_skill(st)) {
        _skill_cd_timer = 0.0f;  // 立即可用技能
        _behavior_state = 2;     // 切换到技能状态
    }

    // 根据预测调整
    if (pred == PlayerActionType::HEAL) {
        _skill_cd_timer *= 0.6f;  // 预测喝药→技能加速
    } else if (pred == PlayerActionType::ATTACK && agent->should_pressure_close(st)) {
        _behavior_state = 3;  // 预测攻击+可压制→战略后撤
    }
}

// M2: 识别玩家本帧实际动作 — 优先级: 攻击/技能(0.5s内) > 闪避(位移>200px) > 喝药(HP上升)
PlayerActionType MirrorCombatDirector::_detect_player_action(Player* player,
                                                             double gt,
                                                             float dx, float dy,
                                                             float* last_hp) {
    if (gt - player->_last_attack_time < 0.5f) return PlayerActionType::ATTACK;
    if (gt - player->_last_skill_time  < 0.5f) return PlayerActionType::SKILL;
    if (dx * dx + dy * dy > 200.0f * 200.0f)   return PlayerActionType::DODGE;
    float hp = player->combat.current_hp;
    bool healed = (*last_hp > 0.0f) && (hp > *last_hp + 1.0f);
    *last_hp = hp;
    return healed ? PlayerActionType::HEAL : PlayerActionType::NONE;
}

// M4e: 在线决策动作映射 — Thompson 采样臂 → 行为状态
bool MirrorCombatDirector::_apply_online_action(int act) {
    if (act < 0) return false;   // 观察期: 走规则
    switch ((MirrorAction)act) {
    case MirrorAction::APPROACH: _behavior_state = 0; break;
    case MirrorAction::RETREAT:  _behavior_state = 3; break;
    case MirrorAction::SKILL:    _behavior_state = 2; _skill_cd_timer = 0.0f; break;
    case MirrorAction::COMBO:    _behavior_state = 1; break;
    }
    return true;
}

// ============================================================
// 追击移动
// ============================================================
void MirrorCombatDirector::_chase_player(Monster* boss, Player* player, float dt) {
    float dx = player->entity.rect.x - boss->entity.rect.x;
    float dy = player->entity.rect.y - boss->entity.rect.y;
    float len = hypotf(dx, dy);
    if (len < 1) return;
    float speed = boss->ai ? 120.0f : 90.0f;
    boss->entity.position.x += dx / len * speed * dt;
    boss->entity.position.y += dy / len * speed * dt;
    boss->entity.sync_rect();
}
