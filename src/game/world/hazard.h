#pragma once
// G6.3: Hazard system — environmental gameplay on landmark rooms
#include <string>
#include <vector>
#include <unordered_map>

struct HazardDef {
    std::string id, biome, landmark_id;
    std::string effect;       // "slow_zone" | "burn_tick" | "confuse" | "deflect"
    float interval = 3.0f;
    int damage = 0;
    float slow_factor = 1.0f;
    float param = 0.0f;
    std::string message;
};

extern std::vector<HazardDef> g_hazards;
extern std::unordered_map<std::string, std::vector<const HazardDef*>> g_hazards_by_landmark;

bool load_hazard_defs(const char* json_path = "resources/hazards.json");
std::vector<const HazardDef*> get_hazards_for_landmark(const std::string& landmark_id);
