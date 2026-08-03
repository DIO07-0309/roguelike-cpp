#pragma once
#include "player_behavior_data.h"

// ============================================================
// F15.1: PlayerBehaviorRecorder — global singleton recorder
// Hooks into existing code paths at 4 minimal points.
// Zero gameplay change. Data collected across floors 1-14.
// ============================================================

class PlayerBehaviorRecorder {
public:
    static PlayerBehaviorRecorder& inst();

    // Data access
    PlayerBehaviorData& data() { return _data; }
    const PlayerBehaviorData& data() const { return _data; }

    // Hook 1: WeaponExecutor calls this on every weapon attack
    void on_weapon_attack(const char* weapon_type_name);

    // Hook 2: CombatCoordinator calls this on every skill use
    void on_skill_use(const char* skill_id);

    // Hook 3: PlayerController calls this each tick with movement delta
    void on_player_moved(float dx, float dy);

    // Hook 4: Called when player takes damage
    void on_player_damaged(int amount, int floor);

    // Save/Load
    void save_to_file(const char* path) const;
    void load_from_file(const char* path);

    void reset();

private:
    PlayerBehaviorRecorder() = default;
    PlayerBehaviorData _data;
};

// Global accessor
#define g_behavior PlayerBehaviorRecorder::inst()
