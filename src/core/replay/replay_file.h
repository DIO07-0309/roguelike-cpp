#pragma once
#include <string>
#include <vector>
#include <cstdint>

// ============================================================
// G4.5: ReplayFile — deterministic replay format
// ============================================================

struct ModSnapshot { std::string id, version; };

struct InputFrame {
    int frame = 0;
    int action_id = 0;   // numeric action ID
};

struct ReplayFile {
    int version = 1;
    std::string game_version;
    uint32_t seed = 0;
    std::vector<ModSnapshot> mods;
    std::vector<InputFrame> actions;
    // G4.5: hash chain verification
    std::vector<uint64_t> hash_chain;
};

// ── action name ↔ id mapping (must match InputMap defaults) ──
int  replay_action_id(const char* action_name);
const char* replay_action_name(int id);
int  replay_action_count();

// ── JSON serialization ──
std::string save_replay(const ReplayFile& rf);
bool load_replay(const std::string& path, ReplayFile& rf);
