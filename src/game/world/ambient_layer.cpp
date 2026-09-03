// G11.2: AmbientLayer — biomes.json ambient 实装 + 情绪 vignette
#include "world/ambient_layer.h"
#include "world/biome.h"
#include <cmath>
#include <algorithm>

// 确定性视觉随机 (与 gameplay RNG 隔离; G9.3 visual stream 之外的轻量哈希)
static float _vrand(unsigned int& s) {
    s = s * 1664525u + 1013904223u;
    return (float)(s >> 8 & 0xFFFF) / 65536.0f;
}

void AmbientLayer::set_biome(const BiomeDef* biome) {
    _cfg = AmbientCfg{};                 // 缺省: 无粒子
    if (!biome) { _particles.clear(); return; }
    // biomes.json ambient 段 → cfg (字段缺失走缺省)
    _cfg.count = std::min(24, biome->ambient.count);
    _cfg.color = biome->ambient.color;
    _cfg.size_min = biome->ambient.size_min;
    _cfg.size_max = biome->ambient.size_max;
    _cfg.speed = biome->ambient.speed;
    _cfg.rise = biome->ambient.rise;
    _cfg.life_min = biome->ambient.life_min;
    _cfg.life_max = biome->ambient.life_max;
    _particles.clear();
}

void AmbientLayer::set_mood(float explore_ratio, float kill_momentum) {
    _explore_ratio = std::clamp(explore_ratio, 0.0f, 1.0f);
    _kill_momentum = std::clamp(kill_momentum, 0.0f, 1.0f);
}

void AmbientLayer::_spawn_one(AmbientParticle& p, int map_w, int map_h) const {
    unsigned int seed_base = (unsigned int)(GetTime() * 1000.0f);
    unsigned int s = ((unsigned int)p.x * 73856093u ^ (unsigned int)p.y * 19349663u)
                   ^ seed_base;
    float r1 = _vrand(s), r2 = _vrand(s), r3 = _vrand(s), r4 = _vrand(s);
    p.x = r1 * map_w;
    p.y = r2 * map_h;
    p.size = _cfg.size_min + (_cfg.size_max - _cfg.size_min) * r3;
    p.life = p.max_life = _cfg.life_min + (_cfg.life_max - _cfg.life_min) * r4;
    p.vy = _cfg.rise ? -_cfg.speed : _cfg.speed;
    p.vx = (r1 - 0.5f) * _cfg.speed * 0.4f;   // 轻微横向漂移
    p.alpha = 140;
}

void AmbientLayer::_spawn_all(int map_w, int map_h) {
    _particles.resize((size_t)_cfg.count);
    for (auto& p : _particles) _spawn_one(p, map_w, map_h);
}

void AmbientLayer::update(float dt, int map_w, int map_h,
                          float cam_x, float cam_y, int sw, int sh) {
    (void)cam_x; (void)cam_y; (void)sw; (void)sh;
    if (_cfg.count <= 0) return;
    if (_particles.empty()) { _spawn_all(map_w, map_h); return; }
    for (auto& p : _particles) {
        p.life -= dt;
        if (p.life <= 0) { _spawn_one(p, map_w, map_h); continue; }
        p.y += p.vy * dt;
        p.x += p.vx * dt + sinf(p.y * 0.05f) * 0.15f;   // 蜿蜒感
        // 出界回收 (仅垂直方向)
        if (p.y < -8) p.y = (float)map_h;
        if (p.y > map_h + 8) p.y = 0;
    }
    _vignette_pulse *= (1.0f - std::min(1.0f, dt * 3.0f));   // 脉动衰减
}

void AmbientLayer::draw(float cam_x, float cam_y, int sw, int sh) const {
    (void)sw; (void)sh;
    if (_cfg.count <= 0) return;
    Color c = _cfg.color;
    for (auto& p : _particles) {
        float sx = p.x - cam_x, sy = p.y - cam_y;
        if (sx < -4 || sx > sw + 4 || sy < -4 || sy > sh + 4) continue;
        float fade = p.life / p.max_life;                  // 首尾渐隐
        float a = (float)p.alpha * std::min(1.0f, fade * 2.0f)
                * std::min(1.0f, (p.max_life - p.life) * 2.0f + 0.3f);
        Color pc = {c.r, c.g, c.b, (unsigned char)a};
        if (p.size <= 1.5f) DrawPixel((int)sx, (int)sy, pc);
        else DrawCircle(sx, sy, p.size, pc);
    }
}

void AmbientLayer::draw_vignette(int sw, int sh) const {
    // AI 情绪: 探索 → 冷色渐晕 (未探索越多越冷越暗)
    // 击杀势头 → 暖色脉动边缘 (战斗升温)
    float cold = 1.0f - _explore_ratio;                  // 未探索比例
    float warm = _kill_momentum;
    if (cold < 0.05f && warm < 0.05f) return;            // 全亮无情绪
    int band = 40;
    for (int i = 0; i < band; i++) {
        float t = (float)i / band;                       // 0=最外 1=最内
        float cold_a = cold * 210.0f * (1.0f - t) * (1.0f - t);
        if (cold_a > 1)
            DrawRectangle(0, i, sw, 1, {10, 14, 38, (unsigned char)cold_a});
        float warm_a = warm * 140.0f * (1.0f - t) * (1.0f - t)
                     * (0.7f + 0.3f * sinf((float)GetTime() * 6.0f));
        if (warm_a > 1)
            DrawRectangle(0, sh - 1 - i, sw, 1,
                         {255, 90, 30, (unsigned char)warm_a});
    }
}
