#pragma once
#include <string>
#include <vector>
#include "world_state.h"
#include "build_score.h"

class WorldState;
class QuestManager;
class RelationshipSystem;
class StoryDirector;

// ============================================================
// D4 Step5.5: BossNarrative — Boss读取世界的叙事层
// ============================================================
struct BossDialogue {
    StoryStage  stage = StoryStage::INTRO;
    int         boss_floor = 0;          // 5/10/15
    WorldFlag   required_flag = WorldFlag::NONE;
    BuildType   build = BuildType::NONE;
    int         relation_threshold = 0;  // trust阈值(仅特定NPC)
    int         npc_id = 0;             // 关联NPC
    const char* intro = nullptr;
    const char* phase2 = nullptr;
    const char* death = nullptr;
    bool        once = false;
};

// Boss 调整器 Hook (D5 Step1 正式启用)
struct BossModifierHook {
    bool  disable_phase = false;
    bool  disable_skill = false;
    bool  add_minions = false;
    float hp_scale = 1.0f;
    float damage_scale = 1.0f;
    const char* modifier_text = nullptr;  // UI显示: "HP-20%  Elite+" etc.
};

class BossNarrative {
public:
    BossNarrative();

    // 根据所有上下文查找匹配对话
    const BossDialogue* find_intro(int boss_floor, const WorldState& ws, BuildType bt,
                                    const RelationshipSystem& rels,
                                    const StoryDirector& story) const;
    const BossDialogue* find_phase2(int boss_floor, const WorldState& ws, BuildType bt) const;
    const BossDialogue* find_death(int boss_floor, const WorldState& ws, BuildType bt) const;

    // 计算 BossModifierHook (目前返回默认值)
    static BossModifierHook calc_modifier(int boss_floor, const WorldState& ws,
                                           BuildType bt, const RelationshipSystem& rels);
private:
    std::vector<BossDialogue> _dialogues;
};

// 预设对话引用
extern const BossDialogue BOSS_DIALOGUE_BLOOD_RITUAL;
extern const BossDialogue BOSS_DIALOGUE_CURSE;
extern const BossDialogue BOSS_DIALOGUE_SAVED_PRIEST;
extern const BossDialogue BOSS_DIALOGUE_FIRE_MAGE;
extern const BossDialogue BOSS_DIALOGUE_POISON;
extern const BossDialogue BOSS_DIALOGUE_TIME;
extern const BossDialogue BOSS_DIALOGUE_MELEE;
