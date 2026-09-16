# A2.2: 粒子纹理生成器 — 16x16 RGBA 像素风 (dust/ember/firefly)
# 输出: assets/textures/particles/*.png  (确定性: 无随机, 全部公式化)
# 用法: conda run python tools/make_particle_textures.py
import os
from PIL import Image

OUT_DIR = os.path.join("assets", "textures", "particles")
SIZE = 16
C = (SIZE - 1) / 2.0  # 中心 7.5


def _put(img, x, y, rgba):
    if 0 <= x < SIZE and 0 <= y < SIZE:
        img.putpixel((int(x), int(y)), rgba)


def _radial(img, r0, r1, color_fn):
    for y in range(SIZE):
        for x in range(SIZE):
            d = ((x - C) ** 2 + (y - C) ** 2) ** 0.5
            if d <= r1:
                _put(img, x, y, color_fn(d))


def make_dust():
    """灰紫尘埃: 柔和小球 + 一侧暗斑, 低对比 (贴地微粒感)"""
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))

    def col(d):
        a = max(0, int(210 - d * 46))
        shade = 150 if d < 3 else 120
        return (shade, shade - 8, shade + 18, a)

    _radial(img, 0, 4.6, col)
    _put(img, 5, 9, (90, 82, 105, 150))   # 小碎屑细节
    _put(img, 10, 6, (90, 82, 105, 150))
    return img


def make_ember():
    """橙红余烬: 白热核 + 橙环 + 上飘渐灭尾迹 (火头朝下, 尾在上)"""
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))

    def col(d):
        if d < 1.4:
            return (255, 240, 190, 255)
        if d < 2.8:
            return (255, 140, 40, 235)
        return (200, 60, 15, max(0, int(180 - d * 30)))

    _radial(img, 0, 4.8, col)
    for i in range(5):                     # 尾迹: 向上逐颗变暗变小
        _put(img, 8 + (i % 2), 2 + i, (255, 90 + i * 20, 20, 150 - i * 28))
    return img


def make_firefly():
    """紫白幽光: 亮核 + 十字星芒 (深渊悬浮灯感)"""
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))

    def col(d):
        if d < 1.6:
            return (245, 235, 255, 255)
        if d < 3.2:
            return (170, 110, 220, 200)
        return (110, 60, 160, max(0, int(140 - d * 22)))

    _radial(img, 0, 4.2, col)
    for k in range(3, 8):                  # 十字星芒
        a = 170 - k * 20
        for x, y in ((8, 8 - k), (8, 8 + k), (8 - k, 8), (8 + k, 8)):
            _put(img, x, y, (220, 190, 255, max(0, a)))
    return img


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, img in (("dust", make_dust()), ("ember", make_ember()),
                      ("firefly", make_firefly())):
        path = os.path.join(OUT_DIR, f"particle_{name}.png")
        img.save(path)
        print(f"[particles] wrote {path} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
