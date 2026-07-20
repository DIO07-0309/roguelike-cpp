#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "core/registry_provider.h"  // G4.1: MergeMode

// ============================================================
// G3.2: SkillDef — 技能配置数据 (纯数据, 不含 execute/on_level_up 行为)
// 6 个具体 Skill Class 保留 C++ — 只数据化构造参数
// 属于 Data 层, 位于 src/data/
// ============================================================

struct SkillTriggerDef {
    std::string buff_id;         // "poison" | "slow" | "attack_up" | ...
    int stacks = 1;
    float chance = 1.0f;
    std::string target;          // "enemy" | "self"
};

struct SkillEvoDef {
    int level = 1;
    std::string name;            // "广域斩"
    std::string desc;            // "攻击范围+30%"
    std::vector<std::string> required_tags; // ["melee"] | ["fire","aoe"] | ...
};

struct SkillDef {
    std::string id;              // "slash" | "fireball" | ...
    std::string class_type;      // "slash" | "fireball" | "iron_skin" | ...
    std::string name;            // "斩击"
    float cooldown = 2.0f;
    int max_level = 3;

    // 标签
    std::vector<std::string> tags;     // ["melee", "combo"]

    // 触发器
    std::vector<SkillTriggerDef> triggers;  // 命中/使用后触发的 Buff

    // 进化路径
    std::vector<SkillEvoDef> evolutions;

    // 被动专用 (空=主动技能)
    std::string modifier_key;            // "def_flat" | "atk_flat"
    int base_value = 0;
    std::vector<int> values_per_level;   // {0, 3, 7, 12}
};

// ── 全局注册表 ──
bool load_skill_defs(const std::string& json_path);
int  load_skill_defs_from_json(const char* json_text, MergeMode mode,
                                const char* id_namespace = nullptr);  // G4.2: namespace
const SkillDef* get_skill_def(const std::string& id);
const std::unordered_map<std::string, SkillDef>& get_all_skill_defs();
bool is_skill_defs_loaded();
