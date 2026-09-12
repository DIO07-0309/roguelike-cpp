// M6-v2c: HD2D Shader Bank 实现 — GLSL 懒加载/缓存/回退标记
// 加载路径: assets/shaders/<name>.fs (+ 可选 <vs_name>.vs; 缺省用 raylib 默认 vs)
// raylib 5.0 LoadShader(nullptr, fs) 会自动挂默认顶点布局 (后处理 fs 适用);
// 地形系 shader 必须传 hd2d_world.vs (需要 fragWorldPos 输出)
#include "hd2d_shader_bank.h"
#include "core/logger.h"
#include <map>
#include <string>
#include <cstring>

HD2DShaderBank& HD2DShaderBank::inst() {
    static HD2DShaderBank bank;
    return bank;
}

// ── 磁盘路径拼装: assets/shaders/<name>.<ext> ──
static std::string _shader_path(const char* name, const char* ext) {
    std::string path = "assets/shaders/";
    path += name;
    path += ".";
    path += ext;
    return path;
}

Shader HD2DShaderBank::load(const char* name, const char* vs_name) {
    auto it = _shaders.find(name);
    if (it != _shaders.end())
        return it->second.valid ? it->second.shader : Shader{};

    Entry entry;
    const char* vs_path = vs_name ? _shader_path(vs_name, "vs").c_str() : nullptr;
    const char* fs_path = _shader_path(name, "fs").c_str();
    // vs 缺文件时 raylib 自动回退默认顶点 (LoadShader nullptr 分支)
    entry.shader = LoadShader(vs_path, fs_path);
    entry.valid = entry.shader.id > 0;
    if (!entry.valid) {
        LOG_WARN("HD2D shader [%s] 加载失败, 该项回退默认管线", name);
        entry.shader = {};
    } else {
        LOG_INFO("HD2D shader [%s] 加载成功 (id=%u)", name, entry.shader.id);
    }
    _shaders[name] = entry;
    return entry.valid ? entry.shader : Shader{};
}

bool HD2DShaderBank::is_valid(const char* name) const {
    auto it = _shaders.find(name);
    return it != _shaders.end() && it->second.valid;
}

void HD2DShaderBank::unload_all() {
    for (auto& kv : _shaders) {
        if (kv.second.valid) UnloadShader(kv.second.shader);
    }
    _shaders.clear();
}
