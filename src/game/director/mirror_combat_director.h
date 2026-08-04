#pragma once
#include <vector>
#include <string>
#include "types/weapon_types.h"

class Player;
class Monster;
class MirrorAgent;
class BossAI;
struct Effect;

// ============================================================
// MirrorCombatDirector — 接管F15 Mirror Boss全部战斗决策
// 组合模式: BossSystemDirector持有 → 不继承BossAI
// ============================================================

struct MirrorSkillDef {
    std::string name;
    float cooldown = 3.0f;
    float dmg_mult = 1.0f;
    float range = 100.0f;    // pixels
    int   skill_type = 0;    // 0=melee, 1=projectile, 2=self_heal, 3=aoe, 4=time_stop
    float last_used = -99.0f;
};

class MirrorCombatDirector {
public:
    MirrorCombatDirector();
    void init(const Player* player, Monster* boss, BossAI* bai,
              MirrorAgent* agent);
    // 每帧主循环 — 返回true表示本帧被镜像战斗占用
    bool tick(float dt, Monster* boss, Player* player, double game_time,
              MirrorAgent* agent, std::vector<Effect>* effects);

    // 公开供BossSystemDirector读取
    int  combo_stage() const { return _combo_stage; }
    int  behavior_state() const { return _behavior_state; }
    const std::vector<MirrorSkillDef>& skills() const { return _skills; }

private:
    void _weapon_attack(Monster* boss, Player* player, double gt,
                        std::vector<Effect>* effects);
    void _mirror_skill(Monster* boss, Player* player, double gt,
                       int skill_idx, std::vector<Effect>* effects);
    void _ai_decide(Monster* boss, Player* player, double gt,
                    MirrorAgent* agent);
    // M4e: 在线决策动作映射 — act<0 返回false (观察期走规则)
    bool _apply_online_action(int act);
    void _chase_player(Monster* boss, Player* player, float dt);

    // 武器镜像数据
    int   _combo_stage = 0;
    float _combo_timer = 0.0f;
    float _attack_cd = 1.2f;
    float _last_attack = -99.0f;
    float _stage_mults[3] = {1.0f, 1.0f, 1.0f};
    float _stage_ranges[3] = {48.0f, 48.0f, 48.0f};
    int   _total_stages = 1;
    // 技能镜像数据
    std::vector<MirrorSkillDef> _skills;
    int   _skill_idx = 0;
    float _skill_cd_timer = 0.0f;
    // AI 状态
    int   _behavior_state = 0;  // 0=approach, 1=attack, 2=skill, 3=retreat
    float _decision_timer = 0.0f;
    float _preferred_dist = 200.0f;
    float _aggression_bonus = 1.0f;
    // Boss 引用
    Monster* _boss = nullptr;
    BossAI*  _bai = nullptr;
    // M4e: 在线自适应反馈
    MirrorAgent* _agent = nullptr;
    float _last_player_x = 0.0f;   // 玩家闪避检测
    float _last_player_y = 0.0f;
};
