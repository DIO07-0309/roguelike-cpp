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
    // G9-fix: recipes use "kind", loader used "type" → defaulted to "ring"
    s.type = j.value("type", "");
    if (s.type.empty()) s.type = j.value("kind", "ring");
    s.radius        = j.value("radius", 32.0f);
    s.count         = j.value("count", 1);
    s.duration      = j.value("duration", 0.40f);
    s.color_preset  = j.value("color", "");
    s.direction_rad = j.value("direction", 0.0f);
    s.target_dist   = j.value("target_dist", 0.0f);
    s.layers        = j.value("layers", 1);
    s.layer_delay   = j.value("layer_delay", 0.0f);
    s.delay         = j.value("delay", 0.0f);
    return s;
}

static VFXRecipe _parse_recipe(const json& j, const std::string& id) {
    VFXRecipe r;
    r.id          = id;
    r.description = j.value("description", "");
    r.sfx         = j.value("sfx", "");
    r.hit_sfx     = j.value("hit_sfx", "");
    r.camera_shake = j.value("camera_shake", 0);
    if (j.contains("steps") && j["steps"].is_array())
        for (auto& s : j["steps"])
            r.steps.push_back(_parse_step(s));
    return r;
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

    int count = 0;
    // G5.8.8-fix: 支持 object 根 { "recipes": {...} } 与旧版数组根
    if (j.is_object() && j.contains("recipes")) {
        for (auto& [id, entry] : j["recipes"].items()) {
            g_vfx_recipes[id] = _parse_recipe(entry, id);
            count++;
        }
    } else if (j.is_array()) {
        for (auto& entry : j) {
            std::string id = entry.value("id", "");
            if (id.empty()) continue;
            g_vfx_recipes[id] = _parse_recipe(entry, id);
            count++;
        }
    } else {
        printf("[VFX_RECIPE] root must be array or {recipes:{...}}\n");
        return false;
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
