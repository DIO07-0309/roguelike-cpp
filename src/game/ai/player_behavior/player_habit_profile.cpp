#include "ai/player_behavior/player_habit_profile.h"
#include <cstdio>

const char* PlayerHabitProfile::style_name() const {
    switch (style) {
    case PlayerStyle::AGGRESSIVE: return "AGGRESSIVE";
    case PlayerStyle::DEFENSIVE:  return "DEFENSIVE";
    case PlayerStyle::SNIPER:     return "SNIPER";
    case PlayerStyle::MAGE:       return "MAGE";
    default: return "BALANCED";
    }
}

const char* PlayerHabitProfile::counter_strategy_text() const {
    static char buf[256];
    snprintf(buf, sizeof(buf),
        "Style: %s | Fav skill: %d | "
        "Attack: %s | Skill spam: %s | Dodge: %s | Heal: %s",
        style_name(),
        predicted_fav_skill,
        predict_attack_heavy  ? "bait" : "normal",
        predict_skill_spam    ? "interrupt" : "normal",
        predict_low_dodge     ? "pressure" : "respect",
        predict_panic_heal    ? "punish" : "normal"
    );
    return buf;
}
