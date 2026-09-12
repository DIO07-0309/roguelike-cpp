#pragma once
#include "raylib.h"
#include "raymath.h"                 // MatrixIdentity/MatrixLookAt/MatrixOrtho
#include <vector>

// ============================================================
// M6-v2e: 光空间深度 RT — Shadow map 前半 (深度 caster)
// 职责 (单一): 深度 fbo 生命周期 + 光空间正交矩阵计算 + 几何深度 pass
// 红线: 纯表现层; 失败 (fbo/深度纹理不支持) → is_ready()=false,
//   renderer 走 blob shadow 回退, 不崩溃
// 光: 固定 45° 俯角方向光 (与 _light_dir 同源), 覆盖相机视野范围
// ============================================================

class HD2DShadowCaster {
public:
    static HD2DShadowCaster& inst();

    // scene_w/h: 场景 RT 尺寸; depth map 尺寸 = scene/2 (性能/质量折中)
    bool ensure_init(int scene_w, int scene_h);
    bool is_ready() const { return _ready; }
    void shutdown();

    // 每帧渲染前: 光空间正交相机跟随玩家 (focus 为相机焦点世界坐标)
    void update_light_camera(Vector3 cam_focus);

    // 深度 pass: 把墙盒几何画进深度 fbo (caller 传墙列表; 实体阴影
    // 走 blob 回退 — billboard alpha 几何会全投影, 不进 depth pass)
    // outer_fbo: 深度 pass 完成后必须恢复绑定的外层渲染目标
    // (raylib 5.0 无查询当前 FBO API; 由 caller 注入 — 通常为主 RT fbo)
    void render_depth(const class GameScene& gs,
                      const std::vector<struct HD2DDrawItem>& items,
                      unsigned int outer_fbo);

    // 主 pass 采样用: 光空间 view/proj 分开暴露 (shader 内 mat 相乘;
    // 地形系 shader 的 mvp 已含主相机, 阴影矩阵需独立乘模型位置)
    const Matrix& light_view() const { return _light_view; }
    const Matrix& light_proj() const { return _light_proj; }
    unsigned int depth_tex_id() const { return _depth_tex_id; }
    int map_width() const { return _map_w; }
    int map_height() const { return _map_h; }
    float depth_texel() const { return 1.0f / (float)_map_w; }
    float texel_world_size() const { return _texel_world_size; }

private:
    HD2DShadowCaster() = default;

    bool _ready = false;
    unsigned int _fbo_id = 0;
    unsigned int _depth_tex_id = 0;
    int _map_w = 0;
    int _map_h = 0;
    float _texel_world_size = 0.0f;   // 世界单位/深度像素 (bias 用)

    Matrix _light_view = MatrixIdentity();
    Matrix _light_proj = MatrixIdentity();
    Camera3D _light_camera = {};

    bool _create_depth_target(int scene_w, int scene_h);
    void _draw_wall_depth(const struct HD2DDrawItem& item);

    HD2DShadowCaster(const HD2DShadowCaster&) = delete;
    HD2DShadowCaster& operator=(const HD2DShadowCaster&) = delete;
};
