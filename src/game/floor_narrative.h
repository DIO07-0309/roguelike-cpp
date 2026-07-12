#pragma once
#include <cstdint>

// ============================================================
// D4 Step3: FloorNarrative — 楼层叙事系统
// 每层拥有身份(Title/Subtitle/Ambience/Dialogue)
// 后续 NPC/Quest/Boss/Ending 全部查询此处
// ============================================================

struct FloorNarrative {
    int  floor;
    const char* title;          // 楼层名称 e.g. "沉睡牢狱"
    const char* subtitle;       // 英文副标题 e.g. "The Sleeping Prison"
    const char* description;    // 1-2句环境描写
    const char* enter_dialogue; // 首次进入独白 (nullptr=无)
    const char* exit_dialogue;  // 离开楼层独白
    const char* boss_hint;      // Boss前3层的伏笔 (nullptr=无)
    const char* narrations[5];  // 3-5条环境旁白 (nullptr终止)
    const char* ambience;       // 环境音效 hook (D5接入, 目前仅保存)
    int  ambient_color_r, ambient_color_g, ambient_color_b; // 环境色调
};

struct NarrativeState {
    bool floor_intro_played[15];    // 每层入场已播
    int  last_narration_idx;        // 上次旁白索引 (防重复)
    float narration_timer;           // 旁白计时器
};

// ---- 查询 ----
const FloorNarrative* get_floor_narrative(int floor);  // floor ∈ [1,15]

// ---- 章节 ----
const char* get_chapter_title(int chapter);     // "第一章: 地牢入口"
const char* get_chapter_subtitle(int chapter);   // "Chapter I: The Dungeon Gate"

// ---- 随机旁白 ----
const char* pick_random_narration(int floor, NarrativeState& state);
