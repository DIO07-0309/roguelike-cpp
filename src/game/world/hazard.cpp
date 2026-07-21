// G6.3: Hazard loader
#include "world/hazard.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>

using json = nlohmann::json;
std::vector<HazardDef> g_hazards;
std::unordered_map<std::string, std::vector<const HazardDef*>> g_hazards_by_landmark;

bool load_hazard_defs(const char* json_path) {
    try {
        std::ifstream f(json_path);
        if (!f.is_open()) { printf("[Hazard] Cannot open %s\n", json_path); return false; }
        json data = json::parse(f);
        g_hazards.clear(); g_hazards_by_landmark.clear();
        for (auto& obj : data) {
            HazardDef hz;
            hz.id = obj["id"].get<std::string>();
            hz.biome = obj["biome"].get<std::string>();
            hz.landmark_id = obj["landmark_id"].get<std::string>();
            hz.effect = obj["effect"].get<std::string>();
            hz.interval = obj.value("interval", 3.0f);
            hz.damage = obj.value("damage", 0);
            hz.slow_factor = obj.value("slow_factor", 1.0f);
            hz.param = obj.value("param", 0.0f);
            hz.message = obj.value("message", "");
            g_hazards.push_back(hz);
            g_hazards_by_landmark[hz.landmark_id].push_back(&g_hazards.back());
        }
        printf("[Hazard] Loaded %zu hazards across %zu landmarks\n",
               g_hazards.size(), g_hazards_by_landmark.size());
        return true;
    } catch (const std::exception& e) {
        printf("[Hazard] Error: %s\n", e.what()); return false;
    }
}

std::vector<const HazardDef*> get_hazards_for_landmark(const std::string& landmark_id) {
    auto it = g_hazards_by_landmark.find(landmark_id);
    if (it != g_hazards_by_landmark.end()) return it->second;
    return {};
}
