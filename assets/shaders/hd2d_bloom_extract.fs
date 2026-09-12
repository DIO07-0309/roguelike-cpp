# M6-v2c: Bloom 亮部提取 (bright-pass) — 阈值以上部分平方衰减输出
# 输入: scene RT 纹理; 输出: 1/4 分辨率亮部图
#version 330
in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;
uniform float threshold;    // 亮度阈值 (0~1, 建议联动场景照明均值)
uniform float softness;     // 平滑宽度

void main()
{
    vec3 c = texture(texture0, fragTexCoord).rgb;
    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));   // Rec.709 亮度
    float contribution = smoothstep(threshold, threshold + softness, lum);
    // 平方衰减: 亮部越亮 bloom 越浓, 暗部不泄漏
    finalColor = vec4(c * contribution * contribution, 1.0);
}
