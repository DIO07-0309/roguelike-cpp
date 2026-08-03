#pragma once
#include <string>
#include <unordered_map>

// ============================================================
// F15.1: PlayerBehaviorData — 14-floor combat data collection
// Lightweight POD. Saved as behavior.json alongside save.json.
// Collected passively — zero gameplay change during recording.
// ============================================================

struct PlayerBehaviorData {
    // Weapon usage: weapon_type → attack_count
    // "DAGGER":52, "SWORD":120, "CROSSBOW":33, ...
    std::unordered_map<std::string, int> weapon_attacks;
    int weapon_attacks_total = 0;

    // Skill usage: skill_id → use_count
    std::unordered_map<std::string, int> skill_uses;
    int skill_uses_total = 0;

    // Damage received
    int total_damage_taken = 0;
    float damage_per_floor[15] = {};

    // Movement: direction counts
    int move_left = 0, move_right = 0, move_up = 0, move_down = 0;
    int dodge_count = 0; // player moved >200px in one tick = dodge

    // Floors recorded (for save/load tracking)
    int floors_recorded = 0;

    void reset() { *this = PlayerBehaviorData{}; }
    void record_weapon_attack(const char* wt_name);
    void record_skill_use(const char* skill_id);
    void record_damage_taken(int amount, int floor);
    void record_movement(float dx, float dy);
};
