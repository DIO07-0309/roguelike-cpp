// M6-v2c: 高斯模糊 (可分离 blur) — 9 tap 高斯核, 方向由 uDir 控制
// 乒乓 pass: 水平 → 垂直, 各一次, 作用于 1/4 分辨率亮部图
#version 330
in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;
uniform vec2 uDir;          // (texel_size_x, 0) 水平 / (0, texel_size_y) 垂直

void main()
{
    // 9-tap 高斯 (σ≈2.2, 权重和=1)
    float w[5] = float[](0.227027, 0.194594, 0.121621, 0.054054, 0.016216);
    vec3 sum = texture(texture0, fragTexCoord).rgb * w[0];
    for (int i = 1; i < 5; i++)
    {
        sum += texture(texture0, fragTexCoord + uDir * float(i)).rgb * w[i];
        sum += texture(texture0, fragTexCoord - uDir * float(i)).rgb * w[i];
    }
    finalColor = vec4(sum, 1.0);
}
