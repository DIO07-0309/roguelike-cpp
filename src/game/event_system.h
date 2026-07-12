#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <random>

class Player;
struct ChapterConfig;

// ============================================================
// D4 Step1: Dynamic Event System — Core types
// ============================================================

enum class EventType {
    NONE = 0,
    MERCHANT,        // 旅行商人: 卖药水/Relic/技能进化/Buff
    AMBUSH,          // 伏击: 四周刷怪, 双倍经验
    CURSED_ROOM,     // 诅咒房: 拿传奇奖励得永久诅咒
    ALTAR_CHOICE,    // 3选1祭坛
    STATUE,          // 神秘雕像: 随机Buff/Debuff/Nothing
    PRISONER,        // 囚犯: 救→NPC, 不救→金币
    LOST_CAMP,       // 迷失营地: 恢复+剧情+日志
    TREASURE_GUARD,  // 宝库守卫: Boss战赢传奇奖励
    BLOOD_RITUAL,    // 血祭: 献祭HP换强化
    NOTHING,         // 空房间: 仅剧情文字
};

struct DungeonEvent {
    EventType type = EventType::NONE;
    bool triggered = false;
    int  room_index = -1;    // 所在房间索引
    int  tile_x = 0, tile_y = 0;  // 事件中心瓦片坐标
    std::string title;
    std::string desc;
    std::string text;    // 随机剧情文本
};

// ---- 生成 ----
DungeonEvent generate_event(int floor, const ChapterConfig& ch, std::mt19937& rng);

// ---- 执行 (返回消息, 格式: "REWARD:xxx" 或 "MSG:xxx") ----
std::string execute_event(DungeonEvent& ev, Player* player, int floor);

// ---- 查询辅助 ----
const char* event_type_name(EventType t);
std::vector<std::string> event_text_pool(int idx);  // 随机文本池 (复用seed保证同层同内容)
