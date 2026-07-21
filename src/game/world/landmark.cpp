// G6.2: Landmark loader
#include "world/landmark.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>

using json = nlohmann::json;
std::vector<LandmarkDef> g_landmarks;
std::unordered_map<std::string, std::vector<const LandmarkDef*>> g_landmarks_by_biome;

bool load_landmark_defs(const char* json_path) {
    try {
        std::ifstream f(json_path);
        if (!f.is_open()) { printf("[Landmark] Cannot open %s\n", json_path); return false; }
        json data = json::parse(f);
        g_landmarks.clear(); g_landmarks_by_biome.clear();
        for (auto& obj : data) {
            LandmarkDef lm;
            lm.id = obj["id"].get<std::string>();
            lm.biome = obj["biome"].get<std::string>();
            lm.weight = obj.value("weight", 20);
            auto& tc = obj["tile_color"];
            lm.tile_color = {(uint8_t)tc[0].get<int>(),(uint8_t)tc[1].get<int>(),
                             (uint8_t)tc[2].get<int>(),255};
            lm.icon = obj.value("icon", "?");
            lm.message = obj.value("message", "");
            lm.discovery_msg = obj.value("discovery_msg", "");
            g_landmarks.push_back(lm);
            g_landmarks_by_biome[lm.biome].push_back(&g_landmarks.back());
        }
        printf("[Landmark] Loaded %zu landmarks across %zu biomes\n",
               g_landmarks.size(), g_landmarks_by_biome.size());
        return true;
    } catch (const std::exception& e) {
        printf("[Landmark] Error: %s\n", e.what()); return false;
    }
}

std::vector<const LandmarkDef*> get_landmarks_for_biome(const std::string& biome_id) {
    auto it = g_landmarks_by_biome.find(biome_id);
    if (it != g_landmarks_by_biome.end()) return it->second;
    return {};
}

const LandmarkDef* get_landmark_by_id(const std::string& lm_id) {
    for (auto& lm : g_landmarks) if (lm.id == lm_id) return &lm;
    return nullptr;
}
