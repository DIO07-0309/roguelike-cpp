#include "arena_manager.h"
#include "player.h"
#include "monster.h"
#include "combat_system.h"
#include <cmath>

void ArenaManager::spawn_lava(float x, float y, float dur) {
    DangerZone z;
    z.type = DangerType::LAVA;
    z.world_x = x; z.world_y = y; z.radius = 56;
    z.remaining = dur; z.warn_timer = 0.5f;
    z.damage = 3;
    z.warn_color = {255,120,30,100}; z.active_color = {255,60,20,180};
    _zones.push_back(z);
}

void ArenaManager::spawn_shadow_wall(float x, float y, float dur) {
    DangerZone z;
    z.type = DangerType::SHADOW_WALL;
    z.world_x = x; z.world_y = y; z.radius = 40;
    z.remaining = dur; z.warn_timer = 0.6f;
    z.damage = 4;
    z.warn_color = {120,60,180,100}; z.active_color = {140,70,200,180};
    _zones.push_back(z);
}

void ArenaManager::spawn_void_crack(float x, float y, float dur) {
    DangerZone z;
    z.type = DangerType::VOID_CRACK;
    z.world_x = x; z.world_y = y; z.radius = 50;
    z.remaining = dur; z.warn_timer = 0.7f;
    z.damage = 5;
    z.warn_color = {80,40,140,100}; z.active_color = {100,50,160,200};
    _zones.push_back(z);
}

void ArenaManager::spawn_fire_column(float x, float y, float dur) {
    DangerZone z;
    z.type = DangerType::FIRE_COLUMN;
    z.world_x = x; z.world_y = y; z.radius = 44;
    z.remaining = dur; z.warn_timer = 0.45f;
    z.damage = 4;
    z.warn_color = {255,180,40,100}; z.active_color = {255,140,20,200};
    _zones.push_back(z);
}

void ArenaManager::clear() { _zones.clear(); }

void ArenaManager::tick(float dt, Player* player,
    std::vector<std::unique_ptr<Monster>>& monsters) {
    for (auto& z : _zones) {
        if (z.warn_timer > 0) {
            z.warn_timer -= dt;
            // 预警阶段: 仅显示, 不减remaining
        } else {
            z.remaining -= dt;
            // 伤害判定: 玩家
            if (player) {
                float dx = player->entity.rect.x + player->entity.rect.width/2 - z.world_x;
                float dy = player->entity.rect.y + player->entity.rect.height/2 - z.world_y;
                if (sqrtf(dx*dx + dy*dy) < z.radius)
                    player->combat.take_damage(z.damage);
            }
            // 伤害判定: 怪物
            for (auto& m : monsters) {
                if (!m->combat.is_alive) continue;
                float dx = m->entity.rect.x + m->entity.rect.width/2 - z.world_x;
                float dy = m->entity.rect.y + m->entity.rect.height/2 - z.world_y;
                if (sqrtf(dx*dx + dy*dy) < z.radius)
                    m->combat.take_damage(z.damage);
            }
        }
    }
    // 移除过期zone
    _zones.erase(std::remove_if(_zones.begin(), _zones.end(),
        [](auto& z) { return z.remaining <= 0 && z.warn_timer <= 0; }), _zones.end());
}

void ArenaManager::draw(float cam_x, float cam_y) const {
    for (auto& z : _zones) {
        float sx = z.world_x - cam_x, sy = z.world_y - cam_y;
        float r = z.radius;
        if (z.is_warning()) {
            // 预警: 半透明闪烁圆
            float pulse = 0.6f + 0.4f * sinf((float)GetTime() * 12);
            Color c = z.warn_color; c.a = (unsigned char)(c.a * pulse);
            DrawRing({sx, sy}, r * 0.5f, r, 0, 360, 24, c);
        } else {
            DrawRing({sx, sy}, r * 0.4f, r, 0, 360, 16, z.active_color);
        }
    }
}
