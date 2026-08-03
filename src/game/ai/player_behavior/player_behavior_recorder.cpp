#include "ai/player_behavior/player_behavior_recorder.h"
#include <cstdio>
#include <cmath>

// ── Data methods ──
void PlayerBehaviorData::record_weapon_attack(const char* wt_name) {
    if (!wt_name) return;
    weapon_attacks[wt_name]++;
    weapon_attacks_total++;
}

void PlayerBehaviorData::record_skill_use(const char* skill_id) {
    if (!skill_id) return;
    skill_uses[skill_id]++;
    skill_uses_total++;
}

void PlayerBehaviorData::record_damage_taken(int amount, int floor) {
    if (amount <= 0) return;
    total_damage_taken += amount;
    int idx = floor - 1;
    if (idx >= 0 && idx < 15) damage_per_floor[idx] += (float)amount;
}

void PlayerBehaviorData::record_movement(float dx, float dy) {
    if (dx < -20) move_left++;
    else if (dx > 20) move_right++;
    if (dy < -20) move_up++;
    else if (dy > 20) move_down++;
    // Dodge detection: large displacement in one tick
    if (fabsf(dx) > 200.0f || fabsf(dy) > 200.0f) dodge_count++;
}

// ── Recorder ──
PlayerBehaviorRecorder& PlayerBehaviorRecorder::inst() {
    static PlayerBehaviorRecorder rec;
    return rec;
}

void PlayerBehaviorRecorder::on_weapon_attack(const char* wt_name) {
    _data.record_weapon_attack(wt_name);
}

void PlayerBehaviorRecorder::on_skill_use(const char* skill_id) {
    _data.record_skill_use(skill_id);
}

void PlayerBehaviorRecorder::on_player_moved(float dx, float dy) {
    _data.record_movement(dx, dy);
}

void PlayerBehaviorRecorder::on_player_damaged(int amount, int floor) {
    _data.record_damage_taken(amount, floor);
}

void PlayerBehaviorRecorder::reset() { _data.reset(); }

void PlayerBehaviorRecorder::save_to_file(const char* path) const {
    FILE* f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "{\n");
    // Weapon usage
    fprintf(f, "  \"weapon_attacks\": {");
    bool first = true;
    for (auto& kv : _data.weapon_attacks) {
        if (!first) fprintf(f, ",");
        fprintf(f, "\n    \"%s\": %d", kv.first.c_str(), kv.second);
        first = false;
    }
    fprintf(f, "\n  },\n");
    fprintf(f, "  \"weapon_attacks_total\": %d,\n", _data.weapon_attacks_total);
    // Skill usage
    fprintf(f, "  \"skill_uses\": {");
    first = true;
    for (auto& kv : _data.skill_uses) {
        if (!first) fprintf(f, ",");
        fprintf(f, "\n    \"%s\": %d", kv.first.c_str(), kv.second);
        first = false;
    }
    fprintf(f, "\n  },\n");
    fprintf(f, "  \"skill_uses_total\": %d,\n", _data.skill_uses_total);
    fprintf(f, "  \"total_damage_taken\": %d,\n", _data.total_damage_taken);
    // Movement
    fprintf(f, "  \"move_left\": %d,\n", _data.move_left);
    fprintf(f, "  \"move_right\": %d,\n", _data.move_right);
    fprintf(f, "  \"move_up\": %d,\n", _data.move_up);
    fprintf(f, "  \"move_down\": %d,\n", _data.move_down);
    fprintf(f, "  \"dodge_count\": %d,\n", _data.dodge_count);
    fprintf(f, "  \"floors_recorded\": %d\n", _data.floors_recorded);
    fprintf(f, "}\n");
    fclose(f);
}

void PlayerBehaviorRecorder::load_from_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return;
    // Simple key-value JSON parser (same style as save_manager)
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char key[128] = "";
        int val = 0;
        if (sscanf(line, " \"%127[^\"]\": %d", key, &val) == 2) {
            std::string ks(key);
            if (ks == "weapon_attacks_total") _data.weapon_attacks_total = val;
            else if (ks == "skill_uses_total")    _data.skill_uses_total = val;
            else if (ks == "total_damage_taken")  _data.total_damage_taken = val;
            else if (ks == "move_left")   _data.move_left = val;
            else if (ks == "move_right")  _data.move_right = val;
            else if (ks == "move_up")     _data.move_up = val;
            else if (ks == "move_down")   _data.move_down = val;
            else if (ks == "dodge_count") _data.dodge_count = val;
            else if (ks == "floors_recorded") _data.floors_recorded = val;
        }
    }
    fclose(f);
}
