#pragma once
#include "raylib.h"

// ============================================================
// M6-v2c: HD2D 后处理链 — Bloom 两 pass + 嵌套 FBO 恢复
// 管线: scene RT (960x640) → bright-extract (1/4) → 高斯乒乓
//   (水平+垂直) → [回外层 FBO] additive 全屏叠加 (BeginBlendMode)
// 设计变更 (vs 初稿): composite 不写回场景 RT — raylib 5.0 即时模式
//   quad 上多 sampler 绑定不可靠, 改为 caller 在 2D 绘制流里叠
//   模糊亮部图 (BLEND_ADDITIVE), 语义等价且只依赖稳定 API
// 红线: 纯屏幕空间, 不读 GameScene; shader 失败 → 整链跳过
// ============================================================

class HD2DPostFX {
public:
    static HD2DPostFX& inst();

    // w/h = 场景 RT 尺寸; 懒初始化 RT + shader (失败 → bloom 永久停用)
    bool ensure_init(int scene_w, int scene_h);
    bool is_ready() const { return _ready; }

    // Pass 1+2: 亮部提取+模糊到内部 RT (嵌套 FBO 已自恢复)
    void process(RenderTexture2D scene_rt);

    // Pass 3: 在 caller 当前绘制目标上 additive 叠加 bloom
    // (必须在 process 之后、无 BeginTextureMode 的普通 2D 流内调用)
    void draw_overlay();

    void set_params(float threshold, float softness, float intensity);

    void shutdown();

private:
    HD2DPostFX() = default;

    bool _ready = false;
    int _scene_w = 0;
    int _scene_h = 0;
    float _threshold = 0.72f;     // bright-pass 阈值
    float _softness = 0.18f;      // 阈值平滑宽
    float _intensity = 0.55f;     // 叠加强度 (draw_overlay tint alpha)

    Shader _extract_shader = {};
    Shader _blur_shader = {};
    bool _shaders_ok = false;

    RenderTexture2D _quarter_a = {};   // 1/4 分辨率乒乓对
    RenderTexture2D _quarter_b = {};

    bool _load_shaders();
    void _extract_pass(RenderTexture2D scene_rt);
    void _blur_once(RenderTexture2D src_rt, RenderTexture2D dst_rt,
                    const float dir[2]);
    void _blur_passes();

    HD2DPostFX(const HD2DPostFX&) = delete;
    HD2DPostFX& operator=(const HD2DPostFX&) = delete;
};
