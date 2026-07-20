#pragma once

// ============================================================
// D4.6 Step2: CombatFeelSystem — 统一打击感常数
// 所有HitStop/Shake/Crit/DamageNumber从此读取
// ============================================================
struct CombatFeelSystem {
    // ── G5.7: HitStop tuning (juicier) ──
    static constexpr float LIGHT_HIT     = 0.030f;
    static constexpr float HEAVY_HIT     = 0.075f;
    static constexpr float BOSS_HIT      = 0.10f;
    static constexpr float CRITICAL_HIT  = 0.07f;
    static constexpr float KILL_SLOWMO   = 0.14f;
    static constexpr float RELIC_PICKUP  = 0.20f;
    static constexpr float BUILD_COMPLETE = 0.18f;
    static constexpr float SKILL_EVOLVED  = 0.12f;  // G5.7

    // ── G5.7: CameraShake tuning (more impact) ──
    static constexpr float SHAKE_LIGHT   = 3.0f;
    static constexpr float SHAKE_MEDIUM  = 8.0f;
    static constexpr float SHAKE_HEAVY   = 15.0f;
    static constexpr float SHAKE_BOSS    = 20.0f;
    static constexpr float SHAKE_CRIT    = 5.0f;   // G5.7
    static constexpr float SHAKE_COMBO   = 4.0f;   // G5.7

    // ── Damage float tuning ──
    static constexpr float DMG_FLOAT_LIFE_LIGHT  = 0.55f;
    static constexpr float DMG_FLOAT_LIFE_HEAVY  = 0.75f;
    static constexpr float DMG_FLOAT_LIFE_CRIT   = 0.90f;
    static constexpr float DMG_FLOAT_SCALE_MIN   = 1.0f;
    static constexpr float DMG_FLOAT_SCALE_CRIT  = 1.6f;  // crit numbers are bigger

    // Combo里程碑 (hit数)
    static constexpr int COMBO_5   = 5;
    static constexpr int COMBO_10  = 10;
    static constexpr int COMBO_20  = 20;
    static constexpr int COMBO_MAX = 30;

    // 暴击 (combo驱动: 段数越高暴击率越高)
    static float crit_chance(int combo_count) {
        if (combo_count >= 4) return 0.30f;  // 重击 30%
        if (combo_count == 3) return 0.15f;
        if (combo_count == 2) return 0.08f;
        return 0.03f; // combo 1: 3%
    }

    static int crit_multiplier() { return 2; }  // crit = ×2

    // Combo消息
    static const char* combo_message(int count) {
        if (count >= 30) return "PERFECT COMBO!";
        if (count >= 20) return "COMBO ×20!";
        if (count >= 10) return "COMBO ×10!";
        if (count >= 5)  return "5 HIT!";
        return nullptr;
    }
};
