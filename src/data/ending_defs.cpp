#include "data/ending_defs.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>

using json = nlohmann::json;

// ============================================================
// G2.5: EndingDef 全局注册表 (key = type int, 与 EndingType enum 一致)
// ============================================================

static std::unordered_map<int, EndingDef> g_ending_defs;
static bool g_ending_defs_loaded = false;

static EndingDef _parse_ending(const json& j) {
    EndingDef def;
    def.id         = j.value("id", "");
    def.type       = j.value("type", 0);
    def.name       = j.value("name", "");
    def.title      = j.value("title", "");
    def.sky_color  = j.value("sky_color", "");
    def.final_line = j.value("final_line", "");
    def.meta_soul_bonus      = j.value("meta_soul_bonus", 0);
    def.meta_knowledge_bonus = j.value("meta_knowledge_bonus", 0);
    return def;
}

int load_ending_defs_from_json(const char* json_text, MergeMode mode,
                                const char* id_namespace) {
    (void)id_namespace;
    if (!json_text || !json_text[0]) return 0;
    json j; try { j = json::parse(json_text); } catch(...) { return 0; }
    if (!j.is_array()) return 0;
    int count=0,skipped=0,dups=0,over=0;
    for(auto& entry:j){
        auto def=_parse_ending(entry);
        if(def.id.empty()){skipped++;continue;}
        if(g_ending_defs.count(def.type)){if(mode==MergeMode::Replace){g_ending_defs[def.type]=def;over++;}else dups++;continue;}
        g_ending_defs[def.type]=def;count++;
    }
    g_ending_defs_loaded=true;
    printf("[ENDING_DEFS] +%d",count);if(over>0)printf(" ~%d",over);printf("\n");
    return count+over;
}

bool load_ending_defs(const std::string& json_path) {
    std::ifstream f(json_path);
    if (!f.is_open()) {
        printf("[ENDING_DEFS] ERROR: cannot open %s\n", json_path.c_str());
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return load_ending_defs_from_json(text.c_str(), MergeMode::Skip) > 0;
}

const EndingDef* get_ending_def(int type) {
    auto it = g_ending_defs.find(type);
    return (it != g_ending_defs.end()) ? &it->second : nullptr;
}

const std::unordered_map<int, EndingDef>& get_all_ending_defs() {
    return g_ending_defs;
}

bool is_ending_defs_loaded() { return g_ending_defs_loaded; }
