#pragma once
#include "raylib.h"

// ============================================================
// M4f: SpriteRenderer — 像素精灵渲染管线骨架
// 数据流: SpriteDef(定义) → Texture2D(ResourceManager缓存)
//        → draw_sprite() 统一入口
// 素材未到位时用 gen_pixel_tile() 程序化像素纹理占位
// 素材到位后只需把 SpriteDef.path 指向文件, 管线不变
// ============================================================

// 精灵定义 — 对齐 ART_ASSET_PLAN 规格 (32×32 tile, 4方向×5帧)
struct SpriteDef {
    const char* path = nullptr;      // assets/*.png; nullptr = 程序化占位
    int frame_w = 32;                // 单帧宽 px
    int frame_h = 32;                // 单帧高 px
    int frame_count = 1;             // 行内帧数
};

class SpriteRenderer {
public:
    // 帧索引 → 纹理源矩形 (第 row 行第 col 列)
    static Rectangle frame_rect(const SpriteDef& def, int frame);

    // 统一绘制入口: 源矩形 → 目标矩形 (tint 乘色)
    static void draw_sprite(Texture2D tex, const SpriteDef& def, int frame,
                            Rectangle dst, Color tint = WHITE);

    // 程序化像素纹理: 32×32, 基色+噪点+砖缝(墙)/接缝(地板)
    // 返回纹理由调用方持有 (ResourceManager 缓存), 失败返回 {0}
    static Texture2D gen_pixel_tile(Color base, bool wall);

    // M4f.2: 程序化角色/怪物占位精灵 32×32
    // variant: 0=人形 1=圆形(史莱姆) 2=大体型(Boss) 3=箭形(Charger)
    //          4=方甲(Tank) 5=炸弹(Bomber) 6=尖帽(Summoner/Shaman)
    //          eye_dir: 0下1上2左3右
    static Texture2D gen_pixel_sprite(Color body, Color accent,
                                      int variant, int eye_dir = 0);

    // M4f.2: 程序化 VFX 爆点占位 32×32 (中心闪光 + 8 向放射)
    static Texture2D gen_pixel_blast(Color c);

private:
    static void _fill_body(Image* img, Color body, int variant);
    static void _draw_eyes(Image* img, Color accent, int variant, int eye_dir);
    static void _draw_person_body(Image* img, Color body, bool big);
    static void _draw_circle_body(Image* img, Color body);
    static void _draw_shapes(Image* img, Color body, int variant);
    static void _draw_blast_lines(Image* img, Color c);
};
