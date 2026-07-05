#pragma once
#include <cstdint>

// ============================================================
// 全局游戏常量 (迁移自 config.py)
// ============================================================

// ---- 窗口 ----
inline constexpr int WINDOW_WIDTH = 960;
inline constexpr int WINDOW_HEIGHT = 640;
inline constexpr int FPS = 60;
inline constexpr const char* WINDOW_TITLE = "Roguelike — C++ Edition";

// ---- 字体 ----
// 自动检测：开发环境用 msyh.ttc，打包后放在 assets/ 目录
inline const char* CJK_FONT_PATH = "C:/Windows/Fonts/msyh.ttc";

// ---- 地图 ----
inline constexpr int TILE_SIZE = 32;
inline constexpr int MAP_WIDTH = 40;
inline constexpr int MAP_HEIGHT = 30;
inline constexpr int DUNGEON_MIN_PARTITION = 8;
inline constexpr int DUNGEON_MIN_ROOM = 5;
inline constexpr int DUNGEON_ROOM_MARGIN = 1;
inline constexpr int DUNGEON_CORRIDOR_MIN = 1;
inline constexpr int DUNGEON_CORRIDOR_MAX = 2;

// ---- 关卡 ----
inline constexpr int MAX_FLOORS = 15;
inline constexpr int BOSS_FLOORS[] = {5, 10, 15};
inline constexpr int XP_PER_KILL_BASE = 10;
inline constexpr int XP_PER_KILL_BOSS = 150;

inline constexpr float FLOOR_MONSTER_HP_MULT[] = {
    1.0f, 1.15f, 1.3f, 1.5f, 1.0f,
    1.6f, 1.8f, 2.0f, 2.2f, 1.0f,
    2.4f, 2.7f, 3.0f, 3.3f, 1.0f
};
inline constexpr float FLOOR_MONSTER_ATK_MULT[] = {
    1.0f, 1.12f, 1.25f, 1.4f, 1.0f,
    1.5f, 1.65f, 1.8f, 2.0f, 1.0f,
    2.2f, 2.4f, 2.6f, 2.9f, 1.0f
};
inline constexpr int FLOOR_MONSTER_COUNT[] = {
    5, 5, 6, 6, 1, 6, 6, 7, 7, 1, 7, 7, 8, 8, 1
};

// ---- 玩家 ----
inline constexpr float PLAYER_SPEED = 200.0f;
inline constexpr int PLAYER_MAX_HP = 100;
inline constexpr int PLAYER_ATTACK = 10;
inline constexpr int PLAYER_PDEF = 3;
inline constexpr int PLAYER_MDEF = 1;
inline constexpr float PLAYER_ATTACK_RANGE = 1.5f;
inline constexpr int INVENTORY_MAX = 16;
inline constexpr float LOOT_DROP_CHANCE = 0.5f;
inline constexpr float PICKUP_RANGE = 2.0f;

// ---- 怪物 ----
inline constexpr float MONSTER_SIGHT = 6.0f;
inline constexpr float MONSTER_SPEED = 80.0f;
inline constexpr float MONSTER_PATROL = 2.0f;

// ---- 颜色 (Raylib Color) ----
inline constexpr Color COLOR_BLACK    {0, 0, 0, 255};
inline constexpr Color COLOR_WHITE    {255, 255, 255, 255};
inline constexpr Color COLOR_GRAY     {128, 128, 128, 255};
inline constexpr Color COLOR_DARK_GRAY{64, 64, 64, 255};
inline constexpr Color COLOR_RED      {200, 50, 50, 255};
inline constexpr Color COLOR_GREEN    {50, 200, 50, 255};
inline constexpr Color COLOR_BLUE     {50, 50, 200, 255};
inline constexpr Color COLOR_YELLOW   {200, 200, 50, 255};
inline constexpr Color COLOR_GOLD     {255, 200, 50, 255};
inline constexpr Color COLOR_DARK_RED {140, 20, 20, 255};
inline constexpr Color COLOR_CYBER_BLUE{80, 120, 255, 255};

inline bool is_boss_floor(int floor) {
    return floor == 5 || floor == 10 || floor == 15;
}
