#include "arena_manager.h"
#include "player.h"
#include "monster.h"
#include "combat_system.h"
#include <cmath>

// ============================================================
// G2.3: ArenaManager 重构 — 统一入口 + 纯执行
// DangerType 由 BossDef::arena 决定, 位置由 ArenaManager 计算
// ============================================================

// ── 辅助: danger_type 字符串 → DangerZone ──
DangerZone ArenaManager::_create_zone(const std::string& dt, float x, float y,
                                       float dur) {
    DangerZone z;
    z.world_x = x; z.world_y = y; z.remaining = dur; z.warn_timer = 0.5f;
    if (dt == "lava") {
        z.type = DangerType::LAVA; z.radius = 56; z.damage = 3;
        z.warn_color = {255,120,30,100}; z.active_color = {255,60,20,180};
    } else if (dt == "shadow_wall") {
        z.type = DangerType::SHADOW_WALL; z.radius = 40; z.damage = 4;
        z.warn_color = {120,60,180,100}; z.active_color = {140,70,200,180};
    } else if (dt == "void_crack") {
        z.type = DangerType::VOID_CRACK; z.radius = 50; z.damage = 5;
        z.warn_color = {80,40,140,100}; z.active_color = {100,50,160,200};
    } else if (dt == "fire_column") {
        z.type = DangerType::FIRE_COLUMN; z.radius = 44; z.damage = 4;
        z.warn_color = {255,180,40,100}; z.active_color = {255,140,20,200};
    } else {  // fallback
        z.type = DangerType::LAVA; z.radius = 48; z.damage = 3;
        z.warn_color = {255,100,30,100}; z.active_color = {255,50,20,180};
    }
    return z;
}

// ── 辅助: 计算生成位置 (避开玩家, 偏向 Boss 前方) ──
void ArenaManager::_calc_position(float bx, float by, float px, float py,
                                   float radius, float& out_x, float& out_y) {
    // 偏向玩家方向 (Boss→Player 方向 + 随机偏移)
    float dx = px - bx, dy = py - by;
    float len = sqrtf(dx * dx + dy * dy);
    float ndx = (len > 1) ? dx / len : 0.0f;
    float ndy = (len > 1) ? dy / len : 0.0f;
    // 在 Boss→Player 方向的基础上, 加入 ±60° 随机偏移
    float angle = atan2f(ndy, ndx) + ((float)(rng() % 120) - 60) * 0.0174533f;
    float dist = radius * 0.5f + (float)(rng() % (int)(radius * 0.5f));
    out_x = bx + cosf(angle) * dist;
    out_y = by + sinf(angle) * dist;
}

// ── G2.3: 统一事件执行入口 ──
void ArenaManager::execute_event(const ArenaEvent& ev, const BossArenaDef& cfg,
                                  float bx, float by, float px, float py) {
    switch (ev.type) {
    case ArenaEventType::SPAWN_ZONE: {
        int to_spawn = ev.count;
        int slots = cfg.max_zones - (int)_zones.size();
        if (slots <= 0) return;
        if (to_spawn > slots) to_spawn = slots;
        for (int i = 0; i < to_spawn; i++) {
            float zx, zy;
            _calc_position(bx, by, px, py, cfg.spawn_radius, zx, zy);
            float dur = ev.duration > 0 ? ev.duration : cfg.zone_duration;
            _zones.push_back(_create_zone(cfg.danger_type, zx, zy, dur));
        }
        break;
    }
    case ArenaEventType::CLEAR_ALL:
        _zones.clear();
        break;
    case ArenaEventType::INTENSIFY:
        // 所有现存 zone 重置 duration (延长 Boss 战压力)
        for (auto& z : _zones) z.remaining = ev.duration;
        break;
    }
}

void ArenaManager::clear() { _zones.clear(); }

void ArenaManager::tick(float dt, Player* player,
    std::vector<std::unique_ptr<Monster>>& monsters) {
    for (auto& z : _zones) {
        if (z.warn_timer > 0) {
            z.warn_timer -= dt;
        } else {
            z.remaining -= dt;
            if (player) {
                float dx = player->entity.rect.x + player->entity.rect.width/2 - z.world_x;
                float dy = player->entity.rect.y + player->entity.rect.height/2 - z.world_y;
                if (sqrtf(dx*dx + dy*dy) < z.radius)
                    player->combat.take_damage(z.damage);
            }
            for (auto& m : monsters) {
                if (!m->combat.is_alive) continue;
                float dx = m->entity.rect.x + m->entity.rect.width/2 - z.world_x;
                float dy = m->entity.rect.y + m->entity.rect.height/2 - z.world_y;
                if (sqrtf(dx*dx + dy*dy) < z.radius)
                    m->combat.take_damage(z.damage);
            }
        }
    }
    _zones.erase(std::remove_if(_zones.begin(), _zones.end(),
        [](auto& z) { return z.remaining <= 0 && z.warn_timer <= 0; }), _zones.end());
}

void ArenaManager::draw(float cam_x, float cam_y) const {
    for (auto& z : _zones) {
        float sx = z.world_x - cam_x, sy = z.world_y - cam_y;
        float r = z.radius;
        if (z.is_warning()) {
            float pulse = 0.6f + 0.4f * sinf((float)GetTime() * 12);
            Color c = z.warn_color; c.a = (unsigned char)(c.a * pulse);
            DrawRing({sx, sy}, r * 0.5f, r, 0, 360, 24, c);
        } else {
            DrawRing({sx, sy}, r * 0.4f, r, 0, 360, 16, z.active_color);
        }
    }
}
