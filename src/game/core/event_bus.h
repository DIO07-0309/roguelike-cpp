#pragma once
#include <functional>
#include <vector>
#include <string>
#include <unordered_map>
#include "event_types.h"

// ============================================================
// D7 Step5: EventBus — 全局事件总线 (单例)
// 解耦模块通信, Combat/Boss/Quest/Presentation 统一入口
// ============================================================

using EventCallback = std::function<void(const GameEvent&)>;

class EventBus {
public:
    static EventBus& inst();

    // ── 订阅/注销 ──
    void subscribe(GameEventType type, const EventCallback& cb,
                   const char* subscriber_name = nullptr);
    void unsubscribe(GameEventType type, void* owner);

    // ── 广播 ──
    void emit(const GameEvent& event);
    void emit(GameEventType type, void* sender = nullptr, int val = 0,
              float fval = 0.0f, const char* str = nullptr);

    // ── 管理 ──
    void clear();
    int  listener_count(GameEventType type) const;

private:
    EventBus() = default;
    struct Sub {
        EventCallback callback;
        void* owner = nullptr;
        const char* name;
    };
    std::unordered_map<int, std::vector<Sub>> _listeners;
};
