#pragma once
#include <string>
#include <vector>
#include <cstdint>

class Player;

// ============================================================
// D4 Step4: NPC & Quest & Dialogue Framework
// ============================================================

enum class NPCType { MERCHANT, BLACKSMITH, PRIEST, SCHOLAR, PRISONER, PILGRIM, GHOST, UNKNOWN };

// 单次对话片段 (多页)
struct DialoguePage {
    std::string speaker;   // 说话者名 e.g. "神官"
    std::string text;      // 此页对话内容
};

// 全局 NPC 状态 (World Memory — 后续 D5/D6 多结局读取)
struct NPCState {
    int  id = 0;
    bool met = false;
    bool finished = false;   // 对话/任务已完成
    int  relation = 0;       // -2=敌对, 0=中立, 2=友善, 5=亲密
};

// QuestType kept for future use (Quest struct moved to quest_manager.h)
enum class QuestType { NONE, FIND, KILL, DELIVER, TALK, EXPLORE };

// ---- Dialogue 演出状态 ----
struct DialogueState {
    bool active = false;
    int  page = 0;
    float timer = 0.0f;     // 逐字显示计时器
    std::vector<DialoguePage> pages;
    // 完成后回调: 设置 NPCState.finished = true
    NPCState* target_npc = nullptr;
};

// ---- 查询 NPC 配置 (每层) ----
struct NPCData {
    NPCType type;
    const char* name;
    const char* title;
    const char* first_dialogue[5];   // 5段首次对话 (nullptr终止)
    const char* repeat_dialogue[2];  // 再次对话 (nullptr终止)
    const char* farewell;            // 告别语
};

const NPCData* get_npc_config(int floor, int slot);  // slot=0或1
int            get_npc_count_for_floor(int floor);
