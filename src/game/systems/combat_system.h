#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <random>
#include "entity.h"
#include "combat_stats.h"
#include "types/combat_types.h"

class Player;
class Monster;
class GameMap;

// ============================================================
// CombatSystem — 战斗公式 + Buff 管理
// ============================================================
extern std::mt19937 rng;

// ---- 伤害公式 ----
int calculate_damage(int atk, int def, AttackType type = AttackType::PHYSICAL);

// 找最近可攻击目标 (实现移入 combat_system.cpp)
Monster* find_attack_target(Rectangle attacker_rect,
                             const std::vector<Monster*>& targets,
                             float range = 1.5f);

// 锥形范围内的目标 (用于斩击) — 定义在 skill.cpp
std::vector<Monster*> get_targets_in_cone(Player* caster,
    const std::vector<Monster*>& targets, int cone_range);

// Buff / Relic 类型定义 — 见 types/combat_types.h
// ============================================================

// 加载/查询 Buff 配置表
bool load_buff_defs(const std::string& json_path);
const BuffDef* get_buff_def(const std::string& id);
Color get_buff_hud_color(const std::string& id);   // 用于 HUD 渲染

// BuffEvent / BuffTrigger — 见 types/combat_types.h

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

// BuffTrigger — 见 types/combat_types.h

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
    // D3: 圣物偏好的构筑标签 (from JSON "tags" comma-separated)
    std::vector<BuildTag> favorite_tags;
};

bool load_relic_defs(const std::string& json_path);
const RelicDef* get_relic_def(const std::string& id);
bool player_has_relic(const Player* p, const std::string& id);
std::vector<std::string> get_all_relic_ids();
std::string get_relic_display_name(const std::string& id);
std::string get_relic_short_name(const std::string& id);
Color get_relic_hud_color(const std::string& id);
