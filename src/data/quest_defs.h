#pragma once
#include <string>
#include <unordered_map>
#include "core/registry_provider.h"  // G4.1: MergeMode

// ============================================================
// G2.4: QuestDef — 任务配置数据 (纯数据, 无 Runtime 状态)
// Runtime state 保留在 QuestManager::_quests 的 Quest::state 字段
// 属于 Data 层, 位于 src/data/
// ============================================================

struct QuestRewardDef {
    std::string relic_id;        // 奖励圣物 (空=无)
    std::string buff_id;         // 奖励 Buff (空=无)
    int buff_stacks = 1;
    std::string set_flag;        // 完成后设置的 WorldFlag 字符串
    std::string counter_name;    // counter 名
    int counter_add = 1;
    std::string story_msg;       // 完成提示文字
    int npc_id = 0;
    int trust = 0, respect = 0, fear = 0, gratitude = 0, corruption = 0;
};

struct QuestDef {
    std::string id;              // "save_prisoner" | "kill_warden" | ...
    int quest_id = 0;            // 数字 ID
    std::string title;
    std::string desc;
    int chapter = 0;             // 章节限制, 0=any
    bool hidden = false;
    bool auto_accept = false;

    // 解锁条件
    std::string required_flag;   // WorldFlag 字符串
    std::string required_stage;  // "chapter_1"/"chapter_2"/"final"/""
    int required_counter = 0;
    std::string counter_key;

    // 完成条件
    std::string complete_flag;   // WorldFlag 字符串 (空=手动)

    // NPC 关联
    int npc_floor = 0;           // 0=无NPC关联

    // 链式任务
    int next_quest_id = 0;

    // 奖励
    QuestRewardDef reward;
};

// ── 全局注册表 ──
bool load_quest_defs(const std::string& json_path);
int  load_quest_defs_from_json(const char* json_text, MergeMode mode,
                                const char* id_namespace = nullptr);  // G4.2: namespace
const QuestDef* get_quest_def(int quest_id);
const std::unordered_map<int, QuestDef>& get_all_quest_defs();
bool is_quest_defs_loaded();
