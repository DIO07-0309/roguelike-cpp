#pragma once
#include <string>

// ============================================================
// SpecialRoom — 特殊房间数据 (B8)
// ============================================================

class Player;

enum class SpecialRoomType { ALTAR, TREASURE, FOUNTAIN };

struct SpecialRoom {
    int cx = 0, cy = 0;          // 房间中心 tile 坐标
    int rx = 0, ry = 0, rw = 0, rh = 0; // 房间矩形
    SpecialRoomType type = SpecialRoomType::ALTAR;
    bool triggered = false;
    bool discovered = false;   // B10: 玩家是否已走进过该房间
};

// 生成时用: 把索引映射到类型 (0→ALTAR, 1→TREASURE, 2→FOUNTAIN)
SpecialRoomType special_room_from_index(int idx);

// IO 边界用: enum ↔ string
std::string      special_room_to_string(SpecialRoomType type);
SpecialRoomType  special_room_from_string(const std::string& s);

// B8 统一交互入口 — GameScene 只调这一个函数
std::string execute_special_room(SpecialRoomType type, Player* player);
