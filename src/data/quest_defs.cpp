#include "data/quest_defs.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>

using json = nlohmann::json;

// ============================================================
// G2.4: QuestDef 全局注册表 (key = quest_id, 与 QuestManager 兼容)
// ============================================================

static std::unordered_map<int, QuestDef> g_quest_defs;
static bool g_quest_defs_loaded = false;

static QuestRewardDef _parse_reward(const json& j) {
    QuestRewardDef rw;
    rw.relic_id     = j.value("relic_id", "");
    rw.buff_id      = j.value("buff_id", "");
    rw.buff_stacks  = j.value("buff_stacks", 1);
    rw.set_flag     = j.value("set_flag", "");
    rw.counter_name = j.value("counter_name", "");
    rw.counter_add  = j.value("counter_add", 1);
    rw.story_msg    = j.value("story_msg", "");
    rw.npc_id       = j.value("npc_id", 0);
    rw.trust        = j.value("trust", 0);
    rw.respect      = j.value("respect", 0);
    rw.fear         = j.value("fear", 0);
    rw.gratitude    = j.value("gratitude", 0);
    rw.corruption   = j.value("corruption", 0);
    return rw;
}

static QuestDef _parse_quest(const json& j) {
    QuestDef def;
    def.id           = j.value("id", "");
    def.quest_id     = j.value("quest_id", 0);
    def.title        = j.value("title", "");
    def.desc         = j.value("desc", "");
    def.chapter      = j.value("chapter", 0);
    def.hidden       = j.value("hidden", false);
    def.auto_accept  = j.value("auto_accept", false);
    def.required_flag    = j.value("required_flag", "");
    def.required_stage   = j.value("required_stage", "");
    def.required_counter = j.value("required_counter", 0);
    def.counter_key      = j.value("counter_key", "");
    def.complete_flag    = j.value("complete_flag", "");
    def.npc_floor        = j.value("npc_floor", 0);
    def.next_quest_id    = j.value("next_quest_id", 0);

    if (j.contains("reward") && j["reward"].is_object())
        def.reward = _parse_reward(j["reward"]);

    return def;
}

// ── G4.1 ──
int load_quest_defs_from_json(const char* json_text, MergeMode mode,
                                const char* id_namespace) {
    if (!json_text || !json_text[0]) return 0;
    json j;
    try { j = json::parse(json_text); }
    catch (const std::exception& e) { printf("[QUEST_DEFS] parse: %s\n", e.what()); return 0; }
    if (!j.is_array()) return 0;
    std::string ns=id_namespace?(std::string(id_namespace)+":"):"";
    int count=0,skipped=0,dups=0,over=0;
    for(auto& entry:j){
        auto def=_parse_quest(entry);
        if(def.id.empty()){skipped++;continue;}
        if(g_quest_defs.count(def.quest_id)){if(mode==MergeMode::Replace){g_quest_defs[def.quest_id]=def;over++;}else dups++;continue;}
        g_quest_defs[def.quest_id]=def;count++;
    }
    g_quest_defs_loaded=true;
    printf("[QUEST_DEFS] +%d",count);if(over>0)printf(" ~%d",over);printf("\n");
    return count+over;
}

bool load_quest_defs(const std::string& json_path) {
    std::ifstream f(json_path);
    if (!f.is_open()) {
        printf("[QUEST_DEFS] ERROR: cannot open %s\n", json_path.c_str());
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return load_quest_defs_from_json(text.c_str(), MergeMode::Skip) > 0;
}

const QuestDef* get_quest_def(int quest_id) {
    auto it = g_quest_defs.find(quest_id);
    return (it != g_quest_defs.end()) ? &it->second : nullptr;
}

const std::unordered_map<int, QuestDef>& get_all_quest_defs() {
    return g_quest_defs;
}

bool is_quest_defs_loaded() { return g_quest_defs_loaded; }
