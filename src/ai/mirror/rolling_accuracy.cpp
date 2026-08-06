#include "ai/mirror/rolling_accuracy.h"

static constexpr size_t kWindowSize = 32;

void RollingAccuracy::reset() {
    _window.clear();
    _hits = 0;
    _total = 0;
}

void RollingAccuracy::add(bool hit) {
    if (_window.size() == kWindowSize) {
        if (_window.front()) _hits--;
        _window.pop_front();
    }
    _window.push_back(hit);
    if (hit) _hits++;
    _total++;
}

float RollingAccuracy::accuracy() const {
    return _window.empty() ? 0.0f : (float)_hits / (float)_window.size();
}

int RollingAccuracy::hit_count() const { return _hits; }

int RollingAccuracy::total() const { return _total; }
