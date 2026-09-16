// M6-v2c: Bloom 合成 — 场景 + 模糊亮部 additive 叠加 + 强度控制
// 这是终 pass: 直接画回外层 RT (rlDisableFramebuffer 恢复后由 caller 用
// BeginShaderMode + DrawTexturePro 完成, 或本 shader 内两纹理混合)
// 此处采用两纹理混合方案: texture0=场景, texture1=模糊亮部
#version 330
in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;   // 场景原色
uniform sampler2D texture1;   // blur 后亮部
uniform float intensity;      // bloom 强度 (0 关闭)

void main()
{
    vec3 scene_color = texture(texture0, fragTexCoord).rgb;
    vec3 bloom_color = texture(texture1, fragTexCoord).rgb;
    finalColor = vec4(scene_color + bloom_color * intensity, 1.0);
}
