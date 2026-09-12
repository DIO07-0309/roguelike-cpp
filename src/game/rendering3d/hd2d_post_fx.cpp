// M6-v2c: HD2D 后处理链实现 — Bloom (extract → blur 乒乓 → additive 叠加)
// 全屏搬运用 DrawTexturePro(-y 翻转惯例); 每次 EndTextureMode 后手动恢复
// 外层 FBO (raylib 5.0 盲绑 FBO 0 坑, 见头注释)
#include "hd2d_post_fx.h"
#include "hd2d_shader_bank.h"
#include "core/logger.h"
#include "rlgl.h"

HD2DPostFX& HD2DPostFX::inst() {
    static HD2DPostFX fx;
    return fx;
}

bool HD2DPostFX::ensure_init(int scene_w, int scene_h) {
    if (_ready) {
        if (scene_w == _scene_w && scene_h == _scene_h) return true;
        shutdown();                        // 尺寸变化 → 重建 (防御性)
    }
    _scene_w = scene_w;
    _scene_h = scene_h;
    if (!_load_shaders()) return false;   // shader 链缺一不可

    int qw = scene_w / 4, qh = scene_h / 4;   // 1/4 分辨率 (240x160)
    _quarter_a = LoadRenderTexture(qw, qh);
    _quarter_b = LoadRenderTexture(qw, qh);
    _ready = _quarter_a.id > 0 && _quarter_b.id > 0;
    if (_ready) LOG_INFO("HD2D bloom 链就绪 (%dx%d quarter RT)", qw, qh);
    else { LOG_WARN("HD2D bloom RT 创建失败"); shutdown(); }
    return _ready;
}

bool HD2DPostFX::_load_shaders() {
    auto& bank = HD2DShaderBank::inst();
    _extract_shader = bank.load("hd2d_bloom_extract", nullptr);
    _blur_shader = bank.load("hd2d_bloom_blur", nullptr);
    _shaders_ok = _extract_shader.id > 0 && _blur_shader.id > 0;
    if (!_shaders_ok) LOG_WARN("HD2D bloom shader 不全, bloom 停用");
    return _shaders_ok;
}

void HD2DPostFX::set_params(float threshold, float softness, float intensity) {
    _threshold = threshold;
    _softness = softness;
    _intensity = intensity;
}

void HD2DPostFX::process(RenderTexture2D scene_rt) {
    if (!_ready) return;
    // 外层 FBO (scene_tree 主 RT; process 运行在其 BeginTextureMode 内)
    unsigned int outer_fbo = scene_rt.id;
    _extract_pass(scene_rt);
    _blur_passes();
    // raylib 5.0 EndTextureMode 盲绑 FBO 0 + viewport/投影重置 — 全手动恢复:
    // 1) 绑回主 RT; 2) 视口; 3) 投影 = 960x640 y-down ortho (与
    // BeginTextureMode(main_target) 所设一致, 后续 HUD 2D 绘制依赖它)
    rlEnableFramebuffer(outer_fbo);
    rlViewport(0, 0, _scene_w, _scene_h);
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlOrtho(0, (double)_scene_w, (double)_scene_h, 0, 0.0, 1.0);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
}

void HD2DPostFX::_extract_pass(RenderTexture2D scene_rt) {
    BeginTextureMode(_quarter_a);
    ClearBackground(BLANK);
    BeginShaderMode(_extract_shader);
    SetShaderValue(_extract_shader,
                   GetShaderLocation(_extract_shader, "threshold"),
                   &_threshold, SHADER_UNIFORM_FLOAT);
    SetShaderValue(_extract_shader,
                   GetShaderLocation(_extract_shader, "softness"),
                   &_softness, SHADER_UNIFORM_FLOAT);
    Rectangle src = {0, 0, (float)_scene_w, -(float)_scene_h};
    Rectangle dst = {0, 0, (float)_quarter_a.texture.width,
                     (float)_quarter_a.texture.height};
    DrawTexturePro(scene_rt.texture, src, dst, {0, 0}, 0.0f, WHITE);
    EndShaderMode();
    EndTextureMode();
}

void HD2DPostFX::_blur_once(RenderTexture2D src_rt, RenderTexture2D dst_rt,
                            const float dir[2]) {
    BeginTextureMode(dst_rt);
    ClearBackground(BLANK);
    BeginShaderMode(_blur_shader);
    SetShaderValue(_blur_shader, GetShaderLocation(_blur_shader, "uDir"),
                   dir, SHADER_UNIFORM_VEC2);
    Rectangle src = {0, 0, (float)src_rt.texture.width,
                     -(float)src_rt.texture.height};
    Rectangle dst = {0, 0, (float)dst_rt.texture.width,
                     (float)dst_rt.texture.height};
    DrawTexturePro(src_rt.texture, src, dst, {0, 0}, 0.0f, WHITE);
    EndShaderMode();
    EndTextureMode();
}

void HD2DPostFX::_blur_passes() {
    float dir_h[2] = {1.0f / (float)_quarter_a.texture.width, 0.0f};
    float dir_v[2] = {0.0f, 1.0f / (float)_quarter_a.texture.height};
    _blur_once(_quarter_a, _quarter_b, dir_h);   // A → B (水平)
    _blur_once(_quarter_b, _quarter_a, dir_v);   // B → A (垂直)
}

// ── additive 全屏叠加 (在普通 2D 绘制流内; caller 已恢复外层 FBO) ──
void HD2DPostFX::draw_overlay() {
    if (!_ready) return;
    Rectangle src = {0, 0, (float)_quarter_a.texture.width,
                     -(float)_quarter_a.texture.height};
    Color tint = {255, 255, 255,
                  (unsigned char)(_intensity * 255.0f)};
    BeginBlendMode(BLEND_ADDITIVE);
    DrawTexturePro(_quarter_a.texture, src,
                   {0, 0, (float)_scene_w, (float)_scene_h},
                   {0, 0}, 0.0f, tint);
    EndBlendMode();
}

void HD2DPostFX::shutdown() {
    if (_quarter_a.id > 0) UnloadRenderTexture(_quarter_a);
    if (_quarter_b.id > 0) UnloadRenderTexture(_quarter_b);
    _quarter_a = {};
    _quarter_b = {};
    _ready = false;
    _shaders_ok = false;
}
