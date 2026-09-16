// M6-v2c: 共享世界空间顶点着色器 (地形雾/岩浆共用)
// rlgl 即时模式几何: vertexPosition 即世界坐标 (view 变换在 mvp 内),
// 故 fragWorldPos 直接透传, 无需 model 矩阵
// 标准 rlgl attribute 命名 (vertexPosition/vertexTexCoord/vertexColor) —
// LoadShader 按名自动绑定 location, mvp 由 rlgl 绘制时自动写入
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;

out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragWorldPos;

uniform mat4 mvp;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragWorldPos = vertexPosition;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
