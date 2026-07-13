#pragma once
#include <vector>
#include <memory>
#include "types/boss_types.h"

class Player;
class Monster;

// D5 Step4: ArenaManager — Boss战场动态元素

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
