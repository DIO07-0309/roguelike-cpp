#include "data/element_defs.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>

using json = nlohmann::json;

static std::unordered_map<std::string, ElementDef> g_element_defs;
static bool g_loaded = false;

bool load_element_defs(const std::string& json_path) {
    std::ifstream f(json_path);
    if (!f.is_open()) {
        printf("[ELEMENT_DEF] cannot open %s\n", json_path.c_str());
        return false;
    }
    json j;
    try { f >> j; } catch (const std::exception& e) {
        printf("[ELEMENT_DEF] JSON error: %s\n", e.what());
        return false;
    }
    if (!j.contains("elements") || !j["elements"].is_array()) return false;
    int count = 0;
    for (auto& e : j["elements"]) {
        ElementDef d;
        d.id          = e.value("id", "");
        d.name        = e.value("name", "");
        d.description = e.value("description", "");
        d.base_value  = e.value("base_value", 0);
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
            d.id          = e.value("id", "");
            d.name        = e.value("name", "");
            d.description = e.value("description", "");
            d.base_value  = e.value("base_value", 0);
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
