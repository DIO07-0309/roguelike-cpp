#pragma once
#include "replay_file.h"
#include <vector>

// ============================================================
// G4.5: ReplayRecorder — record JustPressed actions per frame
// ============================================================

class ReplayRecorder {
public:
    void start(uint32_t seed, const char* game_ver,
               const std::vector<ModSnapshot>& mods);
    void tick();                               // advance frame
    void record(const char* action_name);      // record JustPressed
    void record_hash(uint64_t h);              // G4.5: hash chain
    bool is_active() const { return _active; }

    const ReplayFile& file() const { return _file; }
    ReplayFile& file() { return _file; }

private:
    bool _active = false;
    int  _frame = 0;
    ReplayFile _file;
};
