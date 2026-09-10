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
# ── M5-D 变异族/剩余补图: 同族变体在基础形态上换配色+特征 ──
def gen_archer():
    img, d = base()
    skin = (110, 150, 70, 255)       # 哥布林绿
    dark = (70, 100, 45, 255)
    bow = (170, 120, 70, 255)        # 木弓
    string_c = (240, 230, 210, 255)
    outline_rect(d, 4, 3, 11, 12, skin)
    rect(d, 5, 8, 10, 11, dark)       # 皮甲
    px(d, 6, 5, (255, 255, 255, 255)); px(d, 9, 5, (255, 255, 255, 255))  # 目
    # 长耳
    px(d, 3, 4, skin); px(d, 2, 3, skin)
    px(d, 12, 4, skin); px(d, 13, 3, skin)
    # 弓 (右手持)
    for y in range(3, 12):
        px(d, 13, y, bow)
    px(d, 12, 3, string_c); px(d, 12, 11, string_c)
    for y in range(4, 11):
        px(d, 12, y, string_c)
    save(img, 'mon_archer.png')


def gen_frost_slime():
    img, d = base()
    ice = (150, 210, 240, 255)
    dark = (100, 150, 190, 255)
    core = (240, 252, 255, 255)
    outline_rect(d, 3, 5, 12, 12, ice)
    rect(d, 4, 8, 11, 11, dark)       # 腹暗
    rect(d, 6, 6, 9, 8, core)         # 冰核
    px(d, 5, 6, (255, 255, 255, 255)); px(d, 10, 6, (255, 255, 255, 255))  # 目
    # 顶部冰晶尖
    for x in (5, 8, 11):
        px(d, x, 3, core); px(d, x, 4, ice)
    save(img, 'mon_frost_slime.png')


def gen_lightning_orb():
    img, d = base()
    orb = (250, 230, 110, 255)       # 电光黄
    core = (255, 255, 240, 255)
    bolt = (255, 250, 200, 255)
    # 球体 + 电弧纹
    outline_rect(d, 4, 4, 11, 11, orb)
    rect(d, 6, 6, 9, 9, core)
    # 交叉电弧 (斜向锯齿)
    for x, y in ((5, 5), (10, 5), (5, 10), (10, 10)):
        px(d, x, y, bolt)
    px(d, 8, 2, bolt); px(d, 8, 3, bolt)      # 顶刺
    px(d, 8, 12, bolt); px(d, 8, 13, bolt)    # 底刺
    px(d, 2, 7, bolt); px(d, 3, 7, bolt)      # 左刺
    px(d, 12, 7, bolt); px(d, 13, 7, bolt)    # 右刺
    save(img, 'mon_lightning_orb.png')


# ── M5-E 收尾: elite_slime (精英史莱姆 — 深渊怪池, 王冠金瞳) ──
def gen_elite_slime():
    img, d = base()
    body = (160, 220, 130, 255)     # 亮翠体 (普通 slime 偏灰绿)
    dark = (100, 150, 85, 255)
    crown = (255, 210, 60, 255)    # 精英金冠
    gem = (255, 80, 80, 255)        # 冠珠红
    outline_rect(d, 3, 5, 12, 12, body)
    rect(d, 4, 8, 11, 11, dark)            # 腹暗
    rect(d, 6, 7, 9, 8, (220, 255, 200, 255))  # 核心亮斑
    px(d, 5, 6, (255, 255, 255, 255)); px(d, 10, 6, (255, 255, 255, 255))  # 目
    # 金冠 (三尖)
    for x in (5, 8, 11):
        px(d, x, 3, crown)
    rect(d, 5, 4, 10, 4, crown)
    px(d, 8, 3, gem)                       # 冠珠
    save(img, 'mon_elite_slime.png')


def gen_poison_wyrm():
    img, d = base()
    body = (130, 170, 70, 255)       # 毒绿
    dark = (85, 115, 45, 255)
    drop = (200, 255, 120, 255)     # 毒液亮滴
    outline_rect(d, 2, 7, 13, 12, body)
    for x in (5, 9, 12):
        for y in range(7, 13):
            px(d, x, y, dark)       # 节缝
    # 蛇头 (前端抬起)
    outline_rect(d, 11, 3, 14, 6, body)
    px(d, 13, 4, (255, 255, 255, 255))  # 目
    px(d, 14, 6, drop)              # 毒牙滴液
    # 背部毒泡
    px(d, 4, 6, drop); px(d, 7, 5, drop); px(d, 10, 6, drop)
    save(img, 'mon_poison_wyrm.png')


def gen_golem():
    img, d = base()
    rock = (140, 130, 120, 255)     # 岩石灰
    dark = (95, 88, 82, 255)
    core = (255, 160, 60, 255)     # 魔核橙
    outline_rect(d, 2, 3, 13, 13, rock)
    rect(d, 3, 6, 12, 12, dark)      # 岩层
    rect(d, 6, 7, 9, 10, core)      # 胸口魔核
    px(d, 7, 8, (255, 230, 160, 255))
    px(d, 5, 4, (255, 160, 60, 255)); px(d, 10, 4, (255, 160, 60, 255))  # 眼=核色
    # 裂纹
    px(d, 4, 9, (60, 55, 50, 255)); px(d, 4, 10, (60, 55, 50, 255))
    px(d, 11, 8, (60, 55, 50, 255)); px(d, 11, 9, (60, 55, 50, 255))
    save(img, 'mon_golem.png')


def gen_mon_necromancer():
    img, d = base()
    robe = (100, 130, 85, 255)      # 亡语者绿袍
    dark = (60, 80, 52, 255)
    skel = (230, 225, 205, 255)     # 骨白
    outline_rect(d, 4, 2, 11, 12, robe)
    rect(d, 5, 8, 10, 11, dark)      # 袍摆
    rect(d, 5, 4, 10, 6, (30, 40, 26, 255))  # 兜帽阴影
    px(d, 6, 5, skel); px(d, 9, 5, skel)   # 骷髅脸白点
    px(d, 6, 5, (120, 255, 110, 255)); px(d, 9, 5, (120, 255, 110, 255))  # 绿瞳
    # 骨杖
    for y in range(3, 13):
        px(d, 13, y, (160, 150, 120, 255))
    px(d, 13, 2, skel); px(d, 12, 3, skel)
    save(img, 'mon_necromancer.png')


def gen_storm_elemental():
    img, d = base()
    body = (120, 170, 230, 255)     # 雷云蓝
    dark = (80, 110, 160, 255)
    bolt = (255, 250, 140, 255)     # 闪电黄
    outline_rect(d, 3, 4, 12, 11, body)
    rect(d, 4, 7, 11, 10, dark)     # 云底
    px(d, 5, 6, (255, 255, 255, 255)); px(d, 10, 6, (255, 255, 255, 255))  # 电目
    # 云朵锯齿下摆 + 落雷
    for i, x in enumerate((3, 5, 8, 10, 12)):
        px(d, x, 12, body); px(d, x, 13, dark)
    px(d, 8, 14, bolt); px(d, 8, 15, bolt)   # 中央落雷
    # 顶部电弧
    px(d, 5, 2, bolt); px(d, 6, 3, bolt)
    px(d, 10, 2, bolt); px(d, 9, 3, bolt)
    save(img, 'mon_storm_elemental.png')


def gen_blood_priest():
    img, d = base()
    robe = (190, 40, 80, 255)       # 血祭司红袍
    dark = (120, 20, 50, 255)
    gold = (255, 210, 120, 255)    # 金饰
    outline_rect(d, 4, 2, 11, 12, robe)
    rect(d, 5, 8, 10, 11, dark)      # 袍内
    rect(d, 5, 4, 10, 6, (150, 30, 60, 255))  # 面纱
    px(d, 6, 5, (255, 120, 140, 255)); px(d, 9, 5, (255, 120, 140, 255))  # 目
    # 圣徽 (胸口金十字)
    px(d, 8, 7, gold); px(d, 8, 8, gold)
    px(d, 7, 7, gold); px(d, 9, 7, gold)
    # 手杖顶血滴
    for y in range(3, 13):
        px(d, 13, y, (120, 100, 60, 255))
    px(d, 13, 2, (255, 60, 90, 255))
    save(img, 'mon_blood_priest.png')


def gen_stone_guardian():
    img, d = base()
    stone = (150, 140, 125, 255)    # 石像
    dark = (105, 98, 88, 255)
    glow = (255, 230, 120, 255)    # 符文金
    outline_rect(d, 2, 2, 13, 13, stone)
    rect(d, 3, 6, 12, 12, dark)     # 底座暗
    # 石甲块分割
    for x in range(4, 12, 4):
        for y in range(3, 13):
            px(d, x, y, dark)
    # 发光符文眼 + 胸符
    px(d, 5, 4, glow); px(d, 10, 4, glow)
    px(d, 7, 8, glow); px(d, 8, 8, glow); px(d, 7, 9, glow); px(d, 8, 9, glow)
    save(img, 'mon_stone_guardian.png')


def gen_iron_sentinel():
    img, d = base()
    iron = (170, 175, 185, 255)    # 铁灰
    dark = (110, 115, 125, 255)
    visor = (120, 200, 255, 255)   # 目镜蓝
    outline_rect(d, 3, 2, 12, 13, iron)
    rect(d, 4, 7, 11, 11, dark)     # 甲身
    outline_rect(d, 4, 3, 11, 6, iron)  # 头盔
    # 一字目镜
    rect(d, 5, 4, 9, 5, visor)
    # 肩甲
    px(d, 2, 6, iron); px(d, 2, 7, iron)
    px(d, 13, 6, iron); px(d, 13, 7, iron)
    # 中缝
    for y in range(7, 12):
        px(d, 8, y, dark)
    save(img, 'mon_iron_sentinel.png')


def gen_bone_soldier():
    img, d = base()
    bone = (220, 215, 190, 255)    # 骨白
    dark = (150, 145, 125, 255)
    rust = (140, 90, 50, 255)     # 锈甲
    outline_rect(d, 4, 2, 11, 12, bone)
    rect(d, 5, 6, 10, 11, dark)     # 骨节暗
    rect(d, 6, 5, 9, 7, rust)       # 破胸甲
    px(d, 6, 3, (255, 255, 255, 255)); px(d, 9, 3, (255, 90, 60, 255))  # 眼窝 (一点红)
    # 盾 (左手)
    rect(d, 2, 6, 4, 11, rust)
    px(d, 3, 8, dark)
    # 锈剑 (右手)
    for y in range(4, 12):
        px(d, 13, y, (180, 175, 160, 255))
    px(d, 13, 3, (230, 228, 220, 255))
    save(img, 'mon_bone_soldier.png')


def gen_skeleton_archer():
    img, d = base()
    bone = (210, 205, 185, 255)
    dark = (140, 135, 118, 255)
    bow = (150, 110, 70, 255)
    string_c = (230, 225, 210, 255)
    outline_rect(d, 4, 3, 11, 12, bone)
    rect(d, 5, 8, 10, 11, dark)     # 骨盆暗
    # 眼窝黑 + 额裂纹
    rect(d, 5, 4, 7, 5, (40, 38, 34, 255))
    rect(d, 8, 4, 10, 5, (40, 38, 34, 255))
    px(d, 8, 2, dark); px(d, 8, 3, dark)
    # 弓
    for y in range(3, 12):
        px(d, 13, y, bow)
    for y in range(4, 11):
        px(d, 12, y, string_c)
    px(d, 12, 3, string_c); px(d, 12, 11, string_c)
    save(img, 'mon_skeleton_archer.png')


def gen_goblin_hunter():
    img, d = base()
    skin = (110, 150, 70, 255)     # 哥布林绿
    dark = (70, 100, 45, 255)
    net_c = (200, 180, 130, 255)  # 猎网
    outline_rect(d, 4, 3, 11, 12, skin)
    rect(d, 5, 7, 10, 11, dark)    # 皮甲
    px(d, 6, 5, (255, 220, 80, 255)); px(d, 9, 5, (255, 220, 80, 255))  # 黄目
    # 长耳
    px(d, 3, 4, skin); px(d, 2, 3, skin)
    px(d, 12, 4, skin); px(d, 13, 3, skin)
    # 猎网 (背后)
    px(d, 13, 5, net_c); px(d, 13, 6, net_c); px(d, 14, 5, net_c)
    px(d, 14, 7, net_c); px(d, 13, 8, net_c)
    save(img, 'mon_goblin_hunter.png')


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


# ── M5-D 深渊/补图批: dark_mage/void_walker/night_stalker/ice_warden/blood_leech ──
def gen_dark_mage():
    img, d = base()
    robe = (70, 50, 110, 255)       # 暗术师长袍
    dark = (48, 34, 78, 255)
    orb  = (180, 90, 255, 255)     # 虚空法球
    # 兜帽人形
    outline_rect(d, 4, 1, 11, 6, robe)
    rect(d, 5, 4, 10, 6, dark)      # 帽内阴影
    px(d, 6, 4, orb); px(d, 9, 4, orb)   # 紫瞳
    outline_rect(d, 3, 7, 12, 12, robe)
    rect(d, 6, 8, 9, 11, dark)      # 袍内缝
    # 悬浮法球 (右手侧)
    px(d, 13, 5, orb); px(d, 13, 6, orb); px(d, 12, 5, orb)
    px(d, 14, 4, (230, 180, 255, 255))
    save(img, 'mon_dark_mage.png')


def gen_void_walker():
    img, d = base()
    body = (60, 40, 90, 255)        # 虚空行者暗紫
    rift = (170, 120, 230, 255)     # 裂隙亮紫
    void_dark = (36, 24, 60, 255)
    # 步行人形 + 身体裂隙
    outline_rect(d, 4, 2, 11, 12, body)
    rect(d, 6, 5, 9, 10, void_dark)  # 体腔虚空
    # 裂隙纹 (斜向亮线)
    for i in range(3):
        px(d, 6 + i, 5 + i, rift); px(d, 7 + i, 8 + i, rift)
    px(d, 5, 4, (255, 255, 255, 255)); px(d, 10, 4, (255, 255, 255, 255))  # 白瞳
    # 虚空触须 (下摆)
    for x in (4, 6, 9, 11):
        px(d, x, 13, body); px(d, x, 14, rift)
    save(img, 'mon_void_walker.png')


def gen_night_stalker():
    img, d = base()
    fur = (90, 70, 60, 255)         # 野兽棕
    dark = (58, 44, 38, 255)
    eye = (255, 220, 80, 255)       # 兽目黄
    # 匍匐兽形: 低矮宽身 + 竖耳
    outline_rect(d, 2, 6, 13, 12, fur)
    rect(d, 3, 9, 12, 11, dark)      # 腹部暗
    # 双竖耳
    px(d, 4, 4, fur); px(d, 4, 5, fur); px(d, 5, 4, fur)
    px(d, 11, 4, fur); px(d, 11, 5, fur); px(d, 10, 4, fur)
    # 兽目 + 尖牙
    px(d, 5, 7, eye); px(d, 10, 7, eye)
    px(d, 5, 10, (255, 245, 230, 255)); px(d, 10, 10, (255, 245, 230, 255))
    # 前爪伸展 (潜行姿态)
    px(d, 1, 12, fur); px(d, 14, 12, fur)
    px(d, 0, 13, dark); px(d, 15, 13, dark)
    save(img, 'mon_night_stalker.png')


def gen_ice_warden():
    img, d = base()
    ice = (140, 200, 230, 255)       # 冰晶蓝
    ice_dark = (90, 140, 180, 255)
    core = (220, 250, 255, 255)     # 冰核亮
    # 方正守卫甲 + 冰晶尖
    outline_rect(d, 3, 3, 12, 12, ice)
    rect(d, 4, 8, 11, 11, ice_dark)  # 甲下暗
    # 头部冰晶尖 (三峰冠)
    for x in (5, 8, 11):
        px(d, x, 0, core); px(d, x, 1, core); px(d, x, 2, ice)
    # 冰核 (胸口)
    rect(d, 6, 5, 9, 7, core)
    px(d, 7, 6, (255, 255, 255, 255))
    # 冰瞳
    px(d, 5, 4, (240, 255, 255, 255)); px(d, 10, 4, (240, 255, 255, 255))
    save(img, 'mon_ice_warden.png')


def gen_blood_leech():
    img, d = base()
    body = (170, 40, 60, 255)       # 血红环节
    dark = (110, 22, 40, 255)
    gloss = (255, 130, 150, 255)   # 湿亮
    # 蠕虫环节体 (波浪分节)
    outline_rect(d, 2, 6, 13, 11, body)
    for x in (5, 8, 11):
        for y in range(6, 12):
            px(d, x, y, dark)       # 节缝
    # 吸盘 (前端)
    rect(d, 1, 7, 2, 10, dark)
    px(d, 1, 8, (255, 200, 210, 255)); px(d, 1, 9, (255, 200, 210, 255))
    # 背部高光 (湿润感)
    for x in range(4, 12, 2):
        px(d, x, 6, gloss)
    # 拟态小眼
    px(d, 12, 8, (255, 240, 240, 255))
    save(img, 'mon_blood_leech.png')


# ── M5-C F5/F10 Boss 专属图 (visual_id 数据驱动; F15 镜像有意用玩家形象不生成) ──
def _boss_common(img, d, skin, armor, glow):
    """Boss 通用画法: 16x16 全幅 + 顶部王冠光 + 底部光环."""
    outline_rect(d, 2, 2, 13, 13, skin)
    rect(d, 3, 3, 12, 12, armor)
    # 顶部王冠光
    for x in (4, 8, 12):
        px(d, x, 0, glow); px(d, x, 1, glow)
    return img


def gen_shadow_knight():
    img, d = base()
    skin = (58, 52, 84, 255)        # 暗紫影躯
    armor = (90, 86, 120, 255)      # 影钢甲
    glow = (148, 120, 210, 255)     # 幽紫光
    _boss_common(img, d, skin, armor, glow)
    # 独有: 骑士头盔缝 + 双红目
    rect(d, 5, 5, 10, 6, (44, 38, 66, 255))
    px(d, 6, 6, (255, 60, 60, 255)); px(d, 9, 6, (255, 60, 60, 255))
    # 长剑 (竖持右肩)
    for y in range(4, 13):
        px(d, 12, y, (180, 190, 210, 255))
    px(d, 12, 3, (230, 240, 255, 255))
    save(img, 'boss_shadow_knight.png')


def gen_necromancer():
    img, d = base()
    skin = (70, 90, 60, 255)        # 腐绿袍
    armor = (100, 120, 80, 255)     # 法袍亮部
    glow = (160, 255, 140, 255)     # 亡灵绿光
    _boss_common(img, d, skin, armor, glow)
    # 独有: 兜帽阴影脸 + 绿瞳
    rect(d, 5, 4, 10, 7, (34, 44, 30, 255))
    px(d, 6, 6, glow); px(d, 9, 6, glow)
    # 骷髅法杖 (左手)
    for y in range(3, 13):
        px(d, 3, y, (150, 110, 70, 255))
    px(d, 3, 2, (240, 240, 220, 255)); px(d, 2, 3, (240, 240, 220, 255))
    px(d, 4, 3, (240, 240, 220, 255))
    save(img, 'boss_necromancer.png')


def gen_vampire():
    img, d = base()
    skin = (210, 160, 170, 255)     # 苍白皮肤
    armor = (120, 20, 40, 255)      # 血燕尾服
    glow = (255, 200, 200, 255)     # 血色光
    _boss_common(img, d, skin, armor, glow)
    # 独有: 高领披风 + 红瞳尖牙
    rect(d, 4, 8, 5, 13, (90, 12, 30, 255)); rect(d, 10, 8, 11, 13, (90, 12, 30, 255))
    rect(d, 6, 6, 9, 7, skin)
    px(d, 6, 6, (255, 40, 40, 255)); px(d, 9, 6, (255, 40, 40, 255))
    px(d, 7, 8, (255, 240, 240, 255)); px(d, 8, 8, (255, 240, 240, 255))
    save(img, 'boss_vampire.png')


def gen_fire_demon():
    img, d = base()
    skin = (200, 70, 30, 255)       # 熔岩躯
    armor = (255, 150, 50, 255)     # 火焰甲
    glow = (255, 230, 120, 255)     # 烈焰光
    _boss_common(img, d, skin, armor, glow)
    # 独有: 双巨角 + 熔岩裂纹
    for i in range(4):
        px(d, 4 - i, 3 - i, (250, 210, 80, 255))
        px(d, 11 + i, 3 - i, (250, 210, 80, 255))
    for x, y in ((6, 10), (7, 10), (8, 12), (9, 12)):
        px(d, x, y, (255, 220, 90, 255))
    # 火瞳
    px(d, 6, 6, glow); px(d, 9, 6, glow)
    save(img, 'boss_fire_demon.png')


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
    gen_shadow_knight()
    gen_necromancer()
    gen_vampire()
    gen_fire_demon()
    gen_dark_mage()
    gen_void_walker()
    gen_night_stalker()
    gen_ice_warden()
    gen_blood_leech()
    gen_archer()
    gen_frost_slime()
    gen_elite_slime()
    gen_lightning_orb()
    gen_poison_wyrm()
    gen_golem()
    gen_mon_necromancer()
    gen_storm_elemental()
    gen_blood_priest()
    gen_stone_guardian()
    gen_iron_sentinel()
    gen_bone_soldier()
    gen_skeleton_archer()
    gen_goblin_hunter()
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

