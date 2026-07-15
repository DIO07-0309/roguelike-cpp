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
    MERCHANT,        // 旅行商人
    AMBUSH,          // 伏击: 四周刷怪
    CURSED_ROOM,     // 诅咒房
    ALTAR_CHOICE,    // 3选1祭坛
    STATUE,          // 神秘雕像
    PRISONER,        // 囚犯
    LOST_CAMP,       // 迷失营地
    TREASURE_GUARD,  // 宝库守卫
    BLOOD_RITUAL,    // 血祭
    NOTHING,         // 空房间
    // D8 Step6: new event types
    TRAP,            // 陷阱房: 扣血/Burn/Poison/Fear/Freeze, 可能反给奖励
    MYSTERY,         // 完全随机: 从任意事件池抽取
    BLESSING,        // 祝福房: 永久成长/攻击/生命/速度/恢复
    CURSE,           // 诅咒: Curse/减速/攻击下降/失去金币
    LORE,            // 剧情: 纯文本沉浸
    NPC_EVENT,       // NPC事件: 接受任务/完成任务/提升好感
    RELIC_DROP,     // 圣物房: 直接获得Relic
    TREASURE_CACHE,  // 宝箱缓存: 金币+装备+Buff+Relic随机
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
