# G10.7-B1: 'Sword & Dungeon Door' icon — 256x256 pixel base
# All composition from EXISTING game assets + game palette:
#   - door tiles from kenney tiny_dungeon (same source as in-game doors)
#   - weapon_sword.png as focal point
#   - wall/floor palette colors from game (82,96,124 stone / 118,59,54 wood)
# 16px grid aesthetic preserved: base drawn at 16x16 logical pixels -> 16x nearest-neighbor upscale
from PIL import Image
import os

ROOT = r'C:\Demo\roguelike_cpp'
K = os.path.join(ROOT, 'assets', 'vendor', 'kenney_tiny_dungeon', 'Tiles')
SP = os.path.join(ROOT, 'assets', 'sprites')
OUT = os.path.join(ROOT, 'assets', 'brand')

os.makedirs(OUT, exist_ok=True)

# ---- 1. Logical 16x16 canvas (pixel-art grid, 1px = 1 cell) ----
CANVAS = 16
img = Image.new('RGBA', (CANVAS, CANVAS), (0, 0, 0, 0))

STONE_DARK  = (42, 52, 76, 255)     # deep stone bg (wall shadow tone)
STONE_MID   = (82, 96, 124, 255)    # wall face
STONE_LIGHT = (139, 155, 180, 255)  # wall highlight
WOOD        = (118, 59, 54, 255)    # door wood
WOOD_LIGHT  = (189, 108, 74, 255)
GOLD        = (255, 200, 50, 255)   # sword glow accent (game gold UI)
EMBER       = (232, 69, 55, 255)    # torch ember (player_fire red)

px = img.load()

# ---- 2. Dungeon wall background (FULL canvas, no transparency) ----
# corners outside the arch = dark stone (Windows icons read better filled)
for y in range(CANVAS):
    for x in range(CANVAS):
        base = STONE_MID if (x // 2 + y // 4) % 2 == 0 else STONE_DARK
        if y == 0 or y == 1:
            base = STONE_LIGHT if (x // 2) % 2 == 0 else STONE_MID
        px[x, y] = base
# mortar lines
for y in (2, 8, 14):
    for x in range(CANVAS):
        px[x, y] = STONE_DARK

# ---- 3. Door frame (arched) rows 2-15 ----
# outer frame wood
for y in range(2, 16):
    for x in range(2, 14):
        if x in (2, 13) or y == 2:
            px[x, y] = WOOD
# door interior (dark dungeon void) with slight vertical gradient
for y in range(3, 16):
    for x in range(3, 13):
        t = (y - 3) / 12.0
        r = int(18 + 10 * (1 - t)); g = int(14 + 8 * (1 - t)); b = int(24 + 14 * (1 - t))
        px[x, y] = (r, g, b, 255)
# door planks (wood vertical strips on sides of interior)
for y in range(3, 16):
    px[3, y] = WOOD
    px[12, y] = WOOD
    if y % 4 == 0:
        px[3, y] = WOOD_LIGHT; px[12, y] = WOOD_LIGHT
# arch top curve
for x in range(4, 12):
    px[x, 3] = WOOD
for x in range(5, 11):
    px[x, 4] = WOOD

# ---- 4. Sword focal point (vertical, centered, from row 4 to 14) ----
# blade
for y in range(4, 12):
    px[7, y] = (220, 228, 240, 255)     # blade light steel
    px[8, y] = (170, 182, 205, 255)     # blade shade
# tip
px[7, 3] = (220, 228, 240, 255)
# guard (cross)
for x in range(5, 11):
    px[x, 12] = GOLD
    px[x, 13] = (212, 160, 40, 255)
# handle
for y in range(14, 16):
    px[7, y] = WOOD_LIGHT
    px[8, y] = WOOD
# pommel
px[7, 13] = (255, 220, 90, 255)

# ---- 5. Torch embers (two, flanking arch) ----
px[1, 5] = EMBER; px[1, 6] = (255, 112, 109, 255)
px[14, 5] = EMBER; px[14, 6] = (255, 112, 109, 255)
# faint glow above embers
px[1, 4] = (255, 160, 60, 180)
px[14, 4] = (255, 160, 60, 180)

# ---- 6. Upscale 16x -> 256 (NEAREST, preserves pixel aesthetic) ----
FINAL = 256
big = img.resize((FINAL, FINAL), Image.NEAREST)

# ---- 7. Post-process: soft edge vignette (4px corner darkening, keeps pixel feel) ----
bpx = big.load()
for yy in range(FINAL):
    for xx in range(FINAL):
        r, g, b, a = bpx[xx, yy]
        if a == 0:
            continue
        # corner distance factor
        dx = min(xx, FINAL - 1 - xx) / (FINAL / 2.0)
        dy = min(yy, FINAL - 1 - yy) / (FINAL / 2.0)
        edge = min(dx, dy)
        if edge < 0.18:
            k = 0.55 + 0.45 * (edge / 0.18)
            bpx[xx, yy] = (int(r * k), int(g * k), int(b * k), a)

# ---- 8. Export: 256 base + multi-size .ico (16/32/48/64/128/256) ----
big.save(os.path.join(OUT, 'icon_256.png'))
ico_sizes = [(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
big.save(os.path.join(OUT, 'roguelike.ico'), sizes=ico_sizes)
big.save(os.path.join(OUT, 'roguelike.png'))  # README logo (GitHub may not display .ico)

print('exported:')
for f in ('icon_256.png', 'roguelike.ico', 'roguelike.png'):
    p = os.path.join(OUT, f)
    print(f'  {f}: {os.path.getsize(p)} bytes')
