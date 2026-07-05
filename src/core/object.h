#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

// ============================================================
// Object — 所有类的基类，提供信号槽机制
// ============================================================
class Object {
public:
    virtual ~Object() = default;

    // 信号连接: signal.connect([](int x) { ... });
    template<typename... Args>
    struct Signal {
        using Slot = std::function<void(Args...)>;

        int connect(Slot slot) {
            int id = _next_id++;
            _slots.emplace_back(id, std::move(slot));
            return id;
        }

        void disconnect(int id) {
            _slots.erase(std::remove_if(_slots.begin(), _slots.end(),
                [id](const auto& p) { return p.first == id; }), _slots.end());
        }

        void emit(Args... args) {
            for (auto& [id, slot] : _slots) {
                slot(args...);
            }
        }

        void clear() { _slots.clear(); }

    private:
        int _next_id = 1;
        std::vector<std::pair<int, Slot>> _slots;
    };

protected:
    Object() = default;
};
