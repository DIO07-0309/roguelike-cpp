#pragma once
#include <vector>
#include <memory>
#include <string>
#include "ai.h"

// 前向声明
class Monster;
class Player;
class GameMap;
struct Effect;

// ============================================================
// B15: BossState — Boss 行为状态机
// ============================================================
enum class BossState {
    IDLE,        // 空闲/追逐 (使用基础 AI)
    ATTACK,      // 普攻
    CHARGE,      // 冲锋: 蓄力→冲刺
    SHOCKWAVE,   // 冲击波: 蓄力→AOE
    SUMMON,      // 召唤: 召唤小怪
    DEFEND,      // D8: 防御: 举盾减伤 (Golem)
};

// ============================================================
// BossSkill — Boss 专属技能 (保留 B7 兼容)
// ============================================================
struct BossSkill {
    std::string name;
    float cooldown;
    float last_use_time = -999.0f;
    std::string fx_kind = "circle";
    float fx_radius = 60.0f;
    Color fx_color{200, 40, 40, 255};

    BossSkill(const std::string& n, float cd);

    bool can_use(double t) const;
    void mark_used(double t);

    virtual std::string execute(Monster* boss, Player* player,
                                std::vector<Monster*>& monsters,
                                GameMap* map, double game_time) = 0;
};

// B15: 冲锋技能 — 锁定玩家方向, 蓄力0.6s后高速冲刺
class ChargeSkill : public BossSkill {
public:
    ChargeSkill();
    std::string execute(Monster* boss, Player* player,
                        std::vector<Monster*>& monsters,
                        GameMap* map, double game_time) override;
    // 蓄力/冲锋阶段变量
    float windup_left = 0.0f;
    float dash_duration = 0.0f;
    float dash_dx = 0.0f, dash_dy = 0.0f;
};

// B15: 冲击波 — 蓄力后向四周释放 AOE
class ShockwaveSkill : public BossSkill {
public:
    ShockwaveSkill();
    std::string execute(Monster* boss, Player* player,
                        std::vector<Monster*>& monsters,
                        GameMap* map, double game_time) override;
    float windup_left = 0.0f;
};

// 召唤 (保留 B7, B15 增强为固定循环)
class SummonMinions : public BossSkill {
public:
    SummonMinions();
    std::string execute(Monster* boss, Player* player,
                        std::vector<Monster*>& monsters,
                        GameMap* map, double game_time) override;
};

// ============================================================
// BossAI — 继承 MonsterAI，状态机驱动技能循环 + Phase2
// ============================================================
class BossAI : public MonsterAI {
public:
    BossAI();
    void update(Monster* self, Player* player, GameMap* map,
                double dt, double game_time,
                std::vector<Monster*>* all_monsters = nullptr,
                std::vector<Effect>* effects = nullptr) override;

    // B15: 技能循环状态
    BossState boss_state = BossState::IDLE;
    int skill_cycle_index = 0;     // 0→Charge, 1→norm, 2→Shockwave, 3→norm, 4→Summon, repeat
    int normal_attack_count = 0;   // 当前已普攻次数

    bool is_enraged = false;
    bool phase2 = false;
    float phase2_pause = 0.0f;     // 进入二阶段暂停计时

    // D8 Step2: Boss-specific config
    int   skill_cycle_bias = 6;    // Necromancer: 改成更频繁的召唤循环
    float golem_shield_pct = 0.0f; // Golem: DEFEND 减伤比例

    // 三个技能实例 (B15 新)
    std::unique_ptr<ChargeSkill>   _charge;
    std::unique_ptr<ShockwaveSkill> _shockwave;
    std::unique_ptr<SummonMinions>  _summon;

private:
    void _enter_phase2(Monster* self);
    void _tick_boss_state(Monster* self, Player* player, GameMap* map,
                          double dt, double gt,
                          std::vector<Monster*>* all, std::vector<Effect>* effects);
    float _hp_ratio(Monster* self) const;
    int   _next_cycle_skill();  // 返回 -1=普攻, 0=Charge, 1=Shockwave, 2=Summon
};

// 工厂
Monster* spawn_boss(int tile_x, int tile_y, int floor);

struct BossInfo {
    std::string name, title, lore, skills_text;
    int max_hp, attack, pdef, mdef;
    Color color;
};
BossInfo get_boss_info(int floor);

// ============================================================
// D8 Step2: BossType — 新增 Boss 类型 + BossFactory
// ============================================================
enum class BossType {
    SHADOW_KNIGHT,  // 暗影骑士 (F5)
    FIRE_DEMON,     // 地狱火魔 (F10)
    DEMON_LORD,     // 深渊之主 (F15)
    NECROMANCER,    // 亡灵法师 (F5/F10)
    GOLEM,          // 远古魔像 (F10/F15)
};

struct BossTemplate {
    BossType type;
    int floor;
    const char* name;
    const char* title;
    const char* lore;
    bool is_summoner;    // Necromancer
    bool is_defender;    // Golem
    int max_hp, attack, pdef, mdef;
    Color color;
    float summon_speed;  // Phase2 summoned count multiplier
    float shield_pct;    // Golem DEFEND damage reduction
};

// BossFactory: 按类型创建 + 配置化
Monster* boss_factory_create(BossType type, int tile_x, int tile_y, int floor,
                              std::vector<Monster*>* out_monsters = nullptr,
                              GameMap* map = nullptr);
const BossTemplate* boss_factory_lookup(BossType type);
BossType boss_type_for_floor(int floor, uint32_t seed);  // 支持随机Boss
