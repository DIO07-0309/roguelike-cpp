#pragma once
#include "raylib.h"
#include <vector>
#include <memory>

class GameScene;
struct HD2DDrawItem;

// ============================================================
// M6-HD2D: HD-2D 渲染器 — 2D 逻辑层不动, 3D 表现层切片
// 设计约束 (P1-C8 教训, 必须遵守):
//   1. 本模块只读 GameScene 状态, 绝不写 gameplay 状态
//   2. 视觉随机只吃 visual_rng (RNG-001/002 红线)
//   3. sim 无头模式不初始化 3D (main.cpp 已保证)
//   4. 切换开关 g_hd2d_mode: true 走本渲染器, false 走原 2D 路径
// ============================================================

// 一帧的 3D 绘制项 (由 HD2DSceneBuilder 从 GameScene 状态提取)
struct HD2DDrawItem {
    enum class Kind { FLOOR_TILE, WALL_BLOCK, ENTITY_BILLBOARD, FX_QUAD };
    Kind kind = Kind::FLOOR_TILE;
    int tile_x = 0;                 // 世界 tile 坐标 (32px/格)
    int tile_y = 0;
    Vector3 world_pos = {0, 0, 0};  // 3D 位置 (世界坐标直转, y=高度)
    float size = 32.0f;            // billboard 宽 / tile 边长
    float height = 32.0f;          // 墙高
    Color tint = WHITE;
    Texture2D texture = {};        // billboard/地板贴图 (0 = 纯色)
    Rectangle tex_src = {};        // 贴图源矩形 (帧动画)
    bool flip_x = false;
    float sort_y = 0.0f;           // billboard 深度排序键 (世界 y)
};

// 单房间切片: 960x640 目标 → 3D 透视相机 + 地形 + billboard
class HD2DRenderer {
public:
    static HD2DRenderer& inst();

    // 生命周期 (首次 --hd2d 启动时懒初始化; 失败则回退 2D)
    bool ensure_init(int target_w, int target_h);
    void shutdown();
    bool is_ready() const { return _ready; }

    // 主入口: 用 GameScene 状态构建绘制列表并渲染一帧
    // (内部: clear → 相机 → 场景构建 → 绘制 → 后处理回 2D target)
    void render_frame(GameScene& gs);

private:
    HD2DRenderer() = default;
    bool _ready = false;
    int _target_w = 960;
    int _target_h = 640;

    Camera3D _camera = {};
    Vector3 _camera_focus = {0, 0, 0};
    float _camera_yaw = 0.0f;       // 观察朝向 (切片固定 45° 俯角)

    // 场景数据缓存
    std::vector<HD2DDrawItem> _draw_items;

    // 切片内简单光照 (无 shader 依赖版: 环境光 + 方向光)
    Vector3 _light_dir = {0.35f, -1.0f, 0.25f};

    void _setup_camera();
    void _draw_scene(GameScene& gs);
    void _draw_floor_tile(const HD2DDrawItem& item);
    void _draw_wall_block(const HD2DDrawItem& item);
    void _draw_billboard(const HD2DDrawItem& item);
    void _draw_fx_quad(const HD2DDrawItem& item);
    void _apply_post_processing(GameScene& gs);

    HD2DRenderer(const HD2DRenderer&) = delete;
    HD2DRenderer& operator=(const HD2DRenderer&) = delete;
};

// 全局开关 (main.cpp --hd2d 设置; 默认 false = 原 2D 路径)
extern bool g_hd2d_mode;
