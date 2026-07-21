#pragma once
// G6.1: Biome system — floor-based visual/content theming
#include <string>
#include <vector>
#include <unordered_map>
#include "raylib.h"

struct TilePalette {
    Color wall_top, wall_face, wall_brick, wall_moss, wall_highlight;
    Color floor_base, floor_joint, floor_dirt, floor_b, floor_c, grid_line;
};

struct BiomeDef {
    std::string id, name, name_en;
    int floor_start, floor_end;       // inclusive range [start, end]
    TilePalette palette;
    std::vector<std::string> enemy_pool;
    std::vector<float> enemy_weights;
    std::string boss_id;              // "5" = shadow_knight, "10" = fire_demon, etc.
    std::string bgm;                  // "prison" | "volcano" | "abyss"
};

// Registry
extern std::vector<BiomeDef> g_biomes;
extern std::unordered_map<int, const BiomeDef*> g_floor_to_biome;

bool load_biome_defs(const char* json_path = "resources/biomes.json");
const BiomeDef* get_biome_for_floor(int floor);
