#include "boss_timeline.h"

void BossTimeline::record(float bt, const char* label) {
    _events.push_back({bt, label});
}

void BossTimeline::clear() { _events.clear(); }

const char* BossTimeline::last_label() const {
    return _events.empty() ? "NONE" : _events.back().label;
}
