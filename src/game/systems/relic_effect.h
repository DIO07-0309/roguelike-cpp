#pragma once
#include <string>

enum class RelicTrigger {
    PASSIVE,
    ON_HIT,
    ON_KILL,
    PRE_DAMAGE,
    ON_HURT,
    ON_FLOOR_ENTER,
    ON_SKILL_CAST,
    ON_DEATH,
};

enum class RelicEffectType {
    MODIFY_STAT,
    ADD_BUFF,
    HEAL,
    DEAL_AOE_DAMAGE,
    DAMAGE_REDUCTION,
    RESET_COOLDOWNS,
    SPAWN_PROJECTILE,
};

enum class RelicTarget {
    SELF,
    NEAREST_ENEMY,
    ALL_ENEMIES,
};

struct RelicEffectDef {
    RelicTrigger trigger = RelicTrigger::PASSIVE;
    RelicEffectType type = RelicEffectType::MODIFY_STAT;
    RelicTarget target = RelicTarget::SELF;

    std::string stat;
    float value = 0.0f;
    int   value2 = 0;
    std::string buff_id;
    float chance = 1.0f;
};
