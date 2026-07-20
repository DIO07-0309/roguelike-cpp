#include "data/dialogue_defs.h"
#include "core/merge_patch.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <unordered_map>
#include <cstdio>

using json = nlohmann::json;

// ============================================================
// G2.1: DialogueDef 全局注册表
// ============================================================

static std::unordered_map<std::string, DialogueDef> g_dialogue_defs;
static std::unordered_map<std::string, std::string> _dialogue_src;  // G4.3
static bool g_dialogue_defs_loaded = false;

// ── 辅助: JSON → DialogueDef ──
static DialogueDef _parse_dialogue(const json& j) {
    DialogueDef def;
    def.id         = j.value("id", "");
    def.boss_floor = j.value("boss_floor", 0);
    def.stage      = j.value("stage", "");
    def.required_flag   = j.value("required_flag", "");
    def.required_build  = j.value("required_build", "");
    def.relation_threshold = j.value("relation_threshold", 0);
    def.npc_id      = j.value("npc_id", 0);
    def.intro_text  = j.value("intro_text", "");
    def.phase2_text = j.value("phase2_text", "");
    def.death_text  = j.value("death_text", "");
    def.once        = j.value("once", false);
    return def;
}

// ── G4.1 ──
int load_dialogue_defs_from_json(const char* json_text, MergeMode mode,
                                   const char* id_namespace) {
    if (!json_text || !json_text[0]) return 0;
    json j;
    try { j = json::parse(json_text); }
    catch (const std::exception& e) { printf("[DIALOGUE_DEFS] parse: %s\n", e.what()); return 0; }
    if (!j.is_array()) return 0;
    std::string ns=id_namespace?(std::string(id_namespace)+":"):"";
    int count=0,over=0,merged=0;
    for(auto& entry:j){
        bool is_patch=(mode==MergeMode::MergePatch&&entry.value("__patch",false));
        std::string rid=entry.value("id","");if(!ns.empty())rid=ns+rid;
        bool exists=g_dialogue_defs.count(rid)>0;
        if(mode==MergeMode::Skip&&exists)continue;
        if(is_patch&&exists){json stored;try{stored=json::parse(_dialogue_src[rid]);}catch(...){continue;}
            auto def=_parse_dialogue(_merge_patch(stored,entry));def.id=rid;
            g_dialogue_defs[rid]=def;_dialogue_src[rid]=_merge_patch(stored,entry).dump();merged++;continue;}
        auto def=_parse_dialogue(entry);
        if(def.id.empty())continue;if(!ns.empty())def.id=ns+def.id;
        if(exists){if(mode==MergeMode::Replace||mode==MergeMode::MergePatch){g_dialogue_defs[def.id]=def;over++;_dialogue_src[def.id]=entry.dump();}continue;}
        g_dialogue_defs[def.id]=def;count++;_dialogue_src[def.id]=entry.dump();
    }
    g_dialogue_defs_loaded=true;
    printf("[DIALOGUE_DEFS] +%d",count);if(over>0)printf(" ~%d",over);if(merged>0)printf(" %% %d",merged);printf("\n");
    return count+over+merged;
}

// ── 公开接口 ──

bool load_dialogue_defs(const std::string& json_path) {
    std::ifstream f(json_path);
    if (!f.is_open()) {
        printf("[DIALOGUE_DEFS] ERROR: cannot open %s\n", json_path.c_str());
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return load_dialogue_defs_from_json(text.c_str(), MergeMode::Skip) > 0;
}

const DialogueDef* get_dialogue_def(const std::string& id) {
    auto it = g_dialogue_defs.find(id);
    return (it != g_dialogue_defs.end()) ? &it->second : nullptr;
}

const std::unordered_map<std::string, DialogueDef>& get_all_dialogue_defs() {
    return g_dialogue_defs;
}

bool is_dialogue_defs_loaded() { return g_dialogue_defs_loaded; }
