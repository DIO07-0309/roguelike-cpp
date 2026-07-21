#pragma once
// G6.2: Landmark system — per-biome environmental storytelling rooms
#include <string>
#include <vector>
#include <unordered_map>
#include "raylib.h"

struct LandmarkDef {
    std::string id, biome;
    int weight = 20;
    Color tile_color = {60,40,40,255};
    std::string icon = "?";
    std::string message, discovery_msg;
};

extern std::vector<LandmarkDef> g_landmarks;
extern std::unordered_map<std::string, std::vector<const LandmarkDef*>> g_landmarks_by_biome;

bool load_landmark_defs(const char* json_path = "resources/landmarks.json");
std::vector<const LandmarkDef*> get_landmarks_for_biome(const std::string& biome_id);
const LandmarkDef* get_landmark_by_id(const std::string& lm_id);
