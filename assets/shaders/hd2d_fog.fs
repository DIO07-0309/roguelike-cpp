// M6-v2e: 地形 shader (雾 + shadow map 采样 + 点光源) — 克自 v2c hd2d_fog.fs
// vertexColor: tile tint (FOV 探索压暗语义保留)
// 阴影: 光空间投影 → depth tex 比较 (PCF 3x3); shadow map 缺失时
//   uniform shadowEnabled=0 → 跳过采样 (blob 回退, renderer 控制)
// 点光源: uniform 数组 pos/range/color (岩浆/火把; 上限 8, count 控制)
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragWorldPos;
out vec4 finalColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

// ── 距离雾 (v2c) ──
uniform vec3 viewPos;
uniform vec4 fogColor;
uniform float fogStart;
uniform float fogEnd;

// ── shadow map (v2e) ──
uniform sampler2D shadowMap;        // 光空间深度纹理
uniform mat4 lightViewProj;         // 光空间矩阵
uniform float shadowEnabled;        // 0/1 (深度 RT 失败时 0)
uniform float shadowTexel;          // 1/depthmap 尺寸 (PCF 步长)
uniform float shadowBias;           // 深度偏移 (抗 acne)

// ── 点光源 (v2e) ──
uniform int pointLightCount;
uniform vec3 pointLightPos[8];
uniform vec3 pointLightColor[8];    // rgb*强度
uniform float pointLightRange[8];

float sample_shadow(vec3 world_pos)
{
    // 世界 → 光空间裁裁坐标; 深度图 [0,1] 对比
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

vec3 point_light(vec3 world_pos, vec3 base_rgb)
{
    vec3 lit = vec3(0.0);
    for (int i = 0; i < pointLightCount; i++) {
        vec3 d = pointLightPos[i] - world_pos;
        float dist2 = dot(d, d);
        float range = pointLightRange[i];
        if (dist2 < range * range) {
            float atten = 1.0 - sqrt(dist2) / range;   // 线性衰减
            atten *= atten;                            // 平方化更柔
            lit += pointLightColor[i] * atten;
        }
    }
    return lit;
}

void main()
{
    vec4 texel_color = texture(texture0, fragTexCoord);
    vec3 color = (texel_color * colDiffuse * fragColor).rgb;

    // v2e: 点光源叠加 (additive, 不受雾/阴影衰减 — 自发光源附近即亮)
    color += point_light(fragWorldPos, color) * fragColor.rgb;

    // v2e: shadow map 阴影系数 (直射光; 失败=1 全亮)
    float shadow = (shadowEnabled > 0.5) ? sample_shadow(fragWorldPos) : 1.0;
    // M6-i.1: 环境光保底 0.62→0.78 — 实测贴图 176 被压到 22 (12.5%),
    // 材质完全不可读; 0.78 满阴影仍有层次 (137) 但群系色可辨
    color *= 0.78 + 0.22 * shadow;

    // v2c: 距离雾
    float fog_factor = smoothstep(fogStart, fogEnd, length(fragWorldPos - viewPos));
    color = mix(color, fogColor.rgb, fog_factor);

    finalColor = vec4(color, (texel_color.a * colDiffuse.a * fragColor.a));
}
