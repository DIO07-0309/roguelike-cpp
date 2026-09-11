// M6-HD2D: HD-2D 渲染器实现 — 垂直切片第一步 (骨架 + 相机 + 地形/billboard)
// 切片范围: 透视相机 45° 俯视 / 墙体盒子伪光照 / 实体 billboard / 基础后处理
#include "hd2d_renderer.h"
#include "hd2d_scene_builder.h"
#include "scenes/game_scene.h"
#include "world/game_map.h"
#include "entities/player.h"
#include "entities/monster.h"
#include <algorithm>

bool g_hd2d_mode = false;

HD2DRenderer& HD2DRenderer::inst() {
    static HD2DRenderer renderer;
    return renderer;
}

bool HD2DRenderer::ensure_init(int target_w, int target_h) {
    if (_ready) return true;
    _target_w = target_w;
    _target_h = target_h;
    _setup_camera();
    _ready = true;
    return true;
}

void HD2DRenderer::shutdown() {
    _draw_items.clear();
    _ready = false;
}

// ── 相机: 45° 俯视透视, focus 跟随玩家 (2D 世界像素 → 3D x/z, 高度 → y) ──
void HD2DRenderer::_setup_camera() {
    _camera.position = {0, 640, 320};
    _camera.target = {0, 0, 0};
    _camera.up = {0, 1, 0};
    _camera.fovy = 50.0f;
    _camera.projection = CAMERA_PERSPECTIVE;
    _camera_yaw = 0.0f;
}

void HD2DRenderer::render_frame(GameScene& gs) {
    if (!_ready) return;

    // 1. 只读提取绘制列表 (scene_builder 无 gameplay 副作用)
    _draw_items.clear();
    hd2d::build_scene(gs, _draw_items);

    // 2. 相机聚焦玩家世界坐标
    _camera_focus = {0, 0, 0};
    if (gs.player) {
        _camera_focus.x = gs.player->entity.rect.x;
        _camera_focus.z = gs.player->entity.rect.y;
    }
    _draw_scene(gs);
    _apply_post_processing(gs);
}

// ── 场景绘制: 相机定位 + 分 kind 绘制 (地形 → 实体 → 特效) ──
void HD2DRenderer::_draw_scene(GameScene& gs) {
    float cam_dist = 640.0f;
    _camera.position = {
        _camera_focus.x,
        _camera_focus.y + cam_dist * 0.7071f,
        _camera_focus.z + cam_dist * 0.7071f
    };
    _camera.target = _camera_focus;

    BeginMode3D(_camera);
    ClearBackground({12, 14, 24, 255});

    for (const auto& item : _draw_items) {
        if (item.kind == HD2DDrawItem::Kind::FLOOR_TILE) _draw_floor_tile(item);
        else if (item.kind == HD2DDrawItem::Kind::WALL_BLOCK) _draw_wall_block(item);
    }
    for (const auto& item : _draw_items)
        if (item.kind == HD2DDrawItem::Kind::ENTITY_BILLBOARD) _draw_billboard(item);
    for (const auto& item : _draw_items)
        if (item.kind == HD2DDrawItem::Kind::FX_QUAD) _draw_fx_quad(item);
    EndMode3D();
}

// ── 地板: XZ 平面单格 quad (贴图 or 纯色) ──
void HD2DRenderer::_draw_floor_tile(const HD2DDrawItem& item) {
    Vector3 pos = item.world_pos;
    if (item.texture.id > 0) {
        // 贴图地板: 顶视 billboard 平铺 (v1 近似; v2 换 shader 平面采样)
        Rectangle src = item.tex_src.width > 0 ? item.tex_src
            : Rectangle{0, 0, (float)item.texture.width, (float)item.texture.height};
        DrawBillboardRec(_camera, item.texture, src,
                         {pos.x, 0.01f, pos.z}, {item.size, item.size}, item.tint);
    } else {
        DrawPlane(pos, {item.size, item.size}, item.tint);
    }
}

// ── 墙体: 拉伸盒子, 顶面亮/侧面暗的伪光照 (无 shader 版) ──
void HD2DRenderer::_draw_wall_block(const HD2DDrawItem& item) {
    Vector3 pos = item.world_pos;
    float h = item.height;
    Color side = {
        (unsigned char)(item.tint.r * 0.7f),
        (unsigned char)(item.tint.g * 0.7f),
        (unsigned char)(item.tint.b * 0.7f), item.tint.a
    };
    DrawCube({pos.x, h * 0.5f, pos.z}, item.size, h, item.size, side);
    Color top = {
        (unsigned char)std::min(item.tint.r * 1.1f, 255.0f),
        (unsigned char)std::min(item.tint.g * 1.1f, 255.0f),
        (unsigned char)std::min(item.tint.b * 1.1f, 255.0f), item.tint.a
    };
    DrawCube({pos.x, h, pos.z}, item.size, 1.0f, item.size, top);
}

// ── Billboard: 面向相机的精灵, 脚点落地, 帧矩形裁剪 ──
void HD2DRenderer::_draw_billboard(const HD2DDrawItem& item) {
    Vector3 pos = item.world_pos;
    float w = item.size;
    float h = item.size * 1.5f;
    if (item.texture.id > 0) {
        Rectangle src = item.tex_src.width > 0 ? item.tex_src
            : Rectangle{0, 0, (float)item.texture.width, (float)item.texture.height};
        DrawBillboardRec(_camera, item.texture, src,
                         {pos.x, h * 0.5f, pos.z}, {w, h}, item.tint);
    } else {
        DrawCube({pos.x, h * 0.5f, pos.z}, w * 0.5f, h, w * 0.25f, item.tint);
    }
    // 接地阴影: 半透明黑扁片 (v2 换真阴影贴图)
    DrawCube({pos.x, 0.05f, pos.z}, w * 0.55f, 0.08f, w * 0.35f, {0, 0, 0, 100});
}

// ── 特效: 发光脉冲片 (轻量; v2 换 additive shader) ──
void HD2DRenderer::_draw_fx_quad(const HD2DDrawItem& item) {
    Vector3 pos = item.world_pos;
    float pulse = 0.8f + 0.2f * sinf((float)GetTime() * 6.0f + pos.x * 0.1f);
    float s = item.size * pulse;
    DrawCube({pos.x, 0.15f, pos.z}, s, 0.06f, s * 0.6f, item.tint);
}

// ── 后处理占位: 轻色彩分级 + 地平雾 (v2 接 bloom/DOF shader) ──
void HD2DRenderer::_apply_post_processing(GameScene& gs) {
    (void)gs;
    DrawRectangle(0, 0, _target_w, _target_h, {20, 18, 46, 28});
    for (int i = 0; i < 6; i++) {
        int alpha = 50 - i * 8;
        if (alpha <= 0) break;
        DrawRectangle(0, _target_h - (6 - i) * 24, _target_w, 24,
                      {16, 20, 40, (unsigned char)alpha});
    }
}
