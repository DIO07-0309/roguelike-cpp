#pragma once
#include <vector>
#include <memory>
#include "raylib.h"

class Player;
class Monster;

// ============================================================
// D5 Step4: ArenaManager — Boss战场动态元素
// 以后Boss/Event/SpecialRoom共用
// ============================================================

enum class DangerType { LAVA, SHADOW_WALL, VOID_CRACK, FIRE_COLUMN, SOUL_POOL, SPIKE_ZONE };

struct DangerZone {
    DangerType type;
    float world_x, world_y;
    float radius = 48.0f;
    float remaining = 0.0f;
    float warn_timer = 0.0f;
    int   damage = 0;
    Color warn_color{255,80,40,100};
    Color active_color{255,40,20,180};

    bool is_warning() const { return warn_timer > 0; }
};

class ArenaManager {
public:
    void spawn_lava(float x, float y, float dur);
    void spawn_shadow_wall(float x, float y, float dur);
    void spawn_void_crack(float x, float y, float dur);
    void spawn_fire_column(float x, float y, float dur);
    void clear();

    void tick(float dt, Player* player, std::vector<std::unique_ptr<Monster>>& monsters);
    void draw(float cam_x, float cam_y) const;
    const std::vector<DangerZone>& zones() const { return _zones; }
private:
    std::vector<DangerZone> _zones;
};
