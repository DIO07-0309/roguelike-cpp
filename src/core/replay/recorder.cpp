#include "recorder.h"

void ReplayRecorder::start(uint32_t seed, const char* game_ver,
                            const std::vector<ModSnapshot>& mods) {
    _active = true;
    _frame = 0;
    _file = ReplayFile{};
    _file.version = 1;
    _file.game_version = game_ver ? game_ver : "0.8.x";
    _file.seed = seed;
    _file.mods = mods;
}

void ReplayRecorder::tick() {
    if (!_active) return;
    _frame++;
}

void ReplayRecorder::record(const char* action_name) {
    if (!_active) return;
    int id = replay_action_id(action_name);
    if (id < 0) return;
    _file.actions.push_back({_frame, id});
}

void ReplayRecorder::record_hash(uint64_t h) {
    if (!_active) return;
    _file.hash_chain.push_back(h);
}
