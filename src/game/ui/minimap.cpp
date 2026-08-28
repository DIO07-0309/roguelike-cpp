#include "minimap.h"
#include "config.h"
#include <cmath>

// ============================================================
// MinimapRenderer (Phase 3)
// ============================================================

// tile → 小地图屏幕矩形（面板左上角为原点）
Rectangle MinimapRenderer::tile_to_screen(int tx, int ty, Rectangle panel) {
    return { panel.x + (float)(tx * MINIMAP_TILE_SIZE),
             panel.y + (float)(ty * MINIMAP_TILE_SIZE),
             (float)MINIMAP_TILE_SIZE, (float)MINIMAP_TILE_SIZE };
}

// tile 类型 → 小地图颜色。visible 控制亮度（当前可见=亮，记忆=暗）
Color MinimapRenderer::color_for_tile(TileType t, bool visible) {
    Color base;
    switch (t) {
        case TileType::FLOOR:      base = {60, 50, 40, 255}; break;
        case TileType::WALL:       base = {90, 90, 110, 255}; break;
        case TileType::DOOR:       base = {150, 110, 45, 255}; break;
        case TileType::STAIRS_DOWN:base = {230, 200, 60, 255}; break;
        case TileType::LAVA:       base = {180, 60, 30, 255}; break;
        default:                   base = {60, 50, 40, 255}; break;
    }
    if (!visible) {
        // 记忆态：压暗 60%
        base.r = (unsigned char)(base.r * 0.55f);
        base.g = (unsigned char)(base.g * 0.55f);
        base.b = (unsigned char)(base.b * 0.55f);
    }
    return base;
}

// 未探索 → 不绘制
bool MinimapRenderer::should_draw_tile(const GameMap& map, int tx, int ty) {
    return map.isExplored(tx, ty);
}

// Boss 最后已知位置：仅当该 tile 已被探索才显示（未探索区绝不泄露）
bool MinimapRenderer::should_show_boss(const GameMap& map, int tx, int ty) {
    return tx >= 0 && ty >= 0 && map.isExplored(tx, ty);
}

// 楼梯：发现后（tile 已探索）永久地标
bool MinimapRenderer::should_show_stairs(const GameMap& map, int tx, int ty) {
    return map.isExplored(tx, ty);
}

// 实体（怪/物品）：仅当前 is_visible 才显示，离开视野消失
bool MinimapRenderer::should_show_entity(const GameMap& map, int tx, int ty) {
    return map.isVisible(tx, ty);
}

// 主绘制
void MinimapRenderer::draw(const GameMap& map, const MinimapInput& input,
                           Rectangle panel) const {
    // 面板背景 + 边框
    DrawRectangleRounded(panel, 0.08f, 8, {18, 18, 26, 235});
    DrawRectangleRoundedLines(panel, 0.08f, 8, 2, {100, 100, 180, 255});

    // 地形（只画已探索）
    for (int ty = 0; ty < map.height; ty++) {
        for (int tx = 0; tx < map.width; tx++) {
            if (!should_draw_tile(map, tx, ty)) continue;
            Rectangle r = tile_to_screen(tx, ty, panel);
            Color c = color_for_tile(map.tile_at(tx, ty), map.isVisible(tx, ty));
            DrawRectangleRec(r, c);
        }
    }

    // 标记（Boss / 楼梯；Boss 始终显示，楼梯需已探索）
    if (input.boss_marker.visible)
        _draw_marker(input.boss_marker, panel);
    if (should_show_stairs(map, input.stairs_marker.tx, input.stairs_marker.ty))
        _draw_marker(input.stairs_marker, panel);
    // 实体标记（仅当前可见的怪物/物品）
    for (const auto& m : input.markers)
        if (should_show_entity(map, m.tx, m.ty)) _draw_marker(m, panel);

    // 玩家（恒显）
    DrawRectangleRec(tile_to_screen(input.player_tx, input.player_ty, panel), WHITE);
}

// 标记绘制：中心填充 + 描边（独立小方法）
void MinimapRenderer::_draw_marker(const MinimapMarker& m, Rectangle panel) const {
    Rectangle r = tile_to_screen(m.tx, m.ty, panel);
    DrawRectangleRec(r, m.color);
    DrawRectangleLinesEx(r, 1.0f, {0, 0, 0, 160});
}
