#include "replay_file.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>
#include <cstring>

using json = nlohmann::json;

// ============================================================
// Action ID mapping (must match InputMap::setup_defaults order)
// ============================================================

static const char* ACTION_NAMES[] = {
    "move_up","move_down","move_left","move_right",
    "attack","pickup","inventory","skill_1","skill_2",
    "skill_3","skill_4","descend","confirm","cancel",
    "fullscreen",nullptr
};
static constexpr int ACTION_COUNT = 15;

int replay_action_id(const char* name) {
    for (int i = 0; i < ACTION_COUNT; i++)
        if (ACTION_NAMES[i] && strcmp(ACTION_NAMES[i], name) == 0)
            return i;
    return -1;
}

const char* replay_action_name(int id) {
    if (id >= 0 && id < ACTION_COUNT) return ACTION_NAMES[id];
    return "?";
}

int replay_action_count() { return ACTION_COUNT; }

// ============================================================
// Serialization helpers (used by Recorder/Player)
// ============================================================

std::string save_replay(const ReplayFile& rf) {
    json j;
    j["version"] = rf.version;
    j["game_version"] = rf.game_version;
    j["seed"] = rf.seed;
    j["mods"] = json::array();
    for (auto& m : rf.mods) {
        json mj;
        mj["id"] = m.id;
        mj["version"] = m.version;
        j["mods"].push_back(mj);
    }
    j["actions"] = json::array();
    for (auto& a : rf.actions) {
        json aj;
        aj["f"] = a.frame;
        aj["a"] = a.action_id;
        j["actions"].push_back(aj);
    }
    j["hash_chain"] = json::array();
    for (auto h : rf.hash_chain)
        j["hash_chain"].push_back(h);
    return j.dump(2);
}

bool load_replay(const std::string& path, ReplayFile& rf) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    json j;
    try { f >> j; } catch (...) { return false; }

    rf.version = j.value("version", 1);
    rf.game_version = j.value("game_version", "");
    rf.seed = j.value("seed", 0u);
    rf.mods.clear();
    if (j.contains("mods") && j["mods"].is_array())
        for (auto& mj : j["mods"])
            rf.mods.push_back({mj.value("id",""), mj.value("version","")});
    rf.actions.clear();
    if (j.contains("actions") && j["actions"].is_array())
        for (auto& aj : j["actions"])
            rf.actions.push_back({aj.value("f",0), aj.value("a",0)});
    rf.hash_chain.clear();
    if (j.contains("hash_chain") && j["hash_chain"].is_array())
        for (auto& hv : j["hash_chain"])
            rf.hash_chain.push_back(hv.get<uint64_t>());
    return true;
}
