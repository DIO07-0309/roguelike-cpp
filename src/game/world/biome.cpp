// G6.1: Biome system loader
#include "world/biome.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>

using json = nlohmann::json;

std::vector<BiomeDef> g_biomes;
std::unordered_map<int, const BiomeDef*> g_floor_to_biome;

static Color _parse_color(const json& arr) {
    return Color{(uint8_t)arr[0].get<int>(), (uint8_t)arr[1].get<int>(),
                 (uint8_t)arr[2].get<int>(), 255};
}

static TilePalette _parse_palette(const json& obj) {
    TilePalette p{};
    p.wall_top = _parse_color(obj["wall_top"]); p.wall_face = _parse_color(obj["wall_face"]);
    p.wall_brick = _parse_color(obj["wall_brick"]); p.wall_moss = _parse_color(obj["wall_moss"]);
    p.wall_highlight = _parse_color(obj["wall_highlight"]);
    p.floor_base = _parse_color(obj["floor_base"]); p.floor_joint = _parse_color(obj["floor_joint"]);
    p.floor_dirt = _parse_color(obj["floor_dirt"]); p.floor_b = _parse_color(obj["floor_b"]);
    p.floor_c = _parse_color(obj["floor_c"]); p.grid_line = _parse_color(obj["grid_line"]);
    return p;
}

bool load_biome_defs(const char* json_path) {
    try {
        std::ifstream f(json_path);
        if (!f.is_open()) { printf("[Biome] Cannot open %s\n", json_path); return false; }
        json data = json::parse(f);
        g_biomes.clear(); g_floor_to_biome.clear();
        for (auto& obj : data) {
            BiomeDef b;
            b.id = obj["id"].get<std::string>();
            b.name = obj.value("name", "");
            b.name_en = obj.value("name_en", "");
            auto& fr = obj["floor_range"];
            b.floor_start = fr[0].get<int>(); b.floor_end = fr[1].get<int>();
            b.palette = _parse_palette(obj["tile_palette"]);
            if (obj.contains("enemy_pool")) {
                for (auto& e : obj["enemy_pool"]) b.enemy_pool.push_back(e.get<std::string>());
                for (auto& w : obj["enemy_weights"]) b.enemy_weights.push_back(w.get<float>());
            }
            b.boss_id = obj.value("boss_id", "");
            b.bgm = obj.value("bgm", "");
            g_biomes.push_back(b);
            const BiomeDef* bp = &g_biomes.back();
            for (int f = b.floor_start; f <= b.floor_end; ++f)
                g_floor_to_biome[f] = bp;
        }
        printf("[Biome] Loaded %zu biomes (floors 1-15 mapped)\n", g_biomes.size());
        return true;
    } catch (const std::exception& e) {
        printf("[Biome] Error: %s\n", e.what());
        return false;
    }
}

const BiomeDef* get_biome_for_floor(int floor) {
    auto it = g_floor_to_biome.find(floor);
    return it != g_floor_to_biome.end() ? it->second : nullptr;
}
