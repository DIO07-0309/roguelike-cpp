// M6-v2e: 光空间深度 RT 实现 — rlgl 原语 depth-only fbo + 墙几何深度 pass
// 深度纹理走 GL_DEPTH_ATTACHMENT (无色彩附件; GPU depth-only 可用)
#include "hd2d_shadow_caster.h"
#include "hd2d_renderer.h"          // HD2DDrawItem
#include "core/logger.h"
#include "rlgl.h"
#include "config.h"                 // TILE_SIZE
#include <cmath>

HD2DShadowCaster& HD2DShadowCaster::inst() {
    static HD2DShadowCaster caster;
    return caster;
}

bool HD2DShadowCaster::ensure_init(int scene_w, int scene_h) {
    if (_ready) return true;
    _ready = _create_depth_target(scene_w / 2, scene_h / 2);
    if (_ready) LOG_INFO("HD2D shadow: 深度 RT 就绪 (%dx%d)", _map_w, _map_h);
    else LOG_WARN("HD2D shadow: 深度 RT 不支持, 回退 blob shadow");
    return _ready;
}

// ── depth-only fbo: rlgl 原语组装 (fbo + depth 纹理附件) ──
bool HD2DShadowCaster::_create_depth_target(int map_w, int map_h) {
    _map_w = map_w;
    _map_h = map_h;
    _fbo_id = rlLoadFramebuffer(map_w, map_h);
    if (_fbo_id == 0) return false;
    // useRenderBuffer=false → 深度可采样纹理 (非 renderbuffer)
    _depth_tex_id = rlLoadTextureDepth(map_w, map_h, false);
    if (_depth_tex_id == 0) { _fbo_id = 0; return false; }
    rlFramebufferAttach(_fbo_id, _depth_tex_id, RL_ATTACHMENT_DEPTH,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    if (!rlFramebufferComplete(_fbo_id)) {
        rlUnloadFramebuffer(_fbo_id);
        _fbo_id = 0;
        _depth_tex_id = 0;
        return false;
    }
    return true;
}

void HD2DShadowCaster::shutdown() {
    if (_fbo_id > 0) rlUnloadFramebuffer(_fbo_id);
    if (_depth_tex_id > 0) rlUnloadTexture(_depth_tex_id);
    _fbo_id = 0;
    _depth_tex_id = 0;
    _ready = false;
}

// ── 光空间相机: 45° 方向光正交投影, 罩住视野半径 ~16 tile ──
// (与 renderer._light_dir 同源的固定方向; 光随相机焦点平移)
void HD2DShadowCaster::update_light_camera(Vector3 cam_focus) {
    float world_radius = TILE_SIZE * 17.0f;   // 覆盖 ±16 tile 视野 + 1 缓冲
    _texel_world_size = (world_radius * 2.0f) / (float)_map_w;

    // 光位置 = 焦点上方沿 -light_dir 反向 (0.35, -1, 0.25)
    Vector3 light_pos = {
        cam_focus.x - 0.35f * world_radius,
        cam_focus.y + world_radius,
        cam_focus.z - 0.25f * world_radius
    };
    _light_camera.position = light_pos;
    _light_camera.target = cam_focus;
    _light_camera.up = {0, 1, 0};
    _light_camera.fovy = world_radius * 2.0f;    // 正交 fovy = 世界高度
    _light_camera.projection = CAMERA_ORTHOGRAPHIC;

    _light_view = MatrixLookAt(_light_camera.position, _light_camera.target,
                               _light_camera.up);
    float r = world_radius;
    _light_proj = MatrixOrtho(-r, r, -r, r, 0.0, world_radius * 4.0);
}

// ── 深度 pass: 只画墙 (billboard 实体走 blob 回退, 见头注释) ──
void HD2DShadowCaster::render_depth(
        const GameScene& gs, const std::vector<HD2DDrawItem>& items,
        unsigned int outer_fbo) {
    (void)gs;
    if (!_ready) return;
    // 备份主相机矩阵 (rlSetMatrix* 直接替换内部状态, 需手动还原)
    Matrix saved_proj = rlGetMatrixProjection();
    Matrix saved_modelview = rlGetMatrixModelview();

    rlEnableFramebuffer(_fbo_id);
    rlViewport(0, 0, _map_w, _map_h);
    rlSetMatrixProjection(_light_proj);
    rlSetMatrixModelview(_light_view);
    rlClearScreenBuffers();                     // depth-only fbo: 清深度
    rlEnableDepthTest();

    for (const auto& item : items)
        if (item.kind == HD2DDrawItem::Kind::WALL_BLOCK) _draw_wall_depth(item);

    rlDisableDepthTest();
    // 还原: 绑回外层渲染目标 (caller 注入; 5.0 无当前 FBO 查询 API)
    rlEnableFramebuffer(outer_fbo);
    rlSetMatrixProjection(saved_proj);
    rlSetMatrixModelview(saved_modelview);
    rlViewport(0, 0, _map_w * 2, _map_h * 2);   // = scene_w/h (map 为其一半)
}

// ── 单墙深度 quad ×4 面 + 顶面 (复用 renderer._wall_quad 顶点公式) ──
void HD2DShadowCaster::_draw_wall_depth(const HD2DDrawItem& item) {
    Vector3 pos = item.world_pos;
    float h = item.height;
    float e = item.size * 0.5f;
    rlBegin(RL_QUADS);
    // 北/南/东/西 + 顶 (纯深度, 无 UV/颜色需求; 法线省略)
    // 北
    rlVertex3f(pos.x - e, 0, pos.z + e); rlVertex3f(pos.x + e, 0, pos.z + e);
    rlVertex3f(pos.x + e, h, pos.z + e); rlVertex3f(pos.x - e, h, pos.z + e);
    // 南
    rlVertex3f(pos.x - e, 0, pos.z - e); rlVertex3f(pos.x + e, 0, pos.z - e);
    rlVertex3f(pos.x + e, h, pos.z - e); rlVertex3f(pos.x - e, h, pos.z - e);
    // 东
    rlVertex3f(pos.x + e, 0, pos.z - e); rlVertex3f(pos.x + e, 0, pos.z + e);
    rlVertex3f(pos.x + e, h, pos.z + e); rlVertex3f(pos.x + e, h, pos.z - e);
    // 西
    rlVertex3f(pos.x - e, 0, pos.z - e); rlVertex3f(pos.x - e, 0, pos.z + e);
    rlVertex3f(pos.x - e, h, pos.z + e); rlVertex3f(pos.x - e, h, pos.z - e);
    // 顶
    rlVertex3f(pos.x - e, h, pos.z - e); rlVertex3f(pos.x + e, h, pos.z - e);
    rlVertex3f(pos.x + e, h, pos.z + e); rlVertex3f(pos.x - e, h, pos.z + e);
    rlEnd();
}
