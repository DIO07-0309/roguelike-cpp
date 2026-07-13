#pragma once
#include <string>
#include <vector>

// ============================================================
// D5 Step6: BossTimeline — 记录Boss战所有关键时刻
// 供Replay/Debug/Ending使用
// ============================================================
struct TimelineEvent {
    float time = 0.0f;         // 第几秒
    const char* label = nullptr; // e.g. "INTRO","PHASE2","DEATH","ARENA_SPAWN","EVENT"
};

class BossTimeline {
public:
    void record(float battle_time, const char* label);
    void clear();

    const std::vector<TimelineEvent>& events() const { return _events; }
    int count() const { return (int)_events.size(); }
    const char* last_label() const;
private:
    std::vector<TimelineEvent> _events;
};
