#pragma once
#include <cstdint>

// ============================================================
// D1: FloorConfig — 每层 / 每章的数据配置
// 后续 D2~D6 所有章节内容统一从此读取，消除 floor==X 硬编码
// ============================================================

// 单层配置
struct FloorConfig {
    int floor;                   // 1-15
    int chapter;                 // 0=序章, 1=地牢入口, 2=幽暗深渊, 3=熔岩炼狱
    const char* chapter_label;   // "地牢入口" / "幽暗深渊" / "熔岩炼狱"

    // 剧情 (D1: 进入楼层时显示一句消息，nullptr = 不显示)
    const char* story_msg;       // 首次进入本层的剧情文字

    // 难度系数
    float hp_mult;               // 怪物 HP 倍率
    float atk_mult;              // 怪物 ATK 倍率
    int   monster_count;         // 基础怪物数量

    // D8: 怪物池 (累积权重，各浮层可独立调整)
    int enemy_weights[12];       // [0]=NORMAL [1]=Archer [2]=Shaman [3]=Bomber [4]=Tank [5]=Elite [6]=Charger [7]=Summoner [8]=Sniper [9]=Controller [10]=Ambush [11]=Guardian

    // 特殊房间数量
    int special_room_count;

    // 楼层类型
    bool is_boss;                // Boss 层
    bool is_rest_floor;          // 休息层 (D4: 资源增多、怪物减少)

    // D2 Step4: 怪物协同概率 (0.0~1.0, 按章节增长)
    float team_coop_chance;

    // D2 Step5: 战场元素密度 (每房间放置个数)
    int arena_density;

    // 音效
    const char* bgm;             // "dungeon" | "boss"
};

// 章节配置（5 章）
struct ChapterConfig {
    int    chapter;              // 0-4
    const char* name;            // "地牢入口"
    int    start_floor;          // 本段起始楼层
    int    end_floor;            // 本段结束楼层 (含)
};

// ---- 查询接口 ----
const FloorConfig*   get_floor_config(int floor_number);       // floor ∈ [1,15]
const ChapterConfig* get_chapter_config(int chapter_index);    // chapter ∈ [0,4]
const char*          get_floor_story_msg(int floor_number);    // 剧情消息, 可 nullptr
int                  get_chapter_for_floor(int floor_number);
