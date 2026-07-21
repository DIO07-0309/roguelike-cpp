#pragma once
#include <string>
#include <vector>
#include <unordered_map>

// ============================================================
// G5.8.5: VFXRecipe — 表现层数据驱动配方
//
// 技能 → Recipe → Primitive → Effect → Renderer
//
// 新技能不需写 C++ VFX 方法, 只需在 vfx_recipes.json 加条目
// Mod 技能可在 mods/<name>/vfx_recipes.json 中定义
// ============================================================

// ── 单步原语指令 ──
struct VFXStep {
    std::string type;           // "ring"|"beam"|"lightning"|"explosion"|"shockwave"
                                // |"slash_arc"|"smoke"|"spark"|"aura"|"flash"
    float radius = 32.0f;       // 范围/半径
    int   count = 1;            // 层数/分支/粒子数
    float duration = 0.40f;     // 持续(秒)
    std::string color_preset;   // "ice"|"fire"|"lightning"|"blood"|"shadow"
                                // |"holy"|"nature"|"void"|"gold"|"white"
    float direction_rad = 0.0f; // 方向 (弧度, slash_arc用)
    float target_dist = 0.0f;   // beam/lightning 目标距离
};

// ── 完整配方 ──
struct VFXRecipe {
    std::string id;             // "ice_nova" | "slash" | ...
    std::string description;
    std::vector<VFXStep> steps;
};

// ── 全局注册表 ──
bool load_vfx_recipes(const std::string& json_path);
const VFXRecipe* get_vfx_recipe(const std::string& id);
const std::unordered_map<std::string, VFXRecipe>& get_all_vfx_recipes();
bool is_vfx_recipes_loaded();
