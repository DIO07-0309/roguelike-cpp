#include "floor_manager.h"
#include "player.h"
#include "monster.h"
#include "ai.h"
#include "game_map.h"
#include "combat_system.h"
#include "config.h"
#include "floor_config.h"
#include "growth_curve.h"

// D1: 从 FloorConfig 读取概率 → 返回怪物类型
static const char* _pick_monster_type(const FloorConfig& cfg) {
    int w[6];
    for (int i = 0; i < 6; i++) w[i] = cfg.enemy_weights[i];
    // 计算总权重
    int total = 0;
    for (int i = 0; i < 6; i++) total += w[i];
    if (total <= 0) return "slime";
    int roll = (int)(rng() % (uint32_t)total);
    int sum = 0;
    for (int i = 0; i < 6; i++) {
        if (w[i] == 0) continue;
        sum += w[i];
        if (roll < sum) {
            switch (i) {
                case 0: return (rng() % 3 == 0) ? "orc" : "slime";
                case 1: return "archer";
                case 2: return "shaman";
                case 3: return "bomber";
                case 4: return "tank";
                case 5: return "elite";
            }
        }
    }
    return "slime"; // fallback
}

void FloorManager::spawn_floor_monsters(int floor_number, GameMap* map,
                                         std::vector<std::unique_ptr<Monster>>& out_monsters,
                                         const std::vector<std::pair<int,int>>& rooms) {
    const FloorConfig* cfg = get_floor_config(floor_number);
    // D4.6: HP/ATK 从统一 GrowthCurveSystem 读取 (不再使用 cfg->hp_mult/atk_mult)
    const GrowthCurve& gc = g_growth.curve(floor_number);
    float hp_m  = gc.monster_hp;
    float atk_m = gc.monster_atk;
    int count = cfg->monster_count;

    int ri = 1;
    while ((int)out_monsters.size() < count && ri < 500) {
        auto [tx, ty] = rooms[ri % rooms.size()];
        int off_x = (int)(rng() % 5) - 2;
        int off_y = (int)(rng() % 5) - 2;
        int stx = tx + off_x, sty = ty + off_y;
        if (map->is_walkable(stx, sty)) {
            auto [px, py] = map->tile_to_pixel(stx, sty);
            const char* type = _pick_monster_type(*cfg);
            auto* m = spawn_monster(px, py, type);
            m->combat.max_hp = (int)(m->combat.max_hp * hp_m);
            m->combat.current_hp = m->combat.max_hp;
            m->combat.attack = (int)(m->combat.attack * atk_m);
            m->entity.sync_rect();
            if (m->ai) m->ai->team_coop_chance = cfg->team_coop_chance;
            out_monsters.emplace_back(m);
        }
        ri++;
    }
}

bool FloorManager::is_floor_cleared(const std::vector<std::unique_ptr<Monster>>& monsters) {
    for (auto& m : monsters)
        if (m->combat.is_alive) return false;
    return true;
}

int FloorManager::check_floor_transition(const InputMap& input, int current_floor,
                                          GameMap* map, const Player* player,
                                          std::pair<int,int> stairs_pos) {
    if (!player || !map) return -1;

    auto [tx, ty] = map->pixel_to_tile(
        player->entity.rect.x + player->entity.rect.width / 2,
        player->entity.rect.y + player->entity.rect.height / 2);

    if (std::make_pair(tx, ty) != stairs_pos) return -1;
    if (!input.is_action_just_pressed("descend")) return -1;

    if (current_floor >= MAX_FLOORS) return MAX_FLOORS; // 通关
    return current_floor + 1; // 下楼
}
