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
    WHIRLWIND,   // G5.4: 旋风斩 (Shadow Knight Phase2)
    LASER_BARRAGE,// G5.4: 激光弹幕 (Fire Demon Phase2)
    GRAVITY_PULL,// G5.4: 引力拉扯 (Demon Lord Phase2)
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

// G5.4: 旋风斩 (Shadow Knight Phase2) — 360°持续旋转+移动
class WhirlwindSkill : public BossSkill {
public:
    WhirlwindSkill();
    std::string execute(Monster* boss, Player* player,
                        std::vector<Monster*>& monsters,
                        GameMap* map, double game_time) override;
    float windup_left = 0.0f;
    float spin_duration = 0.0f;
    int   spin_hit_count = 0;
};

// G5.4: 激光弹幕 (Fire Demon Phase2) — 扇形3方向远程贯穿弹
class LaserBarrageSkill : public BossSkill {
public:
    LaserBarrageSkill();
    std::string execute(Monster* boss, Player* player,
                        std::vector<Monster*>& monsters,
                        GameMap* map, double game_time) override;
    float windup_left = 0.0f;
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

    // G1 Step6: Phase2 参数 (来自 BossDef, 替代硬编码)
    float _phase2_hp_threshold = 0.50f;
    float _phase2_pause = 0.50f;
    float _phase2_speed_mult = 1.50f;
    float _phase2_atk_mult = 1.25f;
    float _phase2_cd_mult = 0.70f;

    // G2.3: Arena 配置指针 (来自 BossDef, 在 boss_factory_create 中设置)
    const struct BossArenaDef* _arena_cfg = nullptr;

    // 三个技能实例 (B15 新)
    std::unique_ptr<ChargeSkill>    _charge;
    std::unique_ptr<ShockwaveSkill> _shockwave;
    std::unique_ptr<SummonMinions>  _summon;
    // G5.4: Phase2 signature skill instances
    std::unique_ptr<WhirlwindSkill>   _whirlwind;
    std::unique_ptr<LaserBarrageSkill> _laser;

    const char* _boss_id = nullptr;   // G5.4: 当前 Boss ID 用于 phase2 行为分支

private:
    void _enter_phase2(Monster* self);
    void _tick_boss_state(Monster* self, Player* player, GameMap* map,
                          double dt, double gt,
                          std::vector<Monster*>* all, std::vector<Effect>* effects);
    float _hp_ratio(Monster* self) const;
    int   _next_cycle_skill();  // 返回 -1=普攻, 0=Charge, 1=Shockwave, 2=Summon
};

// 工厂
Monster* spawn_boss(int tile_x, int tile_y, int floor);  // deprecated: use boss_factory_create

// ============================================================
// D8 Step2: BossType — Boss 类型枚举
// ============================================================
enum class BossType {
    SHADOW_KNIGHT,  // 暗影骑士 (F5)
    FIRE_DEMON,     // 地狱火魔 (F10)
    DEMON_LORD,     // 深渊之主 (F15)
    NECROMANCER,    // 亡灵法师 (F5)
    GOLEM,          // 远古魔像 (F10)
    VAMPIRE,        // G1 Step6: 血族伯爵 (F5)
};

// ── 前向声明 (boss_defs.h 在 boss.cpp 中引用) ──
struct BossDef;

// BossFactory: 数据驱动创建
Monster* boss_factory_create(BossType type, int tile_x, int tile_y, int floor,
                              std::vector<Monster*>* out_monsters = nullptr,
                              GameMap* map = nullptr);
BossType boss_type_for_floor(int floor, uint32_t seed);

// G1 Step6: visual_id → Color 映射 (表现层, 未来替换为 texture)
Color get_boss_visual_color(const std::string& visual_id);
const char* get_boss_skills_text(const BossDef* def);
