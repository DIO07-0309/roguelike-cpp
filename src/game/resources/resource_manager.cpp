#include "resource_manager.h"
#include "core/logger.h"
#include "combat_system.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>
#include <set>
#include <dirent.h>
#include <sys/stat.h>

// ═══════════════════════════════════════════════════════════════
// G9.4-fix: 运行时字库自动扫描
//   不再手工维护 FONT_CP 数组
//   启动时自动扫描 src/ + resources/ 所有 .cpp/.h/.json
//   提取每一个 CJK 字符的码位 → 零遗漏
// ═══════════════════════════════════════════════════════════════

// ── 判断是否为 CJK / 全角符号 ──
static inline bool _is_wide_codepoint(int cp) {
    return (cp >= 0x2000);
}

// ── 从 UTF-8 字符串中提取所有宽字符码位 ──
static void _extract_codepoints(const char* s, std::set<int>& out) {
    while (*s) {
        unsigned char b = (unsigned char)*s;
        int cp = 0;
        if ((b & 0x80) == 0)        { cp = b; s++; }
        else if ((b & 0xE0) == 0xC0) { cp = (b&0x1F)<<6|(s[1]&0x3F); s+=2; }
        else if ((b & 0xF0) == 0xE0) { cp = (b&0x0F)<<12|(s[1]&0x3F)<<6|(s[2]&0x3F); s+=3; }
        else { s++; continue; }
        if (_is_wide_codepoint(cp)) out.insert(cp);
    }
}

// ── 扫描一个文本文件，提取宽字符码位 ──
static void _scan_file(const char* path, std::set<int>& out) {
    FILE* f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<char> buf(sz + 1);
    size_t n = fread(buf.data(), 1, sz, f);
    fclose(f);
    if (n == 0 || n > sz) return;
    buf[n] = '\0';
    _extract_codepoints(buf.data(), out);
}

// ── 递归扫描目录 (POSIX dirent, MinGW 可用) ──
static void _scan_dir(const char* dir, const char* ext, std::set<int>& out) {
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* de;
    while ((de = readdir(d)) != nullptr) {
        if (de->d_name[0] == '.') continue;
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", dir, de->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            _scan_dir(full, ext, out);
        } else {
            const char* e = strrchr(de->d_name, '.');
            if (e && strcmp(e, ext) == 0) _scan_file(full, out);
        }
    }
    closedir(d);
}

static int _build_font_cp(std::vector<int>& out) {
    // ── (a) ASCII + 基础符号 (永远不会变) ──
    for (int i = 0x20; i <= 0x7E; i++) out.push_back(i);
    static int _extras[] = {
        0x00B7, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2026,
        0x2190, 0x2191, 0x2192, 0x2193, 0x2208, 0x2260, 0x2265,
        0x2500, 0x25A0, 0x25B2, 0x25B6, 0x25CB, 0x2605,
        0x2713, 0x2717, 0x2744, 0x3001, 0x3002, 0x3010, 0x3011,
        0xFF01, 0xFF08, 0xFF09, 0xFF0C, 0xFF1A, 0xFF1F, 0xFEFF,
        0x2014, 0x2550, 0x2194, 0x2620,
    };
    for (int cp : _extras) out.push_back(cp);

    // ── (b) 运行时扫描全部源文件 + JSON，自动收集 CJK 字符 ──
    std::set<int> cjk;
    _scan_dir("src", ".cpp", cjk);
    _scan_dir("src", ".h", cjk);
    _scan_dir("resources", ".json", cjk);

    for (int cp : cjk) out.push_back(cp);

    // ── (c) 排序 + 去重 ──
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return (int)out.size();
}

// ═══════════════════════════════════════════════════════════════
// 单例
// ═══════════════════════════════════════════════════════════════
ResourceManager& ResourceManager::inst() {
    static ResourceManager rm;
    return rm;
}

ResourceManager::~ResourceManager() { unload_all(); }

void ResourceManager::load_all() {
    if (_loaded) return;
    _loaded = true;
    _init_font();
    LOG_INFO("ResourceManager: fonts loaded");
}

void ResourceManager::unload_all() {
    _free_font();
    for (auto& [_, s] : _sound_cache) UnloadSound(s);
    _sound_cache.clear();
    _json_cache.clear();
    _loaded = false;
}

void ResourceManager::_init_font() {
    const char* font_paths[] = {
        "assets/simhei.ttf",
        "assets/msyh.ttc",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/simsun.ttc",
        "C:/Windows/Fonts/msyhbd.ttc",
        nullptr
    };

    static std::vector<int> cp_vec;
    static int cp_count = 0;
    static bool cp_built = false;
    if (!cp_built) {
        cp_count = _build_font_cp(cp_vec);
        cp_built = true;
        LOG_INFO("ResourceManager: 扫描 %d 个码位 (src/ + resources/)", cp_count);
    }

    for (int i = 0; font_paths[i]; i++) {
        if (!FileExists(font_paths[i])) { LOG_INFO("  跳过: %s", font_paths[i]); continue; }
        _font = LoadFontEx(font_paths[i], 32, cp_vec.data(), cp_count);
        _font_small_ = LoadFontEx(font_paths[i], 18, cp_vec.data(), cp_count);
        LOG_INFO("  尝试: %s -> 字形:%d/%d", font_paths[i], _font.glyphCount, cp_count);
        if (_font.texture.id > 0 && _font.glyphCount > 200) {
            _font_ok = true;
            SetTextureFilter(_font.texture, TEXTURE_FILTER_BILINEAR);
            SetTextureFilter(_font_small_.texture, TEXTURE_FILTER_BILINEAR);
            LOG_INFO("字体: %s (atlas OK)", font_paths[i]);
            return;
        }
        UnloadFont(_font); UnloadFont(_font_small_);
    }
    _font = GetFontDefault(); _font_small_ = GetFontDefault(); _font_ok = false;
    LOG_WARN("无中文字体 — 英文界面");
}

void ResourceManager::_free_font() {
    if (_font.texture.id != GetFontDefault().texture.id && _font.texture.id > 0) {
        UnloadFont(_font); UnloadFont(_font_small_);
    }
}

Font& ResourceManager::font(int size) { return (size <= 20) ? _font_small_ : _font; }
Font& ResourceManager::font_small() { return _font_small_; }

// ═══════════════════════════════════════════════════════════════
// JSON / Sound cache (unchanged)
// ═══════════════════════════════════════════════════════════════

std::string ResourceManager::load_json_text(const char* path) {
    auto it = _json_cache.find(path);
    if (it != _json_cache.end()) return it->second;
    FILE* f = fopen(path, "rb");
    if (!f) { LOG_ERROR("Resource: cannot open %s", path); return ""; }
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string buf(sz + 1, '\0');
    fread(&buf[0], 1, sz, f);
    fclose(f);
    _json_cache[path] = buf;
    return buf;
}

void ResourceManager::invalidate_json_cache() { _json_cache.clear(); }

Sound ResourceManager::load_sound(const char* path) {
    auto it = _sound_cache.find(path);
    if (it != _sound_cache.end()) return it->second;
    if (!FileExists(path)) { Sound dummy = {0}; return dummy; }
    Sound s = LoadSound(path);
    _sound_cache[path] = s;
    return s;
}
