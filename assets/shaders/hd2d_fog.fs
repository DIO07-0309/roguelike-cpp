# M6-v2c: 地形距离雾片元 — 克隆 rlgl 默认采样管线 + 平滑距离雾
# vertexColor 已由 rlColor4ub 写入 (tile tint / FOV 探索压暗语义保留)
# uniform: viewPos(相机世界坐标) fogColor fogStart fogEnd — 每帧由 renderer 设置
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragWorldPos;
out vec4 finalColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 viewPos;
uniform vec4 fogColor;
uniform float fogStart;
uniform float fogEnd;

void main()
{
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec4 color = texelColor * colDiffuse * fragColor;
    // 平滑距离雾: fogStart 内全清晰 → fogEnd 处全雾色 (smoothstep 自带钳制)
    float fog_factor = smoothstep(fogStart, fogEnd, length(fragWorldPos - viewPos));
    finalColor = mix(color, fogColor, fog_factor);
}
