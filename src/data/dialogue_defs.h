#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "core/registry_provider.h"  // G4.1: MergeMode

// ============================================================
// G2.1: DialogueDef — 剧情对话配置数据 (纯数据, 无 Gameplay/Render 依赖)
// 一条 DialogueDef 对应一条 BossDialogue — 1:1 映射, 不拆分
// 属于 Data 层, 位于 src/data/
// 未来: NPC 对话 / 商店台词 / 教程提示 / 结局文本 统一复用此模式
// ============================================================

struct DialogueDef {
    std::string id;                 // "sk_blood" | "nec_default" | ...
    int boss_floor = 0;             // 0=通用(匹配任意层), 5/10/15=特定层
    std::string stage;              // "chapter_1"|"chapter_2"|"final"|""=any
    std::string required_flag;      // "blood_ritual"|"saved_prisoner"|...|""=none
    std::string required_build;     // "berserker"|"poison_master"|...|""=none
    int relation_threshold = 0;     // ≥值 → 触发 (0=无条件)
    int npc_id = 0;                 // 关联NPC (0=无)
    std::string intro_text;         // 开场对话 (空=无)
    std::string phase2_text;        // Phase2 对话 (空=无)
    std::string death_text;         // 死亡对话 (空=无)
    bool once = false;              // 仅触发一次
};

// ── 全局注册表 ──
bool load_dialogue_defs(const std::string& json_path);
int  load_dialogue_defs_from_json(const char* json_text, MergeMode mode,
                                   const char* id_namespace = nullptr);  // G4.2: namespace
const DialogueDef* get_dialogue_def(const std::string& id);
const std::unordered_map<std::string, DialogueDef>& get_all_dialogue_defs();
bool is_dialogue_defs_loaded();
