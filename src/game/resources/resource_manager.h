#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "raylib.h"
#include "game/rendering/sprite_renderer.h"   // M4f.4: SpriteDef

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

    // ── M4f: Texture (文件路径 → Texture, 自动缓存; 失败返回 {0}) ──
    Texture2D load_texture(const char* path);
    // 程序化像素纹理缓存 (key 唯一, 如 "wall_1a2a2e"); 同 key 复用
    Texture2D procedural_tile(const char* key, Color base, bool wall);
    // M4f.2: 程序化角色/怪物占位精灵缓存
    Texture2D procedural_sprite(const char* key, Color body, Color accent,
                                int variant, int eye_dir = 0);
    // M4f.2: 程序化 VFX 爆点缓存
    Texture2D procedural_fx(const char* key, Color c);

    // ── M4f.4: 数据驱动精灵 (resources/sprites.json → 文件纹理) ──
    bool load_sprite_config();
    // 命中 sprites.json 返回纹理并填出参 def; 未命中返回 {0} (调用方回退程序化)
    Texture2D sprite_by_key(const char* key, SpriteDef& out_def);

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
    // M4f: 纹理缓存 (path/key → Texture)
    std::unordered_map<std::string, Texture2D> _texture_cache;
    // M4f.4: 数据驱动精灵定义 (key → SpriteDef)
    std::unordered_map<std::string, SpriteDef> _sprite_defs;
    bool _sprite_cfg_ok = false;
};
