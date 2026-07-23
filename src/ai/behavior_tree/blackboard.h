#pragma once
// G8.1: Blackboard — shared state key-value store
// G8.3: MCTS copies blackboard for future-state simulation.
// G8.4: RL observations are blackboard snapshots.

#include <unordered_map>
#include <string>
#include <any>
#include <stdexcept>

namespace bt {

class Blackboard {
public:
    template<typename T>
    void set(const std::string& key, T value) {
        _data[key] = value;
    }

    template<typename T>
    T get(const std::string& key) const {
        auto it = _data.find(key);
        if (it == _data.end())
            throw std::runtime_error("Blackboard key not found: " + key);
        return std::any_cast<T>(it->second);
    }

    template<typename T>
    T get(const std::string& key, T default_val) const {
        auto it = _data.find(key);
        if (it == _data.end()) return default_val;
        try { return std::any_cast<T>(it->second); }
        catch (...) { return default_val; }
    }

    bool has(const std::string& key) const {
        return _data.find(key) != _data.end();
    }

    void clear() { _data.clear(); }

private:
    std::unordered_map<std::string, std::any> _data;
};

} // namespace bt
