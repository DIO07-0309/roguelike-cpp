// M6-HD2D: 场景构建器实现 — GameScene 只读状态 → HD2DDrawItem 列表
// 切片范围: 地板/墙 tile (纯色起步) + 玩家/怪 billboard (贴图) + 特效
// 红线: 只读 gs; 无 gameplay 副作用; 视觉随机只吃 visual_rng (本文件未用随机)
#include "hd2d_scene_builder.h"
#include "scenes/game_scene.h"
#include "world/game_map.h"
#include "entities/player.h"
#include "entities/monster.h"
#include "resources/resource_manager.h"
#include "rendering/sprite_renderer.h"
#include "config.h"                 // TILE_SIZE
#include <algorithm>

namespace hd2d {

// ── tile 颜色回退: 纯色地形 (切片 v1; v2 接 tile 贴图) ──
static Color _tile_color(TileType t, bool visible_now) {
    Color base;
    switch (t) {
    case TileType::WALL:      base = {74, 78, 96, 255};  break;
    case TileType::FLOOR:     base = {40, 42, 58, 255};  break;
    case TileType::LAVA:      base = {200, 90, 30, 255}; break;
    case TileType::STAIRS_DOWN: base = {180, 160, 60, 255}; break;
    case TileType::DOOR:      base = {130, 90, 50, 255}; break;
    default:                  base = {46, 48, 64, 255};  break;
    }
    // fog: 已探索不可见 = 压暗 40%
    if (!visible_now) {
        base.r = base.r * 6 / 10; base.g = base.g * 6 / 10; base.b = base.b * 6 / 10;
    }
    return base;
}

// ── 地形: 玩家周围可见 tile → 地板/墙 item ──
static void _build_terrain(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    const GameMap* map = gs.game_map.get();
    if (!map) return;

    int cx = 0, cy = 0;
    if (gs.player) {
        cx = (int)(gs.player->entity.rect.x / TILE_SIZE);
        cy = (int)(gs.player->entity.rect.y / TILE_SIZE);
    }
    int x0 = std::max(0, cx - 16), x1 = std::min(map->width - 1, cx + 16);
    int y0 = std::max(0, cy - 12), y1 = std::min(map->height - 1, cy + 12);

    for (int ty = y0; ty <= y1; ty++) {
        for (int tx = x0; tx <= x1; tx++) {
            if (!map->isExplored(tx, ty)) continue;
            TileType t = map->tile_at(tx, ty);
            HD2DDrawItem item;
            item.tile_x = tx; item.tile_y = ty;
            item.world_pos = {(float)tx * TILE_SIZE + TILE_SIZE * 0.5f, 0,
                              (float)ty * TILE_SIZE + TILE_SIZE * 0.5f};
            item.size = (float)TILE_SIZE;
            item.tint = _tile_color(t, map->isVisible(tx, ty));
            if (t == TileType::WALL) {
                item.kind = HD2DDrawItem::Kind::WALL_BLOCK;
                item.height = TILE_SIZE * 1.25f;
            } else {
                item.kind = HD2DDrawItem::Kind::FLOOR_TILE;
            }
            out.push_back(item);
        }
    }
}

// ── 实体: 玩家 + 存活怪 → billboard (贴图 = 2D 同源精灵) ──
static void _build_entities(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    auto& res = ResourceManager::inst();

    if (gs.player && gs.player->combat.is_alive) {
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::ENTITY_BILLBOARD;
        const auto& r = gs.player->entity.rect;
        item.world_pos = {r.x + r.width * 0.5f, 0, r.y + r.height * 0.5f};
        item.size = 36.0f;
        item.sort_y = r.y;
        SpriteDef def;
        item.texture = res.sprite_by_key("player_default", def);
        if (item.texture.id > 0) item.tex_src = SpriteRenderer::frame_rect(def, 0);
        else item.tint = {90, 160, 255, 255};
        item.flip_x = (gs.player->direction == Direction::LEFT);
        out.push_back(item);
    }
    for (auto& m : gs.monsters) {
        if (!m || !m->combat.is_alive) continue;
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::ENTITY_BILLBOARD;
        const auto& r = m->entity.rect;
        item.world_pos = {r.x + r.width * 0.5f, 0, r.y + r.height * 0.5f};
        item.size = 34.0f;
        item.sort_y = r.y;
        SpriteDef def;
        if (!m->sprite_override.empty())
            item.texture = res.sprite_by_key(m->sprite_override.c_str(), def);
        if (item.texture.id == 0)
            item.texture = res.sprite_by_key("mon_orc", def);
        if (item.texture.id > 0) item.tex_src = SpriteRenderer::frame_rect(def, 0);
        else item.tint = {220, 80, 80, 255};
        out.push_back(item);
    }
}

// ── 特效: active_effects 存活项 → 发光片 ──
static void _build_effects(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    for (const auto& e : gs.active_effects) {
        if (e.elapsed >= e.duration) continue;
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::FX_QUAD;
        item.world_pos = {e.world_x, 0, e.world_y};
        item.size = e.radius;
        item.tint = e.color;
        out.push_back(item);
    }
}

void build_scene(GameScene& gs, std::vector<HD2DDrawItem>& out_items) {
    _build_terrain(gs, out_items);
    _build_entities(gs, out_items);
    _build_effects(gs, out_items);
}

} // namespace hd2d
