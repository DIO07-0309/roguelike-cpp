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
    static char buf[320];
    const char* counter = fight_back_rate > 0.6f ? "retaliator"
                       : (fight_back_rate > 0.0f && fight_back_rate < 0.3f
                          ? "panicker" : "normal");
    snprintf(buf, sizeof(buf),
        "Style: %s | Attack: %s | Skill: %s | Dodge: %s | Heal: %s | "
        "Counter: %s | Face: %.0f%% | Rhythm: %.1fs",
        style_name(),
        predict_attack_heavy ? "bait" : "normal",
        predict_skill_spam   ? "interrupt" : "normal",
        predict_low_dodge    ? "pressure" : "respect",
        predict_panic_heal   ? "punish" : "normal",
        counter,
        face_enemy_rate * 100.0f,
        attack_rhythm_var
    );
    return buf;
}
