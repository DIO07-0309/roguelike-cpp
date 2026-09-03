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

// G11.2: 群系氛围粒子配置 (biomes.json "ambient" 段)
struct AmbientDef {
    int   count = 0;                    // 同屏粒子数 (0=禁用)
    Color color = {140, 135, 150, 200};
    float size_min = 1.0f, size_max = 2.5f;
    float speed = 12.0f;               // 像素/秒
    bool  rise = true;                 // true=上飘(余烬/幽光) false=下落(尘埃/雪)
    float life_min = 2.5f, life_max = 6.0f;
};

struct BiomeDef {
    std::string id, name, name_en;
    int floor_start, floor_end;       // inclusive range [start, end]
    TilePalette palette;
    AmbientDef ambient;               // G11.2: 氛围层
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
