#include "data/element_defs.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>

using json = nlohmann::json;

static std::unordered_map<std::string, ElementDef> g_element_defs;
static bool g_loaded = false;

static void _parse_fields(ElementDef& d, const json& e) {
    d.id          = e.value("id", "");
    d.name        = e.value("name", "");
    d.description = e.value("description", "");
    d.crit_base      = e.value("crit_base", 15.0f);
    d.crit_growth    = e.value("crit_growth", 0.75f);
    d.crit_multiplier = e.value("crit_multiplier", 1.5f);
    d.freeze_counter_max = e.value("freeze_counter_max", 3);
    d.freeze_stage3 = e.value("freeze_stage3", 100.0f);
    d.freeze_stage2 = e.value("freeze_stage2", 50.0f);
    d.freeze_stage1 = e.value("freeze_stage1", 10.0f);
    d.dot_scale_base  = e.value("dot_scale_base", 0.05f);
    d.dot_scale_growth = e.value("dot_scale_growth", 0.005f);
    d.dot_duration    = e.value("dot_duration", 3.0f);
    // G10.3 VFX recipe IDs
    if (e.contains("vfx") && e["vfx"].is_object()) {
        d.vfx.hit       = e["vfx"].value("hit", "");
        d.vfx.critical  = e["vfx"].value("critical", "");
        d.vfx.slow      = e["vfx"].value("slow", "");
        d.vfx.freeze    = e["vfx"].value("freeze", "");
        d.vfx.apply     = e["vfx"].value("apply", "");
        d.vfx.tick      = e["vfx"].value("tick", "");
        d.vfx.level_up  = e["vfx"].value("level_up", "");
    }
}

bool load_element_defs(const std::string& json_path) {
    std::ifstream f(json_path);
    if (!f.is_open()) {
        printf("[ELEMENT_DEF] cannot open %s\n", json_path.c_str());
        return false;
    }
    json j;
    try { f >> j; } catch (const std::exception& ex) {
        printf("[ELEMENT_DEF] JSON error: %s\n", ex.what());
        return false;
    }
    if (!j.contains("elements") || !j["elements"].is_array()) return false;
    int count = 0;
    for (auto& e : j["elements"]) {
        ElementDef d;
        _parse_fields(d, e);
        if (d.id.empty()) continue;
        g_element_defs[d.id] = d;
        count++;
    }
    g_loaded = true;
    printf("[ELEMENT_DEF] Loaded %d elements\n", count);
    return true;
}

const ElementDef* get_element_def(const std::string& id) {
    auto it = g_element_defs.find(id);
    return (it != g_element_defs.end()) ? &it->second : nullptr;
}

const std::unordered_map<std::string, ElementDef>& get_all_element_defs() {
    return g_element_defs;
}

int load_element_defs_from_json(const char* json_text, MergeMode mode,
                                 const char* id_namespace) {
    if (!json_text) return 0;
    int count = 0;
    try {
        json j = json::parse(json_text);
        if (!j.contains("elements") || !j["elements"].is_array()) return 0;
        for (auto& e : j["elements"]) {
            ElementDef d;
            _parse_fields(d, e);
            if (d.id.empty()) continue;
            std::string key = id_namespace
                ? std::string(id_namespace) + ":" + d.id : d.id;
            if (mode == MergeMode::Skip && g_element_defs.count(key)) continue;
            d.id = key;
            g_element_defs[key] = d;
            count++;
        }
    } catch (...) {}
    g_loaded = true;
    return count;
}

bool is_element_defs_loaded() { return g_loaded; }
