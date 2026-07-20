#pragma once
#include "replay_file.h"
#include <cstdint>

// ============================================================
// G4.5: ReplayPlayer — inject recorded actions as fake input
// ============================================================

class ReplayPlayer {
public:
    bool load(const std::string& path);      // load .rpy file
    void start();                             // begin replay
    void tick();
    bool is_active() const { return _active; }
    bool is_finished() const;

    // ── fake input: call once per frame per action ──
    bool is_action_just_pressed(const char* action_name) const;

    // ── hash verification ──
    bool verify_hash(int frame, uint64_t actual_hash) const;
    uint32_t replay_seed() const { return _file.seed; }

private:
    bool _active = false;
    int  _frame = 0;
    mutable size_t _cursor = 0;     // next action index (const-correct)
    ReplayFile _file;
};
