#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "raylib.h"

// ============================================================
// D7 Step4: ResourceManager — 统一资源管理层
// 所有 Font/JSON/Sound 加载/缓存/释放集中于此
// ============================================================

class ResourceManager {
public:
    // ── 单例 ──
    static ResourceManager& inst();

    // ── 生命周期 ──
    void load_all();
    void unload_all();

    // ── Font ──
    Font& font(int size = 32);          // 返回主字体 (32px)
    Font& font_small();                 // 小字体 (18px)
    bool  font_loaded() const { return _font_ok; }

    // ── JSON (返回文件内容字符串, 由各系统自行解析) ──
    std::string load_json_text(const char* path);
    void       invalidate_json_cache(); // 开发环境热重载

    // ── Sound (文件路径 → Sound) ──
    Sound load_sound(const char* path); // MP3/WAV, 自动缓存

private:
    ResourceManager() = default;
    ~ResourceManager();

    void _init_font();
    void _free_font();

    Font  _font = {0}, _font_small_{0};
    bool  _font_ok = false;
    bool  _loaded = false;

    // JSON 缓存 (path → content)
    std::unordered_map<std::string, std::string> _json_cache;

    // Sound 缓存 (path → Sound)
    std::unordered_map<std::string, Sound> _sound_cache;
};
