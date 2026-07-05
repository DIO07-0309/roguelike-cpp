#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <random>
#include "entity.h"
#include "combat_stats.h"

class Player;
class Monster;
class GameMap;

// ============================================================
// CombatSystem — 战斗公式 (纯静态函数)
// ============================================================
extern std::mt19937 rng;

inline int calculate_damage(int atk, int def, AttackType type = AttackType::PHYSICAL) {
    float base = std::max(1.0f, atk - def * 0.5f);
    float variance = 0.8f + (float)(rng() % 401) / 1000.0f;  // 0.8 ~ 1.2
    return std::max(1, (int)(base * variance));
}

inline Monster* find_attack_target(Rectangle attacker_rect,
                                    const std::vector<Monster*>& targets,
                                    float range = 1.5f) {
    float range_px = range * 32.0f;
    Monster* best = nullptr;
    float best_dist = 1e9f;
    for (auto* t : targets) {
        if (!t || !t->combat.is_alive) continue;
        float dx = attacker_rect.x + attacker_rect.width/2
                 - t->entity.rect.x - t->entity.rect.width/2;
        float dy = attacker_rect.y + attacker_rect.height/2
                 - t->entity.rect.y - t->entity.rect.height/2;
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist <= range_px && dist < best_dist) {
            best = t; best_dist = dist;
        }
    }
    return best;
}

// 锥形范围内的目标 (用于斩击) — 定义在 skill.cpp
std::vector<Monster*> get_targets_in_cone(Player* caster,
    const std::vector<Monster*>& targets, int cone_range);
