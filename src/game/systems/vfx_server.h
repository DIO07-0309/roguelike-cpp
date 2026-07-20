#pragma once
#include <string>
#include <memory>
#include <vector>
#include "entity.h"
#include "item.h"
#include "types/combat_types.h"
#include "attack_evolution_state.h"  // G1

class Player;

// Effect / Buff — 见 types/combat_types.h

// ============================================================
// VFXServer — 特效管理 (单例)
// ============================================================
class VFXServer {
public:
    std::vector<Effect> effects;

    void update(float dt);
    void draw(float cam_x, float cam_y) const;

    void player_attack(float cx, float cy, float range,
                       const AttackEvolutionState& evo = AttackEvolutionState{});  // G1
    void slash_skill(float cx, float cy, Direction dir, int level);
    void fireball(float cx, float cy, float tx, float ty, int level);
    void heal(float cx, float cy, int level);
    void monster_attack(float mx, float my, float px, float py, Color c);
    void boss_cone(float cx, float cy);
    void boss_circle(float cx, float cy);
    void boss_summon(float cx, float cy);
    void time_stop(float cx, float cy);
    void hit_flash(float x, float y, float size);

private:
    void _sparks(float cx, float cy, int n, Color c, float dur);
};
