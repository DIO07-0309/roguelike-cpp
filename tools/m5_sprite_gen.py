# M5 素材生成器 — 按现有 48 张手工像素画的画法规律程序化生成
# 规律 (analyze_sprites.py 解码): 16x16, RGBA, 统一描边色(63,38,49),
# 2-3 高饱和主色 + 高光/暗部, 透明背景
import os
from PIL import Image, ImageDraw

OUT = 'assets/sprites'
OUTLINE = (63, 38, 49, 255)          # 全局深色描边
CANVAS = 16


def base():
    img = Image.new('RGBA', (CANVAS, CANVAS), (0, 0, 0, 0))
    return img, ImageDraw.Draw(img)


def px(d, x, y, c):
    d.point((x, y), fill=c)


def rect(d, x0, y0, x1, y1, c):
    d.rectangle([x0, y0, x1, y1], fill=c)


def outline_rect(d, x0, y0, x1, y1, fill, outline=OUTLINE):
    d.rectangle([x0, y0, x1, y1], fill=fill, outline=outline)


def eye_pair(d, cx, y, color=(255, 255, 255, 255)):
    px(d, cx - 2, y, color); px(d, cx - 2, y + 1, (63, 38, 49, 255))
    px(d, cx + 1, y, color); px(d, cx + 1, y + 1, (63, 38, 49, 255))


def save(img, name):
    img.save(os.path.join(OUT, name))
    print('saved', name)


# ── M5-B 兜底怪: shadow_stalker (暗影潜行者 — 监狱+深渊双出场) ──
def gen_shadow_stalker():
    img, d = base()
    body = (58, 48, 92, 255)       # 深紫影躯
    glow = (148, 110, 220, 255)    # 高光紫
    eye = (255, 80, 120, 255)      # 发光红眼
    # 悬浮披风轮廓: 上宽下收的幽灵形
    outline_rect(d, 3, 2, 12, 4, body)
    outline_rect(d, 2, 5, 13, 9, body)
    outline_rect(d, 3, 10, 12, 12, body)
    # 披风下摆锯齿 (幽灵尾)
    for i, x in enumerate((3, 5, 7, 9, 11)):
        px(d, x, 13, body); px(d, x, 14, body)
        if i % 2 == 0:
            px(d, x, 15, OUTLINE)
    # 内阴影渐变
    rect(d, 4, 6, 11, 8, (44, 36, 72, 255))
    # 高光边 (左肩受光)
    for x in range(3, 8):
        px(d, x, 5, glow)
    # 发光眼
    px(d, 5, 5, eye); px(d, 10, 5, eye)
    px(d, 5, 4, (255, 160, 180, 255)); px(d, 10, 4, (255, 160, 180, 255))
    save(img, 'mon_shadow_stalker.png')


# ── M5-B 兜底怪: fire_imp (火魔仆从 — 火山30%出场) ──
def gen_fire_imp():
    img, d = base()
    body = (232, 96, 40, 255)      # 火焰橙躯干
    dark = (160, 52, 28, 255)      # 暗部
    horn = (252, 190, 60, 255)     # 亮黄角/爪
    # 小恶魔人形: 头大身小
    outline_rect(d, 4, 2, 11, 8, body)
    rect(d, 5, 6, 10, 7, dark)     # 嘴部暗色
    # 小角
    px(d, 4, 1, horn); px(d, 11, 1, horn)
    px(d, 3, 0, horn); px(d, 12, 0, horn)
    # 眼 (黄瞳)
    px(d, 6, 4, (255, 240, 160, 255)); px(d, 9, 4, (255, 240, 160, 255))
    # 身体+短腿
    outline_rect(d, 5, 9, 10, 12, body)
    px(d, 6, 13, dark); px(d, 9, 13, dark)
    # 尾巴带火尖
    px(d, 12, 10, body); px(d, 13, 9, body); px(d, 14, 8, horn)
    # 火焰光环 (肩部两侧)
    px(d, 3, 8, horn); px(d, 12, 8, horn)
    save(img, 'mon_fire_imp.png')


# ── M5-B 兜底怪: elite_orc (精英兽人 — 火山25%, 比普通 orc 更重甲) ──
def gen_elite_orc():
    img, d = base()
    skin = (240, 92, 60, 255)      # 红调兽人肤 (普通 orc 是 189,108,74)
    armor = (110, 118, 130, 255)   # 重甲铁灰
    armor_hi = (160, 170, 185, 255)
    spike = (230, 232, 240, 255)   # 甲刺亮色
    # 宽厚躯干 (比普通 orc 大一圈占满画幅)
    outline_rect(d, 2, 3, 13, 13, skin)
    # 胸甲 (铁灰大块 + 肩甲)
    outline_rect(d, 3, 5, 12, 10, armor)
    rect(d, 4, 6, 11, 9, armor_hi)  # 胸甲高光
    rect(d, 4, 8, 11, 9, armor)     # 甲纹
    # 肩刺
    px(d, 2, 4, spike); px(d, 13, 4, spike)
    # 头部 + 怒目
    outline_rect(d, 5, 1, 10, 4, skin)
    px(d, 6, 2, (255, 255, 255, 255)); px(d, 9, 2, (255, 255, 255, 255))
    px(d, 6, 3, (232, 69, 55, 255)); px(d, 9, 3, (232, 69, 55, 255))
    # 獠牙
    px(d, 5, 4, (255, 244, 220, 255)); px(d, 10, 4, (255, 244, 220, 255))
    # 腿
    rect(d, 4, 14, 6, 15, skin); rect(d, 9, 14, 11, 15, skin)
    save(img, 'mon_elite_orc.png')


# ── M5-B 兜底怪: shadow_assassin (暗影刺客 — 深渊25%, 苗条+双刃) ──
def gen_shadow_assassin():
    img, d = base()
    cloak = (72, 60, 110, 255)     # 深紫斗篷
    dark = (48, 40, 78, 255)
    blade = (170, 230, 255, 255)   # 幽蓝双刃
    blade_hi = (230, 250, 255, 255)
    # 苗条人形: 头巾斗篷
    outline_rect(d, 5, 1, 10, 5, cloak)
    rect(d, 6, 3, 9, 4, dark)      # 面巾
    px(d, 6, 3, (255, 120, 120, 255)); px(d, 9, 3, (255, 120, 120, 255))  # 露眼
    # 收窄躯干 (苗条感: 4px 宽)
    outline_rect(d, 5, 6, 10, 12, cloak)
    rect(d, 7, 7, 8, 11, dark)     # 中缝
    # 双刃: 左右各一把短匕
    for bx in (2, 13):
        for i in range(4):
            px(d, bx, 8 + i, blade)
        px(d, bx, 8, blade_hi)
        px(d, bx + (1 if bx < 8 else -1), 12, (150, 120, 80, 255))  # 刀柄
    # 腿
    px(d, 6, 13, cloak); px(d, 7, 14, dark); px(d, 8, 13, cloak); px(d, 9, 14, dark)
    save(img, 'mon_shadow_assassin.png')


# ── M5-A 群系贴图: wall (3 套, 参照现有 wall.png 的 4x4 砖纹) ──
def gen_wall(name, bright, mid, dark, extra=None):
    img, d = base()
    # 4 行砖 x 每行 2 块 (16px 行高 4px), 参照原图 4px 砖格
    for row in range(4):
        y0 = row * 4
        y1 = y0 + 3
        # 行底缝
        for x in range(16):
            px(d, x, y1, dark)
        # 两块砖 (交错)
        offset = 0 if row % 2 == 0 else 4
        for seg in range(3):
            x0 = seg * 8 - offset
            for y in range(y0, y1):
                for x in range(x0 + 1, x0 + 7):
                    if 0 <= x < 16:
                        px(d, x, y, bright if (x + y) % 5 else mid)
            for y in range(y0, y1):
                if 0 <= x0 < 16:
                    px(d, x0, y, mid)
    if extra:
        extra(d)
    img.save(os.path.join(OUT, name))
    print('saved', name)


def prison_wall():
    def moss(d):
        for x, y in ((2, 2), (3, 2), (13, 10), (14, 10), (6, 14), (7, 14)):
            px(d, x, y, (96, 140, 90, 255))
    return gen_wall('wall_prison.png',
                    (139, 155, 180, 255),   # 亮石
                    (82, 96, 124, 255),     # 中缝 (沿用现有配色)
                    (52, 60, 84, 255),
                    moss)


def volcano_wall():
    def lava(d):
        # 熔岩裂缝发光 (亮橙核心 + 黄尖)
        for x, y in ((4, 6), (5, 6), (4, 7), (11, 12), (12, 12), (12, 13), (1, 14)):
            px(d, x, y, (255, 160, 60, 255))
        px(d, 5, 5, (255, 230, 110, 255))
        px(d, 11, 11, (255, 230, 110, 255))
    return gen_wall('wall_volcano.png',
                    (140, 92, 82, 255),   # 玄武岩棕红 (提亮)
                    (96, 58, 52, 255),
                    (58, 34, 32, 255),
                    lava)


def abyss_wall():
    def runes(d):
        # 发光符文 (亮紫核心 + 白点, 在深底上醒目)
        for x, y in ((3, 3), (12, 5), (6, 11), (13, 13)):
            px(d, x, y, (210, 170, 255, 255))
            px(d, x, y + 1, (255, 240, 255, 255))
        # 幽光裂缝
        for x, y in ((8, 2), (8, 3), (4, 8), (10, 12)):
            px(d, x, y, (150, 110, 220, 255))
    return gen_wall('wall_abyss.png',
                    (110, 98, 142, 255),   # 深紫石 (提亮保持三档可见)
                    (74, 64, 104, 255),
                    (44, 36, 66, 255),
                    runes)


# ── M5-A 群系贴图: floor (3 套; 现有 floor 是纯色, game 端加接缝) ──
def gen_floor(name, base_col, speckle, speckle_positions):
    img, d = base()
    for y in range(16):
        for x in range(16):
            px(d, x, y, base_col)
    for x, y in speckle_positions:
        px(d, x, y, speckle)
    img.save(os.path.join(OUT, name))
    print('saved', name)


def main():
    os.makedirs(OUT, exist_ok=True)
    gen_shadow_stalker()
    gen_fire_imp()
    gen_elite_orc()
    gen_shadow_assassin()
    prison_wall()
    volcano_wall()
    abyss_wall()
    gen_floor('floor_prison.png', (176, 172, 168, 255),
              (150, 146, 142, 255),
              [(3, 5), (11, 2), (6, 12), (13, 9), (1, 14)])
    gen_floor('floor_volcano.png', (122, 82, 70, 255),
              (96, 62, 54, 255),
              [(2, 4), (9, 8), (13, 3), (5, 13), (12, 12)])
    gen_floor('floor_abyss.png', (74, 66, 100, 255),
              (58, 50, 82, 255),
              [(4, 3), (10, 6), (14, 11), (2, 9), (7, 14)])


if __name__ == '__main__':
    main()

