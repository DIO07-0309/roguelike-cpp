# M6-v2c: 岩浆动画材质片元 — 世界坐标噪声 + 双向流动 + emissive 脉动
# 程序化熔岩: 无贴图依赖, value-noise 生成暗壳/亮流/热核三层,
# 与 2D game_map LAVA tile (深底+裂纹亮脉+热核, game_map.cpp:548) 同风格分段
# 搭配 hd2d_world.vs (共享世界空间顶点)
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragWorldPos;
out vec4 finalColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float uTime;

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float value_noise(vec2 p)
{
    vec2 cell = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);            // smoothstep 插值
    float a = hash21(cell);
    float b = hash21(cell + vec2(1.0, 0.0));
    float c = hash21(cell + vec2(0.0, 1.0));
    float d = hash21(cell + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main()
{
    // 世界坐标 ~62.5px 一个噪声周期 (约 2 tile), 双向缓慢流动
    vec2 noise_pos = fragWorldPos.xz * 0.016;
    float n = value_noise(noise_pos + vec2(uTime * 0.06, uTime * 0.045));
    n += 0.5 * value_noise(noise_pos * 2.13 - vec2(uTime * 0.09, uTime * 0.05));
    // 熔岩三层: 暗壳 → 亮流 → 热核 (与 2D 深底/裂纹亮橙/热核黄 对应)
    vec3 crust = vec3(0.24, 0.06, 0.03);
    vec3 flow  = vec3(0.90, 0.25, 0.05);
    vec3 core  = vec3(1.00, 0.65, 0.20);
    vec3 lava_color = mix(crust, flow, smoothstep(0.35, 0.60, n));
    lava_color = mix(lava_color, core, smoothstep(0.78, 0.95, n));
    // emissive 呼吸 ±15% (频率 4.0 与 2D LAVA pulse 同频)
    float pulse = 0.85 + 0.15 * sin(uTime * 4.0 + n * 6.28318);
    // fragColor.rgb 编码探索态压暗 (白=可见 / 60%灰=已探索不可见)
    finalColor = vec4(lava_color * pulse * fragColor.rgb,
                      fragColor.a * colDiffuse.a);
}
