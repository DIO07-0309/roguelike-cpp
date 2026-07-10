#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <random>
#include "entity.h"
#include "combat_stats.h"

class Player;
class Monster;
class GameMap;

// ============================================================
// CombatSystem — 战斗公式 + Buff 管理
// ============================================================
extern std::mt19937 rng;

// ---- 伤害公式 ----
inline int calculate_damage(int atk, int def, AttackType type = AttackType::PHYSICAL) {
    float base = std::max(1.0f, atk - def * 0.5f);
    float variance = 0.8f + (float)(rng() % 401) / 1000.0f;
    return std::max(1, (int)(base * variance));
}

// 找最近可攻击目标 (实现移入 combat_system.cpp)
Monster* find_attack_target(Rectangle attacker_rect,
                             const std::vector<Monster*>& targets,
                             float range = 1.5f);

// 锥形范围内的目标 (用于斩击) — 定义在 skill.cpp
std::vector<Monster*> get_targets_in_cone(Player* caster,
    const std::vector<Monster*>& targets, int cone_range);

// ============================================================
// Buff 系统
// ============================================================

// Buff 完整定义 (逻辑 + HUD 显示，来自 resources/buffs.json)
struct BuffDef {
    std::string id;

    // 逻辑
    float duration = 0.0f;
    int max_stacks = 1;
    float tick_interval = 0.0f;
    int tick_damage = 0;

    // 显示
    std::string display_name;       // "中毒"
    std::string short_name;         // "毒"
    unsigned char hud_color_r = 200;
    unsigned char hud_color_g = 200;
    unsigned char hud_color_b = 200;
};

// 加载/查询 Buff 配置表
bool load_buff_defs(const std::string& json_path);
const BuffDef* get_buff_def(const std::string& id);
Color get_buff_hud_color(const std::string& id);   // 用于 HUD 渲染

// Buff 事件 (轻量级, 用于日志 / 飘字 / UI)
enum class BuffEventType { APPLIED, TICK_DAMAGE, EXPIRED };
struct BuffEvent {
    BuffEventType type;
    std::string buff_id;
    std::string target;     // 目标名
    int stacks = 0;
    int value = 0;          // tick damage / 0
};

// 逐帧结算 (使用 while 处理大 dt 跨多个 tick_interval)
// events 可选 — 非 nullptr 时写入事件
void tick_buffs(Player* p, float dt, std::vector<BuffEvent>* events = nullptr);
void tick_buffs(Monster* m, float dt, std::vector<BuffEvent>* events = nullptr,
                const Player* relic_owner = nullptr);  // B11: venom_fang

// 施加 (events 可选)
void apply_buff(Player* p, const std::string& id, int stacks = 1,
                std::vector<BuffEvent>* events = nullptr);
void apply_buff(Monster* m, const std::string& id, int stacks = 1,
                std::vector<BuffEvent>* events = nullptr);

// 有效属性 (含 Buff 加成)
int  get_effective_attack(const Player* p);
int  get_effective_attack(const Monster* m);
float get_effective_speed(const Player* p);
float get_effective_speed(const Monster* m, float base_speed);
int  get_effective_max_hp(const Player* p);  // B11: blood_charm
void heal_player(Player* p, int amount);     // B11: 治疗 (按 effective max_hp clamp)

// Buff 显示 helper (集中管理，避免 if-else 散落)
std::string get_buff_display_name(const std::string& id);   // "poison" → "中毒"
std::string get_buff_short_name(const std::string& id);     // "poison" → "毒"
std::string format_buff_time(float sec);                     // 3.2 → "3.2s"

// ============================================================
// BuffTrigger — 统一 Buff 触发规则 (技能/怪物/道具共用)
// ============================================================
enum class BuffTarget { SELF, ENEMY };

struct BuffTrigger {
    std::string buff_id;          // "poison" | "slow" | "attack_up"
    int stacks = 1;               // 施加层数
    float chance = 1.0f;          // 触发概率 (0.0~1.0, 1.0=必定触发)
    BuffTarget target = BuffTarget::ENEMY;
};

// 对敌人目标应用 triggers (技能命中 / 怪物普攻)
void apply_triggers(const std::vector<BuffTrigger>& triggers,
                    Player* self, Monster* enemy);

// 对自身应用 triggers (道具使用 / 自身技能)
void apply_triggers_self(const std::vector<BuffTrigger>& triggers, Player* self);

// ============================================================
// B11: Relic 系统 — 局内圣物配置与查询
// ============================================================
struct RelicDef {
    std::string id;
    std::string name;        // "血纹护符"
    std::string short_name;  // "血"
    std::string desc;        // "进入新楼层时恢复20%生命"
    std::string rarity = "common"; // B12: "common" | "rare" | "epic"
    float param = 0.0f;
    int   param2 = 0;
    int hud_color_r = 200;
    int hud_color_g = 200;
    int hud_color_b = 200;
};

bool load_relic_defs(const std::string& json_path);
const RelicDef* get_relic_def(const std::string& id);
bool player_has_relic(const Player* p, const std::string& id);
std::vector<std::string> get_all_relic_ids();
std::string get_relic_display_name(const std::string& id);
std::string get_relic_short_name(const std::string& id);
Color get_relic_hud_color(const std::string& id);
