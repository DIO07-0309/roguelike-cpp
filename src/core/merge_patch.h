#pragma once
#include <nlohmann/json.hpp>
#include <string>

// ============================================================
// G4.3: MergePatch helper — shared by all nlohmann-based loaders
// ============================================================

inline nlohmann::json _merge_patch(const nlohmann::json& target, const nlohmann::json& patch) {
    nlohmann::json result = target;
    for (auto it = patch.begin(); it != patch.end(); ++it) {
        if (it.key() == "__patch") continue;
        if (it.value().is_object() && result.contains(it.key()) && result[it.key()].is_object())
            result[it.key()] = _merge_patch(result[it.key()], it.value());
        else
            result[it.key()] = it.value();
    }
    return result;
}
