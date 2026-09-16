// A1.1: 3D-aware billboard 真轮廓描边 — Sprite Alpha-Mask 8 邻域
// 与旧 4 向偏移的区别: 描边精确贴合精灵剪影 (含对角), 单 draw call 完成
// uTexelOffset 由 CPU 端换算: 世界基准宽 → 屏幕 px (clamp 1~4) → UV 偏移
// (fragTexCoord 是图集 UV: rlgl 已把 src 矩形映到 tex_uv 空间)
// 剪影阈值 0.5 与 hd2d_depth.fs 阴影 pass 同源 (像素画硬 alpha)
// 描边不进 shadow map: 本体几何在 caster 侧不变, 本 shader 只服务主 pass
// A3: 实体接收阴影 — 与地形同源采样 (lightViewProj + PCF 3x3 + 0.78 环境保底),
//   逐像素投影: 身上影带与地面阴影对齐, 高于遮挡物的部分自然恢复亮度
// 搭配 hd2d_world.vs; 加载失败由 C++ 回退旧 4 向偏移描边
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragWorldPos;
out vec4 finalColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float uTexelOffset;      // 8 邻域采样 UV 步长
uniform vec4 uOutlineColor;      // 描边色 (CPU 传入, 不硬编码)
uniform float uAlphaThreshold;   // 剪影 alpha 阈值

// ── A3: shadow map (与 hd2d_fog.fs 同字段; 缺失时 shadowEnabled=0) ──
uniform sampler2D shadowMap;
uniform mat4 lightViewProj;
uniform float shadowEnabled;
uniform float shadowTexel;
uniform float shadowBias;

float sample_shadow(vec3 world_pos)
{
    vec4 lp = lightViewProj * vec4(world_pos, 1.0);
    vec3 proj = lp.xyz / lp.w;
    proj = proj * 0.5 + 0.5;                  // NDC → [0,1]
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0
        || proj.z > 1.0) return 1.0;          // 光范围外不投影
    float lit = 0.0;
    for (int ox = -1; ox <= 1; ox++) {
        for (int oy = -1; oy <= 1; oy++) {
            float depth = texture(shadowMap,
                proj.xy + vec2(ox, oy) * shadowTexel).r;
            lit += (proj.z - shadowBias <= depth) ? 1.0 : 0.0;
        }
    }
    return lit / 9.0;                          // PCF 3x3 软化
}

// 8 方向: 4 正向 + 4 对角 (对角长度一致 → 轮廓环均匀)
const vec2 DIRS[8] = vec2[8](
    vec2( 1.0,  0.0), vec2(-1.0,  0.0),
    vec2( 0.0,  1.0), vec2( 0.0, -1.0),
    vec2( 0.7071,  0.7071), vec2(-0.7071,  0.7071),
    vec2( 0.7071, -0.7071), vec2(-0.7071, -0.7071));

void main()
{
    vec4 body = texture(texture0, fragTexCoord);
    // A3: 阴影系数 (0=全影 0.78 / 1=受光; 与地形 0.78+0.22*shadow 同源)
    float sh = (shadowEnabled > 0.5) ? sample_shadow(fragWorldPos) : 1.0;
    vec3 shade = vec3(0.78 + 0.22 * sh);
    // 本体: 精灵色 × 顶点 tint (呼吸/fade/群系压暗全走这条) × 阴影
    if (body.a >= uAlphaThreshold) {
        finalColor = vec4(body.rgb * fragColor.rgb * colDiffuse.rgb * shade,
                          body.a * fragColor.a * colDiffuse.a);
        return;
    }
    // 轮廓环: 自身透明 + 任一邻域为剪影 → 描边色 (随 tint alpha 同步淡出)
    for (int i = 0; i < 8; i++) {
        if (texture(texture0, fragTexCoord + DIRS[i] * uTexelOffset).a
                >= uAlphaThreshold) {
            finalColor = vec4(uOutlineColor.rgb * shade,
                              uOutlineColor.a * fragColor.a * colDiffuse.a);
            return;
        }
    }
    // 外部空隙: 不产片元 (防描边色在精灵框内扩散成黑块)
    discard;
}
