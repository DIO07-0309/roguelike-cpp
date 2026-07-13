#pragma once
#include <string>
#include <vector>
#include "types/boss_types.h"

class WorldState;
class QuestManager;
class RelationshipSystem;
class StoryDirector;

// D4 Step5.5: BossNarrative — Boss读取世界的叙事层

class BossNarrative {
public:
    BossNarrative();
    const BossDialogue* find_intro(int boss_floor, const WorldState& ws, BuildType bt,
                                    const RelationshipSystem& rels,
                                    const StoryDirector& story) const;
    const BossDialogue* find_phase2(int boss_floor, const WorldState& ws, BuildType bt) const;
    const BossDialogue* find_death(int boss_floor, const WorldState& ws, BuildType bt) const;
    static BossModifierHook calc_modifier(int boss_floor, const WorldState& ws,
                                           BuildType bt, const RelationshipSystem& rels);
private:
    std::vector<BossDialogue> _dialogues;
};

extern const BossDialogue BOSS_DIALOGUE_BLOOD_RITUAL;
extern const BossDialogue BOSS_DIALOGUE_CURSE;
extern const BossDialogue BOSS_DIALOGUE_SAVED_PRIEST;
extern const BossDialogue BOSS_DIALOGUE_FIRE_MAGE;
extern const BossDialogue BOSS_DIALOGUE_POISON;
extern const BossDialogue BOSS_DIALOGUE_TIME;
extern const BossDialogue BOSS_DIALOGUE_MELEE;
