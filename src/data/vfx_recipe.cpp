#include "data/vfx_recipe.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>

using json = nlohmann::json;

// ============================================================
// G5.8.5: VFXRecipe 全局注册表
// ============================================================

static std::unordered_map<std::string, VFXRecipe> g_vfx_recipes;
static bool g_vfx_loaded = false;

static VFXStep _parse_step(const json& j) {
    VFXStep s;
    s.type          = j.value("type", "ring");
    s.radius        = j.value("radius", 32.0f);
    s.count         = j.value("count", 1);
    s.duration      = j.value("duration", 0.40f);
    s.color_preset  = j.value("color", "");
    s.direction_rad = j.value("direction", 0.0f);
    s.target_dist   = j.value("target_dist", 0.0f);
    return s;
}

bool load_vfx_recipes(const std::string& json_path) {
    std::ifstream f(json_path);
    if (!f.is_open()) {
        printf("[VFX_RECIPE] ERROR: cannot open %s\n", json_path.c_str());
        return false;
    }
    json j;
    try { f >> j; } catch (const std::exception& e) {
        printf("[VFX_RECIPE] JSON error: %s\n", e.what());
        return false;
    }
    if (!j.is_array()) { printf("[VFX_RECIPE] root must be array\n"); return false; }

    int count = 0;
    for (auto& entry : j) {
        VFXRecipe r;
        r.id          = entry.value("id", "");
        r.description = entry.value("description", "");
        if (r.id.empty()) continue;
        if (entry.contains("steps") && entry["steps"].is_array())
            for (auto& s : entry["steps"])
                r.steps.push_back(_parse_step(s));
        g_vfx_recipes[r.id] = r;
        count++;
    }

    g_vfx_loaded = true;
    printf("[VFX_RECIPE] Loaded %d recipes from %s\n", count, json_path.c_str());
    return true;
}

const VFXRecipe* get_vfx_recipe(const std::string& id) {
    auto it = g_vfx_recipes.find(id);
    return (it != g_vfx_recipes.end()) ? &it->second : nullptr;
}

const std::unordered_map<std::string, VFXRecipe>& get_all_vfx_recipes() { return g_vfx_recipes; }
bool is_vfx_recipes_loaded() { return g_vfx_loaded; }
