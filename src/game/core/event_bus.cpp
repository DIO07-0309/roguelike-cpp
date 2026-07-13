#include "event_bus.h"
#include <algorithm>
#include <cstdio>

// ═══════════════════════════════════════════════════════════════
// 单例
// ═══════════════════════════════════════════════════════════════
EventBus& EventBus::inst() {
    static EventBus bus;
    return bus;
}

// ═══════════════════════════════════════════════════════════════
// 订阅
// ═══════════════════════════════════════════════════════════════
void EventBus::subscribe(GameEventType type, const EventCallback& cb,
                          const char* name) {
    int idx = (int)type;
    Sub sub{cb, nullptr, name};
    _listeners[idx].push_back(sub);
#ifdef _DEBUG
    printf("[EventBus] SUB %s ← %s\n",
           name ? name : "anon",
           type == GameEventType::PLAYER_LEVEL_UP ? "PLAYER_LEVEL_UP" :
           type == GameEventType::MONSTER_DIED ? "MONSTER_DIED" :
           type == GameEventType::BOSS_DEAD ? "BOSS_DEAD" :
           type == GameEventType::RELIC_GAIN ? "RELIC_GAIN" :
           type == GameEventType::QUEST_COMPLETE ? "QUEST_COMPLETE" :
           type == GameEventType::FLOOR_ENTER ? "FLOOR_ENTER" : "?");
#endif
}

void EventBus::unsubscribe(GameEventType type, void* owner) {
    int idx = (int)type;
    auto& vec = _listeners[idx];
    vec.erase(std::remove_if(vec.begin(), vec.end(),
        [owner](const Sub& s) { return s.owner == owner; }), vec.end());
}

// ═══════════════════════════════════════════════════════════════
// 广播
// ═══════════════════════════════════════════════════════════════
void EventBus::emit(const GameEvent& event) {
    int idx = (int)event.type;
    auto it = _listeners.find(idx);
    if (it == _listeners.end()) return;
    for (auto& sub : it->second) {
        if (sub.callback) sub.callback(event);
    }
}

void EventBus::emit(GameEventType type, void* sender, int val,
                     float fval, const char* str) {
    GameEvent ev{type, sender, val, fval, str};
    emit(ev);
}

// ═══════════════════════════════════════════════════════════════
// 管理
// ═══════════════════════════════════════════════════════════════
void EventBus::clear() { _listeners.clear(); }
int  EventBus::listener_count(GameEventType type) const {
    auto it = _listeners.find((int)type);
    return (it != _listeners.end()) ? (int)it->second.size() : 0;
}
