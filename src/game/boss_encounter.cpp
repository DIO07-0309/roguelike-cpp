#include "boss_encounter.h"
#include "item.h"  // rng

void BossEncounterController::start(int bf) {
    _phase = EncounterPhase::OPENING;
    _phase_timer = 0; _total_time = 0; _event_cooldown = 12.0f;
    _last_stand_triggered = false; _boss_floor = bf; _arena_spawned = 0;
    _report = BossBattleReport{};
}

void BossEncounterController::tick(float dt, float boss_hp_pct) {
    _total_time += dt;
    _phase_timer += dt;
    _event_cooldown -= dt;

    // 阶段转换: OPENING→PRESSURE (15s or HP<75%)
    if (_phase == EncounterPhase::OPENING && (_total_time > 15 || boss_hp_pct < 0.75f))
        _phase = EncounterPhase::PRESSURE;
    // PRESSURE→CONTROL (HP<40% or time>60s)
    if (_phase == EncounterPhase::PRESSURE && (boss_hp_pct < 0.40f || _total_time > 60))
        _phase = EncounterPhase::CONTROL;
    // CONTROL→LAST_STAND (HP<15%)
    if (_phase == EncounterPhase::CONTROL && boss_hp_pct < 0.15f && !_last_stand_triggered) {
        _phase = EncounterPhase::LAST_STAND;
        _last_stand_triggered = true;
    }
}

void BossEncounterController::end(BossReplayMemory& mem, int dmg_d, int dmg_t, int az) {
    _arena_spawned = az;
    _phase = EncounterPhase::ENDED;
    _report = generate_battle_report(mem, dmg_d, dmg_t, _total_time, az);
}

bool BossEncounterController::should_trigger_event() const {
    if (_event_cooldown > 0) return false;
    return (_total_time > 20) && ((rng() % 100) < 25);  // 25%概率触发
}

const char* BossEncounterController::phase_name() const {
    switch (_phase) {
        case EncounterPhase::OPENING:    return "OPENING";
        case EncounterPhase::PRESSURE:   return "PRESSURE";
        case EncounterPhase::CONTROL:    return "CONTROL";
        case EncounterPhase::LAST_STAND: return "LAST_STAND";
        case EncounterPhase::ENDED:      return "ENDED";
    }
    return "?";
}
