// M6-v2f: 深度 pass 片元 — alpha discard (billboard 透明像素不写深度)
// 与 hd2d_world.vs 配对 (mvp 由 rlgl 绘制时自动写入 = 光空间矩阵)
// depth-only fbo 无色彩附件, 输出色无意义; 只为 discard 语义
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragWorldPos;
out vec4 finalColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

void main()
{
    // 像素画硬边缘 (alpha 0 或 255 为主), 0.5 阈值取剪影
    float a = texture(texture0, fragTexCoord).a;
    if (a * colDiffuse.a * fragColor.a < 0.5) discard;
    finalColor = vec4(1.0);
}
