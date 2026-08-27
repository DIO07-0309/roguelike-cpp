#pragma once
#include <string>
#include <unordered_map>

struct RelicEffectState {
    float timer = 0.0f;
    int   charges = 0;
    bool  activated = false;
};

struct RelicEffectRuntime {
    std::unordered_map<std::string, RelicEffectState> states;

    RelicEffectState& get(const std::string& relic_id) {
        return states[relic_id];
    }

    const RelicEffectState& get(const std::string& relic_id) const {
        auto it = states.find(relic_id);
        static RelicEffectState empty;
        return it != states.end() ? it->second : empty;
    }

    void reset() { states.clear(); }
};
