#include "boss_cinematic.h"
#include "camera_director.h"

void BossCinematicController::start_intro() {
    _phase = CinematicPhase::INTRO;
    _timer = CameraDirector::INTRO_FREEZE;
}

void BossCinematicController::trigger_phase2() {
    _phase = CinematicPhase::PHASE2_TRANSITION;
    _timer = CameraDirector::PHASE2_PAUSE;
}

void BossCinematicController::trigger_last_stand() {
    _phase = CinematicPhase::LAST_STAND;
    _timer = CameraDirector::LASTSTAND_FREEZE;
}

void BossCinematicController::trigger_death() {
    _phase = CinematicPhase::DEATH;
    _timer = CameraDirector::DEATH_SLOWMO;
}

void BossCinematicController::tick(float dt) {
    if (_phase == CinematicPhase::NONE) return;
    _timer -= dt;
    if (_timer <= 0) _phase = CinematicPhase::NONE;
}

const char* BossCinematicController::phase_name() const {
    switch (_phase) {
        case CinematicPhase::INTRO:    return "INTRO";
        case CinematicPhase::PHASE2_TRANSITION: return "PHASE2";
        case CinematicPhase::LAST_STAND: return "LAST_STAND";
        case CinematicPhase::DEATH:    return "DEATH";
        default: return "NONE";
    }
}
