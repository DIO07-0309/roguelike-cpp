#pragma once
#include "boss_replay.h"

// ============================================================
// D5 Step5: BossEncounterController — Boss战阶段控制器
// ============================================================

enum class EncounterPhase { OPENING, PRESSURE, CONTROL, LAST_STAND, ENDED };

class BossEncounterController {
public:
    void start(int boss_floor);
    void tick(float dt, float boss_hp_pct);
    void end(BossReplayMemory& mem, int dmg_done, int dmg_taken, int arena_zones);

    EncounterPhase phase() const { return _phase; }
    float phase_timer() const { return _phase_timer; }
    float total_time() const { return _total_time; }
    bool should_trigger_event() const;
    const char* phase_name() const;
    BossBattleReport report() const { return _report; }
    int  adaptive_strategy() const { return _report.replay.analyze_strategy(); }

private:
    EncounterPhase _phase = EncounterPhase::OPENING;
    float _phase_timer = 0.0f;
    float _total_time = 0.0f;
    float _event_cooldown = 0.0f;
    bool  _last_stand_triggered = false;
    int   _boss_floor = 0;
    int   _arena_spawned = 0;
    BossBattleReport _report;
};
