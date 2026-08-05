#include "game/rendering/sprite_renderer.h"
#include "combat_system.h"   // rng()
#include <algorithm>
#include <cmath>
#include <cstring>

Rectangle SpriteRenderer::frame_rect(const SpriteDef& def, int frame) {
    int col = frame % std::max(1, def.frame_count);
    int row = frame / std::max(1, def.frame_count);
    return {(float)(col * def.frame_w), (float)(row * def.frame_h),
            (float)def.frame_w, (float)def.frame_h};
}

void SpriteRenderer::draw_sprite(Texture2D tex, const SpriteDef& def,
                                 int frame, Rectangle dst, Color tint) {
    if (tex.id <= 0) return;
    Rectangle src = frame_rect(def, frame);
    DrawTexturePro(tex, src, dst, {0, 0}, 0.0f, tint);
}

// ── 程序化像素纹理辅助 (≤40 行/函数) ──
static void _add_noise(Image* img, Color base) {
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) {
            if (rng() % 100 < 8) {
                int d = (rng() % 36) - 18;
                ImageDrawPixel(img, x, y,
                    {(unsigned char)std::clamp((int)base.r + d, 0, 255),
                     (unsigned char)std::clamp((int)base.g + d, 0, 255),
                     (unsigned char)std::clamp((int)base.b + d, 0, 255), 255});
            }
        }
}

static Color _dim(Color c, float f) {
    return {(unsigned char)(c.r * f), (unsigned char)(c.g * f),
            (unsigned char)(c.b * f), 255};
}

static void _draw_wall_details(Image* img, Color base) {
    Color joint = _dim(base, 0.5f);
    for (int y = 0; y < 32; y += 8)
        for (int x = 0; x < 32; x++)
            ImageDrawPixel(img, x, y, joint);
    for (int x = 0; x < 32; x += 8)
        for (int y = 0; y < 32; y++)
            ImageDrawPixel(img, x, y, joint);
    Color bright = {(unsigned char)std::clamp((int)base.r + 30, 0, 255),
                    (unsigned char)std::clamp((int)base.g + 30, 0, 255),
                    (unsigned char)std::clamp((int)base.b + 30, 0, 255), 255};
    for (int x = 0; x < 32; x++)
        for (int y = 0; y < 2; y++)
            ImageDrawPixel(img, x, y, bright);
    for (int x = 0; x < 32; x++)
        for (int y = 30; y < 32; y++)
            ImageDrawPixel(img, x, y, joint);
}

static void _draw_floor_details(Image* img, Color base) {
    Color joint = _dim(base, 0.6f);
    for (int x = 0; x < 32; x++) {
        ImageDrawPixel(img, x, 15, joint);
        ImageDrawPixel(img, x, 31, joint);
    }
    for (int y = 0; y < 32; y++)
        ImageDrawPixel(img, 15, y, joint);
}

// 程序化像素纹理: 基色 + 确定性噪点 + 砖缝/接缝
Texture2D SpriteRenderer::gen_pixel_tile(Color base, bool wall) {
    Image img = GenImageColor(32, 32, base);
    _add_noise(&img, base);
    if (wall) _draw_wall_details(&img, base);
    else      _draw_floor_details(&img, base);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

// ═══════════════════════════════════════════════════════════════
// M4f.2: 程序化角色占位精灵
// ═══════════════════════════════════════════════════════════════

static void _draw_body_shadow(Image* img, int ox, int oy, int w, int h) {
    for (int y = oy + h; y < oy + h + 3; y++)
        for (int x = ox; x < ox + w; x++)
            ImageDrawPixel(img, x, y, {0, 0, 0, 70});
}

// 亮度提升 (呼吸帧/发带用)
static Color _brighten(Color c, int amt) {
    return {(unsigned char)std::min(255, (int)c.r + amt),
            (unsigned char)std::min(255, (int)c.g + amt),
            (unsigned char)std::min(255, (int)c.b + amt), c.a};
}

void SpriteRenderer::_draw_person_body(Image* img, Color body, bool big) {
    int ox = big ? 4 : 8, wd = big ? 24 : 16;
    int oy = big ? 5 : 4, ht = big ? 26 : 18;
    // 头 (顶部) + 躯干
    ImageDrawRectangle(img, ox + wd / 4, oy, wd / 2, wd / 2, body);
    ImageDrawRectangle(img, ox, oy + wd / 2, wd, ht - wd / 2, body);
    // 顶端 accent 发带
    Color cap = _brighten(body, 40);
    for (int x = ox + wd / 4; x < ox + 3 * wd / 4; x++)
        for (int y = oy; y < oy + 2; y++)
            ImageDrawPixel(img, x, y, cap);
    _draw_body_shadow(img, ox, oy, wd, ht);
}

void SpriteRenderer::_draw_circle_body(Image* img, Color body) {
    ImageDrawCircle(img, 16, 15, 13, body);
    _draw_body_shadow(img, 3, 2, 26, 26);
}

// M4f.3: 差异化体型 — 箭形(Charger) / 方甲(Tank) / 炸弹(Bomber) / 尖帽(法师)
static void _draw_arrow_body(Image* img, Color body) {
    for (int i = 0; i < 26; i++) {          // 三角: 顶在下 (冲刺指向)
        int y = 4 + i;
        int half = i < 24 ? 4 + i / 4 : 13;
        for (int x = 16 - half; x <= 16 + half; x++)
            ImageDrawPixel(img, x, y, body);
    }
    ImageDrawRectangle(img, 13, 6, 6, 4, {255, 60, 40, 255});   // 冲刺亮条
}

static void _draw_block_body(Image* img, Color body) {
    ImageDrawRectangle(img, 4, 6, 24, 22, body);                // 方甲
    ImageDrawRectangle(img, 6, 8, 20, 18, {50, 50, 70, 255});   // 内甲暗
    ImageDrawRectangle(img, 10, 2, 12, 4, body);                // 头盔
    for (int y = 10; y < 24; y += 6)                            // 甲缝
        for (int x = 6; x < 26; x++)
            ImageDrawPixel(img, x, y, {20, 20, 30, 255});
}

static void _draw_bomber_body(Image* img, Color body) {
    ImageDrawCircle(img, 16, 17, 12, body);                     // 圆身
    ImageDrawRectangle(img, 15, 2, 2, 8, {200, 120, 40, 255});  // 引信
    ImageDrawRectangle(img, 14, 0, 4, 2, {255, 220, 80, 255});  // 火花
}

static void _draw_hat_body(Image* img, Color body) {
    ImageDrawRectangle(img, 8, 12, 16, 14, body);               // 袍身
    for (int i = 0; i < 12; i++) {                              // 锥帽
        int y = i;
        int half = 10 - i / 2;
        for (int x = 16 - half; x <= 16 + half; x++)
            ImageDrawPixel(img, x, y, {150, 120, 200, 255});
    }
    ImageDrawCircle(img, 16, 16, 5, {240, 220, 255, 255});     // 水晶
}

void SpriteRenderer::_draw_shapes(Image* img, Color body, int variant) {
    switch (variant) {
        case 3: _draw_arrow_body(img, body); break;
        case 4: _draw_block_body(img, body); break;
        case 5: _draw_bomber_body(img, body); break;
        case 6: _draw_hat_body(img, body); break;
        default: break;
    }
}

void SpriteRenderer::_fill_body(Image* img, Color body, int variant) {
    if (variant == 1)      _draw_circle_body(img, body);
    else if (variant == 2) _draw_person_body(img, body, true);
    else if (variant >= 3) _draw_shapes(img, body, variant);
    else                   _draw_person_body(img, body, false);
}

void SpriteRenderer::_draw_eyes(Image* img, Color accent, int variant,
                                int eye_dir) {
    (void)accent;
    if (variant == 1) {   // 圆形: 双眼
        ImageDrawCircle(img, 11, 14, 4, WHITE);
        ImageDrawCircle(img, 21, 14, 4, WHITE);
        ImageDrawCircle(img, 12, 15, 2, {20, 20, 20, 255});
        ImageDrawCircle(img, 22, 15, 2, {20, 20, 20, 255});
        return;
    }
    // 人形: 单眼 + 朝向偏移
    int ex = 16, ey = 16;
    switch (eye_dir) {
        case 1: ey = 13; break;   // up
        case 2: ex = 13; break;   // left
        case 3: ex = 19; break;   // right
        default: ey = 19; break;  // down
    }
    ImageDrawRectangle(img, ex - 2, ey - 2, 4, 4, WHITE);
    ImageDrawRectangle(img, ex - 1, ey - 1, 2, 2, {20, 20, 20, 255});
}

// RGBA8 帧拷贝到 spritesheet (raylib 5.0 无 ImageDrawImage)
static void _blit_frame(Image* sheet, const Image& fr, int row) {
    for (int y = 0; y < fr.height; y++) {
        int dst = ((row * fr.height + y) * sheet->width + 0) * 4;
        int src = (y * fr.width + 0) * 4;
        memcpy((unsigned char*)sheet->data + dst,
               (unsigned char*)fr.data + src, (size_t)fr.width * 4);
    }
}

// M4f.3: 2 帧 spritesheet (32×64: 待机/呼吸) — 与 frame_rect 管线直通
Texture2D SpriteRenderer::gen_pixel_sprite(Color body, Color accent,
                                           int variant, int eye_dir) {
    Image sheet = GenImageColor(32, 64, {0, 0, 0, 0});
    for (int f = 0; f < 2; f++) {
        Image fr = GenImageColor(32, 32, {0, 0, 0, 0});
        Color c = f == 0 ? body : _brighten(body, 18);
        _fill_body(&fr, c, variant);
        _add_noise(&fr, c);
        _draw_eyes(&fr, accent, variant, eye_dir);
        _blit_frame(&sheet, fr, f);
        UnloadImage(fr);
    }
    Texture2D tex = LoadTextureFromImage(sheet);
    UnloadImage(sheet);
    return tex;
}

// ═══════════════════════════════════════════════════════════════
// M4f.2: 程序化 VFX 爆点
// ═══════════════════════════════════════════════════════════════

void SpriteRenderer::_draw_blast_lines(Image* img, Color c) {
    for (int i = 0; i < 8; i++) {
        float a = i * 45.0f * DEG2RAD;
        ImageDrawLine(img, 16, 16,
                      (int)(16 + cosf(a) * 12), (int)(16 + sinf(a) * 12), c);
    }
}

Texture2D SpriteRenderer::gen_pixel_blast(Color c) {
    Image img = GenImageColor(32, 32, {0, 0, 0, 0});
    _draw_blast_lines(&img, c);
    _add_noise(&img, c);
    ImageDrawRectangle(&img, 13, 13, 6, 6, WHITE);
    ImageDrawRectangle(&img, 14, 14, 4, 4, {255, 230, 160, 255});
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}
