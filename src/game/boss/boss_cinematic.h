#pragma once
#include <string>
#include "types/boss_types.h"

// D5 Step6: BossCinematicController — Boss战演出控制

class BossCinematicController {
public:
    void start_intro();
    void trigger_phase2();
    void trigger_last_stand();
    void trigger_death();
    void tick(float dt);
    bool  is_running() const { return _phase != CinematicPhase::NONE; }
    CinematicPhase phase() const { return _phase; }
    float timer() const { return _timer; }
    const char* phase_name() const;
    // 指令式: 当前是否需要 HUD/UI 全屏效果
    bool should_draw_overlay() const { return _phase != CinematicPhase::NONE; }
    bool should_freeze_player() const { return _phase == CinematicPhase::INTRO && _timer > 0; }
private:
    CinematicPhase _phase = CinematicPhase::NONE;
    float _timer = 0.0f;
};
