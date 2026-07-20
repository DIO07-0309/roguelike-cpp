#include "player.h"
#include <cstdio>

bool ReplayPlayer::load(const std::string& path) {
    return ::load_replay(path, _file);
}

void ReplayPlayer::start() {
    _active = true;
    _frame = 0;
    _cursor = 0;
}

void ReplayPlayer::tick() {
    if (!_active) return;
    _frame++;
    // check if we've exhausted all actions
    if (_cursor >= _file.actions.size()) return;
}

bool ReplayPlayer::is_finished() const {
    if (!_active) return false;
    // done when all actions consumed AND at least past last action frame
    if (_cursor < _file.actions.size()) return false;
    if (_file.actions.empty()) return _frame > 60; // min 60 frame
    return _frame > _file.actions.back().frame + 10;
}

bool ReplayPlayer::is_action_just_pressed(const char* action_name) const {
    if (!_active) return false;
    int id = replay_action_id(action_name);
    if (id < 0) return false;

    // consume all matching actions at current frame
    // (mutable: cursor advances on read — "just pressed" semantic)
    while (_cursor < _file.actions.size()) {
        auto& a = _file.actions[_cursor];
        if (a.frame > _frame) return false;   // not yet
        if (a.frame == _frame && a.action_id == id) {
            _cursor++;
            return true;
        }
        if (a.frame < _frame) { _cursor++; continue; } // skip stale
        if (a.frame == _frame && a.action_id != id) return false;
    }
    return false;
}

bool ReplayPlayer::verify_hash(int frame, uint64_t actual) const {
    if (frame < 0 || frame >= (int)_file.hash_chain.size()) return false;
    return _file.hash_chain[frame] == actual;
}
