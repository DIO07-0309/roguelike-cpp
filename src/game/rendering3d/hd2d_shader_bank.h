#pragma once
#include "raylib.h"
#include <map>
#include <string>

// ============================================================
// M6-v2c: HD2D Shader Bank — GLSL 加载 + 缓存 + 安全回退
// 职责 (单一): assets/shaders/ 下 .vs/.fs → raylib Shader 的
//   懒加载/按名查找/退出卸载; 编译失败记录并让 renderer 回退默认管线
// 红线: 只服务 rendering3d 表现层, 不触碰 gameplay; 失败不崩溃只降级
// ============================================================

class HD2DShaderBank {
public:
    static HD2DShaderBank& inst();

    // name: 不含扩展名 (如 "hd2d_fog" → hd2d_fog.fs + hd2d_world.vs 共享顶点)
    // vs_name: 顶点 shader 名 (地形系共用 hd2d_world; 纯屏后处理传 nullptr = 用默认 vs)
    // 返回 {id>0} 成功; 失败返回空 Shader 并记录 (调用方走回退路径)
    Shader load(const char* name, const char* vs_name);

    // 是否加载成功过 (回退判定用)
    bool is_valid(const char* name) const;

    // shader 数量 (诊断日志用)
    int count() const { return (int)_shaders.size(); }

    void unload_all();

private:
    HD2DShaderBank() = default;

    struct Entry {
        Shader shader = {};
        bool valid = false;          // 编译链接成功
    };

    // name → shader 条目 (懒加载缓存)
    // (map 而非 unordered_map: 条目 ≤6, 遍历确定性友好)
    std::map<std::string, Entry> _shaders;

    HD2DShaderBank(const HD2DShaderBank&) = delete;
    HD2DShaderBank& operator=(const HD2DShaderBank&) = delete;
};
