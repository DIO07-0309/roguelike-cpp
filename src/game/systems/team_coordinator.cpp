#include "team_coordinator.h"
#include "monster.h"
#include "ai.h"              // AIState enum for archer_active check
#include "player.h"
#include "game_map.h"
#include <cmath>

// ============================================================
// G2.2: TeamCoordinator — 怪物协同分析器实现
// 纯函数: 输入 monster list + player, 输出 TeamDecision
// 无副作用, 无状态, 不修改任何对象
// ============================================================

static float _dist2(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1, dy = y2 - y1;
    return dx * dx + dy * dy;
}

// ── 辅助: 找指定 TeamRole 的最近盟友 ──
Monster* TeamCoordinator::_find_nearest_ally_with_role(
    const std::vector<Monster*>& allies, const Monster* self,
    TeamRole role, float ax, float ay) {
    Monster* best = nullptr;
    float best_d2 = (8.0f * 32.0f) * (8.0f * 32.0f);
    for (auto* a : allies) {
        if (!a || a == self || !a->combat.is_alive) continue;
        if (a->team_role != role) continue;
        float ax2 = a->entity.rect.x + a->entity.rect.width / 2;
        float ay2 = a->entity.rect.y + a->entity.rect.height / 2;
        float d2 = _dist2(ax, ay, ax2, ay2);
        if (d2 < best_d2) { best_d2 = d2; best = a; }
    }
    return best;
}

// ── 辅助: 找最近石柱 ──
bool TeamCoordinator::_find_nearest_cover(
    const GameMap* map, float ax, float ay,
    float& out_cx, float& out_cy, float max_dist) {
    if (!map) return false;
    float best_d2 = max_dist * max_dist;
    bool found = false;
    for (auto& ao : map->arena_objects) {
        if (ao.type != ArenaObjectType::ROCK || !ao.active) continue;
        float cx = ao.tile_x * 32.0f + 16;
        float cy = ao.tile_y * 32.0f + 16;
        float d2 = _dist2(ax, ay, cx, cy);
        if (d2 < best_d2) { best_d2 = d2; out_cx = cx; out_cy = cy; found = true; }
    }
    return found;
}

// ============================================================
// 核心: evaluate() — 分析单个怪物的协同上下文
// ============================================================
TeamDecision TeamCoordinator::evaluate(
    const Monster* self, const Player& player,
    const std::vector<Monster*>& all, const GameMap* map) {

    TeamDecision dec;
    TeamRole role = self->team_role;
    if (role == TeamRole::NONE) return dec;

    float ax = self->entity.rect.x + self->entity.rect.width / 2;
    float ay = self->entity.rect.y + self->entity.rect.height / 2;
    float px = player.entity.rect.x + player.entity.rect.width / 2;
    float py = player.entity.rect.y + player.entity.rect.height / 2;
    float dp = sqrtf(_dist2(ax, ay, px, py));

    // ── 收集附近盟友 (8 tile 内) ──
    std::vector<Monster*> allies;
    float range2 = (8.0f * 32.0f) * (8.0f * 32.0f);
    for (auto* o : all) {
        if (!o || o == self || !o->combat.is_alive || o->is_boss) continue;
        float ox = o->entity.rect.x + o->entity.rect.width / 2;
        float oy = o->entity.rect.y + o->entity.rect.height / 2;
        if (_dist2(ax, ay, ox, oy) < range2) allies.push_back(o);
    }
    if (allies.empty()) return dec;

    // 找最近 Tank
    Monster* tank = _find_nearest_ally_with_role(allies, self, TeamRole::FRONTLINE, ax, ay);

    // ═══════════════════════════════════════════════════════
    // FRONTLINE: 保护后排 + 寻找掩体
    // ═══════════════════════════════════════════════════════
    if (role == TeamRole::FRONTLINE && dp < 6.0f * 32.0f) {
        // 优先: 站在石柱和玩家之间
        float cx, cy;
        if (_find_nearest_cover(map, ax, ay, cx, cy, 3.0f * 32.0f)) {
            float mx = cx - px, my = cy - py;
            float len = sqrtf(mx * mx + my * my);
            if (len > 1) { dec.move_x = mx / len; dec.move_y = my / len; return dec; }
        }
        // 次选: 保护最近后排
        Monster* backline = _find_nearest_ally_with_role(allies, self, TeamRole::BACKLINE, ax, ay);
        if (!backline)
            backline = _find_nearest_ally_with_role(allies, self, TeamRole::SUPPORT, ax, ay);
        if (backline) {
            float bx = backline->entity.rect.x + backline->entity.rect.width / 2;
            float by = backline->entity.rect.y + backline->entity.rect.height / 2;
            float mx = (px + bx) / 2 - ax, my = (py + by) / 2 - ay;
            float len = sqrtf(mx * mx + my * my);
            if (len > 1) { dec.move_x = mx / len; dec.move_y = my / len; }
        }
        return dec;
    }

    // ═══════════════════════════════════════════════════════
    // BACKLINE: 保持距离 + 寻找掩体
    // ═══════════════════════════════════════════════════════
    if (role == TeamRole::BACKLINE) {
        float cx, cy;
        if (_find_nearest_cover(map, ax, ay, cx, cy, 3.5f * 32.0f)) {
            float mx = cx - ax, my = cy - ay;
            float len = sqrtf(mx * mx + my * my);
            if (len > 1) { dec.move_x = mx / len; dec.move_y = my / len; return dec; }
        }
        if (tank && dp < 5.0f * 32.0f) {
            float dx = ax - px, dy = ay - py;
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 1) { dec.move_x = dx / len; dec.move_y = dy / len; }
        }
        return dec;
    }

    // ═══════════════════════════════════════════════════════
    // SUPPORT: 找需要治疗的盟友 + 保持后方
    // ═══════════════════════════════════════════════════════
    if (role == TeamRole::SUPPORT) {
        Monster* best = nullptr; float best_score = -1;
        for (auto* a : allies) {
            if (!a->combat.is_alive) continue;
            float hp_pct = (float)a->combat.current_hp / a->combat.max_hp;
            float s = (1.0f - hp_pct) * 100;
            if (a->team_role == TeamRole::FRONTLINE) s += 40;
            else if (a->team_role == TeamRole::COMMAND) s += 25;
            if (s > best_score && hp_pct < 0.85f) { best_score = s; best = a; }
        }
        if (best) {
            dec.should_support = true;
            dec.support_target = best;
            dec.support_type = 1;  // heal (实际由 MonsterAI 决定 buff/heal 随机)
        }
        if (dp < 4.0f * 32.0f) {
            float dx = ax - px, dy = ay - py;
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 1) { dec.move_x = dx / len; dec.move_y = dy / len; }
        }
        return dec;
    }

    // ═══════════════════════════════════════════════════════
    // FLANK: 等待 Tank 接敌, 然后绕侧
    // ═══════════════════════════════════════════════════════
    if (role == TeamRole::FLANK) {
        bool tank_fighting = (tank && hypotf(
            tank->entity.rect.x - player.entity.rect.x,
            tank->entity.rect.y - player.entity.rect.y) < 3.0f * 32.0f);
        bool archer_active = false;
        for (auto* a : allies) {
            if (a->team_role == TeamRole::BACKLINE && a->ai && a->ai->state == AIState::ATTACK) {
                archer_active = true; break;
            }
        }
        if (!tank_fighting && !archer_active && dp > 3.0f * 32.0f) {
            float perp_x = -(py - ay), perp_y = (px - ax);
            float plen = sqrtf(perp_x * perp_x + perp_y * perp_y);
            if (plen > 1) { dec.move_x = perp_x / plen; dec.move_y = perp_y / plen; }
        }
        return dec;
    }

    // ═══════════════════════════════════════════════════════
    // COMMAND: 指挥光环建议
    // ═══════════════════════════════════════════════════════
    if (role == TeamRole::COMMAND) {
        dec.should_command = true;
        return dec;
    }

    return dec;
}
