#pragma once
#include <cstdint>
#include <vector>
#include <random>
#include "raylib.h"

// 前向声明
class Monster;
class Player;
class GameMap;
struct Effect;
class MonsterAI;

// ============================================================
// D2 Step3: MonsterSkillType — 敌人主动技能
// ============================================================
enum class MonsterSkillType {
    NONE,
    RAPID_SHOT,   // Archer: 连续3发弹幕
    TOTEM,        // Shaman: 放置图腾(范围友方buff)
    LEAP,         // Bomber: 跳跃接近玩家
    SHIELD,       // Tank: 举盾减伤70%
    SUMMON,       // Elite: 召唤1只小怪
    CHARGE,       // D8: Charger: 蓄力冲刺撞击+击退
    MASS_SUMMON,  // D8: Summoner: 一次召唤2只小怪
    SNIPE,        // G5.3: Sniper 蓄力贯穿弹
    SCATTER,      // G5.3: Controller 锥形散布弹
    AMBUSH_ATTACK,// G5.3: Ambush 隐身→突袭
    GUARD_AURA,   // G5.3: Guardian 群体防御光环
};

// D2 Step3: 技能状态 (每只怪物独立, 非共享)
struct MonsterSkillState {
    MonsterSkillType type = MonsterSkillType::NONE;
    float cooldown = 0.0f;       // 当前冷却
    float max_cooldown = 5.0f;   // 基础冷却 (按type不同)
    float cast_left = 0.0f;      // 蓄力剩余 (>0表示正在蓄力)
    float duration_left = 0.0f;  // 持续技能剩余 (>0表示效果持续中)
    float shot_timer = 0.0f;     // Rapid Shot 连发计时
    int   shot_count = 0;        // Rapid Shot 已发射数
    bool  active = false;        // 技能激活中
};

// ============================================================
// G5.3: AIArchetype — 行为原型 (与 MonsterType/外观解耦)
// ============================================================
enum class AIArchetype {
    DEFAULT,     // 普通追逐攻击
    BOMBER,      // 靠近自爆
    SHAMAN,      // 远程辅助
    SNIPER,      // 远程蓄力高伤
    CONTROLLER,  // 放置危险区域
    AMBUSH,      // 隐身突袭
    GUARDIAN,    // 群体防御光环
    CHARGER,      // 蓄力冲锋
    SUMMONER,     // 召唤小怪
};

// ============================================================
// MonsterAI — 怪物行为状态机 (IDLE → CHASE → ATTACK)
// D2 Step3: Think → Skill → Move → Attack 管线
// ============================================================
enum class AIState { IDLE, CHASE, ATTACK };

class MonsterAI {
public:
    AIState state = AIState::IDLE;
    float sight_range = 6.0f;
    float move_speed = 80.0f;
    float patrol_interval = 2.0f;
    float attack_range = 1.5f;

    MonsterAI(float sight = 6.0f, float speed = 80.0f, float patrol = 2.0f,
              float attack = 1.5f);
    virtual ~MonsterAI() = default;

    virtual void update(Monster* self, Player* player, GameMap* map,
                        double dt, double game_time,
                        std::vector<Monster*>* all_monsters = nullptr,
                        std::vector<Effect>* effects = nullptr);

    // D2 Step3: 怪物技能池 (在 spawn_monster 中分配)
    std::vector<MonsterSkillState> _skills;

    // D2 Step4: Elite 指挥冷却 (10s)
    float _command_cooldown = 0.0f;
    // D2 Step4: 被保护标记 (Tank→Ally 连线目标)
    Monster* _protect_target = nullptr;
    // D2 Step4: 本层协同概率 (由 FloorManager::spawn_floor_monsters 设置)
    float team_coop_chance = 0.0f;

    // G5.3: AI Archetype (行为原型, 与 MonsterType 外观解耦)
    AIArchetype archetype = AIArchetype::DEFAULT;
    float _archetype_timer = 0.0f;   // 通用计时器 (sniper蓄力/controller间隔/ambush冷却)
    bool  _archetype_active = false; // 特殊状态激活中 (ambush隐身中)

protected:
    float _patrol_timer = 0.0f;
    Vector2 _patrol_dir{0, 0};

    void _decide_state(Monster* self, Player* player);
    void _execute_idle(Monster* self, GameMap* map, double dt);
    void _execute_chase(Monster* self, Player* player, GameMap* map, double dt);
    void _execute_attack(Monster* self, Player* player, double game_time,
                         std::vector<Effect>* effects);
    void _apply_movement(Monster* self, GameMap* map, float mx, float my, double dt);
    float _dist_to(Monster* self, Player* player) const;
    void _pick_new_dir();

    // D2 Step4: Team决策 — 协同站位/保护/治疗/侧翼/指挥
    void _team_decision(Monster* self, Player* player, GameMap* map,
                        double dt, std::vector<Monster*>* all);

    // D2 Step3: Think层 — 决策是否释放技能
    bool _think_and_cast(Monster* self, Player* player, GameMap* map,
                         double dt, double gt,
                         std::vector<Monster*>* all, std::vector<Effect>* effects);
    // D2 Step3: 各技能执行
    void _exec_rapid_shot(Monster* self, Player* player, double gt,
                          std::vector<Effect>* effects, MonsterSkillState& sk);
    void _exec_totem(Monster* self, Player* player, std::vector<Monster*>* all,
                     std::vector<Effect>* effects, MonsterSkillState& sk);
    void _exec_leap(Monster* self, Player* player, GameMap* map,
                    MonsterSkillState& sk);
    void _exec_shield(Monster* self, MonsterSkillState& sk);
    void _exec_summon(Monster* self, Player* player, GameMap* map,
                      std::vector<Monster*>* all, std::vector<Effect>* effects,
                      MonsterSkillState& sk);
    // D8 Step1: 新技能执行器
    void _exec_charge(Monster* self, Player* player, GameMap* map,
                      std::vector<Effect>* effects, MonsterSkillState& sk);
    void _exec_mass_summon(Monster* self, Player* player, GameMap* map,
                           std::vector<Monster*>* all, std::vector<Effect>* effects,
                           MonsterSkillState& sk);
};
