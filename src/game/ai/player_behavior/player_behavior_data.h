#pragma once
#include <string>
#include <unordered_map>
#include <cmath>

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

    void record_weapon_attack(const char* wt_name) {
        if (!wt_name) return;
        weapon_attacks[wt_name]++;
        weapon_attacks_total++;
    }
    void record_skill_use(const char* skill_id) {
        if (!skill_id) return;
        skill_uses[skill_id]++;
        skill_uses_total++;
    }
    void record_damage_taken(int amount, int floor) {
        if (amount <= 0) return;
        total_damage_taken += amount;
        int idx = floor - 1;
        if (idx >= 0 && idx < 15) damage_per_floor[idx] += (float)amount;
    }
    void record_movement(float dx, float dy) {
        if (dx < -20) move_left++; else if (dx > 20) move_right++;
        if (dy < -20) move_up++;   else if (dy > 20) move_down++;
        if (fabsf(dx) > 200.0f || fabsf(dy) > 200.0f) dodge_count++;
    }
};
